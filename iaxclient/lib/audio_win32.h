#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#include "gsm.h"

#ifndef audio_win32_h_
#define audio_win32_h_


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
/* parameters for audio out */
#define	OUT_DEPTH 12		/* number of outbut buffer entries */
#define	OUT_PAUSE_THRESHOLD 2 /* number of active entries needed to start output (for smoothing) */

/* audio input buffer headers */
WAVEHDR whin[NWHIN];
/* audio input buffers */
char bufin[NWHIN][320];


WAVEFORMATEX wf;
WHOUT *wh,*wh1,*wh2;

int win_initialize_audio();
void win_shutdown_audio();
void win_play_recv_audio(void *fr, int fr_size);
int win_prepare_audio_buffers();
void win_flush_audio_output_buffers();
int win_process_audio_buffers(unsigned long *outtick, struct peer *most_recent_answer, int iEncodeType);
//int process_call_audio();
int win_audio_ready(int i);
int win_check_audio_packet_size(int i);
int win_get_audio_packet_size(int i);
void win_free_audio_header(int i);
void win_bump_audio_sn();
void *win_get_audio_data(int i);

#endif
