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

static PortAudioStream *iStream, *oStream;

static selectedInput, selectedOutput, selectedRing;

#define FRAMES_PER_BUFFER 80 /* 80 frames == 10ms */

#define RBSZ 1024 /* Needs to be Pow(2), 1024 = 512 samples = 64ms */
static char inRingBuf[RBSZ], outRingBuf[RBSZ]; 
static RingBuffer inRing, outRing;

static int oneStream;
static int virtualMono;

static int running;

/* scan devices and stash pointers to dev structures. 
 *  But, these structures only remain valid while Pa is initialized,
 *  which, with pablio, is only while it's running!
 *  Also, storing these things in two separate arrays loses the actual
 *  PaDeviceID's associated with devices (since their index in these
 *  input/output arrays isn't the same as their index in the combined
 *  array */
static int scan_devices(struct iaxc_audio_driver *d) {
    int nDevices; 
    int i;

    d->nDevices = nDevices = Pa_CountDevices();
    d->devices = malloc(nDevices * sizeof(struct iaxc_audio_device));

    for(i=0;i<nDevices;i++)
    {
	const PaDeviceInfo *pa;	
	struct iaxc_audio_device *dev;

	pa=Pa_GetDeviceInfo(i);
	dev = &(d->devices[i]);

	dev->name = (char *)pa->name;
	dev->devID = i;
	dev->capabilities = 0;

	if(pa->maxInputChannels > 0)
	  dev->capabilities |= IAXC_AD_INPUT;

	if(pa->maxOutputChannels > 0)
	  dev->capabilities |= IAXC_AD_OUTPUT;

	if(i == Pa_GetDefaultInputDeviceID())
	  dev->capabilities |= IAXC_AD_INPUT_DEFAULT;

	if(i == Pa_GetDefaultOutputDeviceID())
	  dev->capabilities |= IAXC_AD_OUTPUT_DEFAULT;
    }
    return 0;
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

int pa_callback(void *inputBuffer, void *outputBuffer,
	    unsigned long framesPerBuffer, PaTimestamp outTime, void *userData ) {
    int totBytes = framesPerBuffer * sizeof(SAMPLE);

    short virtualBuffer[FRAMES_PER_BUFFER * 2];

    if(virtualMono && framesPerBuffer > FRAMES_PER_BUFFER) {
	fprintf(stderr, "ERROR: buffer in callback is too big!\n");
	exit(1);
    }

    if(inputBuffer) {
	/* input overflow might happen here */
	if(virtualMono) {
	  stereo2mono(virtualBuffer, inputBuffer, framesPerBuffer);
	  RingBuffer_Write(&inRing, virtualBuffer, totBytes);
	} else {
	  RingBuffer_Write(&inRing, inputBuffer, totBytes);
	}
    }
    if(outputBuffer)
    {  
	int bWritten;
	/* output underflow might happen here */
	if(virtualMono) {
	  bWritten = RingBuffer_Read(&outRing, virtualBuffer, totBytes);
	  mono2stereo(outputBuffer, virtualBuffer, bWritten/2);
	  bWritten *=2;
	} else {
	  bWritten = RingBuffer_Read(&outRing, outputBuffer, totBytes);
	}

	/* zero underflowed space [ silence might be more golden than garbage? ] */
	if(bWritten < totBytes)
	    memset((char *)outputBuffer + bWritten, 0, totBytes - bWritten);
    }
    return 0; 
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
int pa_openstreams (struct iaxc_audio_driver *d ) {
    PaError err;
#ifndef MACOSX
    /* first, try opening one stream for in/out, Mono */
    /* except for MacOSX, which needs virtual stereo */
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  1, paInt16, NULL,  /* input info */
	      selectedOutput, 1, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      0,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
      
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
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  2, paInt16, NULL,  /* input info */
	      selectedOutput, 2, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      0,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
    
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
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  2, paInt16, NULL,  /* input info */
	      paNoDevice, 0, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      0,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
    if( err != paNoError ) 
    {
	handle_paerror(err, "opening separate input stream");
	return -1;
    }

    err = Pa_OpenStream ( &oStream, 
	      paNoDevice, 0, paInt16, NULL,  /* input info */
	      selectedOutput,  2, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      0,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
    if( err != paNoError ) 
    {
	handle_paerror(err, "opening separate output stream");
	return -1;
    }

    return 0;
}

int pa_start (struct iaxc_audio_driver *d ) {
    PaError err;

    if(running) return 0;
	
    //fprintf(stderr, "starting pa\n");

    if(pa_openstreams(d)) 
      return -1;

    err = Pa_StartStream(iStream); 
    if(err != paNoError)
	return -1;

    if(!oneStream){ 
	err = Pa_StartStream(oStream);
	if(err != paNoError) {
	    Pa_StopStream(iStream);
	    return -1;
	}
    }

    running = 1;
    return 0;
}

int pa_stop (struct iaxc_audio_driver *d ) {
    PaError err;

    if(!running) return 0;

    err = Pa_AbortStream(iStream); 
    err = Pa_CloseStream(iStream); 

    if(!oneStream){ 
	err = Pa_AbortStream(oStream);
	err = Pa_CloseStream(oStream);
    }

    running = 0;
    return 0;
}

void pa_shutdown_audio() {
    CloseAudioStream( iStream );
    if(!oneStream) CloseAudioStream( oStream );
}

void handle_paerror(PaError err, char * where) {
	fprintf(stderr, "PortAudio error at %s: %s\n", where, Pa_GetErrorText(err));
}

int pa_input(struct iaxc_audio_driver *d, void *samples, int *nSamples) {
	static SAMPLE *stereoBuf = NULL;
	static int stereoBufSiz = 0;
	int bytestoread;

	bytestoread = *nSamples * sizeof(SAMPLE);

	/* we don't return partial buffers */
	if(RingBuffer_GetReadAvailable(&inRing) < bytestoread) {
	    *nSamples = 0;
	    return 0;	
	}

	RingBuffer_Read(&inRing, samples, bytestoread);

	return 0;
}

int pa_output(struct iaxc_audio_driver *d, void *samples, int nSamples) {
	static SAMPLE *stereoBuf = NULL;
	static int stereoBufSiz = 0;
	int bytestowrite = nSamples * sizeof(SAMPLE);

	RingBuffer_Write(&outRing, samples, bytestowrite);

	return 0;
}

int pa_select_devices (struct iaxc_audio_driver *d, int input, int output, int ring) {
    selectedInput = input;
    selectedOutput = output;
    selectedRing = ring;
    if(running) {
      pa_stop(d);
      pa_start(d);
    }
    return 0;
}

int pa_selected_devices (struct iaxc_audio_driver *d, int *input, int *output, int *ring) {
    *input = selectedInput;
    *output = selectedOutput;
    *ring = selectedRing;
    return 0;
}

int pa_destroy (struct iaxc_audio_driver *d ) {
    //implementme
    return 0;
}

/* initialize audio driver */
int pa_initialize (struct iaxc_audio_driver *d ) {
    PaError  err;

    /* initialize portaudio */
    if(paNoError != (err = Pa_Initialize()))
	return err;

    /* scan devices */
    scan_devices(d);

    /* setup methods */
    d->initialize = pa_initialize;
    d->destroy = pa_destroy;
    d->select_devices = pa_select_devices;
    d->selected_devices = pa_selected_devices;
    d->start = pa_start;
    d->stop = pa_stop;
    d->output = pa_output;
    d->input = pa_input;

    /* setup private data stuff */
    selectedInput  = Pa_GetDefaultInputDeviceID();
    selectedOutput = Pa_GetDefaultOutputDeviceID();
    selectedRing   = Pa_GetDefaultOutputDeviceID();

    RingBuffer_Init(&inRing, RBSZ, inRingBuf);
    RingBuffer_Init(&outRing, RBSZ, outRingBuf);

    running = 0;

    return 0;
}
