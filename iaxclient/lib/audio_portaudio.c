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
#include "portmixer/px_common/portmixer.h"

#ifdef USE_MEC2
#include "mec3.h"
static echo_can_state_t *ec;
#endif

#ifdef SPAN_EC
#include "ec/echo.h"
static echo_can_state_t *ec;
#endif

#ifdef SPEEX_EC
#include "speex/speex_echo.h"
static SpeexEchoState *ec;
#endif

#define EC_RING_SZ  8192 /* must be pow(2) */


static PortAudioStream *iStream, *oStream, *aStream;
static PxMixer *iMixer = NULL, *oMixer = NULL;

static int selectedInput, selectedOutput, selectedRing;

#define FRAMES_PER_BUFFER 80 /* 80 frames == 10ms */
#define ECHO_TAIL	  4096 /* echo_tail length, in frames must be pow(2) for mec/span ? */

#define RBSZ 8192 /* Needs to be Pow(2), 1024 = 512 samples = 64ms */

/* TUNING:  The following constants may help in tuning for situations
 * where you are getting audio-level under/overruns.
 *
 * If you are running iaxclient on a system where you cannot get
 * low-latency scheduling, you may need to increase these.  This tends
 * to be an issue on non-MacOSX unix systems, when you are not running
 * as root, and cannot ask the OS for higher priority.  
 *
 * RBOUTMAXSZ:  This a target size of the output ringbuffer, where audio
 * for your speakers goes after being decoded and mixed, and before the
 * audio callback asks for it.  It can get larget than this (up to RBSZ,
 * above), but when it does, for a bit, we will start dropping some
 * frames.  For no drops at all, this needs to be set to contain the
 * number of samples in your largest scheduling gap
 *
 * PA_NUMBUFFERS:  This is the number of buffers that the low-level
 * operating system driver will use, for buffering our output (and also
 * our input) between the soundcard and portaudio.  This should also be
 * set to the maximum scheduling delay.  Some drivers, though, will
 * callback _into_ portaudio with a higher priority, so this doesn't
 * necessarily need to be as big as RBOUTMAXSZ, although on linux, it
 * does.  The default is to leave this up to portaudio..
 */

#define RBOUTMAXSZ 80 * 16 /* 16 = bytes/ms -- if average outRing length is more than this many bytes, start dropping */

#define PA_NUMBUFFERS 0
// try setting this to our RBOUTMAXSZ?
//#define PA_NUMBUFFERS (RBOUTMAXSZ / (2*FRAMES_PER_BUFFER))
//
static char inRingBuf[RBSZ], outRingBuf[RBSZ];
static RingBuffer inRing, outRing;

static int outRingLenAvg;

static int oneStream;
static int auxStream;
static int virtualMonoIn;
static int virtualMonoOut;

static int running;

static struct iaxc_sound *sounds;
static int  nextSoundId = 1;

static MUTEX sound_lock;

int pa_start (struct iaxc_audio_driver *d ) ;

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

        if(virtualMonoOut)
	  val = ((short *)dst)[2*i] + ((short *)src)[i];
	else
	  val = ((short *)dst)[i] + ((short *)src)[i];

        if(val > 0x7fff) {
            val = 0x7fff-1;
        } else if (val < -0x7fff) {
            val = -0x7fff+1;
        } 

	if(virtualMonoOut) {
	    dst[2*i] = val;
	    dst[2*i+1] = val;
	} else {
	    dst[i] = val;
	}
	
    }
}

int pa_mix_sounds (void *outputBuffer, unsigned long frames, int channel) {
    struct iaxc_sound *s;
    struct iaxc_sound **sp;
    unsigned long outpos;


  MUTEXLOCK(&sound_lock);
    /* mix each sound into the outputBuffer */
    sp = &sounds;
    while(sp && *sp)  {
      s = *sp;
      outpos = 0;
      
      if(s->channel == channel) {
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
	  mix_slin((short *)outputBuffer + outpos, s->data+s->pos, n); 

	  s->pos += n;
	  outpos += n;
	}
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
  sound->channel = ring;
  sound->id = nextSoundId++; 
  sound->pos = 0;
    
  sound->next = sounds;
  sounds = sound;
  MUTEXUNLOCK(&sound_lock);

  if(!running) pa_start(NULL); /* XXX fixme: start/stop semantics */

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

static void iaxc_echo_can(short *inputBuffer, short *outputBuffer, int n)
{
    static RingBuffer outRing;
    static char outRingBuf[EC_RING_SZ];
    static long bias = 0;
    short  delayedBuf[1024];
    int i;

    /* remove bias -- whether ec is on or not. */
    for(i = 0; i < n; i++) {
	bias +=  ((((long)inputBuffer[i]) << 15) - bias) >> 14;
	inputBuffer[i] -= (bias >> 15);
    }


    /* if ec is off, clear ec state -- this way, we start fresh if/when
     * it's turned back on. */
    if(!(iaxc_filters & IAXC_FILTER_ECHO)) {
	if(ec)  {
#if defined(USE_MEC2) || defined(SPAN_EC)
	  echo_can_free(ec); 
	  ec = NULL;
#endif
#if defined(SPEEX_EC)
	  speex_echo_state_destroy(ec);
	  ec = NULL;
#endif
	}
	    
	return;
    }

    /* we want echo cancellation */

    if(!ec) {
	RingBuffer_Init(&outRing, EC_RING_SZ, &outRingBuf);
#if defined(USE_MEC2) || defined(SPAN_EC)
	ec = echo_can_create(ECHO_TAIL, 0);
#endif
#if defined(SPEEX_EC)
	ec = speex_echo_state_init(FRAMES_PER_BUFFER, ECHO_TAIL); 
#endif
    }

    /* fill outRing */
    RingBuffer_Write(&outRing, outputBuffer, n * 2);

    // Make sure we have enough buffer.
    // Currently, just one FRAMES_PER_BUFFER's worth.
    if(RingBuffer_GetReadAvailable(&outRing) < ((n + 80) * 2) ) 
      return;

    RingBuffer_Read(&outRing, delayedBuf, n * 2);

    
    
#if defined(SPEEX_EC)
    {
      short cancelledBuffer[1024];

      speex_echo_cancel(ec, inputBuffer, delayedBuf, cancelledBuffer, NULL);

      for(i=0;i<n;i++)
	  inputBuffer[i] =  cancelledBuffer[i];
    }
#endif

#if defined(USE_MEC2) || defined(SPAN_EC)
      for(i=0;i<n;i++)  
	inputBuffer[i] = echo_can_update(ec, delayedBuf[i], inputBuffer[i]);
#endif

}

int pa_callback(void *inputBuffer, void *outputBuffer,
	    unsigned long framesPerBuffer, PaTimestamp outTime, void *userData ) {

    int totBytes = framesPerBuffer * sizeof(SAMPLE);

    short virtualInBuffer[FRAMES_PER_BUFFER * 2];
    short virtualOutBuffer[FRAMES_PER_BUFFER * 2];

#if 0
    /* I think this can't happen */
    if(virtualMono && framesPerBuffer > FRAMES_PER_BUFFER) {
	fprintf(stderr, "ERROR: buffer in callback is too big!\n");
	exit(1);
    }
#endif

    if(outputBuffer)
    {  
	int bWritten;
	/* output underflow might happen here */
	if(virtualMonoOut) {
	  bWritten = RingBuffer_Read(&outRing, virtualOutBuffer, totBytes);
	  /* we zero "virtualOutBuffer", then convert the whole thing,
	   * yes, because we use virtualOutBuffer for ec below */
	  if(bWritten < totBytes) {
	      memset(((char *)virtualOutBuffer) + bWritten, 0, totBytes - bWritten);
	      //fprintf(stderr, "*U*");
	  }
	  mono2stereo(outputBuffer, virtualOutBuffer, framesPerBuffer);
	} else {
	  bWritten = RingBuffer_Read(&outRing, outputBuffer, totBytes);
	  if(bWritten < totBytes) {
	      memset((char *)outputBuffer + bWritten, 0, totBytes - bWritten);
	      //fprintf(stderr, "*U*");
	  }
	}

	/* zero underflowed space [ silence might be more golden than garbage? ] */

	pa_mix_sounds(outputBuffer, framesPerBuffer, 0);

	if(!auxStream)
	    pa_mix_sounds(outputBuffer, framesPerBuffer, 1);
    }


    if(inputBuffer) {
	/* input overflow might happen here */
	if(virtualMonoIn) {
	  stereo2mono(virtualInBuffer, inputBuffer, framesPerBuffer);
	  iaxc_echo_can(virtualInBuffer, virtualOutBuffer, framesPerBuffer);

	  RingBuffer_Write(&inRing, virtualInBuffer, totBytes);
	} else {

	  iaxc_echo_can(inputBuffer, outputBuffer, framesPerBuffer);

	  RingBuffer_Write(&inRing, inputBuffer, totBytes);
	}
    }

    return 0; 
}

int pa_aux_callback(void *inputBuffer, void *outputBuffer,
	    unsigned long framesPerBuffer, PaTimestamp outTime, void *userData ) {

    int totBytes = framesPerBuffer * sizeof(SAMPLE);

    /* XXX: need to handle virtualMonoOut case!!! */
    if(outputBuffer)
    {  
        memset((char *)outputBuffer, 0, totBytes);
	pa_mix_sounds(outputBuffer, framesPerBuffer, 1);
    }
    return 0; 
}


/* some commentary here:
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
 *
 * Unfortunately, things get more complicated for MacOSX.  In a reported
 * case, a user has stereo output, but only Mono Input (a particular
 * Plantronics USB headset.  So, we need to handle a new case of mono 
 * input, but virtualMono output.  argh.
 *
 * We could probably do this more cleanly, because there are still cases
 * where we will fail (i.e. if the user has only mono in and out on a Mac).
 *
 * */
int pa_openstreams (struct iaxc_audio_driver *d ) {
    PaError err;

#if 0
//#ifndef MACOSX
    /* first, try opening one stream for in/out, Mono */
    /* except for MacOSX, which needs virtual stereo */
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  1, paInt16, NULL,  /* input info */
	      selectedOutput, 1, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
      
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	virtualMonoIn = virtualMonoOut = 0;
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
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
    
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	virtualMonoIn = virtualMonoOut = 1;
	return 0;
    }

    /* then, we try a single stream, virtual stereo.  Except on linux,
     * see note above */
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  1, paInt16, NULL,  /* input info */
	      selectedOutput, 2, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_callback, 
	      NULL /* userdata */
      );
    
    if( err == paNoError ) {
	/* if this works, set iStream, oStream to this stream */
	oStream = iStream;
	oneStream = 1;
	virtualMonoIn = 0;
	virtualMonoOut = 1;
	return 0;
    }
#endif

    /* finally, we go to the worst case.  Two opens, virtual mono */
    oneStream = 0;
    virtualMonoIn = virtualMonoOut = 1;
    err = Pa_OpenStream ( &iStream, 
	      selectedInput,  2, paInt16, NULL,  /* input info */
	      paNoDevice, 0, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
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
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
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

int pa_openauxstream (struct iaxc_audio_driver *d ) {
    PaError err;

    err = Pa_OpenStream ( &aStream, 
	      paNoDevice, 0, paInt16, NULL,  /* input info */
	      selectedRing,  virtualMonoOut+1, paInt16, NULL,  /* output info */
	      8000.0, 
	      FRAMES_PER_BUFFER,  /* frames per buffer -- 10ms */
	      PA_NUMBUFFERS,   /* numbuffers */  /* use default */
	      0,   /* flags */
	      pa_aux_callback, 
	      NULL /* userdata */
      );
    if( err != paNoError ) 
    {
	handle_paerror(err, "opening separate ring stream");
	return -1;
    }

    return 0;
}

int pa_start (struct iaxc_audio_driver *d ) {
    PaError err;
    static int errcnt=0;

    if(running) return 0;

    /* re-open mixers if necessary */
    if(iMixer)
    { 
	Px_CloseMixer(iMixer);
	iMixer = NULL;
    }

    if(oMixer) 
    {
	Px_CloseMixer(oMixer);
	oMixer = NULL;
    }
	
    //fprintf(stderr, "starting pa\n");

    if(errcnt >= 5) {
	iaxc_usermsg(IAXC_TEXT_TYPE_FATALERROR,
		"iaxclient audio: Can't open Audio Device, we tried 5 times.  Perhaps you do not have an input or output device?");
	return -1; // Give Up.  Too many errors.
    }

    if(pa_openstreams(d))  {
	errcnt++;
        return -1;
    }

    errcnt = 0; // only count consecutive errors.

    err = Pa_StartStream(iStream); 
    if(err != paNoError)
	return -1;

    iMixer = Px_OpenMixer(iStream, 0);

    if(!oneStream){ 
	err = Pa_StartStream(oStream);
	oMixer = Px_OpenMixer(oStream, 0);
	if(err != paNoError) {
	    Pa_StopStream(iStream);
	    return -1;
	}
    }
    
    if(selectedRing != selectedOutput) {
        auxStream = 1;
    } else {
        auxStream = 0;
    }

    if(auxStream){ 
        pa_openauxstream(d); 
	err = Pa_StartStream(aStream);
	if(err != paNoError) {
	    auxStream = 0;
	}
    }

	/* select the microphone as the input source */
	if ( iMixer != NULL )
	{
#ifdef WIN32 // temporary until other impl have this function
		_Px_SetCurrentInputSourceByName( iMixer, "microphone" ) ;
		_Px_SetMicrophoneBoost( iMixer, 0 ) ;
#else
		int n = Px_GetNumInputSources( iMixer ) - 1 ;
		for ( ; n > 0 ; --n )
		{
			if ( strcasecmp( "microphone", Px_GetInputSourceName( iMixer, n ) ) == 0 )
			{
				Px_SetCurrentInputSource( iMixer, n ) ;
			}
		}
#endif
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

    if(auxStream){ 
	err = Pa_AbortStream(aStream);
	err = Pa_CloseStream(aStream);
    }

    running = 0;
    return 0;
}

void pa_shutdown() {
    CloseAudioStream( iStream );
    if(!oneStream) CloseAudioStream( oStream );
    if(auxStream) CloseAudioStream( aStream );
}

void handle_paerror(PaError err, char * where) {
	fprintf(stderr, "PortAudio error at %s: %s\n", where, Pa_GetErrorText(err));
}

int pa_input(struct iaxc_audio_driver *d, void *samples, int *nSamples) {
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
	int bytestowrite = nSamples * sizeof(SAMPLE);
	int outRingLen;

	outRingLen = RingBuffer_GetReadAvailable(&outRing);
	outRingLenAvg = (outRingLenAvg * 9 + outRingLen ) / 10;

	if(outRingLen > RBOUTMAXSZ && outRingLenAvg > RBOUTMAXSZ)  {
	  //fprintf(stderr, "*O*");
	  return 0;
	}


	if(RingBuffer_GetWriteAvailable(&outRing) < bytestowrite)  fprintf(stderr, "O");

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

double pa_input_level_get(struct iaxc_audio_driver *d)
{
	/* iMixer should be non-null if we using either one or two streams */
    if(!iMixer) return -1;

	/* make sure this device supports input volume controls */
	if ( Px_GetNumInputSources( iMixer ) == 0 )
		return -1 ;

    return Px_GetInputVolume(iMixer);
}

double pa_output_level_get(struct iaxc_audio_driver *d){
    PxMixer *mix;

	/* oMixer may be null if we're using one stream,
	   in which case, iMixer should not be null,
	   if it is, return an error */
	   
    if(oMixer)
      mix = oMixer;
    else if (iMixer)
      mix = iMixer;
    else
      return -1;

	/* prefer the pcm output, but default to the master output */
    return Px_GetOutputVolume( mix, Px_SupportsPCMOutputVolume( mix ) ) ;
}

int pa_input_level_set(struct iaxc_audio_driver *d, double level){
    if(!iMixer) return -1;
     
	/* make sure this device supports input volume controls */
	if ( Px_GetNumInputSources( iMixer ) == 0 )
		return -1 ;

    //fprintf(stderr, "setting input level to %f\n", level);
    Px_SetInputVolume(iMixer, level);
    return 0;
}

int pa_output_level_set(struct iaxc_audio_driver *d, double level){
    PxMixer *mix;

    if(oMixer)
      mix = oMixer;
    else if (iMixer)
      mix = iMixer;
    else
      return -1;

	/* prefer the pcm output, but default to the master output */
    Px_SetOutputVolume( mix, Px_SupportsPCMOutputVolume( mix ), level ) ;

    return 0;
}

int pa_mic_boost_get( struct iaxc_audio_driver* d )
{
	int enable = -1 ;
#ifdef WIN32
	if ( iMixer != NULL )
	{
		enable = _Px_GetMicrophoneBoost( iMixer ) ;
	}
#endif
	return enable ;
}

int pa_mic_boost_set( struct iaxc_audio_driver* d, int enable )
{
	int err = -1 ;
#ifdef WIN32
	if ( iMixer != NULL )
	{
		err = _Px_SetMicrophoneBoost( iMixer, enable ) ;
	}
#endif
	return err ;
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
    d->mic_boost_get = pa_mic_boost_get;
    d->mic_boost_set = pa_mic_boost_set;

    /* setup private data stuff */
    selectedInput  = Pa_GetDefaultInputDeviceID();
    selectedOutput = Pa_GetDefaultOutputDeviceID();
    selectedRing   = Pa_GetDefaultOutputDeviceID();
    sounds	   = NULL;
    MUTEXINIT(&sound_lock);

    

    RingBuffer_Init(&inRing, RBSZ, inRingBuf);
    RingBuffer_Init(&outRing, RBSZ, outRingBuf);

    running = 0;

    /* start, then stop streams, in order to test devices, and get mixers */
    pa_start(d);
    /* if input level is very low, raise it a bit;  helps AAGC work properly */
    {
      double level;
      level = pa_input_level_get(d);
      if(level < 0.5)
	pa_input_level_set(d,0.6);
    }
    pa_stop(d);

    return 0;
}
