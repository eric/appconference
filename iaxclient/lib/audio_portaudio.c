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

#include "iaxclient_lib.h"

PABLIO_Stream *stream;

int pa_initialize_audio() {
    PaError  err;

    /* Open simplified blocking I/O layer on top of PortAudio. */
    err = OpenAudioStream( &stream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_WRITE | PABLIO_MONO) );
    if( err != paNoError ) {
		handle_paerror(err, "opening stream");
		return -1;
    }
    return 0;
}

void pa_shutdown_audio() {
    CloseAudioStream( stream );
}

void handle_paerror(PaError err, char * where) {
	fprintf(stderr, "PortAudio error at %s: %s\n", where, Pa_GetErrorText(err));
}

void pa_read_audio_input() {

}

void pa_play_recv_audio(void *fr, int fr_size) {
	if(GetAudioStreamWriteable(stream) < SAMPLES_PER_FRAME * FRAMES_PER_BLOCK)
	      fprintf(stderr, "audio_portaudio: might block writing audio\n");

	// Play the audio as decoded
	WriteAudioStream(stream, fr, SAMPLES_PER_FRAME * FRAMES_PER_BLOCK);
}

void pa_send_audio(struct timeval *lastouttm, struct peer *most_recent_answer, int iEncodeType) {
	SAMPLE samples[SAMPLES_PER_FRAME * FRAMES_PER_BLOCK];

	/* send all available complete frames */
	while(GetAudioStreamReadable(stream) >= FRAMES_PER_BLOCK)
	{
		ReadAudioStream(stream, samples, FRAMES_PER_BLOCK);
		send_encoded_audio(most_recent_answer, samples, iEncodeType);
	}
}

