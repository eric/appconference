#include "iaxclient_lib.h"
#include <process.h>

#ifdef WIN32

#include "audio_win32.h"
#include "audio_portaudio.h"
#include "audio_encode.h"

#endif 

static int iAudioType;
static int iEncodeType;

int netfd;
int port;
int c, i;
char rcmd[RBUFSIZE];
FILE *f;
gsm_frame fo;
time_t	t;
static int answered_call;
static struct iax_session *newcall;

static struct peer *most_recent_answer;

unsigned long lastouttick;

// Parameters:
// audType - Define whether audio is handled by library or externally
int initialize_client(int audType, FILE *file) {
	/* get time of day in milliseconds, offset by tick count (see our
	   gettimeofday() implementation) */
	time(&t);
	startuptime = ((t % 86400) * 1000) - GetTickCount();

	if ( (port = iax_init(0) < 0)) {
		fprintf(stderr, "Fatal error: failed to initialize iax with port %d\n", port);
		return -1;
	}
	netfd = iax_get_fd();

	iAudioType = audType;
	f=file;
	answered_call=0;
	newcall=0;
	lastouttick=0;
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

void shutdown_client() {
	iax_shutdown();
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


void set_encode_format(int fmt)
{
	iEncodeType = fmt;
	iax_set_formats(fmt);
}

void process_calls(void *dummy) {
	while (1) {
		/* service the network stuff */
		service_network(netfd,f);
#ifdef WIN32	
		win_flush_audio_output_buffers();
		service_network(netfd,f);
		if (iAudioType == AUDIO_INTERNAL) {
			win_prepare_audio_buffers();
		}
#else
#endif
		service_network(netfd,f);
		service_audio();
		service_network(netfd,f);
		Sleep(10);
		//if (service_audio() == -1)
		//	break;
		if (!answered_call)
			break;
	}
#ifdef WIN32
	_endthread();
#else
#endif
}

void start_call_processing() {
	if (!answered_call) {
		while(1) {
			service_network(netfd, f);
			if (answered_call)
				break;
		}
	}
#ifdef WIN32
	_beginthread(process_calls, 0, NULL);
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
				service_network(netfd, f);
#ifdef WIN32			
				win_process_audio_buffers(&lastouttick, most_recent_answer, iEncodeType);		
#endif
				service_network(netfd, f);
				break;
			case AUDIO_INTERNAL_PA:
				service_network(netfd, f);
				pa_send_audio(&lastouttick, most_recent_answer, iEncodeType);
				break;
			default:
				service_network(netfd, f);
				external_service_audio();
				service_network(netfd, f);
				break;
		}
	}
	return 0;
}


void handle_audio_event(FILE *f, struct iax_event *e, struct peer *p) {
	int len;
	short fr[160];

	if (check_encoded_audio_length(e, iEncodeType) < 0)
		return;

	len = 0;
	while(len < e->event.voice.datalen) {
//		if(gsm_decode(p->gsmin, (char *) e->event.voice.data + len, fr)) {
		if(decode_audio(e,p,fr,len,iEncodeType)) {
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
		increment_encoded_data_count(&len, iEncodeType);
	}
}

void handle_network_event(FILE *f, struct iax_event *e, struct peer *p)
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
			client_answer_call();
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


void client_call(FILE *f, char *num)
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

	iax_call(peer->session, "WinIAX", num, NULL, 10);
	start_call_processing();
}

void client_answer_call(void) 
{
	if(most_recent_answer)
		iax_answer(most_recent_answer->session);
	printf("Answering call!\n");
	answered_call = 1;
}

void client_dump_call(void)
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

void client_reject_call(void)
{
	iax_reject(most_recent_answer->session, "Call rejected manually.");
	most_recent_answer = 0;
}

void client_send_dtmf(char digit)
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
void service_network(int netfd, FILE *f)
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

void do_iax_event(FILE *f) {
	int sessions = 0;
	struct iax_event *e = 0;
	struct peer *peer;

	while ( (e = iax_get_event(0))) {
		peer = find_peer(e->session);
		if(peer) {
			handle_network_event(f, e, peer);
			iax_event_free(e);
		} else {
			if(e->etype != IAX_EVENT_CONNECT) {
				fprintf(stderr, "Huh? This is an event for a non-existant session?\n");
			}
			sessions++;

			if(sessions >= MAX_SESSIONS) {
				fprintf(f, "Missed a call... too many sessions open.\n");
			}


			if(e->event.connect.callerid && e->event.connect.dnid)
				fprintf(f, "Call from '%s' for '%s'", e->event.connect.callerid, 
				e->event.connect.dnid);
			else if(e->event.connect.dnid) {
				fprintf(f, "Call from '%s'", e->event.connect.dnid);
			} else if(e->event.connect.callerid) {
				fprintf(f, "Call from '%s'", e->event.connect.callerid);
			} else printf("Call from");
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

int was_call_answered()
{
	return answered_call;
}

void external_audio_event(FILE *f, struct iax_event *e, struct peer *p)
{
	// To be coded in the future
	return;
}

void external_service_audio()
{
	// To be coded in the future
	return;
}
