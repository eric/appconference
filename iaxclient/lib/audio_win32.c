#include "iaxclient_lib.h"
#include "audio_win32.h"
#include "audio_encode.h"

/* initialize the sequence variables for the audio in stuff */
unsigned int whinserial,nextwhin;

WHOUT *outqueue = NULL;

int win_initialize_audio() {

	/* setup the format for opening audio channels */
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.nSamplesPerSec = 8000;
	wf.nAvgBytesPerSec = 16000;
	wf.nBlockAlign = 2;
	wf.wBitsPerSample = 16;
	wf.cbSize = 0;
	whinserial = 1;
	nextwhin = 1;
	/* open the audio out channel */
	if (waveOutOpen(&wout,0,&wf,0,0,CALLBACK_NULL) != MMSYSERR_NOERROR)
	{
		fprintf(stderr,"Fatal Error: Failed to open wave output device\n");
		return -1;
	}
	/* open the audio in channel */
	if (waveInOpen(&win,0,&wf,0,0,CALLBACK_NULL) != MMSYSERR_NOERROR)
	{
		fprintf(stderr,"Fatal Error: Failed to open wave input device\n");
		waveOutReset(wout);
		waveOutClose(wout);
		return -1;
	}

	/* initialize the audio in buffer structures */
	memset(&whin,0,sizeof(whin));
	return 0;
}

void win_shutdown_audio() {
	waveInStop(win);
	waveInReset(win);
	waveInClose(win); 
	waveOutReset(wout);
	waveOutClose(wout);
}

void win_play_recv_audio(void *fr, int fr_size)
{	
	int n;
	WHOUT *wh,*wh1;
	//short fr[160];
	static paused_xmit = 0;
	/* get count of pending items in audio output queue */
	n = 0; 
	if (outqueue) 
	{	/* determine number of pending out queue items */
		for(wh = outqueue; wh != NULL; wh = wh->next)
		{
			if (!(wh->w.dwFlags & WHDR_DONE)) n++;
		}
	}
	/* if not too many, send to user, otherwise chuck packet */
	if (n <= OUT_DEPTH) /* if not to chuck packet */
	{
		/* malloc the memory for the queue item */
		wh = (WHOUT *) malloc(sizeof(WHOUT));
		if (wh == (WHOUT *) NULL) /* if error, bail */
		{
			fprintf(stderr,"Outa memory!!!!\n");
			exit(255);
		}
		/* initialize the queue entry */
		memset(wh,0,sizeof(WHOUT));
		/* copy the PCM data from the gsm conversion buffer */
		memcpy((char *)wh->data,(char *)fr,fr_size);
		/* set parameters for data */
		wh->w.lpData = (char *) wh->data;
		wh->w.dwBufferLength = 320;
		
		/* prepare buffer for output */
		if (waveOutPrepareHeader(wout,&wh->w,sizeof(WAVEHDR)))
		{
			fprintf(stderr,"Cannot prepare header for audio out\n");
			exit(255);
		}
		/* if not currently transmitting, hold off a couple of packets for 
			smooth sounding output */
		if ((!n) && (!paused_xmit))
		{
			/* pause output (before starting) */
			waveOutPause(wout);
			/* indicate as such */
			paused_xmit = 1;
		}
		/* queue packet for output on audio device */
		if (waveOutWrite(wout,&wh->w,sizeof(WAVEHDR)))
		{
			fprintf(stderr,"Cannot output to wave output device\n");
			exit(255);
		}
		/* if we are paused, and we have enough packets, start audio */
		if ((n > OUT_PAUSE_THRESHOLD) && paused_xmit)
		{
			/* start the output */
			waveOutRestart(wout);
			/* indicate as such */
			paused_xmit = 0;
		}
		/* insert it onto tail of outqueue */
		if (outqueue == NULL) /* if empty queue */
			outqueue = wh; /* point queue to new entry */
		else /* otherwise is non-empty queue */
		{
			wh1 = outqueue;
			while(wh1->next) wh1 = wh1->next; /* find last entry in queue */
			wh1->next = wh; /* point it to new entry */
		}
	} 
#ifdef	PRINTCHUCK
	else printf("Chucking packet!!\n");
#endif	
}

int win_prepare_audio_buffers()
{
	int i;
	/* go through all audio in buffers, and prepare and queue ones that are currently idle */
	for(i = 0; i < NWHIN; i++)
	{
//		service_network(netfd,f); /* service network stuff here for better performance */
		if (!(whin[i].dwFlags & WHDR_PREPARED)) /* if not prepared, do so */
		{
			/* setup this input buffer header */
			memset(&whin[i],0,sizeof(WAVEHDR));
			whin[i].lpData = bufin[i];
			whin[i].dwBufferLength = 320;
			whin[i].dwUser = whinserial++; /* set 'user data' to current serial number */
			/* prepare the buffer */
			if (waveInPrepareHeader(win,&whin[i],sizeof(WAVEHDR)))
			{
				fprintf(stderr,"Unable to prepare header for input\n");
				return -1;
			}
			/* add it to device (queue) */
			if (waveInAddBuffer(win,&whin[i],sizeof(WAVEHDR)))
			{
				fprintf(stderr,"Unable to prepare header for input\n");
				return -1;
			}
		}
		waveInStart(win); /* start it (if not already started) */
	}
	return 0;
}

void win_flush_audio_output_buffers() 
{
	int i, c;
	if (outqueue) /* if stuff in audio output queue, free it up if its available */
	{
		/* go through audio output queue */
		for(wh = outqueue,wh1 = wh2 = NULL,i = 0; wh != NULL; wh = wh->next)
		{
			//service_network(netfd,f); /* service network here for better performance */
			/* if last one was removed from queue, zot it here */
			if (i && wh1)
			{ 
//				free(wh1->data);
				free(wh1);
//				wh1 = NULL;
				wh1 = wh2;
			}
			i = 0; /* reset "last one removed" flag */
			if (wh->w.dwFlags & WHDR_DONE) /* if this one is done */
			{
				/* prepare audio header */
				if ((c = waveOutUnprepareHeader(wout,&wh->w,sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
				{ 
					fprintf(stderr,"Cannot unprepare audio out header, error %d\n",c);
					exit(255);
				}
				if (wh1 != NULL) /* if there was a last one */
				{
					wh1->next = wh->next;
				} 
				if (outqueue == wh) /* is first one, so set outqueue to next one */
				{
					outqueue = wh->next;
				}
				i = 1; /* set 'to free' flag */
			}

			wh2 = wh1;	/* save old,old wh pointer */
			wh1 = wh; /* save the old wh pointer */
		}
	}
}

/*int process_call_audio()
{
	return 0;
}*/

int win_process_audio_buffers(unsigned long *outtick, struct peer *most_recent_answer, int iEncodeType)
{
	int i;
	unsigned long lastouttick = *outtick;

	for(;;) {
		for(i = 0; i < NWHIN; i++) /* find an available one that's the one we are looking for */
		{
//			service_network(netfd,f); /* service network here for better performance */
			/* if not time to send any more, dont */
			if (GetTickCount() < (lastouttick + OUT_INTERVAL))
			{
				i = NWHIN; /* set to value that WILL exit loop */
				break;
			}
			if (win_audio_ready(i) == 1) {
		
				/* must have read exactly 320 bytes */
				if (win_check_audio_packet_size(i) == 0)
				{
//					fprintf(stderr,"Short audio read, got %d bytes, expected %d bytes\n", whin[i].dwBytesRecorded,
//						get_audio_packet_size(i));
					return -1;
				}
				send_encoded_audio(most_recent_answer, win_get_audio_data(i), iEncodeType);
				lastouttick = GetTickCount(); /* save time of last output */
				/* unprepare (free) the header */
				win_free_audio_header(i);
				/* bump the serial number to look for the next time */
				win_bump_audio_sn();
				/* exit the loop so that we can start at lowest buffer again */
				break;
			}
		} 
		if (i >= NWHIN) break; /* if all found, get out of loop */
	}
	*outtick = lastouttick;
	return 0;
}

int win_audio_ready(int i)
{
	if ((whin[i].dwUser == nextwhin) && (whin[i].dwFlags & WHDR_DONE))  /* if audio is ready */
		return 1;
	return 0;
}

int win_check_audio_packet_size(int i)
{
	if (whin[i].dwBytesRecorded != whin[i].dwBufferLength)
		return 0;
	return 1;
}

int win_get_audio_packet_size(int i)
{
	return whin[i].dwBytesRecorded;
}

void win_free_audio_header(int i)
{
	waveInUnprepareHeader(win,&whin[i],sizeof(WAVEHDR));
	/* initialize the buffer */
	memset(&whin[i],0,sizeof(WAVEHDR));
}

void win_bump_audio_sn()
{
	nextwhin++;
}

void *win_get_audio_data(int i)
{
	return whin[i].lpData;
}
