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
static int virtualMono;
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


/* some commentaty here:
 * 1: MacOSX: MacOSX needs "virtual mono" and a single stream.  That's
 * really the only choice there, and it should always work (Famous last
 * words).
 *
 * 2: Unix/OSS: most cards are OK with real mono, and a single stream.
 * Except some.  For those, a single open with real mono will succeed,
 * but execution will fail.  Maybe others will open OK with a single
 * stream, and real mono, but fail later?
 *
 * The failure mode I saw with a volunteer was that reads/writes would
 * return -enodev (down in the portaudio code).  Bummer.
 *
 * Win32 works fine, in all cases, with a single stream and real mono,
 * so far.
 * */

int pa_initialize_audio() {
    PaError  err;

    /* Open simplified blocking I/O layer on top of PortAudio. */

#ifndef MACOSX
    /* first, try opening one stream for in/out, Mono */
    /* except for MacOSX, which needs virtual stereo */
    err = OpenAudioStream( &iStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_WRITE | PABLIO_MONO) );
    
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	virtualMono = 0;
	return 0;
    }
#endif

#ifndef LINUX
    /* then, we try a single stream, virtual stereo.  Except on linux,
     * see note above */
    err = OpenAudioStream( &iStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_WRITE | PABLIO_STEREO) );
    
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	virtualMono = 1;
	return 0;
    }
#endif

    /* finally, we go to the worst case.  Two opens, virtual mono */
    oneStream = 0;
    virtualMono = 1;
    err = OpenAudioStream( &iStream, SAMPLE_RATE, paInt16,
                           (PABLIO_READ  | PABLIO_STEREO) );
    if( err != paNoError ) 
    {
	handle_paerror(err, "opening separate input stream");
	return -1;
    }
    err = OpenAudioStream( &oStream, SAMPLE_RATE, paInt16,
                           (PABLIO_WRITE  | PABLIO_STEREO) );
    if( err != paNoError ) 
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


void mono2stereo(SAMPLE *out, SAMPLE *in, int nSamples) {
    int i;
    //fprintf(stderr, "mono2stereo: %d samples\n", nSamples);
    for(i=0;i<nSamples;i++) {
	*(out++) = *in;
	*(out++) = *(in++); 
    }
}

void stereo2mono(SAMPLE *out, SAMPLE *in, int nSamples) {
    int i;
    //fprintf(stderr, "stereo2mono: %d samples\n", nSamples);
    for(i=0;i<nSamples;i++) {
	*(out) = *(in++);
	out++; in++;
	//*(out++) += *(in++);
    }
}

void pa_play_recv_audio(void *fr, int fr_size) {
      
	SAMPLE stereobuf[FRAMES_PER_BLOCK * 2];
	SAMPLE *buf;
	if(GetAudioStreamWriteable(oStream) < FRAMES_PER_BLOCK)
	{
	      //fprintf(stderr, "audio_portaudio: audio output overflow\n");
	      return;
	}

	if(virtualMono) {
	    mono2stereo(stereobuf, (SAMPLE *)fr, FRAMES_PER_BLOCK);
	    buf = stereobuf;
	} else {
	    buf = (short *)fr;
	}

	// Play the audio as decoded
	WriteAudioStream(oStream, buf, FRAMES_PER_BLOCK);
}

void pa_send_audio(struct timeval *lastouttm, struct iaxc_call *most_recent_answer, int iEncodeType) {
	SAMPLE samples[FRAMES_PER_BLOCK * 2]; // could be stereo
	SAMPLE monobuf[FRAMES_PER_BLOCK];
	SAMPLE *buf;

	/* send all available complete frames */
	while(GetAudioStreamReadable(iStream) >= (FRAMES_PER_BLOCK))
	{
		ReadAudioStream(iStream, samples, FRAMES_PER_BLOCK);
		if(virtualMono) {
		    stereo2mono(monobuf, samples, FRAMES_PER_BLOCK);
		    buf = monobuf;
		} else {
		    buf = samples;
		}
		send_encoded_audio(most_recent_answer, buf, iEncodeType);
	}
}

