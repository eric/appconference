#include "iaxclient_lib.h"

#if defined(__STDC__) || defined(_MSC_VER)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define DEFAULT_CALLERID_NAME    "Not Available"
#define DEFAULT_CALLERID_NUMBER  "7005551212"

struct iaxc_registration {
    struct iax_session *session;
    int firstpass;
    struct timeval last;
    char host[256];
    char user[256];
    char pass[256];
    long   refresh;
    struct iaxc_registration *next;
};

struct iaxc_registration *registrations = NULL;

struct iaxc_audio_driver audio;

static int iAudioType;

int audio_format_capability;
int audio_format_preferred;

void * post_event_handle = NULL;
int post_event_id = 0;

static int minimum_outgoing_framesize = 160; /* 20ms */

MUTEX iaxc_lock;

int netfd;
int port;
int c, i;

int iaxc_audio_output_mode = 0; // Normal

static int selected_call; // XXX to be protected by mutex?
static struct iaxc_call* calls;
static int nCalls;	// number of calls for this library session

struct timeval lastouttm;

static void iaxc_service_network();
static int service_audio();

/* external global networking replacements */
static iaxc_sendto_t   iaxc_sendto = sendto;
static iaxc_recvfrom_t iaxc_recvfrom = recvfrom;


static THREAD procThread;
#ifdef WIN32
static THREADID procThreadID;
#endif

/* QuitFlag: 0: Running 1: Should Quit, -1: Not Running */
static int procThreadQuitFlag = -1;

static void iaxc_do_pings(void);

iaxc_event_callback_t iaxc_event_callback = NULL;

EXPORT void iaxc_set_silence_threshold(double thr) {
    iaxc_silence_threshold = thr;
    iaxc_set_speex_filters();
}

EXPORT void iaxc_set_audio_output(int mode) {
    iaxc_audio_output_mode = mode;
}


EXPORT int iaxc_get_filters(void) {
      return iaxc_filters;
}

EXPORT void iaxc_set_filters(int filters) {
      iaxc_filters = filters;
      iaxc_set_speex_filters();
}


long iaxc_usecdiff( struct timeval *timeA, struct timeval *timeB ){
      long secs = timeA->tv_sec - timeB->tv_sec;
      long usecs = secs * 1000000;
      usecs += (timeA->tv_usec - timeB->tv_usec);
      return usecs;
}

EXPORT void iaxc_set_event_callback(iaxc_event_callback_t func) {
    iaxc_event_callback = func;
}

EXPORT int iaxc_set_event_callpost(void *handle, int id) {
    post_event_handle = handle;
    post_event_id = id;
    iaxc_event_callback = post_event_callback; 
    return 0;
}

EXPORT void iaxc_free_event(iaxc_event *e) {
    free(e);
}

EXPORT struct iaxc_ev_levels *iaxc_get_event_levels(iaxc_event *e) {
    return &e->ev.levels;
}
EXPORT struct iaxc_ev_text *iaxc_get_event_text(iaxc_event *e) {
    return &e->ev.text;
}
EXPORT struct iaxc_ev_call_state *iaxc_get_event_state(iaxc_event *e) {
    return &e->ev.call;
}


// Messaging functions
static void default_message_callback(char *message) {
  fprintf(stderr, "IAXCLIENT: ");
  fprintf(stderr, message);
  fprintf(stderr, "\n");
}

// Post Events back to clients
EXPORT void iaxc_post_event(iaxc_event e) {
    if(iaxc_event_callback)
    {
	int rv;
	rv = iaxc_event_callback(e);
	if(rv < 0) 
	  default_message_callback("IAXCLIENT: BIG PROBLEM, event callback returned failure!");
	// > 0 means processed
	if(rv > 0) return;

	// else, fall through to "defaults"
    }

    switch(e.type)
    {
	case IAXC_EVENT_TEXT:
	    default_message_callback(e.ev.text.message);
	// others we just ignore too
	return;
    }
}


#define IAXC_ERROR  IAXC_TEXT_TYPE_ERROR
#define IAXC_STATUS IAXC_TEXT_TYPE_STATUS
#define IAXC_NOTICE IAXC_TEXT_TYPE_NOTICE

void iaxc_usermsg(int type, const char *fmt, ...)
{
    va_list args;
    iaxc_event e;

    e.type=IAXC_EVENT_TEXT;
    e.ev.text.type=type;

    va_start(args, fmt);
#ifdef WIN32
    _vsnprintf(e.ev.text.message, IAXC_EVENT_BUFSIZ, fmt, args);
#else
    vsnprintf(e.ev.text.message, IAXC_EVENT_BUFSIZ, fmt, args);
#endif
    va_end(args);

    iaxc_post_event(e);
}


void iaxc_do_levels_callback(float input, float output)
{
    iaxc_event e;
    e.type = IAXC_EVENT_LEVELS;
    e.ev.levels.input = input;
    e.ev.levels.output = output;
    iaxc_post_event(e);
}

void iaxc_do_state_callback(int callNo)
{  
      iaxc_event e;   
      if(callNo < 0 || callNo >= nCalls) return;
      e.type = IAXC_EVENT_STATE;
      e.ev.call.callNo = callNo;
      e.ev.call.state = calls[callNo].state;
      e.ev.call.format = calls[callNo].format;
      strncpy(e.ev.call.remote,        calls[callNo].remote,        IAXC_EVENT_BUFSIZ);
      strncpy(e.ev.call.remote_name,   calls[callNo].remote_name,   IAXC_EVENT_BUFSIZ);
      strncpy(e.ev.call.local,         calls[callNo].local,         IAXC_EVENT_BUFSIZ);
      strncpy(e.ev.call.local_context, calls[callNo].local_context, IAXC_EVENT_BUFSIZ);
      iaxc_post_event(e);
}

EXPORT int iaxc_first_free_call()  {
	int i;
	for(i=0;i<nCalls;i++) 
	    if(calls[i].state == IAXC_CALL_STATE_FREE) 
		return i;
	
	return -1;
}


static void iaxc_clear_call(int toDump)
{
      // XXX libiax should handle cleanup, I think..
      calls[toDump].state = IAXC_CALL_STATE_FREE;
      calls[toDump].format = 0;
      calls[toDump].session = NULL;
      iaxc_do_state_callback(toDump);
}

/* select a call.  */
/* XXX Locking??  Start/stop audio?? */
EXPORT int iaxc_select_call(int callNo) {

	// continue if already selected?
	//if(callNo == selected_call) return;

	if(callNo >= nCalls) {
		iaxc_usermsg(IAXC_ERROR, "Error: tried to select out_of_range call %d", callNo);
		return -1;
	}
  
        // callNo < 0 means no call selected (i.e. all on hold)
	if(callNo < 0) {
	    selected_call = callNo;
	    return 0;
	}
  
	// de-select old call if not also the new call	
	if(callNo != selected_call) {
	    calls[selected_call].state &= ~IAXC_CALL_STATE_SELECTED;
	    selected_call = callNo;
	    iaxc_do_state_callback(selected_call);

	    calls[callNo].state |= IAXC_CALL_STATE_SELECTED;
	}


	// if it's an incoming call, and ringing, answer it.
	if( !(calls[selected_call].state & IAXC_CALL_STATE_OUTGOING) && 
	     (calls[selected_call].state & IAXC_CALL_STATE_RINGING)) {
	    iaxc_answer_call(selected_call);
	} else {
	// otherwise just update state (answer does this for us)
	  iaxc_do_state_callback(selected_call);
	}

	return 0;
}
	  
/* external API accessor */
EXPORT int iaxc_selected_call() {
	return selected_call;
}

EXPORT void iaxc_set_networking(iaxc_sendto_t st, iaxc_recvfrom_t rf) {
    iaxc_sendto = st;
    iaxc_recvfrom = rf;
}

// Parameters:
// audType - Define whether audio is handled by library or externally
EXPORT int iaxc_initialize(int audType, int inCalls) {
	int i;

	/* os-specific initializations: init gettimeofday fake stuff in
	 * Win32, etc) */
	os_init();

	MUTEXINIT(&iaxc_lock);

	if(iaxc_sendto == sendto) {
	    if ( (port = iax_init(0) < 0)) {
		    iaxc_usermsg(IAXC_ERROR, "Fatal error: failed to initialize iax with port %d", port);
		    return -1;
	    }
	    netfd = iax_get_fd();
	} else {
	    iax_set_networking(iaxc_sendto, iaxc_recvfrom);
	}

	nCalls = inCalls;
	/* initialize calls */
	if(nCalls == 0) nCalls = 1; /* 0 == Default? */

	/* calloc zeroes for us */
	calls = calloc(sizeof(struct iaxc_call), nCalls);
	if(!calls)
	{
		iaxc_usermsg(IAXC_ERROR, "Fatal error: can't allocate memory");
		return -1;
	}
	iAudioType = audType;
	selected_call = 0;

	for(i=0; i<nCalls; i++) {
	    strncpy(calls[i].callerid_name,   DEFAULT_CALLERID_NAME,   IAXC_EVENT_BUFSIZ);
	    strncpy(calls[i].callerid_number, DEFAULT_CALLERID_NUMBER, IAXC_EVENT_BUFSIZ);
	}

	gettimeofday(&lastouttm,NULL);
	switch (iAudioType) {
#ifdef USE_WIN_AUDIO
		case AUDIO_INTERNAL:
			if (win_initialize_audio() != 0)
				return -1;
			break;
#endif
		default:
		case AUDIO_INTERNAL_PA:
			if (pa_initialize(&audio))
				return -1;
			break;
		case AUDIO_INTERNAL_FILE:
			if (file_initialize(&audio))
				return -1;
			break;
	}

	audio_format_capability = IAXC_FORMAT_ULAW | IAXC_FORMAT_ALAW | IAXC_FORMAT_GSM | IAXC_FORMAT_SPEEX;
	audio_format_preferred = IAXC_FORMAT_SPEEX;

	return 0;
}

EXPORT void iaxc_shutdown() {
	iaxc_dump_all_calls();

	MUTEXLOCK(&iaxc_lock);
	audio.destroy(&audio);
	MUTEXUNLOCK(&iaxc_lock);

	MUTEXDESTROY(&iaxc_lock);
}


EXPORT void iaxc_set_formats(int preferred, int allowed)
{
	audio_format_capability = allowed;
	audio_format_preferred = preferred;
}

EXPORT void iaxc_set_min_outgoing_framesize(int samples) {
    minimum_outgoing_framesize = samples;
}

EXPORT void iaxc_set_callerid(char *name, char *number) {
    int i;
    
    for(i=0; i<nCalls; i++) {
        strncpy(calls[i].callerid_name,   name,   IAXC_EVENT_BUFSIZ);
        strncpy(calls[i].callerid_number, number, IAXC_EVENT_BUFSIZ);
    }
}

static void iaxc_note_activity(int callNo) {
  if(callNo < 0)
      return;
  //fprintf(stderr, "Noting activity for call %d\n", callNo);
  gettimeofday(&calls[callNo].last_activity, NULL);   
}

static void iaxc_do_pings(void) {
  int i;
  struct timeval now;

  gettimeofday(&now, NULL);
  for(i = 0; i < nCalls; i++)
  {
      long act_since;
      long ping_since;

      if(!(calls[i].state & IAXC_CALL_STATE_ACTIVE))
	  break;

      act_since = iaxc_usecdiff(&now, &calls[i].last_activity)/1000;

      // if we've had any activity in a while, don't worry about anything.
      if(act_since < IAXC_CALL_TIMEOUT/3)
	  break;  /* OK */

      ping_since = iaxc_usecdiff(&now, &calls[i].last_ping)/1000;

      /* if we haven't had activity in a while, and also haven't sent a
       * ping in a while, send a ping.
       */
      if(ping_since > IAXC_CALL_TIMEOUT/3) { 
	  //fprintf(stderr, "Sending Ping for call %d as=%ld, ps=%ld\n", i, act_since, ping_since); 
	  calls[i].last_ping = now;
	  iax_send_ping(calls[i].session);
	  break; 
      }

      /* finally, we've recently sent a ping, and still haven't had any 
       * activity.  If it's been longer then the timeout, timeout the call.
       */
      if(act_since > IAXC_CALL_TIMEOUT) {
	  /* timeout the call. */
	  //fprintf(stderr, "Timing out call %d as=%ld, ps=%ld\n", i, act_since, ping_since); 
	  iax_hangup(calls[i].session,"Timed out waiting for ping or activity");
	  iaxc_usermsg(IAXC_STATUS, "call %d timed out (ping/act = %ld/%ld)", i, ping_since/1000, act_since/1000);
	  iaxc_clear_call(i);
      }

  }

}

void iaxc_refresh_registrations() {
    struct iaxc_registration *cur;
    struct timeval now;

    gettimeofday(&now,NULL);

    for(cur = registrations; cur != NULL; cur=cur->next) {
	if(iaxc_usecdiff(&now, &cur->last) > cur->refresh ) {
	    //fprintf(stderr, "refreshing registration %s:%s@%s\n", cur->user, cur->pass, cur->host);

	    cur->session = iax_session_new();
	    if(!cur->session) {
		    iaxc_usermsg(IAXC_ERROR, "Can't make new registration session");
		    return;
	    }

	    iax_register(cur->session, cur->host, cur->user, cur->pass, 60);
	    cur->last = now;
	}
    }
}

EXPORT void iaxc_process_calls(void) {

#ifdef USE_WIN_AUDIO	
    win_flush_audio_output_buffers();
    if (iAudioType == AUDIO_INTERNAL) {
	    win_prepare_audio_buffers();
    }
#endif
    MUTEXLOCK(&iaxc_lock);
    iaxc_service_network();
    iaxc_do_pings();
    service_audio();
    iaxc_refresh_registrations();

    MUTEXUNLOCK(&iaxc_lock);
}

THREADFUNCDECL(iaxc_processor)
{
    THREADFUNCRET(ret);
    /* Increase Priority */
#ifdef WIN32
    /* Increasing the Thread Priority.  See
     * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/scheduling_priorities.asp
     * for discussion on Win32 scheduling priorities.
     */
      if ( !SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL)  ) {
            fprintf(stderr, "SetThreadPriority failed: %ld.\n", GetLastError());
      }
#endif
#ifdef MACOSX
    /* Presently, OSX allows user-level processes to request RT
     * priority.  The API is nice, but the scheduler presently ignores
     * the parameters (but the API validates that you're not asking for
     * too much).  See
     * http://lists.apple.com/archives/darwin-development/2004/Feb/msg00079.html
     */
    {
      struct thread_time_constraint_policy ttcpolicy;
      int params [2] = {CTL_HW,HW_BUS_FREQ};
      int hzms;
      size_t sz;
      int ret;

      /* get hz */
      sz = sizeof (hzms);
      sysctl (params, 2, &hzms, &sz, NULL, 0);

      /* make hzms actually hz per ms */
      hzms /= 1000;

      /* give us at least how much? 6-8ms every 10ms (we generally need less) */
      ttcpolicy.period = 10 * hzms; /* 10 ms */
      ttcpolicy.computation = 2 * hzms;
      ttcpolicy.constraint = 3 * hzms;
      ttcpolicy.preemptible = 1;

      if ((ret=thread_policy_set(mach_thread_self(),
        THREAD_TIME_CONSTRAINT_POLICY, (int *)&ttcpolicy,
        THREAD_TIME_CONSTRAINT_POLICY_COUNT)) != KERN_SUCCESS) {
            fprintf(stderr, "thread_policy_set failed: %d.\n", ret);
      }    
    }
#endif
#ifdef LINUX
    iaxc_prioboostbegin(); 
#endif

#if 0 /* test canary/watchdog */
	{ int i;
	  struct timeval then;
	  struct timeval now;
		gettimeofday(&then,NULL);
		sleep(2);
		for(i=0;i<5000000;i++) {
		  getpid();
		  gettimeofday(&now,NULL) ;
		  if(now.tv_sec != then.tv_sec) {
		      fprintf(stderr, "tick\n");
		      then = now;
		  }
		  fprintf(stderr, "");
		  getpid();
		}
	}
	fprintf(stderr, "DONE\n");
#endif
    while(1) { 
	iaxc_process_calls();
	iaxc_millisleep(5);	
	if(procThreadQuitFlag)
	  break;
    }
#ifdef LINUX
    iaxc_prioboostend(); 
#endif
    return ret;
}

EXPORT int iaxc_start_processing_thread()
{
      procThreadQuitFlag = 0;
      if( THREADCREATE(iaxc_processor, NULL, procThread, procThreadID) 
	    == THREADCREATE_ERROR)	
	  return -1;

      return 0;
}

EXPORT int iaxc_stop_processing_thread()
{
    if(procThreadQuitFlag >= 0)
    {
	procThreadQuitFlag = 1;
	THREADJOIN(procThread);
    }
    procThreadQuitFlag = -1;
    return 0;
}


static int service_audio()
{
	// we do this here to avoid looking at calls[-1]
	if(selected_call < 0) {
	    static int i=0;
	    if(i++ % 50 == 0) iaxc_do_levels_callback(-99,-99);

	    // make sure audio is stopped
	    audio.stop(&audio);
	    return 0;
	}	

	/* send audio only if incoming call answered, or outgoing call
	 * selected. */
	if( (calls[selected_call].state & IAXC_CALL_STATE_OUTGOING) 
	    || (calls[selected_call].state & IAXC_CALL_STATE_COMPLETE)) 
	{
	    short buf[1024];

	    // make sure audio is running
	    if(audio.start(&audio))
	    {
	    	iaxc_usermsg(IAXC_ERROR, "Can't start audio");
	    }

	    for(;;) {
		int toRead;
		int cmin;

	        /* find mimumum frame size */
	        toRead = minimum_outgoing_framesize; 

		/* use codec minimum if higher */
		if(calls[selected_call].encoder)
		   cmin = calls[selected_call].encoder->minimum_frame_size; 
		else
		   cmin = 1;

		if(cmin > toRead)
		    toRead = cmin;
	   
		/* round up to next multiple */
		if(toRead % cmin)
		  toRead += cmin - (toRead % cmin);

		if(toRead > sizeof(buf)/sizeof(short))
		{
		    fprintf(stderr, "internal error: toRead > sizeof(buf)\n");
		    exit(1);
		}

		if(audio.input(&audio,buf,&toRead))
		{
		    iaxc_usermsg(IAXC_ERROR, "ERROR reading audio\n");
		    break;
		}
		if(!toRead) break;  /* frame not available */
		/* currently, pa will always give us 0 or what we asked
		 * for samples */

		send_encoded_audio(&calls[selected_call], buf, 
		      calls[selected_call].format, toRead);
	    }

	} else {
	    static int i=0;
	    if(i++ % 50 == 0) iaxc_do_levels_callback(-99,-99);

	    // make sure audio is stopped
	    audio.stop(&audio);
	}
	return 0;
}

/* handle IAX text events */
void handle_text_event(struct iax_event *e, int callNo) {
    iaxc_event ev;

   if(callNo < 0)
       return;
    ev.type=IAXC_EVENT_TEXT;
    ev.ev.text.type=IAXC_TEXT_TYPE_IAX;
    ev.ev.text.callNo = callNo;

#ifdef IAXC_IAX2
    strncpy(ev.ev.text.message, e->data, IAXC_EVENT_BUFSIZ);
#endif
    iaxc_post_event(ev);
}

/* DANGER: bad things can happen if iaxc_netstat != iax_netstat.. */
EXPORT int iaxc_get_netstats(int call, int *rtt, struct iaxc_netstat *local, struct iaxc_netstat *remote) {
    return iax_get_netstats(calls[call].session, rtt, (struct iax_netstat *)local, (struct iax_netstat *)remote);
}

/* handle IAX text events */
static void generate_netstat_event(int callNo) {
    iaxc_event ev;
    int i = 0;

    if(callNo < 0)
       return;

    ev.type=IAXC_EVENT_NETSTAT;
    ev.ev.netstats.callNo = callNo;

    /* only post the event if the session is valid, etc */
    if(!iaxc_get_netstats(callNo, &ev.ev.netstats.rtt, &ev.ev.netstats.local, &ev.ev.netstats.remote))
	iaxc_post_event(ev);
}

void handle_audio_event(struct iax_event *e, int callNo) {
	int total_consumed = 0;
	int cur;
	short fr[1024];
	int samples;
	int bufsize = sizeof(fr)/sizeof(short);
	struct iaxc_call *call;

        if(callNo < 0)
            return;

	call = &calls[callNo];

	if(callNo != selected_call) {
	    /* drop audio for unselected call? */
	    return;
	}

	samples = bufsize;

	do {
		cur = decode_audio(call, fr + (bufsize - samples),
		    e->data+total_consumed,e->datalen-total_consumed,
		    call->format, &samples);

		if(cur < 0) {
		    iaxc_usermsg(IAXC_STATUS, "Bad or incomplete voice packet.  Unable to decode. dropping");
		    return;
		}

		total_consumed += cur;
		if(iaxc_audio_output_mode != 0) 
		    continue;
		audio.output(&audio,fr,bufsize-samples);
	} while(total_consumed < e->datalen);
}


void iaxc_handle_network_event(struct iax_event *e, int callNo)
{

        if(callNo < 0)
            return;

	iaxc_note_activity(callNo);

	switch(e->etype) {
		case IAX_EVENT_HANGUP:
			iaxc_usermsg(IAXC_STATUS, "Call disconnected by remote");
			// XXX does the session go away now?
			iaxc_clear_call(callNo);
			
			break;

		case IAX_EVENT_REJECT:
			iaxc_usermsg(IAXC_STATUS, "Call rejected by remote");
			iaxc_clear_call(callNo);
			break;
		case IAX_EVENT_ACCEPT:
			calls[callNo].state |= IAXC_CALL_STATE_RINGING;	
			calls[callNo].format = e->ies.format;
	  //fprintf(stderr, "outgoing call remote accepted, format=%d\n", e->ies.format);
			iaxc_do_state_callback(callNo);
			iaxc_usermsg(IAXC_STATUS,"Call %d ringing", callNo);
//			issue_prompt(f);
			break;
		case IAX_EVENT_ANSWER:
			calls[callNo].state &= ~IAXC_CALL_STATE_RINGING;	
			calls[callNo].state |= IAXC_CALL_STATE_COMPLETE;	
			iaxc_do_state_callback(callNo);
			iaxc_usermsg(IAXC_STATUS,"Call %d answered", callNo);
			//iaxc_answer_call(callNo);
			// notify the user?
 			break;
		case IAX_EVENT_VOICE:
			handle_audio_event(e, callNo); 
			break;
		case IAX_EVENT_TEXT:
			handle_text_event(e, callNo);
			break;
		case IAX_EVENT_RINGA:
			break;
		case IAX_EVENT_PONG:  /* we got a pong */
			//fprintf(stderr, "**********GOT A PONG!\n");
			generate_netstat_event(callNo);
			break;
		default:
			iaxc_usermsg(IAXC_STATUS, "Unknown event: %d for call %d", e->etype, callNo);
			break;
	}
}

EXPORT void iaxc_register(char *user, char *pass, char *host)
{
	struct iaxc_registration *newreg;

	newreg = malloc(sizeof (struct iaxc_registration));
	if(!newreg) {
		iaxc_usermsg(IAXC_ERROR, "Can't make new registration");
		return;
	}

	newreg->session = iax_session_new();
	if(!newreg->session) {
		iaxc_usermsg(IAXC_ERROR, "Can't make new registration session");
		return;
	}

	gettimeofday(&newreg->last,NULL);
	newreg->refresh = 60*1000*1000;  // 60 seconds, in usecs

	strncpy(newreg->host, host, 256);
	strncpy(newreg->user, user, 256);
	strncpy(newreg->pass, pass, 256);

	// so we notify the user.
	newreg->firstpass = 1;

	// send out the initial registration timeout 300 seconds
	iax_register(newreg->session, host, user, pass, 300);

	// add it to the list;
	newreg->next = registrations;
	registrations = newreg;
}

EXPORT void iaxc_call(char *num)
{
	int callNo;
	struct iax_session *newsession;
	char *ext = strstr(num, "/");

	MUTEXLOCK(&iaxc_lock);

        // if no call is selected, get a new appearance
        if(selected_call < 0) {
            callNo = iaxc_first_free_call();
        } else {
            // use selected call if not active, otherwise, get a new appearance
            if(calls[selected_call].state  & IAXC_CALL_STATE_ACTIVE) {
                callNo = iaxc_first_free_call();
            } else {
                 callNo = selected_call;
            }
        }

	if(callNo < 0) {
		iaxc_usermsg(IAXC_STATUS, "No free call appearances");
		goto iaxc_call_bail;
	}

	newsession = iax_session_new();
	if(!newsession) {
		iaxc_usermsg(IAXC_ERROR, "Can't make new session");
		goto iaxc_call_bail;
	}

	calls[callNo].session = newsession;

	/* XXX leak???? */
	calls[callNo].encoder = 0;
	calls[callNo].decoder = 0;

	if(ext) {
	    strncpy(calls[callNo].remote_name, num, IAXC_EVENT_BUFSIZ); 
    	    strncpy(calls[callNo].remote,    ++ext, IAXC_EVENT_BUFSIZ);
    	} else {
	    strncpy(calls[callNo].remote_name, num, IAXC_EVENT_BUFSIZ);
    	    strncpy(calls[callNo].remote,      "" , IAXC_EVENT_BUFSIZ);
     	}

 	strncpy(calls[callNo].local        , calls[callNo].callerid_name, IAXC_EVENT_BUFSIZ);
	strncpy(calls[callNo].local_context, "default", IAXC_EVENT_BUFSIZ);

	calls[callNo].state = IAXC_CALL_STATE_ACTIVE | IAXC_CALL_STATE_OUTGOING;

	/* reset activity and ping "timers" */
	iaxc_note_activity(callNo);
	calls[callNo].last_ping = calls[callNo].last_activity;

#ifdef IAXC_IAX2
	iax_call(calls[callNo].session, calls[callNo].callerid_number,
	                                calls[callNo].callerid_name, num, NULL, 0,
					audio_format_preferred, audio_format_capability);
#else
	iax_call(calls[callNo].session, calls[callNo].callerid_number, num, NULL, 0);
#endif

	// does state stuff also
	iaxc_select_call(callNo);

iaxc_call_bail:
	MUTEXUNLOCK(&iaxc_lock);
}

EXPORT void iaxc_answer_call(int callNo) 
{
	if(callNo < 0)
	    return;
	    
	//fprintf(stderr, "iaxc answering call %d\n", callNo);
	calls[callNo].state |= IAXC_CALL_STATE_COMPLETE;
	calls[callNo].state &= ~IAXC_CALL_STATE_RINGING;
	iax_answer(calls[callNo].session);
	iaxc_do_state_callback(callNo);
}

EXPORT void iaxc_blind_transfer_call(int callNo, char *DestExtn)
{
	iax_transfer(calls[callNo].session, DestExtn);
}

static void iaxc_dump_one_call(int callNo)
{
      if(callNo < 0)
          return;
      if(calls[callNo].state == IAXC_CALL_STATE_FREE) return;
      
      iax_hangup(calls[callNo].session,"Dumped Call");
      iaxc_usermsg(IAXC_STATUS, "Hanging up call %d", callNo);
      iaxc_clear_call(callNo);
}

EXPORT void iaxc_dump_all_calls(void)
{
      int callNo;
      MUTEXLOCK(&iaxc_lock);
	for(callNo=0; callNo<nCalls; callNo++)
	    iaxc_dump_one_call(callNo);
      MUTEXUNLOCK(&iaxc_lock);
}


EXPORT void iaxc_dump_call(void)
{
    if(selected_call >= 0) {
	MUTEXLOCK(&iaxc_lock);
	iaxc_dump_one_call(selected_call);
	MUTEXUNLOCK(&iaxc_lock);
    }
}

EXPORT void iaxc_reject_call(void)
{
    if(selected_call >= 0) {
	MUTEXLOCK(&iaxc_lock);
	// XXX should take callNo?
	iax_reject(calls[selected_call].session, "Call rejected manually.");
	iaxc_clear_call(selected_call);
	MUTEXUNLOCK(&iaxc_lock);
    }
}

EXPORT void iaxc_send_dtmf(char digit)
{
    if(selected_call >= 0) {
	MUTEXLOCK(&iaxc_lock);
	if(calls[selected_call].state & IAXC_CALL_STATE_ACTIVE)
		iax_send_dtmf(calls[selected_call].session,digit);
	MUTEXUNLOCK(&iaxc_lock);
    }
}

static int iaxc_find_call_by_session(struct iax_session *session)
{
	int i;
	for(i=0;i<nCalls;i++)
		if (calls[i].session == session)
			return i;
	return -1;
}

static void iaxc_handle_regreply(struct iax_event *e) {
  struct iaxc_registration *cur;
  // find the registration session

    for(cur = registrations; cur != NULL; cur=cur->next) 
	if(cur->session == e->session) break;

    if(!cur) {
	iaxc_usermsg(IAXC_ERROR, "Unexpected registration reply");
	return;
    }

    if(cur->firstpass) {
	cur->firstpass = 0;
      
#ifdef IAXC_IAX2
	if(e->etype == IAX_EVENT_REGACK ) {
	    iaxc_usermsg(IAXC_STATUS, "Registration accepted");
	} else if(e->etype == IAX_EVENT_REGREJ ) {
	    iaxc_usermsg(IAXC_STATUS, "Registration rejected");
	}
#else // IAX1

	if(e->event.regreply.status == IAX_REG_SUCCESS)
	    iaxc_usermsg(IAXC_STATUS, "Registration accepted");
	else if(e->event.regreply.status == IAX_REG_REJECT)
	    iaxc_usermsg(IAXC_STATUS, "Registration rejected");
	    // XXX should remove from registrations list?
	else if(e->event.regreply.status == IAX_REG_TIMEOUT)
	    iaxc_usermsg(IAXC_STATUS, "Registration timed out");
	else
	    iaxc_usermsg(IAXC_ERROR, "Unknown registration event");
#endif
    }

    // XXX I think the session is no longer valid.. at least, that's
    // what miniphone does, and re-using the session doesn't seem to
    // work!
    iax_destroy(cur->session);
    cur->session = NULL;
}

/* this is what asterisk does */
static int iaxc_choose_codec(int formats) {
    int i;
    static int codecs[] = {
      IAXC_FORMAT_ULAW,
      IAXC_FORMAT_ALAW,
      IAXC_FORMAT_SLINEAR,
      IAXC_FORMAT_G726,
      IAXC_FORMAT_ADPCM,
      IAXC_FORMAT_GSM,
      IAXC_FORMAT_ILBC,
      IAXC_FORMAT_SPEEX,
      IAXC_FORMAT_LPC10,
      IAXC_FORMAT_G729A,
      IAXC_FORMAT_G723_1,
    };
    for(i=0;i<sizeof(codecs)/sizeof(int);i++)
	if(codecs[i] & formats)
	    return codecs[i];
    return 0;
}

static void iaxc_service_network() { 
	struct iax_event *e = 0;
	int callNo;

	while ( (e = iax_get_event(0))) {
		// first, see if this is an event for one of our calls.
		callNo = iaxc_find_call_by_session(e->session);
		if(callNo >= 0) {
			iaxc_handle_network_event(e, callNo);
		} else if 
#ifndef IAXC_IAX2
		( e->etype == IAX_EVENT_REGREP )
#else 
		((e->etype == IAX_EVENT_REGACK ) || (e->etype == IAX_EVENT_REGREJ ))
#endif
		{ 
		    iaxc_handle_regreply(e);
		} else if(e->etype == IAX_EVENT_REGREQ ) {
			iaxc_usermsg(IAXC_ERROR, "Registration requested by someone, but we don't understand!");
		} else  if(e->etype == IAX_EVENT_CONNECT) {
			
			int format = 0;
			callNo = iaxc_first_free_call();

			if(callNo < 0) {
				iaxc_usermsg(IAXC_STATUS, "Incoming Call, but no appearances");
				// XXX Reject this call!, or just ignore?
				iax_reject(e->session, "Too many calls, we're busy!");
				goto bail;
			}

			/* negotiate codec */
			/* first, try _their_ preferred format */
			format = audio_format_capability & e->ies.format; 

			if(!format) {
			    /* then, try our preferred format */
			    format = audio_format_preferred & e->ies.capability; 
			}

			if(!format) {
			    /* finally, see if we have one in common */
			    format = audio_format_capability & e->ies.capability; 

			    /* now choose amongst these, if we got one */
			    if(format)
			    {
				format=iaxc_choose_codec(format);
			    }
			}
			
			if(!format) {
			    iax_reject(e->session, "Could not negotiate common codec");
			    goto bail;
			}

			calls[callNo].format = format;


#ifndef IAXC_IAX2			  
			if(e->event.connect.dnid)
			    strncpy(calls[callNo].local,e->event.connect.dnid,
				IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].local,"unknown",
				IAXC_EVENT_BUFSIZ);

			if(e->event.connect.callerid)
			    strncpy(calls[callNo].remote,
				e->event.connect.callerid, IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].remote,
				"unknown", IAXC_EVENT_BUFSIZ);
#else
			if(e->ies.called_number)
			    strncpy(calls[callNo].local,e->ies.called_number,
				IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].local,"unknown",
				IAXC_EVENT_BUFSIZ);

			if(e->ies.called_context)
			    strncpy(calls[callNo].local_context,e->ies.called_context,
				IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].local_context,"",
    				IAXC_EVENT_BUFSIZ);

			if(e->ies.calling_number)
			    strncpy(calls[callNo].remote,
  				e->ies.calling_number, IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].remote,
    				"unknown", IAXC_EVENT_BUFSIZ);

			if(e->ies.calling_name)
			    strncpy(calls[callNo].remote_name,
    				e->ies.calling_name, IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].remote_name,
    				"unknown", IAXC_EVENT_BUFSIZ);
#endif
			iaxc_note_activity(callNo);
			iaxc_usermsg(IAXC_STATUS, "Call from (%s)", calls[callNo].remote);

			/* XXX leak? */
			calls[callNo].encoder = 0;
			calls[callNo].decoder = 0;
			calls[callNo].session = e->session;
			calls[callNo].state = IAXC_CALL_STATE_ACTIVE|IAXC_CALL_STATE_RINGING;

			iax_accept(calls[callNo].session,format);
			iax_ring_announce(calls[callNo].session);

			iaxc_do_state_callback(callNo);

			iaxc_usermsg(IAXC_STATUS, "Incoming call on line %d", callNo);

		} else {
			iaxc_usermsg(IAXC_STATUS, "Event (type %d) for a non-existant session.  Dropping", e->etype);
		}
bail:
		iax_event_free(e);
	}
}

static void iaxc_external_audio_event(struct iax_event *e, struct iaxc_call *call)
{
	// To be coded in the future
	return;
}

static void iaxc_external_service_audio()
{
	// To be coded in the future
	return;
}

EXPORT int iaxc_audio_devices_get(struct iaxc_audio_device **devs, int *nDevs, int *input, int *output, int *ring) {
      *devs = audio.devices;
      *nDevs = audio.nDevices;
      audio.selected_devices(&audio,input,output,ring);
      return 0;
}

EXPORT int iaxc_audio_devices_set(int input, int output, int ring) {
    int ret = 0;
    MUTEXLOCK(&iaxc_lock);
    ret = audio.select_devices(&audio, input, output, ring);
    MUTEXUNLOCK(&iaxc_lock);
    return ret;
}

EXPORT double iaxc_input_level_get() {
    return audio.input_level_get(&audio);
}

EXPORT double iaxc_output_level_get() {
    return audio.output_level_get(&audio);
}

EXPORT int iaxc_input_level_set(double level) {
    return audio.input_level_set(&audio, level);
}

EXPORT int iaxc_output_level_set(double level) {
    return audio.output_level_set(&audio, level);
}

EXPORT int iaxc_play_sound(struct iaxc_sound *s, int ring) {
    return audio.play_sound(s,ring);
}

EXPORT int iaxc_stop_sound(int id) {
    return audio.stop_sound(id);
}

EXPORT int iaxc_quelch(int callNo, int MOH)
{
	struct iax_session *session = calls[callNo].session;
	if (!session)
		return -1;

	return iax_quelch_moh(session, MOH);
}

EXPORT int iaxc_unquelch(int call)
{
	return iax_unquelch(calls[call].session);
}

EXPORT int iaxc_mic_boost_get( void )
{
	return audio.mic_boost_get( &audio ) ;
}

EXPORT int iaxc_mic_boost_set( int enable )
{
	return audio.mic_boost_set( &audio, enable ) ;
}
