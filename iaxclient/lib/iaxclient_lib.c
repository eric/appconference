#include "iaxclient_lib.h"

#if defined(__STDC__) || defined(_MSC_VER)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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


static int iAudioType;
static int iEncodeType;

MUTEX iaxc_lock;

int netfd;
int port;
int c, i;
char rcmd[RBUFSIZE];

int iaxc_audio_output_mode = 0; // Normal

static int selected_call; // XXX to be protected by mutex?
static struct iaxc_call* calls;
static int nCalls;	// number of calls for this library session

struct timeval lastouttm;

static void do_iax_event();

static THREAD procThread;
static THREADID procThreadID;

/* QuitFlag: 0: Running 1: Should Quit, -1: Not Running */
static int procThreadQuitFlag = -1;

iaxc_event_callback_t iaxc_event_callback = NULL;

void iaxc_set_silence_threshold(double thr) {
    iaxc_silence_threshold = thr;
}

void iaxc_set_audio_output(int mode) {
    iaxc_audio_output_mode = mode;
}

long iaxc_usecdiff( struct timeval *timeA, struct timeval *timeB ){
      long secs = timeA->tv_sec - timeB->tv_sec;
      long usecs = secs * 1000000;
      usecs += (timeA->tv_usec - timeB->tv_usec);
      return usecs;
}


void iaxc_set_event_callback(iaxc_event_callback_t func) {
    iaxc_event_callback = func;
}

// Messaging functions
static void default_message_callback(char *message) {
  fprintf(stderr, "IAXCLIENT: ");
  fprintf(stderr, message);
  fprintf(stderr, "\n");
}

// Post Events back to clients
void iaxc_post_event(iaxc_event e) {
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

static iaxc_usermsg(int type, const char *fmt, ...)
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
      strncpy(e.ev.call.remote, calls[callNo].remote, IAXC_EVENT_BUFSIZ);
      iaxc_post_event(e);
}

static int iaxc_next_free_call()  {
	int i;
	for(i=0;i<nCalls;i++) 
	    if(calls[i].session==NULL) 
		return i;
	
	return -1;
}

static int iaxc_clear_call(int toDump)
{
      if(selected_call == toDump) iaxc_select_call(-1);

      // XXX libiax should handle cleanup, I think..
      calls[toDump].session = NULL;
      calls[toDump].state = IAXC_CALL_STATE_FREE;
      iaxc_do_state_callback(toDump);
}

/* select a call.  -1 == no call */
/* XXX Locking??  Start/stop audio?? */
int iaxc_select_call(int callNo) {
	if(callNo < -1 || callNo >= nCalls) {
		iaxc_usermsg(IAXC_ERROR, "Error: tried to select out_of_range call %d", callNo);
		return -1;
	}

	if(!calls[callNo].session) {
		iaxc_usermsg(IAXC_ERROR, "Error: tried to select inactive call", callNo);
		return -1;
	}
   
	if(selected_call >= 0) {	
	    calls[selected_call].state &= ~IAXC_CALL_STATE_SELECTED;
	    iaxc_do_state_callback(selected_call);
	}

	selected_call = callNo;

	if(callNo >= 0) {
	    calls[callNo].state |= IAXC_CALL_STATE_SELECTED;


	    // if it's an incoming call, and ringing, answer it.
	    if( !(calls[selected_call].state & IAXC_CALL_STATE_OUTGOING) && 
		 (calls[selected_call].state & IAXC_CALL_STATE_RINGING)) {
		iaxc_answer_call(selected_call);
	    } else {
	    // otherwise just update state (answer does this for us)
	      iaxc_do_state_callback(selected_call);
	    }
	    // should do callback to say all are unselected...
	}
}
	  
/* external API accessor */
int iaxc_selected_call() {
	return selected_call;
}

// Parameters:
// audType - Define whether audio is handled by library or externally
int iaxc_initialize(int audType, int inCalls) {
	int i;

	/* os-specific initializations: init gettimeofday fake stuff in
	 * Win32, etc) */
	os_init();

	MUTEXINIT(&iaxc_lock);

	if ( (port = iax_init(0) < 0)) {
		iaxc_usermsg(IAXC_ERROR, "Fatal error: failed to initialize iax with port %d", port);
		return -1;
	}
	netfd = iax_get_fd();

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
	selected_call = -1;

	gettimeofday(&lastouttm,NULL);
	switch (iAudioType) {
		case AUDIO_INTERNAL:
#ifdef USE_WIN_AUDIO
			if (win_initialize_audio() != 0)
				return -1;
#endif
			break;
		case AUDIO_INTERNAL_PA:
			if (pa_initialize_audio() != 0)
				return -1;
			break;
	}
	return 0;
}

void iaxc_shutdown() {
	MUTEXLOCK(&iaxc_lock);
	iaxc_dump_all_calls();
	switch (iAudioType) {
		case AUDIO_INTERNAL:
#ifdef USE_WIN_AUDIO
			win_shutdown_audio();
#endif
			break;
		case AUDIO_INTERNAL_PA:
			pa_shutdown_audio();
			break;
	}
	MUTEXUNLOCK(&iaxc_lock);
	MUTEXDESTROY(&iaxc_lock);
}


void iaxc_set_encode_format(int fmt)
{
	iEncodeType = fmt;
	iax_set_formats(fmt);
}

void iaxc_refresh_registrations() {
    struct iaxc_registration *cur;
    struct timeval now;

    gettimeofday(&now,NULL);

    for(cur = registrations; cur != NULL; cur=cur->next) {
	if(iaxc_usecdiff(&now, &cur->last) > cur->refresh ) {
	    fprintf(stderr, "refreshing registration %s:%s@%s\n", 
		cur->host, cur->user, cur->pass, 300);

	    cur->session = iax_session_new();
	    if(!cur->session) {
		    iaxc_usermsg(IAXC_ERROR, "Can't make new registration session");
		    return;
	    }

	    iax_register(cur->session, cur->host, cur->user, cur->pass, 300);
	    cur->last = now;
	}
    }
}

void iaxc_process_calls(void) {

#ifdef USE_WIN_AUDIO	
    win_flush_audio_output_buffers();
    if (iAudioType == AUDIO_INTERNAL) {
	    win_prepare_audio_buffers();
    }
#endif
    MUTEXLOCK(&iaxc_lock);
    iaxc_service_network(netfd);
    service_audio();
    iaxc_refresh_registrations();

    MUTEXUNLOCK(&iaxc_lock);
}

THREADFUNCDECL(iaxc_processor)
{
    THREADFUNCRET(ret);
    while(1) { 
	iaxc_process_calls();
	iaxc_millisleep(10);	
	if(procThreadQuitFlag)
	  break;
    }
    return ret;
}

int iaxc_start_processing_thread()
{
      procThreadQuitFlag = 0;
      if( THREADCREATE(iaxc_processor, NULL, procThread, procThreadID) 
	    == THREADCREATE_ERROR)	
	  return -1;

      return 0;
}

int iaxc_stop_processing_thread()
{
    if(procThreadQuitFlag >= 0)
    {
	procThreadQuitFlag = 1;
	// It will die eventually on it's own?
	// causes deadlock with wx GUI on MSW.. XXX FixME?
	//THREADJOIN(procThread);
    }
    procThreadQuitFlag = -1;
    return 0;
}


int service_audio()
{
	/* do audio input stuff for buffers that have received data from audio in device already. Must
		do them in serial number order (the order in which they were originally queued). */
	if(selected_call >= 0) /* send audio only if call answered */
	{
		switch (iAudioType) {
			case AUDIO_INTERNAL:
				iaxc_service_network(netfd);
#ifdef USE_WIN_AUDIO			
				win_process_audio_buffers(&lastouttm, &calls[selected_call], iEncodeType);		
#endif
				iaxc_service_network(netfd);
				break;
			case AUDIO_INTERNAL_PA:
				iaxc_service_network(netfd);
				pa_send_audio(&lastouttm, &calls[selected_call], iEncodeType);
				break;
			default:
				iaxc_service_network(netfd);
				iaxc_external_service_audio();
				iaxc_service_network(netfd);
				break;
		}
	} else {
		static int i=0;
		if(i++ % 50 == 0) iaxc_do_levels_callback(-99,-99);
	}
	return 0;
}


void handle_audio_event(struct iax_event *e, int callNo) {
	int total_consumed = 0;
	int cur;
	short fr[160];
	struct iaxc_call *call;

	call = &calls[callNo];

	if(callNo != selected_call) {
	    /* drop audio for unselected call? */
	    return;
	}

#ifdef IAXC_IAX2
	while(total_consumed < e->datalen) {
		cur = decode_audio(call, fr,
		    e->data,e->datalen-total_consumed,
		    iEncodeType);
#else
	while(total_consumed < e->event.voice.datalen) {
		cur = decode_audio(call, fr,
		    e->event.voice.data,e->event.voice.datalen-total_consumed,
		    iEncodeType);
#endif
		if(cur < 0) {
			iaxc_usermsg(IAXC_STATUS, "Bad or incomplete voice packet.  Unable to decode. dropping");
			return;
		} else {  /* its an audio packet to be output to user */
			total_consumed += cur;
			if(iaxc_audio_output_mode != 0) continue;
			switch (iAudioType) {
				case AUDIO_INTERNAL:
#ifdef USE_WIN_AUDIO
					win_flush_audio_output_buffers();
					win_play_recv_audio(fr, sizeof(fr));
#else
#endif
					break;

				case AUDIO_INTERNAL_PA:
					pa_play_recv_audio(fr, sizeof(fr));
					break;
				case AUDIO_EXTERNAL:
					// Add external audio callback here
					break;
			}
		}
	}
}

void iaxc_handle_network_event(struct iax_event *e, int callNo)
{
//	int len,n;
//	WHOUT *wh,*wh1;
//	short fr[160];
//	static paused_xmit = 0;


	switch(e->etype) {
		case IAX_EVENT_HANGUP:
#ifndef IAXC_IAX2  /* IAX2 barfs from this.  Should we do this or not? */
			
			iax_hangup(calls[callNo].session, "Byeee!");
#endif
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
		case IAX_EVENT_RINGA:
			break;
		default:
			iaxc_usermsg(IAXC_STATUS, "Unknown event: %d for call %d", e->etype, callNo);
			break;
	}
}

void iaxc_register(char *user, char *pass, char *host)
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

void iaxc_call(char *num)
{
	int callNo;
	struct iax_session *newsession;

	callNo = iaxc_next_free_call();
	if(callNo < 0) {
		iaxc_usermsg(IAXC_STATUS, "No free call appearances");
		return;
	}

	newsession = iax_session_new();
	if(!newsession) {
		iaxc_usermsg(IAXC_ERROR, "Can't make new session");
		return;
	}

	calls[callNo].session = newsession;

	/* XXX ??? */
	calls[callNo].gsmin = 0;
	calls[callNo].gsmout = 0;

	strncpy(calls[callNo].remote,num,IAXC_EVENT_BUFSIZ);
	calls[callNo].state = IAXC_CALL_STATE_ACTIVE | IAXC_CALL_STATE_OUTGOING;


#ifdef IAXC_IAX2
	iax_call(calls[callNo].session, "7001234567", "IAXClient User", num, NULL, 0);
#else
	iax_call(calls[callNo].session, "7001234567", num, NULL, 10);
#endif

	// does state stuff
	iaxc_select_call(callNo);
}

void iaxc_answer_call(int callNo) 
{
	fprintf(stderr, "iaxc answering call %d\n", callNo);
	calls[callNo].state |= IAXC_CALL_STATE_COMPLETE;
	calls[callNo].state &= ~IAXC_CALL_STATE_RINGING;
	iax_answer(calls[callNo].session);
	iaxc_do_state_callback(callNo);
}

static void iaxc_dump_one_call(int callNo)
{
      if(calls[callNo].state == IAXC_CALL_STATE_FREE) return;
      
      iax_hangup(calls[callNo].session,"Dumped Call");
      iaxc_usermsg(IAXC_STATUS, "Hanging up call %d", callNo);
      iaxc_clear_call(callNo);
}

void iaxc_dump_all_calls(void)
{
      int callNo;
      MUTEXLOCK(&iaxc_lock);
	for(callNo=0; callNo<nCalls; callNo++)
	    iaxc_dump_one_call(callNo);
      MUTEXUNLOCK(&iaxc_lock);
}


void iaxc_dump_call(void)
{
	int toDump = selected_call;
	MUTEXLOCK(&iaxc_lock);
	if(toDump < 0) {
	    iaxc_usermsg(IAXC_ERROR, "Error: tried to dump but no call selected");
	} else {
	    iaxc_dump_one_call(selected_call);
	}
	MUTEXUNLOCK(&iaxc_lock);
}

void iaxc_reject_call(void)
{
	MUTEXLOCK(&iaxc_lock);
	// XXX should take callNo?
	iax_reject(calls[selected_call].session, "Call rejected manually.");
	iaxc_clear_call(selected_call);
	MUTEXUNLOCK(&iaxc_lock);
}

void iaxc_send_dtmf(char digit)
{
	MUTEXLOCK(&iaxc_lock);
	if(selected_call >= 0)
		iax_send_dtmf(calls[selected_call].session,digit);
	MUTEXUNLOCK(&iaxc_lock);
}

static int iaxc_find_call_by_session(struct iax_session *session)
{
	int i;
	for(i=0;i<nCalls;i++)
		if (calls[i].session == session)
			return i;
	return -1;
}

/* handle all network requests, and a pending scheduled event, if any */
void iaxc_service_network(int netfd)
{
	fd_set readfd;
	struct timeval dumbtimer;

	/* set up a timer that falls-through */
	dumbtimer.tv_sec = 0;
	dumbtimer.tv_usec = 0;


		for(;;) /* suck everything outa network stuff */
		{
			FD_ZERO(&readfd);
			FD_SET(netfd, &readfd);
			if (select(netfd + 1, &readfd, 0, 0, &dumbtimer) > 0)
			{
				if (FD_ISSET(netfd,&readfd))
				{
					do_iax_event();
					(void) iax_time_to_next_event();
				} else break;
			} else break;
		}
		do_iax_event(); /* do pending event if any */
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
    cur->session = NULL;
}


static void do_iax_event() {
	struct iax_event *e = 0;
	int callNo;
	struct iax_session *session;

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
			
			callNo = iaxc_next_free_call();

			if(callNo < 0) {
				iaxc_usermsg(IAXC_STATUS, "Incoming Call, but no appearances");
				// XXX Reject this call!, or just ignore?
				iax_reject(e->session, "Too many calls, we're busy!");
				goto bail;
			}

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

			if(e->ies.calling_number)
			    strncpy(calls[callNo].remote,
				e->ies.calling_number, IAXC_EVENT_BUFSIZ);
			else
			    strncpy(calls[callNo].remote,
				"unknown", IAXC_EVENT_BUFSIZ);
#endif
			iaxc_usermsg(IAXC_STATUS, "Call from (%s)", calls[callNo].remote);

			calls[callNo].gsmin = 0;
			calls[callNo].gsmout = 0;
			calls[callNo].session = e->session;
			calls[callNo].state = IAXC_CALL_STATE_ACTIVE|IAXC_CALL_STATE_RINGING;


			// should we even accept?  or, we accept, but
			// don't necessarily answer..
			iax_accept(calls[callNo].session);
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

void iaxc_external_audio_event(struct iax_event *e, struct iaxc_call *call)
{
	// To be coded in the future
	return;
}

void iaxc_external_service_audio()
{
	// To be coded in the future
	return;
}
