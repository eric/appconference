#ifndef _AUDIO_PORTAUDIO_H
#define _AUDIO_PORTAUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include "pablio.h"
#include "audio_encode.h"


#ifdef WIN32
#include "winpoop.h"
#endif

/*
** Note that many of the older ISA sound cards on PCs do NOT support
** full duplex audio (simultaneous record and playback).
** And some only support full duplex at lower sample rates.
*/
#define SAMPLE_RATE           (8000)
#define NUM_SECONDS              (5)
#define SAMPLES_PER_FRAME        (1)
#define FRAMES_PER_BLOCK       (160)

/* Select whether we will use floats or shorts. */
#if 0
#define SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#else
#define SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#endif

int pa_initialize();
void pa_shutdown();
void handle_paerror(PaError err, char *where);
void pa_read_audio_input();
void pa_play_recv_audio(void *fr, int fr_size);
void pa_send_audio(struct timeval *outtm, struct iaxc_call *most_recent_answer, int iEncodeType);

#endif
