#include "iaxclient_lib.h"

static int iAudioType;
static int iEncodeType;

int netfd;
int port;
int c, i;
char rcmd[RBUFSIZE];
FILE *f;
gsm_frame fo;
static int answered_call;
static struct iax_session *newcall;

static struct peer *most_recent_answer;

struct timeval lastouttm;

static struct peer * find_peer(struct iax_session *session);
static void do_iax_event(FILE *f);

static THREAD procThread;
static THREADID procThreadID;

/* QuitFlag: 0: Running 1: Should Quit, -1: Not Running */
static int procThreadQuitFlag = -1;


long iaxc_usecdiff( struct timeval *timeA, struct timeval *timeB ){
      long secs = timeA->tv_sec - timeB->tv_sec;
      long usecs = secs * 1000000;
      usecs += (timeA->tv_usec - timeB->tv_usec);
      return usecs;
}



// Parameters:
// audType - Define whether audio is handled by library or externally
int iaxc_initialize(int audType, FILE *file) {
	/* get time of day in milliseconds, offset by tick count (see our
	   gettimeofday() implementation) */
	os_init();

	if ( (port = iax_init(0) < 0)) {
		fprintf(stderr, "Fatal error: failed to initialize iax with port %d\n", port);
		return -1;
	}
	netfd = iax_get_fd();

	iAudioType = audType;
	f=file;
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
		iaxc_service_network(netfd,f);
		service_audio();
}

THREADFUNCDECL(iaxc_processor)
{
    THREADFUNCRET(ret);
    while(1) { 
	iaxc_process_calls();
	os_millisleep(10);	
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
			iaxc_service_network(netfd, f);
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
#ifdef IAXC_IAX2
	if(1)  /* HACK ALERT! calls don't get into answered state with IAX2 FIX ME FAST */
#else
	if(answered_call) /* send audio only if call answered */
#endif
	{
		switch (iAudioType) {
			case AUDIO_INTERNAL:
				iaxc_service_network(netfd, f);
#ifdef WIN32			
				win_process_audio_buffers(&lastouttm, most_recent_answer, iEncodeType);		
#endif
				iaxc_service_network(netfd, f);
				break;
			case AUDIO_INTERNAL_PA:
				iaxc_service_network(netfd, f);
				pa_send_audio(&lastouttm, most_recent_answer, iEncodeType);
				break;
			default:
				iaxc_service_network(netfd, f);
				iaxc_external_service_audio();
				iaxc_service_network(netfd, f);
				break;
		}
	}
	return 0;
}


void handle_audio_event(FILE *f, struct iax_event *e, struct peer *p) {
	int len;
	short fr[160];

	len = 0;
#ifdef IAXC_IAX2
	while(len < e->datalen) {
#else
	while(len < e->event.voice.datalen) {
#endif
//		if(gsm_decode(p->gsmin, (char *) e->event.voice.data + len, fr)) {
		if(decode_audio(e,p,fr,&len,iEncodeType)) {
			fprintf(stderr, "Bad voice packet.  Unable to decode.\n");
			return;
		} else {  /* its an audio packet to be output to user */
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

void iaxc_handle_network_event(FILE *f, struct iax_event *e, struct peer *p)
{
//	int len,n;
//	WHOUT *wh,*wh1;
//	short fr[160];
//	static paused_xmit = 0;


	switch(e->etype) {
		case IAX_EVENT_HANGUP:
			iax_hangup(most_recent_answer->session, "Byeee!");
			fprintf(f, "Call disconnected by peer\n");
			free(most_recent_answer);
			most_recent_answer = 0;
			answered_call = 0;
			peers = 0;
			newcall = 0;
			
			break;

		case IAX_EVENT_REJECT:
			fprintf(f, "Authentication was rejected\n");
			break;
		case IAX_EVENT_ACCEPT:
			fprintf(f, "Waiting for answer... RING RING\n");
//			issue_prompt(f);
			break;
		case IAX_EVENT_ANSWER:
			iaxc_answer_call();
 			break;
		case IAX_EVENT_VOICE:
			handle_audio_event(f, e, p);
			break;
//				default :
					//fprintf(f, "Don't know how to handle that format %d\n", e->event.voice.format);
			break;
		case IAX_EVENT_RINGA:
			break;
		default:
			fprintf(f, "Unknown event: %d\n", e->etype);
			break;
	}
}


void iaxc_call(FILE *f, char *num)
{
	struct peer *peer;

	if(!newcall)
		newcall = iax_session_new();
	else {
		fprintf(f, "Already attempting to call somewhere, please cancel first!\n");
		return;
	}

	if ( !(peer = malloc(sizeof(struct peer)))) {
		fprintf(f, "Warning: Unable to allocate memory!\n");
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
	iax_call(peer->session, "7001234567", "IAXClient User", num, NULL, 10);
#else
	iax_call(peer->session, "7001234567", num, NULL, 10);
#endif
}

void iaxc_answer_call(void) 
{
	if(most_recent_answer)
		iax_answer(most_recent_answer->session);
	printf("Answering call!\n");
	answered_call = 1;
}

void iaxc_dump_call(void)
{
	if(most_recent_answer)
	{
		iax_hangup(most_recent_answer->session,"");
		free(most_recent_answer);
	}
	printf("Dumping call!\n");
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
void iaxc_service_network(int netfd, FILE *f)
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
					do_iax_event(f);
					(void) iax_time_to_next_event();
				} else break;
			} else break;
		}
		do_iax_event(f); /* do pending event if any */
}

static void do_iax_event(FILE *f) {
	int sessions = 0;
	struct iax_event *e = 0;
	struct peer *peer;

	while ( (e = iax_get_event(0))) {
		peer = find_peer(e->session);
		if(peer) {
			iaxc_handle_network_event(f, e, peer);
			iax_event_free(e);
		} else {
			if(e->etype != IAX_EVENT_CONNECT) {
				fprintf(stderr, "Huh? This is an event for a non-existant session?\n");
			}
			sessions++;

			if(sessions >= MAX_SESSIONS) {
				fprintf(f, "Missed a call... too many sessions open.\n");
			}

#ifndef IAXC_IAX2
			if(e->event.connect.callerid && e->event.connect.dnid)
				fprintf(f, "Call from '%s' for '%s'", e->event.connect.callerid, 
				e->event.connect.dnid);
			else if(e->event.connect.dnid) {
				fprintf(f, "Call from '%s'", e->event.connect.dnid);
			} else if(e->event.connect.callerid) {
				fprintf(f, "Call from '%s'", e->event.connect.callerid);
			} else 
#endif
			    printf("Call from");
			fprintf(f, " (%s)\n", inet_ntoa(iax_get_peer_addr(e->session).sin_addr));

			if(most_recent_answer) {
				fprintf(f, "Incoming call ignored, there's already a call waiting for answer... \
please accept or reject first\n");
				iax_reject(e->session, "Too many calls, we're busy!");
			} else {
				if ( !(peer = malloc(sizeof(struct peer)))) {
					fprintf(f, "Warning: Unable to allocate memory!\n");
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
				fprintf(f, "Incoming call!\n");
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

void iaxc_external_audio_event(FILE *f, struct iax_event *e, struct peer *p)
{
	// To be coded in the future
	return;
}

void iaxc_external_service_audio()
{
	// To be coded in the future
	return;
}
