#include "iaxclient_lib.h"

#ifdef WIN32

#include "audio_win32.h"

#endif 

static int iAudioType;
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
	if (iAudioType == AUDIO_INTERNAL) {
		if (initialize_audio() < 0)
			return -1;
		if (prepare_audio_buffers() < 0)
			return -1;
	}
	return 0;
}

void shutdown_client() {
	iax_shutdown();
	if (iAudioType == AUDIO_INTERNAL) {
		shutdown_audio();
	}
}


int process_calls() {
	/* service the network stuff */
	service_network(netfd,f);
	flush_audio_output_buffers();
	service_network(netfd,f);
	/* do audio input stuff for buffers that have received data from audio in device already. Must
		do them in serial number order (the order in which they were originally queued). */
	if(answered_call) /* send audio only if call answered */
	{
		int i;
		gsm_frame fo;
		for(;;) /* loop until all are found */
		{
			for(i = 0; i < NWHIN; i++) /* find an available one that's the one we are looking for */
			{
				service_network(netfd,f); /* service network here for better performance */
				/* if not time to send any more, dont */
				if (GetTickCount() < (lastouttick + OUT_INTERVAL))
				{
					i = NWHIN; /* set to value that WILL exit loop */
					break;
				}
				if (audio_ready(i)) {
			
					/* must have read exactly 320 bytes */
					if (check_audio_packet_size(i) == 0)
					{
						fprintf(stderr,"Short audio read, got %d bytes, expected %d bytes\n", whin[i].dwBytesRecorded,
							whin[i].dwBufferLength);
						return -1;
					}
					if(!most_recent_answer->gsmout)
							most_recent_answer->gsmout = gsm_create();
					service_network(netfd,f); /* service network here for better performance */
					/* encode the audio from the buffer into GSM format */
					gsm_encode(most_recent_answer->gsmout, (short *) ((char *) whin[i].lpData), fo);
					if(iax_send_voice(most_recent_answer->session,AST_FORMAT_GSM, (char *)fo, sizeof(gsm_frame)) == -1)
						puts("Failed to send voice!"); 
					lastouttick = GetTickCount(); /* save time of last output */
					/* unprepare (free) the header */
					free_audio_header(i);
					/* bump the serial number to look for the next time */
					bump_audio_sn();
					/* exit the loop so that we can start at lowest buffer again */
					break;
				}
			} 
			if (i >= NWHIN) break; /* if all found, get out of loop */
		}
	}
	return 0;
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
			switch(e->event.voice.format) {
				case AST_FORMAT_GSM:
					if (iAudioType == AUDIO_INTERNAL)
						handle_audio_event(f, e, p);
					else
						external_audio_event(f, e, p);
					break;
				default :
					fprintf(f, "Don't know how to handle that format %d\n", e->event.voice.format);
			}
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
