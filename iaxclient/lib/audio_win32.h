#include <windows.h>
#include <mmsystem.h>
#include <iax-client.h>
#include <stdio.h>

#include "gsm.h"

#ifndef audio_win32_h_
#define audio_win32_h_

struct peer {
	int time;
	gsm gsmin;
	gsm gsmout;

	struct iax_session *session;
	struct peer *next;
};

typedef struct whout {
	WAVEHDR w;
	short	data[160];
	struct whout *next;
} WHOUT;

/* stuff for wave audio device */
HWAVEOUT wout;
HWAVEIN win;

/* parameters for audio in */
#define	NWHIN 8				/* number of input buffer entries */
/* NOTE the OUT_INTERVAL parameter *SHOULD* be more around 18 to 20 or so, since the packets should
be spaced by 20 milliseconds. However, in practice, especially in Windoze-95, setting it that high
caused underruns. 10 is just ever so slightly agressive, and the receiver has to chuck a packet
every now and then. Thats about the way it should be to be happy. */
#define	OUT_INTERVAL 10		/* number of ms to wait before sending more data to peer */
/* parameters for audio out */
#define	OUT_DEPTH 12		/* number of outbut buffer entries */
#define	OUT_PAUSE_THRESHOLD 2 /* number of active entries needed to start output (for smoothing) */

/* audio input buffer headers */
WAVEHDR whin[NWHIN];
/* audio input buffers */
char bufin[NWHIN][320];


WAVEFORMATEX wf;
WHOUT *wh,*wh1,*wh2;

int initialize_audio();
void shutdown_audio();
void handle_audio_event(FILE *f, struct iax_event *e, struct peer *p);
int prepare_audio_buffers();
void flush_audio_output_buffers();
//int process_call_audio();
int audio_ready(int i);
int check_audio_packet_size(int i);
void free_audio_header(int i);
void bump_audio_sn();

#endif
