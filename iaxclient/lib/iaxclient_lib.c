#include "iaxclient_lib.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


static int iAudioType;
static int iEncodeType;

int netfd;
int port;
int c, i;
char rcmd[RBUFSIZE];

int iaxc_audio_output_mode = 0; // Normal
static int answered_call;
static struct iax_session *newcall;

static struct peer *most_recent_answer=NULL;
static struct peer *peers;

struct timeval lastouttm;

static struct peer * find_peer(struct iax_session *session);
static void do_iax_event();

static THREAD procThread;
static THREADID procThreadID;

/* QuitFlag: 0: Running 1: Should Quit, -1: Not Running */
static int procThreadQuitFlag = -1;


iaxc_levels_callback_t iaxc_levels_callback = NULL;

void iaxc_set_levels_callback(iaxc_levels_callback_t func) {
    iaxc_levels_callback = func;
}

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


// Messaging functions
static void default_message_callback(char *message) {
  fprintf(stderr, "IAXCLIENT: ");
  fprintf(stderr, message);
  fprintf(stderr, "\n");
}
iaxc_message_callback_t iaxc_error_callback = default_message_callback;
iaxc_message_callback_t iaxc_status_callback = default_message_callback;

void iaxc_set_error_callback(iaxc_message_callback_t func) {
    iaxc_error_callback = func;
}

void iaxc_set_status_callback(iaxc_message_callback_t func) {
    iaxc_status_callback = func;
}

#define IAXC_ERROR  1
#define IAXC_STATUS 2

static iaxc_usermsg(int type, const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
#ifdef WIN32
    _vsnprintf(buf, 250, fmt, args);
#else
    vsnprintf(buf, 250, fmt, args);
#endif
    va_end(args);

    if(type == IAXC_ERROR)
	iaxc_error_callback(buf);
    else
	iaxc_status_callback(buf);
}



// Parameters:
// audType - Define whether audio is handled by library or externally
int iaxc_initialize(int audType) {
	/* get time of day in milliseconds, offset by tick count (see our
	   gettimeofday() implementation) */
	os_init();

	if ( (port = iax_init(0) < 0)) {
		iaxc_usermsg(IAXC_ERROR, "Fatal error: failed to initialize iax with port %d", port);
		return -1;
	}
	netfd = iax_get_fd();

	iAudioType = audType;
	answered_call=0;
	newcall=0;
	gettimeofday(&lastouttm,NULL);
	switch (iAudioType) {
		case AUDIO_INTERNAL:
#ifdef WIN32
			if (win_initialize_audio() != 0)
				return -1;
#else
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
	switch (iAudioType) {
		case AUDIO_INTERNAL:
#ifdef WIN32
			win_shutdown_audio();
#else
#endif
			break;
		case AUDIO_INTERNAL_PA:
			pa_shutdown_audio();
			break;
	}
}


void iaxc_set_encode_format(int fmt)
{
	iEncodeType = fmt;
	iax_set_formats(fmt);
}

void iaxc_process_calls(void) {

#ifdef WIN32	
		win_flush_audio_output_buffers();
		if (iAudioType == AUDIO_INTERNAL) {
			win_prepare_audio_buffers();
		}
#endif
		iaxc_service_network(netfd);
		service_audio();
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
	THREADJOIN(procThread);
    }
    procThreadQuitFlag = -1;
}

void start_call_processing() {
	if (!answered_call) {
		while(1) {
			iaxc_service_network(netfd);
			if (answered_call)
				break;
		}
	}
#ifdef WIN32
	_beginthread(iaxc_process_calls, 0, NULL);
#else
#endif
}

int service_audio()
{
	/* do audio input stuff for buffers that have received data from audio in device already. Must
		do them in serial number order (the order in which they were originally queued). */
	if(answered_call) /* send audio only if call answered */
	{
		switch (iAudioType) {
			case AUDIO_INTERNAL:
				iaxc_service_network(netfd);
#ifdef WIN32			
				win_process_audio_buffers(&lastouttm, most_recent_answer, iEncodeType);		
#endif
				iaxc_service_network(netfd);
				break;
			case AUDIO_INTERNAL_PA:
				iaxc_service_network(netfd);
				pa_send_audio(&lastouttm, most_recent_answer, iEncodeType);
				break;
			default:
				iaxc_service_network(netfd);
				iaxc_external_service_audio();
				iaxc_service_network(netfd);
				break;
		}
	} else {
		static int i=0;
		if((i++ % 50 == 0) && iaxc_levels_callback) iaxc_levels_callback(-99,-99);
	}
	return 0;
}


void handle_audio_event(struct iax_event *e, struct peer *p) {
	int total_consumed = 0;
	int cur;
	short fr[160];

#ifdef IAXC_IAX2
	while(total_consumed < e->datalen) {
		cur = decode_audio(p, fr,
		    e->data,e->datalen-total_consumed,
		    iEncodeType);
#else
	while(total_consumed < e->event.voice.datalen) {
		cur = decode_audio(p, fr,
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
#ifdef WIN32
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

void iaxc_handle_network_event(struct iax_event *e, struct peer *p)
{
//	int len,n;
//	WHOUT *wh,*wh1;
//	short fr[160];
//	static paused_xmit = 0;


	switch(e->etype) {
		case IAX_EVENT_HANGUP:
#ifndef IAXC_IAX2  /* IAX2 barfs from this.  Should we do this or not? */
			
			iax_hangup(most_recent_answer->session, "Byeee!");
#endif
			iaxc_usermsg(IAXC_STATUS, "Call disconnected by remote");
			free(most_recent_answer);
			most_recent_answer = 0;
			answered_call = 0;
			peers = 0;
			newcall = 0;
			
			break;

		case IAX_EVENT_REJECT:
			iaxc_usermsg(IAXC_STATUS, "Authentication rejected by remote");
			break;
		case IAX_EVENT_ACCEPT:
			iaxc_usermsg(IAXC_STATUS,"RING RING");
//			issue_prompt(f);
			break;
		case IAX_EVENT_ANSWER:
			iaxc_answer_call();
 			break;
		case IAX_EVENT_VOICE:
			handle_audio_event(e, p);
			break;
//				default :
					//fprintf(f, "Don't know how to handle that format %d\n", e->event.voice.format);
			break;
		case IAX_EVENT_RINGA:
			break;
		default:
			iaxc_usermsg(IAXC_STATUS, "Unknown event: %d", e->etype);
			break;
	}
}


void iaxc_call(char *num)
{
	struct peer *peer;

	if(!newcall)
		newcall = iax_session_new();
	else {
		iaxc_usermsg(IAXC_STATUS, "Call already in progress");
		return;
	}

	if ( !(peer = malloc(sizeof(struct peer)))) {
		iaxc_usermsg(IAXC_ERROR, "Warning: Unable to allocate memory!");
		return;
	}

	peer->time = time(0);
	peer->session = newcall;
	peer->gsmin = 0;
	peer->gsmout = 0;

	peer->next = peers;
	peers = peer;

	most_recent_answer = peer;

#ifdef IAXC_IAX2
	iax_call(peer->session, "7001234567", "IAXClient User", num, NULL, 0);
#else
	iax_call(peer->session, "7001234567", num, NULL, 10);
#endif
}

void iaxc_answer_call(void) 
{
	if(most_recent_answer)
		iax_answer(most_recent_answer->session);
	iaxc_usermsg(IAXC_STATUS,"Connected");
	answered_call = 1;
}

void iaxc_dump_call(void)
{
	if(most_recent_answer)
	{
		iax_hangup(most_recent_answer->session,"");
		free(most_recent_answer);
	}
	iaxc_usermsg(IAXC_STATUS, "Hanging up");
	answered_call = 0;
	most_recent_answer = 0;
	answered_call = 0;
	peers = 0;
	newcall = 0;
}

void iaxc_reject_call(void)
{
	iax_reject(most_recent_answer->session, "Call rejected manually.");
	most_recent_answer = 0;
}

void iaxc_send_dtmf(char digit)
{
	if(most_recent_answer)
		iax_send_dtmf(most_recent_answer->session,digit);
}

static struct peer *find_peer(struct iax_session *session)
{
	struct peer *cur = peers;
	while(cur) {
		if (cur->session == session)
			return cur;
		cur = cur->next;
	}
	return NULL;
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

static void do_iax_event() {
	int sessions = 0;
	struct iax_event *e = 0;
	struct peer *peer;

	while ( (e = iax_get_event(0))) {
		peer = find_peer(e->session);
		if(peer) {
			iaxc_handle_network_event(e, peer);
			iax_event_free(e);
		} else {
			if(e->etype != IAX_EVENT_CONNECT) {
				iaxc_usermsg(IAXC_STATUS, "Huh? This is an event for a non-existant session?");
			}
			sessions++;

			if(sessions >= MAX_SESSIONS) {
				iaxc_usermsg(IAXC_STATUS, "Missed a call... too many sessions open.");
			}

#ifndef IAXC_IAX2
			if(e->event.connect.callerid && e->event.connect.dnid)
				iaxc_usermsg(IAXC_STATUS, "Call from '%s' for '%s'", e->event.connect.callerid, 
				e->event.connect.dnid);
			else if(e->event.connect.dnid) {
				iaxc_usermsg(IAXC_STATUS, "Call from '%s'", e->event.connect.dnid);
			} else if(e->event.connect.callerid) {
				iaxc_usermsg(IAXC_STATUS, "Call from '%s'", e->event.connect.callerid);
			} else 
#endif
				iaxc_usermsg(IAXC_STATUS, "Call from");

			iaxc_usermsg(IAXC_STATUS, " (%s)", inet_ntoa(iax_get_peer_addr(e->session).sin_addr));

			if(most_recent_answer) {
				iaxc_usermsg(IAXC_STATUS, "Incoming call ignored, there's already a call waiting for answer... \
please accept or reject first");
				iax_reject(e->session, "Too many calls, we're busy!");
			} else {
				if ( !(peer = malloc(sizeof(struct peer)))) {
					iaxc_usermsg(IAXC_STATUS, "Warning: Unable to allocate memory!");
					return;
				}

				peer->time = time(0);
				peer->session = e->session;
				peer->gsmin = 0;
				peer->gsmout = 0;

				peer->next = peers;
				peers = peer;

				iax_accept(peer->session);
				iax_ring_announce(peer->session);
				most_recent_answer = peer;
				iaxc_usermsg(IAXC_STATUS, "Incoming call!");
			}
			iax_event_free(e);
//			issue_prompt(f);
		}
	}
}

int iaxc_was_call_answered()
{
	return answered_call;
}

void iaxc_external_audio_event(struct iax_event *e, struct peer *p)
{
	// To be coded in the future
	return;
}

void iaxc_external_service_audio()
{
	// To be coded in the future
	return;
}
