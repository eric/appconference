/*
 * PortMixer
 * Windows WMME Implementation
 *
 * Copyright (c) 2002
 *
 * Written by Dominic Mazzoni and Augustus Saunders
 *
 * PortMixer is intended to work side-by-side with PortAudio,
 * the Portable Real-Time Audio Library by Ross Bencina and
 * Phil Burk.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#undef __STRICT_ANSI__ //for strupr with ms

#include <windows.h>

#include <stdio.h>

#include "portaudio.h"
#include "pa_host.h"

#include "portmixer.h"

typedef struct PaWMMEStreamData
{
    /* Input -------------- */
    HWAVEIN            hWaveIn;
    WAVEHDR           *inputBuffers;
    int                currentInputBuffer;
    int                bytesPerHostInputBuffer;
    int                bytesPerUserInputBuffer;    /* native buffer size in bytes */
    /* Output -------------- */
    HWAVEOUT           hWaveOut;
} PaWMMEStreamData;

typedef struct PxSrcInfo
{
   char  name[256];
   DWORD lineID;
   DWORD controlID;
} PxSrcInfo;

typedef struct PxInfo
{
   HMIXEROBJ   hInputMixer;
   HMIXEROBJ   hOutputMixer;
   int         numInputs;
   PxSrcInfo   src[32];
   int         useMuxID;
   DWORD       muxID;
   DWORD       inputID;
   DWORD       speakerID;
   DWORD       waveID;
} PxInfo;

/* static function */
static int Px_InitInputVolumeControls( PxMixer* mixer, int hWaveIn ) ;
static int Px_InitOutputVolumeControls( PxMixer* mixer, int hWaveOut ) ;

int Px_GetNumMixers( void *pa_stream )
{
   return 1;
}

const char *Px_GetMixerName( void *pa_stream, int index )
{
   return "Mixer";
}

PxMixer *Px_OpenMixer( void *pa_stream, int index )
{

	/* initialize new mixer object */
	PxInfo* mixer = ( PxMixer* )( malloc( sizeof( PxInfo ) ) ) ;
	mixer->hInputMixer = NULL ;
	mixer->hOutputMixer = NULL ;

	internalPortAudioStream* past = ( internalPortAudioStream* )( pa_stream ) ;
	PaWMMEStreamData* wmmeStreamData = ( PaWMMEStreamData* )( past->past_DeviceData ) ;

	MMRESULT result ;

	if ( wmmeStreamData->hWaveIn != NULL )
	{
		/* initialize input volume controls */
		result = Px_InitInputVolumeControls( 
			( PxInfo* )( mixer ), 
			( UINT )( wmmeStreamData->hWaveIn ) 
		) ;

		if ( result != MMSYSERR_NOERROR )
		{
			free( mixer ) ;
			return NULL ;
		}		
	}
	
	if ( wmmeStreamData->hWaveOut != NULL )
	{
		/* initialize output volume controls */
		result = Px_InitOutputVolumeControls( 
			( PxInfo* )( mixer ),
			( UINT )( wmmeStreamData->hWaveOut ) 
		) ;

		if ( result != MMSYSERR_NOERROR )
		{
			free( mixer ) ;
			return NULL ;
		}		
	}

	// report found info
//	fprintf( stdout, "useMuxID => %d, muxID => %u, inputID => %u, speakerID => %u, waveID => %u\n", 
//		info->useMuxID, info->muxID, info->inputID, info->speakerID, info->waveID ) ;
	
	return mixer ;
}

void VolumeFunction(HMIXEROBJ hMixer, DWORD controlID, PxVolume *volume)
{
   MIXERCONTROLDETAILS details;
   MMRESULT result;
   MIXERCONTROLDETAILS_UNSIGNED value;

   memset(&value, 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));

   details.cbStruct = sizeof(MIXERCONTROLDETAILS);
   details.dwControlID = controlID;
   details.cChannels = 1; /* all channels */
   details.cMultipleItems = 0;
   details.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
   details.paDetails = &value;

   result = mixerGetControlDetails(hMixer, &details,
                                   MIXER_GETCONTROLDETAILSF_VALUE);

   if (*volume < 0.0) {
      *volume = (PxVolume)(value.dwValue / 65535.0);
   }
   else {
      if (result != MMSYSERR_NOERROR)
         return;
      value.dwValue = (unsigned short)(*volume * 65535.0);
      mixerSetControlDetails(hMixer, &details,
                             MIXER_GETCONTROLDETAILSF_VALUE);
   }
}

/*
 Px_CloseMixer() closes a mixer opened using Px_OpenMixer and frees any
 memory associated with it. 
*/

void Px_CloseMixer(PxMixer *mixer)
{
   PxInfo *info = (PxInfo *)mixer;

   if (info->hInputMixer)
      mixerClose((HMIXER)info->hInputMixer);
   if (info->hOutputMixer)
      mixerClose((HMIXER)info->hOutputMixer);
   free( mixer );
}

/*
 Master (output) volume
*/

PxVolume Px_GetMasterVolume( PxMixer *mixer )
{
   PxInfo *info = (PxInfo *)mixer;
   PxVolume vol;

   vol = -1.0;
   VolumeFunction(info->hOutputMixer, info->speakerID, &vol);
   return vol;
}

void Px_SetMasterVolume( PxMixer *mixer, PxVolume volume )
{
   PxInfo *info = (PxInfo *)mixer;

   VolumeFunction(info->hOutputMixer, info->speakerID, &volume);
}

/*
 PCM output volume
*/

int Px_SupportsPCMOutputVolume( PxMixer* mixer ) 
{
	PxInfo* info = ( PxInfo* )( mixer ) ;
	return ( info->waveID == -1 ) ? 0 : 1 ;
}

PxVolume Px_GetPCMOutputVolume( PxMixer *mixer )
{
	PxVolume volume = -1.0 ;

	PxInfo* info = ( PxInfo* )( mixer ) ;
	if ( info == NULL ) return volume ;

	VolumeFunction( info->hOutputMixer, info->waveID, &volume ) ;
	
	return volume ;
}

void Px_SetPCMOutputVolume( PxMixer *mixer, PxVolume volume )
{
	PxInfo* info = ( PxInfo* )( mixer ) ;
	if ( info == NULL ) return ;

	VolumeFunction( info->hOutputMixer, info->waveID, &volume ) ;
}

/*
 All output volumes
*/

int Px_GetNumOutputVolumes( PxMixer *mixer )
{
   PxInfo *info = (PxInfo *)mixer;

   return 2;
}

const char *Px_GetOutputVolumeName( PxMixer *mixer, int i )
{
   PxInfo *info = (PxInfo *)mixer;
   
   if (i==1)
      return "Wave Out";
   else
      return "Master Volume";
}

PxVolume Px_GetOutputVolume( PxMixer *mixer, int i )
{
   PxInfo *info = (PxInfo *)mixer;

   if (i==1)
      return Px_GetPCMOutputVolume(mixer);
   else
      return Px_GetMasterVolume(mixer);
}

void Px_SetOutputVolume( PxMixer *mixer, int i, PxVolume volume )
{
   PxInfo *info = (PxInfo *)mixer;

   if (i==1)
      Px_SetPCMOutputVolume(mixer, volume);
   else
      Px_SetMasterVolume(mixer, volume);
}

/*
 Input sources
*/

int Px_GetNumInputSources( PxMixer *mixer )
{
   PxInfo *info = (PxInfo *)mixer;
   
   return info->numInputs;
}

const char *Px_GetInputSourceName( PxMixer *mixer, int i)
{
   PxInfo *info = (PxInfo *)mixer;
   
   return info->src[i].name;
}

int Px_GetCurrentInputSource( PxMixer *mixer )
{
	PxInfo* info = ( PxInfo* )( mixer ) ;
	
	if ( info->useMuxID == 1 )
	{  
		MIXERCONTROLDETAILS_BOOLEAN flags[32] ;
	
		MIXERCONTROLDETAILS details ;
		details.cbStruct = sizeof( MIXERCONTROLDETAILS ) ;
		details.dwControlID = info->muxID ;
		details.cChannels = 1 ;
		details.cMultipleItems = info->numInputs ;
		details.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN ) ;
		details.paDetails = ( LPMIXERCONTROLDETAILS_BOOLEAN )&flags[0] ;
		
		MMRESULT result = mixerGetControlDetails(
			( HMIXEROBJ )( info->hInputMixer ),
			( LPMIXERCONTROLDETAILS )&details,
			MIXER_GETCONTROLDETAILSF_VALUE
		) ;
		
		if ( result == MMSYSERR_NOERROR )
		{
			int i = 0 ;
			for ( ; i < info->numInputs ; ++i )
			{
				if ( flags[i].fValue )
					return i ;
			}
		}
		else
		{
			// !!! handle errors !!!
		}
	}
	else
	{
		// use altenate input control id
		return info->inputID ;
	}
	
	return 0 ;
}

void Px_SetCurrentInputSource( PxMixer *mixer, int source_index )
{
	PxInfo* info = ( PxInfo* )( mixer ) ;

	if ( info->useMuxID == 1 )
	{  
		MIXERCONTROLDETAILS_BOOLEAN flags[32] ;
		memset( &flags, 0x0, sizeof( flags ) ) ;
		flags[ source_index ].fValue = 1 ;
		
		MIXERCONTROLDETAILS details ;
		details.cbStruct = sizeof( MIXERCONTROLDETAILS ) ;
		details.dwControlID = info->muxID ;
		details.cMultipleItems = info->numInputs ;
		details.cChannels = 1 ; 
		details.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN ) ;
		details.paDetails = ( LPMIXERCONTROLDETAILS_BOOLEAN )&flags[0] ;
	
		MMRESULT result = mixerSetControlDetails(
			( HMIXEROBJ )( info->hInputMixer ),
			( LPMIXERCONTROLDETAILS )&details,
			MIXER_SETCONTROLDETAILSF_VALUE
		) ;
		
		// !!! handle errors !!!
	}
	else
	{
		// we don't have a mux or mixer to work with, 
		// so we use the control id directly
		info->inputID = info->src[source_index].controlID ;
	}

	return ;
}

/*
 Input volume
*/

PxVolume Px_GetInputVolume( PxMixer *mixer )
{
	PxVolume volume = -1.0 ;

	PxInfo* info = ( PxInfo* )( mixer ) ;
	if ( info == NULL ) return volume ;

	if ( info->useMuxID == 1 )
	{
		int src = Px_GetCurrentInputSource( mixer ) ;
		VolumeFunction( info->hInputMixer, info->src[src].controlID, &volume ) ;
	}
	else
	{	
		VolumeFunction( info->hInputMixer, info->inputID, &volume ) ;
	}
	
	return volume ;
}

void Px_SetInputVolume( PxMixer *mixer, PxVolume volume )
{
	PxInfo* info = ( PxInfo* )( mixer ) ;
	if ( info == NULL ) return ;
	
	if ( info->useMuxID == 1 )
	{
		int src = Px_GetCurrentInputSource( mixer ) ;
		VolumeFunction( info->hInputMixer, info->src[src].controlID, &volume ) ;
	}
	else
	{
		VolumeFunction( info->hInputMixer, info->inputID, &volume ) ;
	}
	
	return ;
}

/*
  Balance
*/

int Px_SupportsOutputBalance( PxMixer *mixer )
{
   return 0;
}

PxBalance Px_GetOutputBalance( PxMixer *mixer )
{
   return 0.0;
}

void Px_SetOutputBalance( PxMixer *mixer, PxBalance balance )
{
}

/*
  Playthrough
*/

int Px_SupportsPlaythrough( PxMixer *mixer )
{
   return 0;
}

PxVolume Px_GetPlaythrough( PxMixer *mixer )
{
   return 0.0;
}

void Px_SetPlaythrough( PxMixer *mixer, PxVolume volume )
{
}


//
// alternate control initialization functions
//

static int Px_InitInputVolumeControls( PxMixer* mixer, int hWaveIn ) 
{
	MMRESULT mmr ;

	// cast void pointer
	PxInfo* info = ( PxInfo* )( mixer ) ;
	
	if ( info == NULL ) 
		return MMSYSERR_ERROR ;
	
	//
	// open the mixer device
	//
	
	mmr = mixerOpen( 
		( LPHMIXER )( &info->hInputMixer ), 
		( UINT )( hWaveIn ), 0, 0, 
		MIXER_OBJECTF_HWAVEIN 
	) ;
	
	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;
	
	//
	// get the line info for the wavein line
	//
	
	MIXERLINE mixerLine ;
    mixerLine.cbStruct = sizeof( MIXERLINE ) ;
    mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN ;
	
	mmr = mixerGetLineInfo(
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerLine,
		MIXER_GETLINEINFOF_COMPONENTTYPE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
	{
		mixerClose( (HMIXER)( info->hInputMixer ) ) ;
		return mmr ;
	}
	
	// we now know the number of inputs
	info->numInputs = mixerLine.cConnections ;
	
	//
	// find a mux or mixer control for the wavein line
	//

	// set defaults
	info->useMuxID = 0 ;
	
	LPMIXERCONTROL muxControl = malloc( sizeof( MIXERCONTROL ) * mixerLine.cControls ) ;

	MIXERLINECONTROLS muxLineControls ;
	muxLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
	muxLineControls.dwLineID = mixerLine.dwLineID ;
	muxLineControls.cControls = mixerLine.cControls ;
	muxLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
	muxLineControls.pamxctrl = ( LPMIXERCONTROL )( muxControl ) ;
	
	mmr = mixerGetLineControls(
		info->hInputMixer,
		&muxLineControls,
		MIXER_GETLINECONTROLSF_ALL
	) ;

	int i = 0 ;
	for ( ; i < mixerLine.cControls ; ++i )
	{
		if ( 
			muxControl[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX
			|| muxControl[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER
		)
		{
			// okay, we have a mux control, let's use it
			info->muxID = muxControl[i].dwControlID ;
			info->useMuxID = 1 ;
		}
		else if ( muxControl[i].dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME )
		{
			// normally the master volume, use as our default inputID
			info->inputID = muxControl[i].dwControlID ;
		}
	}

	free( muxControl ) ;

	//
	// gather information about the wavein line volume controls
	// 	

	if ( info->useMuxID == 1 )
	{
		// use mux controls
		
		MIXERCONTROLDETAILS_LISTTEXT mixList[32] ;

		MIXERCONTROLDETAILS details ;
		details.cbStruct = sizeof( MIXERCONTROLDETAILS ) ;
		details.dwControlID = info->muxID ;
		details.cChannels = 1 ;
		details.cbDetails = sizeof( MIXERCONTROLDETAILS_LISTTEXT ) ;
		details.paDetails = ( LPMIXERCONTROLDETAILS_LISTTEXT )&mixList[0] ;
		details.cMultipleItems = info->numInputs ;
		
		mmr = mixerGetControlDetails(
			( HMIXEROBJ )( info->hInputMixer ), 
			( LPMIXERCONTROLDETAILS )&details,
			MIXER_GETCONTROLDETAILSF_LISTTEXT
		) ;
		
		if ( mmr == MMSYSERR_NOERROR )
		{		
			int j = 0 ;
			for ( ; j < info->numInputs ; ++j ) 
			{
				// record the control's name and line id
				strcpy( info->src[j].name, mixList[j].szName ) ;
				info->src[j].lineID = mixList[j].dwParam1 ;

				// now get the control's volume control
				
				MIXERCONTROL control ;
			
				MIXERLINECONTROLS controls ;
				controls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
				controls.dwLineID = mixList[j].dwParam1 ;
				controls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME ;
				controls.cbmxctrl = sizeof( MIXERCONTROL ) ;
				controls.pamxctrl = &control ;
			
				control.cbStruct = sizeof( MIXERCONTROL ) ;
			
				mmr = mixerGetLineControls(
					info->hInputMixer,
					&controls,
					MIXER_GETLINECONTROLSF_ONEBYTYPE
				) ;

				if ( mmr != MMSYSERR_NOERROR )
					break ;
				
				info->src[j].controlID = control.dwControlID ;

//				fprintf( stdout, "INPUT :: index => %d, name => %s, lineID => %u\n", 
//					j, info->src[j].name, info->src[j].lineID, info->src[j].controlID ) ;
			}
		}
	}

	// if there was a problem with the muxID, 
	// reset so we can try the line controls directly
	if ( mmr != MMSYSERR_NOERROR )
	{
		info->useMuxID = 0 ;
	}

	if ( info->useMuxID == 0 )
	{
		// no mux, use line controls instead
	
		// remember number of connections ( sources ) for this line
		int sources = mixerLine.cConnections ;
		int j ;
		
		for ( j = 0 ; j < sources ; ++j )
		{
			//
			// get info about the current connection
			//
	
			MIXERLINE line ;
			line.cbStruct = sizeof( MIXERLINE ) ;
			line.dwSource = j ;
			line.dwDestination = mixerLine.dwDestination ;
	
			mmr = mixerGetLineInfo( 
				( HMIXEROBJ )( info->hInputMixer ),
				&line, 
				MIXER_GETLINEINFOF_SOURCE
			) ;
			
			if ( mmr != MMSYSERR_NOERROR )
				continue ;
	
			//
			// save line info
			//
	
			strcpy( info->src[j].name, line.szName ) ;
			info->src[j].lineID = line.dwLineID ;
			info->src[j].controlID = -1 ; // unfortunately, dwControlID is unsigned....
	
			if ( line.cControls == 0 )
				continue ;
	
			//
			// find line's volume control
			//
	
			LPMIXERCONTROL mixerControl = malloc( sizeof( MIXERCONTROL ) * line.cControls ) ;
			
			// Find a volume control, if any, of the microphone line
			MIXERLINECONTROLS mixerLineControls ;
			mixerLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
			mixerLineControls.dwLineID = line.dwLineID ;
			mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME ;
			mixerLineControls.cControls = line.cControls ;
			mixerLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
			mixerLineControls.pamxctrl = mixerControl ;
			
			mmr = mixerGetLineControls(
				( HMIXEROBJ )( info->hInputMixer ), 
				&mixerLineControls,
				MIXER_GETLINECONTROLSF_ONEBYTYPE
			) ;
			
			if ( mmr != MMSYSERR_NOERROR )
				continue ;
					
			int x = 0 ;
			for ( ; x < line.cControls ; ++x )
			{
				info->src[j].controlID = mixerControl[x].dwControlID ;

				// use first volume control
				break ;
			}
			
//			fprintf( stdout, "INPUT :: index => %d, name => %s, lineID => %u\n", 
//				j, info->src[j].name, info->src[j].lineID ) ;
	
			free( mixerControl ) ;
		}
	}

	//
	// report findings
	//
	
	return MMSYSERR_NOERROR ;
}

static int Px_InitOutputVolumeControls( PxMixer* mixer, int hWaveOut ) 
{
	MMRESULT mmr ;

	// cast void pointer
	PxInfo* info = ( PxInfo* )( mixer ) ;
	
	if ( info == NULL ) 
		return MMSYSERR_ERROR ;
	
	// default win32 speaker control id
	info->speakerID = 0x00000000 ;
	
	//
	// open the mixer device
	//
	
	mmr = mixerOpen( 
		( LPHMIXER )( &info->hOutputMixer ), 
		hWaveOut, 0, 0,
		MIXER_OBJECTF_HWAVEOUT 
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;
	

	// set default values
	info->speakerID = 0 ;
	info->waveID = 0 ;

	//
	// MASTER VOLUME
	//

	while ( 42 ) 
	{
		//
		// get the line info for the dst speakers
		//

		MIXERLINE mixerLine ;
		mixerLine.cbStruct = sizeof( MIXERLINE ) ;
		mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS ;
		
		mmr = mixerGetLineInfo(
			( HMIXEROBJ )( info->hOutputMixer ),
			&mixerLine,
			MIXER_GETLINEINFOF_COMPONENTTYPE
		) ;
		
		if ( mmr != MMSYSERR_NOERROR )
		{
			mixerClose( ( HMIXER )( info->hOutputMixer ) ) ;
			break ;
		}
	
		// no controls, don't go any further
		if ( mixerLine.cControls <= 0 )
			break ;

		//
		// get volume control for dst speakers line
		//
	
		MIXERCONTROL mixerControl ;
		mixerControl.cbStruct = sizeof( MIXERCONTROL ) ;
	
		MIXERLINECONTROLS mixerLineControls ;
		mixerLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
		mixerLineControls.dwLineID = mixerLine.dwLineID ;
		mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME ;
		mixerLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
		mixerLineControls.pamxctrl = &mixerControl ;
		
		mmr = mixerGetLineControls(
			( HMIXEROBJ )( info->hOutputMixer ),
			&mixerLineControls,
			MIXER_GETLINECONTROLSF_ONEBYTYPE
		) ;
		
		if ( mmr != MMSYSERR_NOERROR )
		{
			mixerClose( ( HMIXER )( info->hInputMixer ) ) ;
		}
		else
		{
			// save speaker_id
			info->speakerID = mixerControl.dwControlID ;
		}
	
		break ;
	}
	
	//
	// PCM VOLUME
	//

	while ( 42 ) 
	{
		//
		// get the line info for the dst speakers
		//
	
		MIXERLINE mixerLine ;
		mixerLine.cbStruct = sizeof( MIXERLINE ) ;
		mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT ;
		
		mmr = mixerGetLineInfo(
			( HMIXEROBJ )( info->hOutputMixer ),
			&mixerLine,
			MIXER_GETLINEINFOF_COMPONENTTYPE
		) ;
		
		if ( mmr != MMSYSERR_NOERROR )
		{
			mixerClose( ( HMIXER )( info->hOutputMixer ) ) ;
			break ;
		}
	
		// no controls, don't go any further
		if ( mixerLine.cControls <= 0 )
			break ;
	
		//
		// get volume control for dst speakers line
		//
	
		MIXERCONTROL mixerControl ;
		mixerControl.cbStruct = sizeof( MIXERCONTROL ) ;
	
		MIXERLINECONTROLS mixerLineControls ;
		mixerLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
		mixerLineControls.dwLineID = mixerLine.dwLineID ;
		mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME ;
		mixerLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
		mixerLineControls.pamxctrl = &mixerControl ;
		
		mmr = mixerGetLineControls(
			( HMIXEROBJ )( info->hOutputMixer ),
			&mixerLineControls,
			MIXER_GETLINECONTROLSF_ONEBYTYPE
		) ;
		
		if ( mmr != MMSYSERR_NOERROR )
		{
			mixerClose( ( HMIXER )( info->hInputMixer ) ) ;
		}
		else
		{
			// save speaker_id
			info->waveID = mixerControl.dwControlID ;
		}
		
		break ;
	}
	
	return MMSYSERR_NOERROR ;
}

int Px_SetMicrophoneBoost( PxMixer* mixer, int enable )
{
	MMRESULT mmr = MMSYSERR_ERROR ;
	
	// cast void pointer
	PxInfo* info = ( PxInfo* )( mixer ) ;

	if ( info == NULL ) 
		return MMSYSERR_ERROR ;
		
	//
	// get line info
	//
	
	MIXERLINE mixerLine ;
    mixerLine.cbStruct = sizeof( MIXERLINE ) ;
    mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE ;
	
	mmr = mixerGetLineInfo(
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerLine,
		MIXER_GETLINEINFOF_COMPONENTTYPE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;

	//
	// get all controls
	//

	LPMIXERCONTROL mixerControl = malloc( sizeof( MIXERCONTROL ) * mixerLine.cControls ) ;
	
	MIXERLINECONTROLS mixerLineControls ;
	mixerLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
	mixerLineControls.dwLineID = mixerLine.dwLineID ;
	mixerLineControls.cControls = mixerLine.cControls ;
	mixerLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
	mixerLineControls.pamxctrl = ( LPMIXERCONTROL )( mixerControl ) ;
	
	mmr = mixerGetLineControls(
		( HMIXEROBJ )( info->hInputMixer ), 
		&mixerLineControls,
		MIXER_GETLINECONTROLSF_ALL
	) ;
	
	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;

	//
	// find boost control
	//

	DWORD boost_id = -1 ;
	int x = 0 ;
	
	for ( ; x < mixerLineControls.cControls ; ++x )
	{
		// check control type
		if ( mixerControl[x].dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF )
		{
			// normalize control name
			char* name = _strupr( mixerControl[x].szName ) ;

			// check for 'mic' and 'boost'
			if (
				( strstr( name, "MIC" ) != NULL )
				&& ( strstr( name, "BOOST" ) != NULL )
			)
			{
				boost_id = mixerControl[x].dwControlID ;
				break ;
			}
		}
	}

	if ( boost_id == -1 )
		return MMSYSERR_ERROR ;

	//
	// get control details
	//
	
	MIXERCONTROLDETAILS_BOOLEAN value ;

	MIXERCONTROLDETAILS mixerControlDetails ;
	mixerControlDetails.cbStruct = sizeof( MIXERCONTROLDETAILS ) ;
	mixerControlDetails.dwControlID = boost_id ;
	mixerControlDetails.cChannels = 1 ;
	mixerControlDetails.cMultipleItems = 0 ;
	mixerControlDetails.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN ) ;
	mixerControlDetails.paDetails = &value ;

	mmr = mixerGetControlDetails( 
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerControlDetails,
		MIXER_GETCONTROLDETAILSF_VALUE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;

	//
	// update value
	//

	value.fValue = ( enable == 0 ) ? 0L : 1L ;

	//
	// set control details
	//
	
	mmr = mixerSetControlDetails( 
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerControlDetails,
		MIXER_SETCONTROLDETAILSF_VALUE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return mmr ;
	
	return mmr ;
}

int Px_GetMicrophoneBoost( PxMixer* mixer )
{
	MMRESULT mmr = MMSYSERR_ERROR ;
	
	// cast void pointer
	PxInfo* info = ( PxInfo* )( mixer ) ;

	if ( info == NULL ) 
		return -1 ;
		
	//
	// get line info
	//
	
	MIXERLINE mixerLine ;
    mixerLine.cbStruct = sizeof( MIXERLINE ) ;
    mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE ;
	
	mmr = mixerGetLineInfo(
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerLine,
		MIXER_GETLINEINFOF_COMPONENTTYPE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return -1 ;

	//
	// get all controls
	//

	LPMIXERCONTROL mixerControl = malloc( sizeof( MIXERCONTROL ) * mixerLine.cControls ) ;
	
	MIXERLINECONTROLS mixerLineControls ;
	mixerLineControls.cbStruct = sizeof( MIXERLINECONTROLS ) ;
	mixerLineControls.dwLineID = mixerLine.dwLineID ;
	mixerLineControls.cControls = mixerLine.cControls ;
	mixerLineControls.cbmxctrl = sizeof( MIXERCONTROL ) ;
	mixerLineControls.pamxctrl = ( LPMIXERCONTROL )( mixerControl ) ;
	
	mmr = mixerGetLineControls(
		( HMIXEROBJ )( info->hInputMixer ), 
		&mixerLineControls,
		MIXER_GETLINECONTROLSF_ALL
	) ;
	
	if ( mmr != MMSYSERR_NOERROR )
		return -1 ;

	//
	// find boost control
	//

	DWORD boost_id = -1 ;
	int x = 0 ;
	
	for ( ; x < mixerLineControls.cControls ; ++x )
	{
		// check control type
		if ( mixerControl[x].dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF )
		{
			// normalize control name
			char* name = _strupr( mixerControl[x].szName ) ;

			// check for 'mic' and 'boost'
			if (
				( strstr( name, "MIC" ) != NULL )
				&& ( strstr( name, "BOOST" ) != NULL )
			)
			{
				boost_id = mixerControl[x].dwControlID ;
				break ;
			}
		}
	}

	if ( boost_id == -1 )
		return -1 ;

	//
	// get control details
	//
	
	MIXERCONTROLDETAILS_BOOLEAN value ;

	MIXERCONTROLDETAILS mixerControlDetails ;
	mixerControlDetails.cbStruct = sizeof( MIXERCONTROLDETAILS ) ;
	mixerControlDetails.dwControlID = boost_id ;
	mixerControlDetails.cChannels = 1 ;
	mixerControlDetails.cMultipleItems = 0 ;
	mixerControlDetails.cbDetails = sizeof( MIXERCONTROLDETAILS_BOOLEAN ) ;
	mixerControlDetails.paDetails = &value ;

	mmr = mixerGetControlDetails( 
		( HMIXEROBJ )( info->hInputMixer ),
		&mixerControlDetails,
		MIXER_GETCONTROLDETAILSF_VALUE
	) ;

	if ( mmr != MMSYSERR_NOERROR )
		return -1 ;
	
	return ( int )( value.fValue ) ;
}

int Px_SetCurrentInputSourceByName( PxMixer* mixer, const char* name ) 
{
	// cast void pointer
	PxInfo* info = ( PxInfo* )( mixer ) ;

	// make sure we have a mixer
	if ( info == NULL ) 
		return MMSYSERR_ERROR ;

	// make sure we have a search name
	if ( name == NULL )
		return MMSYSERR_ERROR ;

	//
	// set input source
	//

	int x = 0 ;
	for ( ; x < info->numInputs ; ++x )
	{
		// compare passed name with control name
		if ( strncasecmp( info->src[x].name, name, strlen( name ) ) == 0 )
		{
			// set input source
			Px_SetCurrentInputSource( mixer, x ) ;
			
			// make sure set'ing worked
			if ( Px_GetCurrentInputSource( mixer ) == x )
				return MMSYSERR_NOERROR ;
			else
				return MMSYSERR_ERROR ;
		}
	}

	return MMSYSERR_ERROR ;
}
