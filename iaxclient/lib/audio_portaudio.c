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


static PABLIO_Stream *iStream;
static PABLIO_Stream *oStream;
static int oneStream;
static const PaDeviceInfo **inputDevices;
static const PaDeviceInfo **outputDevices;
static int nInputDevices;
static int nOutputDevices;

/* scan devices and stash pointers to dev structures. 
 *  But, these structures only remain valid while Pa is initialized,
 *  which, with pablio, is only while it's running!
 *  Also, storing these things in two separate arrays loses the actual
 *  PaDeviceID's associated with devices (since their index in these
 *  input/output arrays isn't the same as their index in the combined
 *  array */
static int pa_scan_devices() {
    int nDevices; 
    int i;

    /* we may be called multiple times */
    if(inputDevices){ 
	free(inputDevices);
	inputDevices=NULL;
    }

    if(outputDevices){ 
	free(outputDevices);
	outputDevices=NULL;
    }

    nInputDevices = nOutputDevices = 0;

    nDevices = Pa_CountDevices();

    /* allocate in/out arrays big enough for all devices */
    inputDevices = malloc(nDevices * sizeof(PaDeviceInfo *));
    outputDevices = malloc(nDevices * sizeof(PaDeviceInfo *));

    for(i=0;i<nDevices;i++)
    {
	const PaDeviceInfo *d;	
	d=Pa_GetDeviceInfo(i);

	if(d->maxInputChannels > 0)
	  inputDevices[nInputDevices++] = d;

	if(d->maxOutputChannels > 0)
	  outputDevices[nOutputDevices++] = d;
    }
}

int pa_initialize_audio() {
    PaError  err;

    /* Open simplified blocking I/O layer on top of PortAudio. */
    /* first, try opening one stream for in/out */
    err = OpenAudioStream( &iStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_WRITE | PABLIO_MONO) );
    
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	return 0;
    }

    oneStream = 0;
    err = OpenAudioStream( &iStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_MONO) );
    {
	handle_paerror(err, "opening separate input stream");
	return -1;
    }
    err = OpenAudioStream( &oStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_MONO) );
    {
	handle_paerror(err, "opening separate output stream");
	return -1;
    }

    return 0;
}

void pa_shutdown_audio() {
    CloseAudioStream( iStream );
    if(!oneStream) CloseAudioStream( oStream );
}

void handle_paerror(PaError err, char * where) {
	fprintf(stderr, "PortAudio error at %s: %s\n", where, Pa_GetErrorText(err));
}

void pa_read_audio_input() {

}

void pa_play_recv_audio(void *fr, int fr_size) {
	if(GetAudioStreamWriteable(oStream) < SAMPLES_PER_FRAME * FRAMES_PER_BLOCK)
	{
	      //fprintf(stderr, "audio_portaudio: audio output overflow\n");
	      return;
	}

	// Play the audio as decoded
	WriteAudioStream(oStream, fr, SAMPLES_PER_FRAME * FRAMES_PER_BLOCK);
}

void pa_send_audio(struct timeval *lastouttm, struct iaxc_call *most_recent_answer, int iEncodeType) {
	SAMPLE samples[SAMPLES_PER_FRAME * FRAMES_PER_BLOCK];

	/* send all available complete frames */
	while(GetAudioStreamReadable(iStream) >= FRAMES_PER_BLOCK)
	{
		ReadAudioStream(iStream, samples, FRAMES_PER_BLOCK);
		send_encoded_audio(most_recent_answer, samples, iEncodeType);
	}
}

