
// $Id: frame.c 751 2006-12-11 22:08:45Z sbalea $

/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 * Copyright (C) 2003, 2004 HorizonLive.com, Inc.
 * Copyright (C) 2005, 2006 HorizonWimba, Inc.
 * Copyright (C) 2007 Wimba, Inc.
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * Video Conferencing support added by
 * Neil Stratford <neils@vipadia.com>
 * Copyright (C) 2005, 2005 Vipadia Limited
 *
 * VAD driven video conferencing, text message support
 * and miscellaneous enhancements added by
 * Mihai Balea <mihai at hates dot ms>
 *
 * This program may be modified and distributed under the
 * terms of the GNU General Public License. You should have received
 * a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc.
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "asterisk/autoconfig.h"
#include "frame.h"
#include "member.h"

static struct conf_frame* mix_multiple_speakers(struct conf_frame* frames_in,
		int speakers, int listeners);
static struct conf_frame* mix_single_speaker(struct conf_frame* frames_in);
static void mix_slinear_frames(char* dst, const char* src, int samples);
static struct ast_frame* frame_convert(struct ast_trans_pvt* trans,
		struct ast_frame* fr);
static struct ast_frame* frame_convert_to_slinear( struct ast_trans_pvt* trans, struct ast_frame* fr ) ;
static struct ast_frame* frame_create_slinear(char* data);
static struct ast_frame* get_silent_slinear_frame(void);

struct conf_frame* frame_mix_frames( struct conf_frame* frames_in, int speaker_count, int listener_count )
{
	if ( frames_in == NULL )
		return NULL ;

	struct conf_frame* frames_out = NULL ;

	if ( speaker_count > 1 )
	{
		if ( speaker_count == 2 && listener_count == 0 )
		{
			// optimize here also?
			frames_out = mix_multiple_speakers( frames_in, speaker_count, listener_count ) ;
		}
		else
		{
			// mix spoken frames for sending
			// ( note: this call also releases us from free'ing spoken_frames )
			frames_out = mix_multiple_speakers( frames_in, speaker_count, listener_count ) ;
		}
	}
	else if ( speaker_count == 1 )
	{
		// pass-through frames
		frames_out = mix_single_speaker( frames_in ) ;
	}
	else
	{
		// no frames to send, leave frames_out null
	}

	return frames_out ;
}

static struct conf_frame* mix_single_speaker( struct conf_frame* frames_in )
{
	//
	// 'mix' the frame
	//

	// copy orignal frame to converted array so listeners don't need to re-encode it
	frames_in->converted[ frames_in->member->read_format_index ] = ast_frdup( frames_in->fr ) ;

	// convert frame to slinear, if we have a path
	frames_in->fr = frame_convert_to_slinear(
		frames_in->member->to_slinear,
		frames_in->fr
	) ;

	// set the frame's member to null ( i.e. all listeners )
	frames_in->member = NULL ;

	return frames_in ;
}

static struct conf_frame* mix_multiple_speakers(
	struct conf_frame* frames_in,
	int speakers, int listeners)
{
	//
	// at this point we know that there is more than one frame,
	// and that the frames need to be converted to pcm to be mixed
	//
	// now, if there are only two frames and two members,
	// we can swap them. ( but we'll get to that later. )
	//

	//
	// loop through the spoken frames, making a list of spoken members,
	// and converting gsm frames to slinear frames so we can mix them.
	//

	// pointer to the spoken frames list
	struct conf_frame* cf_spoken = frames_in ;

	// pointer to the new list of mixed frames
	struct conf_frame* cf_sendFrames = NULL ;

	while ( cf_spoken != NULL )
	{
		//
		// while we're looping through the spoken frames, we'll
		// convert the frame to a format suitable for mixing
		//
		// if the frame fails to convert, drop it and treat
		// the speaking member like a listener by not adding
		// them to the cf_sendFrames list
		//

		if ( cf_spoken->member == NULL )
		{
			ast_log( LOG_WARNING, "unable to determine frame member\n" ) ;
		}
		else
		{
			cf_spoken->fr = frame_convert_to_slinear(
				cf_spoken->member->to_slinear,
				cf_spoken->fr
			) ;

			if ( cf_spoken->fr == NULL )
			{
				ast_log( LOG_WARNING, "unable to convert frame to slinear\n" ) ;
			}
			else
			{
				// create new conf frame with last frame as 'next'
				cf_sendFrames = frame_create( cf_spoken->member, cf_sendFrames, NULL ) ;
			}
		}

		// point to the next spoken frame
		cf_spoken = cf_spoken->next ;
	}

	// if necessary, add a frame with a null member pointer.
	// this frame will hold the audio mixed for all listeners
	if ( listeners > 0 )
	{
		cf_sendFrames = frame_create( NULL, cf_sendFrames, NULL ) ;
	}

	//
	// mix the audio
	//

	// convenience pointer that skips over the friendly offset
	char* cp_listenerData ;

	// pointer to the send frames list
	struct conf_frame* cf_send = NULL ;

	for ( cf_send = cf_sendFrames ; cf_send != NULL ; cf_send = cf_send->next )
	{
		// allocate a mix buffer which fill large enough memory to
		// hold a frame, and reset it's memory so we don't get noise
		char* cp_listenerBuffer = ast_calloc( AST_CONF_BUFFER_SIZE, 1 ) ;

		// point past the friendly offset right to the data
		cp_listenerData = cp_listenerBuffer + AST_FRIENDLY_OFFSET ;

		// reset the spoken list pointer
		cf_spoken = frames_in ;

		// really mix the audio
		for ( ; cf_spoken != NULL ; cf_spoken = cf_spoken->next )
		{
			//
			// if the members are equal, and they
			// are not null, do not mix them.
			//
			if (
				( cf_send->member == cf_spoken->member )
				&& ( cf_send->member != NULL )
			)
			{
				// don't mix this frame
			}
			else if ( cf_spoken->fr == NULL )
			{
				ast_log( LOG_WARNING, "unable to mix conf_frame with null ast_frame\n" ) ;
			}
			else if ( !cf_spoken->fr->data )
			{
				ast_log(LOG_ERROR, "unable to mix ast_frame with null data, "
						"spoken member => %s, "
						"send member => %s\n",
						cf_spoken->member->channel_name,
						cf_send->member->channel_name);
			}
			else
			{
				// mix the new frame in with the existing buffer
				mix_slinear_frames(cp_listenerData,
						(char*)cf_spoken->fr->data,
						AST_CONF_BLOCK_SAMPLES);
			}
		}

		// copy a pointer to the frame data to the conf_frame
		cf_send->mixed_buffer = cp_listenerData ;
	}

	//
	// copy the mixed buffer to a new frame
	//

	// reset the send list pointer
	cf_send = cf_sendFrames ;

	while ( cf_send != NULL )
	{
		cf_send->fr = frame_create_slinear( cf_send->mixed_buffer ) ;
		cf_send = cf_send->next ;
	}

	//
	// clean up the spoken frames we were passed
	// ( caller will only be responsible for free'ing returns frames )
	//

	// reset the spoken list pointer
	cf_spoken = frames_in ;

	while ( cf_spoken != NULL )
	{
		// delete the frame
		cf_spoken = frame_delete( cf_spoken ) ;
	}

	// return the list of frames for sending
	return cf_sendFrames ;
}


static struct ast_frame* frame_convert_to_slinear( struct ast_trans_pvt* trans, struct ast_frame* fr )
{
	// check for null frame
	if ( fr == NULL )
	{
		ast_log( LOG_ERROR, "unable to translate null frame to slinear\n" ) ;
		return NULL ;
	}

	// we don't need to duplicate this frame since
	// the normal translation would free it anyway, so
	// we'll just pretend we free'd and malloc'd a new one.
	if ( fr->subclass == AST_FORMAT_SLINEAR )
		return fr ;

	// check for null translator ( after we've checked that we need to translate )
	if ( trans == NULL )
	{
		ast_log( LOG_ERROR, "unable to translate frame with null translation path\n" ) ;
		return NULL ;
	}

	// return the converted frame
	return frame_convert( trans, fr ) ;
}

struct ast_frame* frame_convert_from_slinear( struct ast_trans_pvt* trans, struct ast_frame* fr )
{
	// check for null translator ( after we've checked that we need to translate )
	if ( trans == NULL )
	{
		//ast_log( LOG_ERROR, "unable to translate frame with null translation path\n" ) ;
		return fr ;
	}

	// check for null frame
	if ( fr == NULL )
	{
		ast_log( LOG_ERROR, "unable to translate null slinear frame\n" ) ;
		return NULL ;
	}

	// if the frame is not slinear, return an error
	if ( fr->subclass != AST_FORMAT_SLINEAR )
	{
		ast_log( LOG_ERROR, "unable to translate non-slinear frame\n" ) ;
		return NULL ;
	}

	// return the converted frame
	return frame_convert( trans, fr ) ;
}

static struct ast_frame* frame_convert( struct ast_trans_pvt* trans,
		struct ast_frame* fr )
{
	if ( trans == NULL )
	{
		ast_log( LOG_WARNING, "unable to convert frame with null translator\n" ) ;
		return NULL ;
	}

	if ( fr == NULL )
	{
		ast_log( LOG_WARNING, "unable to convert null frame\n" ) ;
		return NULL ;
	}

	// convert the frame
	struct ast_frame* translated_frame = ast_translate( trans, fr, 1 ) ;

	// check for errors
	if ( translated_frame == NULL )
	{
		ast_log( LOG_ERROR, "unable to translate frame\n" ) ;
		return NULL ;
	}

	// return the translated frame
	return translated_frame ;
}

struct conf_frame* frame_delete( struct conf_frame* cf )
{
	// check for null frames
	if ( cf == NULL )
	{
		ast_log(LOG_ERROR, "unable to delete null conf frame\n");
		return NULL ;
	}

	// check for frame marked as static
	if ( cf->static_frame == 1 )
		return NULL ;

	if ( cf->fr != NULL )
	{
		ast_frfree( cf->fr ) ;
		cf->fr = NULL ;
	}

	int c;

	// make sure converted frames are set to null
	for ( c = 0 ; c < AC_SUPPORTED_FORMATS ; ++c )
	{
		if ( cf->converted[ c ] != NULL )
		{
			ast_frfree( cf->converted[ c ] ) ;
			cf->converted[ c ] = NULL ;
		}
	}

	// get a pointer to the next frame
	// in the list so we can return it
	struct conf_frame* nf = cf->next ;

	ast_free( cf ) ;

	return nf ;
}

struct conf_frame* frame_create( struct ast_conf_member* member,
		struct conf_frame* next, const struct ast_frame* fr )
{
	struct conf_frame* cf = ast_calloc(1, sizeof(struct conf_frame));

	if ( cf == NULL )
		return NULL ;

	cf->member = member ;

	cf->prev = NULL ;
	cf->next = next ;

	// establish relationship to 'next'
	if ( next )
		next->prev = cf ;

	// this holds the ast_frame pointer
	cf->fr = fr ? ast_frdup(fr) : 0;

	return cf;
}

struct conf_frame* frame_copy( struct conf_frame* src )
{
	if ( src == NULL )
	{
		ast_log(LOG_ERROR, "unable to copy null conf frame\n");
		return NULL ;
	}

	return frame_create(src->member, NULL, src->fr);
}

//
// Create a TEXT frame based on a given string
//
struct ast_frame* frame_create_text(const char *text)
{
	struct ast_frame *f;

	f = ast_calloc(1, sizeof(struct ast_frame));
	if ( f == NULL )
		return NULL ;

	f->datalen = strlen(text) + 1;
	f->data = ast_calloc(f->datalen, 1);
	if ( !f->data )
	{
		ast_free(f);
		return NULL ;
	}
	strncpy(f->data, text, f->datalen - 1);

	f->frametype = AST_FRAME_TEXT;
	f->offset = 0;
	f->mallocd = AST_MALLOCD_HDR | AST_MALLOCD_DATA;
	f->src = NULL;

	return f;
}

struct ast_frame* frame_create_slinear( char* data )
{
	struct ast_frame* f ;

	f = ast_calloc( 1, sizeof( struct ast_frame ) ) ;
	if ( f == NULL )
		return NULL ;

	f->frametype = AST_FRAME_VOICE ;
	f->subclass = AST_FORMAT_SLINEAR ;
	f->samples = AST_CONF_BLOCK_SAMPLES ;
	f->offset = AST_FRIENDLY_OFFSET ;
	f->mallocd = AST_MALLOCD_HDR | AST_MALLOCD_DATA ;

	f->datalen = AST_CONF_FRAME_DATA_SIZE ;
	f->data = data ;

	f->src = NULL ;

	return f ;
}

static void mix_slinear_frames( char *dst, const char *src, int samples )
{
	int i, val ;

	for ( i = 0 ; i < samples ; ++i )
	{
		val = ( (short*)dst )[i] + ( (short*)src )[i] ;

		if ( val > 0x7fff )
		{
			( (short*)dst )[i] = 0x7fff - 1 ;
		}
		else if ( val < -0x7fff )
		{
			( (short*)dst )[i] = -0x7fff + 1 ;
		}
		else
		{
			( (short*)dst )[i] = val ;
		}
	}
}

struct conf_frame* frame_get_silent( void )
{
	static struct conf_frame* static_silent_frame = NULL ;

	// we'll let this leak until the application terminates
	if ( static_silent_frame == NULL )
	{
		struct ast_frame* fr = get_silent_slinear_frame() ;

		static_silent_frame = frame_create( NULL, NULL, fr ) ;

		if ( static_silent_frame == NULL )
		{
			ast_log( LOG_WARNING, "unable to create cached silent frame\n" ) ;
			return NULL ;
		}

		// init the 'converted' slinear silent frame
		static_silent_frame->converted[ AC_SLINEAR_INDEX ] = get_silent_slinear_frame() ;

		// mark frame as static so it's not deleted
		static_silent_frame->static_frame = 1 ;
	}

	return static_silent_frame ;
}

static struct ast_frame* get_silent_slinear_frame( void )
{
	static struct ast_frame* f = NULL ;

	// we'll let this leak until the application terminates
	if ( f == NULL )
	{
		char* data = ast_calloc( AST_CONF_BUFFER_SIZE, 1 ) ;
		if ( !data )
			return NULL;
		f = frame_create_slinear( data ) ;
	}

	return f;
}

