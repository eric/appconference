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

#ifdef USE_MEC2
#include "mec2.h"
static echo_can_state_t *ec;
#endif

#ifdef SPAN_EC
#include "ec/echo.h"
static echo_can_state_t *ec;
#endif

#ifdef SPEEX_EC
#include "libspeex/speex_echo.h"
static SpeexEchoState *ec;
#endif

static PortAudioStream *iStream, *oStream;

static selectedInput, selectedOutput, selectedRing;

#define FRAMES_PER_BUFFER 80 /* 80 frames == 10ms */

#define RBSZ 1024 /* Needs to be Pow(2), 1024 = 512 samples = 64ms */
static char inRingBuf[RBSZ], outRingBuf[RBSZ]; 
static RingBuffer inRing, outRing;

static int oneStream;
static int virtualMono;

static int running;

static struct iaxc_sound *sounds;
static int  nextSoundId = 1;

static MUTEX sound_lock;

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

	if(pa->maxOutputChannels > 0) {
	  dev->capabilities |= IAXC_AD_OUTPUT;
	  dev->capabilities |= IAXC_AD_RING;
	}

	if(i == Pa_GetDefaultInputDeviceID())
	  dev->capabilities |= IAXC_AD_INPUT_DEFAULT;

	if(i == Pa_GetDefaultOutputDeviceID()) {
	  dev->capabilities |= IAXC_AD_OUTPUT_DEFAULT;
	  dev->capabilities |= IAXC_AD_RING_DEFAULT;
	}
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

static void mix_slin(short *dst, short *src, int samples) {
    int i=0,val=0;
    for (i=0;i<samples;i++) {

        if(virtualMono)
	  val = ((short *)dst)[2*i] + ((short *)src)[i];
	else
	  val = ((short *)dst)[i] + ((short *)src)[i];

        if(val > 0x7fff) {
            val = 0x7fff-1;
        } else if (val < -0x7fff) {
            val = -0x7fff+1;
        } 

	if(virtualMono) {
	    dst[2*i] = val;
	    dst[2*i+1] = val;
	} else {
	    dst[i] = val;
	}
	
    }
}

int pa_mix_sounds (void *outputBuffer, unsigned long frames) {
    struct iaxc_sound *s;
    struct iaxc_sound **sp;
    unsigned long outpos;


  MUTEXLOCK(&sound_lock);
    /* mix each sound into the outputBuffer */
    sp = &sounds;
    while(sp && *sp)  {
	s = *sp;
	outpos = 0;

	/* loop over the sound until we've played it enough times, or we've filled the outputBuffer */
	for(;;) {
	  int n;

	  if(outpos == frames) break;  /* we've filled the buffer */
	  if(s->pos == s->len) {
	    if(s->repeat == 0) {
	       // XXX free the sound structure, and maybe the buffer!
	       (*sp) = s->next;
	       if(s->malloced)
		  free(s->data);
	       free(s);
	       break; 
	    }
	    s->pos = 0;
	    s->repeat--;
	  }

	  /* how many frames do we add in this loop? */
	  n = ((frames - outpos) < (s->len - s->pos)) ? (frames - outpos) : (s->len - s->pos);

	  /* mix in the frames */
	  mix_slin(outputBuffer + outpos, s->data+s->pos, n); 

	  s->pos += n;
	  outpos += n;
	}
	if((*sp)) /* don't advance if we removed this member */
	  sp = &((*sp)->next);
    }
  MUTEXUNLOCK(&sound_lock);
  return 0;
}

int pa_play_sound(struct iaxc_sound *inSound, int ring) {
  struct iaxc_sound *sound;

  sound = (struct iaxc_sound *)malloc(sizeof(struct iaxc_sound));
  if(!sound) return 1;

  *sound = *inSound;
  
  MUTEXLOCK(&sound_lock);
  sound->id = nextSoundId++; 
  sound->pos = 0;

  sound->next = sounds;
  sounds = sound;
  MUTEXUNLOCK(&sound_lock);

  if(!running) pa_start(); /* XXX fixme: start/stop semantics */

  return sound->id; 
}

int pa_stop_sound(int soundID) {
    struct iaxc_sound **sp;
    struct iaxc_sound *s;
    int retval = 1; /* not found */

  MUTEXLOCK(&sound_lock);
    for(sp = &sounds; *sp; (*sp) = (*sp)->next) {
	s = *sp;	
	if(s->id == soundID) {
	   if(s->malloced)
	     free(s->data);
	   /* remove from list */ 
	   (*sp) = s->next;
	   free(s);
	   
	   retval= 0; /* found */
	   break;
	}
    }
  MUTEXUNLOCK(&sound_lock);

  return retval; /* found? */
}

int pa_callback(void *inputBuffer, void *outputBuffer,
	    unsigned long framesPerBuffer, PaTimestamp outTime, void *userData ) {

    int totBytes = framesPerBuffer * sizeof(SAMPLE);

    short virtualInBuffer[FRAMES_PER_BUFFER * 2];
    short virtualOutBuffer[FRAMES_PER_BUFFER * 2];

    if(virtualMono && framesPerBuffer > FRAMES_PER_BUFFER) {
	fprintf(stderr, "ERROR: buffer in callback is too big!\n");
	exit(1);
    }

    if(outputBuffer)
    {  
	int bWritten;
	/* output underflow might happen here */
	if(virtualMono) {
	  bWritten = RingBuffer_Read(&outRing, virtualOutBuffer, totBytes);
	  mono2stereo(outputBuffer, virtualOutBuffer, bWritten/2);
	  bWritten *=2;
	} else {
	  bWritten = RingBuffer_Read(&outRing, outputBuffer, totBytes);
	}

	/* zero underflowed space [ silence might be more golden than garbage? ] */
	if(bWritten < totBytes)
	    memset((char *)outputBuffer + bWritten, 0, totBytes - bWritten);

	pa_mix_sounds(outputBuffer, framesPerBuffer);
    }


    if(inputBuffer) {
	/* input overflow might happen here */
	if(virtualMono) {
	  stereo2mono(virtualInBuffer, inputBuffer, framesPerBuffer);
#if defined(SPEEX_EC)
	  {
	      /* convert buffers to float, echo cancel, convert back */
	      float finBuffer[160], foutBuffer[160]; fcancBuffer[160];
	      for(i=0;i<160;i++)
	      {
		  finBuffer[i] = virtualInBuffer[i]/32767.0f;
		  foutBuffer[i] = virtualOutBuffer[i]/32767.0f;
	      }
	      speex_echo_cancel(ec, foutBuffer, finBuffer, fcancBuffer, NULL);
	      for(i=0;i<160;i++)
	      {
		  virtualInBuffer[i] =  (short)(fcancBuffer * 32767.0f);
	      }

	  }
#endif
#if defined(USE_MEC2) || defined(SPAN_EC)
	  {   /* Echo Can, for virtualMono */
	      int i;
	      for(i=0;i<framesPerBuffer;i++) 
		virtualInBuffer[i] = echo_can_update(ec, virtualOutBuffer[i], virtualInBuffer[i]);
	  }
#endif
	  RingBuffer_Write(&inRing, virtualInBuffer, totBytes);
	} else {
#if defined(SPEEX_EC)
	  {
	      /* convert buffers to float, echo cancel, convert back */
	      float finBuffer[160], foutBuffer[160]; fcancBuffer[160];
	      for(i=0;i<160;i++)
	      {
		  finBuffer[i] = inBuffer[i]/32767.0f;
		  foutBuffer[i] = outBuffer[i]/32767.0f;
	      }
	      speex_echo_cancel(ec, foutBuffer, finBuffer, fcancBuffer, NULL);
	      for(i=0;i<160;i++)
	      {
		  inBuffer[i] =  (short)(fcancBuffer * 32767.0f);
	      }

	  }
#endif
#if defined(USE_MEC2) || defined(SPAN_EC)
	  {   /* Echo Can, for mono */
	      int i;
	      for(i=0;i<framesPerBuffer;i++) 
		((short *)inputBuffer)[i] = echo_can_update(ec, ((short *)outputBuffer)[i], ((short *)inputBuffer)[i]);
	  }
#endif
	  RingBuffer_Write(&inRing, inputBuffer, totBytes);
	}
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
    if(sounds) return 0;

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

double pa_input_level_get(struct iaxc_audio_driver *d){
    if(!running) return -1;
    return Pa_GetInputLevel(iStream);
}

double pa_output_level_get(struct iaxc_audio_driver *d){
    if(!running) return -1;
    return Pa_GetOutputLevel(oStream);
}

int pa_input_level_set(struct iaxc_audio_driver *d, double level){
    if(!running) return -1;
    return Pa_SetInputLevel(iStream, level);
}

int pa_output_level_set(struct iaxc_audio_driver *d, double level){
    if(!running) return -1;
    return Pa_SetOutputLevel(oStream, level);
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
    d->input_level_get = pa_input_level_get;
    d->input_level_set = pa_input_level_set;
    d->output_level_get = pa_output_level_get;
    d->output_level_set = pa_output_level_set;
    d->play_sound = pa_play_sound;
    d->stop_sound = pa_stop_sound;

    /* setup private data stuff */
    selectedInput  = Pa_GetDefaultInputDeviceID();
    selectedOutput = Pa_GetDefaultOutputDeviceID();
    selectedRing   = Pa_GetDefaultOutputDeviceID();
    sounds	   = NULL;
    MUTEXINIT(&sound_lock);

    

    RingBuffer_Init(&inRing, RBSZ, inRingBuf);
    RingBuffer_Init(&outRing, RBSZ, outRingBuf);

#if defined(USE_MEC2) || defined(SPAN_EC)
    ec = echo_can_create(2048, 0);
#endif
#if defined(SPEEX_EC)
    ec = speex_echo_state_init(160, 2048);
#endif

    running = 0;

    return 0;
}
