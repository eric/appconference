#include <stdio.h>
#include <stdlib.h>
#include "pablio.h"
#include "audio_encode.h"


#ifdef WIN32
#include "winpoop.h"
#endif

/* Values for flags for OpenAudioStream(). */
#define PABLIO_READ     (1<<0)
#define PABLIO_WRITE    (1<<1)
#define PABLIO_READ_WRITE    (PABLIO_READ|PABLIO_WRITE)
#define PABLIO_MONO     (1<<2)
#define PABLIO_STEREO   (1<<3)

/*
** Note that many of the older ISA sound cards on PCs do NOT support
** full duplex audio (simultaneous record and playback).
** And some only support full duplex at lower sample rates.
*/
#define SAMPLE_RATE           (8000)
#define NUM_SECONDS              (5)
#define SAMPLES_PER_FRAME        (1)
#define FRAMES_PER_BLOCK       (160)
#define	OUT_INTERVAL 10		/* number of ms to wait before sending more data to peer */

/* Select whether we will use floats or shorts. */
#if 1
#define SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#else
#define SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#endif

int pa_initialize_audio();
void pa_shutdown_audio();
void handle_paerror(PaError err);
void pa_read_audio_input();
void pa_play_recv_audio(void *fr, int fr_size);
void pa_send_audio(unsigned long *outtick, struct peer *most_recent_answer, int iEncodeType);