/*
 * iaxclient_lib: An Inter-Asterisk eXchange communication library
 *
 * Module: audio_portaudio
 * Purpose: Audio code to provide portaudio driver support for IAX library
 * Developed by: Shawn Lawrence, Terrace Communications Inc.
 * Creation Date: April 18, 2003
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 *
 * IAX library Copyright (c) 2001 Linux Support Services
 * IAXlib is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 *
 * This library uses the PortAudio Portable Audio Library
 * For more information see: http://www.portaudio.com
 * PortAudio Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 */

#include "audio_portaudio.h"

PABLIO_Stream *inStream, *outStream;
SAMPLE samples[SAMPLES_PER_FRAME * FRAMES_PER_BLOCK];

int pa_initialize_audio() {
    PaError  err;

    /* Open simplified blocking I/O layer on top of PortAudio. */
    err = OpenAudioStream( &inStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_MONO) );
    if( err != paNoError ) {
		handle_paerror(err);
		return -1;
	}
    err = OpenAudioStream( &outStream, SAMPLE_RATE, paInt16,
                           (PABLIO_WRITE  | PABLIO_MONO) );
    if( err != paNoError ) {
		handle_paerror(err);
		return -1;
	}
	return 0;
}

void pa_shutdown_audio() {
    CloseAudioStream( inStream );
    CloseAudioStream( outStream );
}

void handle_paerror(PaError err) {
	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
}

void pa_read_audio_input() {

}

void pa_play_recv_audio(void *fr, int fr_size) {
	// Play the audio as decoded
	WriteAudioStream(outStream, fr, SAMPLES_PER_FRAME * FRAMES_PER_BLOCK);
}

void pa_send_audio(struct timeval *lastouttm, struct peer *most_recent_answer, int iEncodeType) {
	struct timeval now;
	short fr[160];
	long i;

	long framecount = GetAudioStreamReadable(inStream);
	for(i = 0; i < framecount; i++) {
		gettimeofday(&now,NULL);
		// See if we should be sending (jitter buffer for IAX)
		if (iaxc_usecdiff(lastouttm,&now) < (OUT_INTERVAL*1000))
		{
			i = framecount;
			break;
		}

		fprintf(stderr, "PORTAUDIO: reading audio\n");
		// Read the audio from the audio buffer
		ReadAudioStream(inStream, samples, FRAMES_PER_BLOCK);

		// Encode it, then send it
		send_encoded_audio(most_recent_answer, samples, iEncodeType);
		// Update the counter
		*lastouttm = now;  /* save time of last output */
	}
}

