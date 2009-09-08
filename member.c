
// $Id: member.c 885 2007-06-27 15:41:18Z sbalea $

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

#include <stdio.h>
#include "asterisk/autoconfig.h"
#include "member.h"
#include "packer.h"
#include "frame.h"
#include "conference.h"

struct ast_conf_soundq
{
	char name[256];
	struct ast_filestream *stream; // the stream
	int muted; // should incoming audio be muted while we play?
	struct ast_conf_soundq *next;
};

static int queue_incoming_audio_frame(struct ast_conf_member *, struct ast_frame *);
static int queue_incoming_video_frame(struct ast_conf_member *, const struct ast_frame *);
static int queue_incoming_dtmf_frame(struct ast_conf_member *, const struct ast_frame *);
static int queue_silent_frame(struct ast_conf_member *, const struct timeval);

int member_set_audio_mute(struct ast_conf_member * member, int mute)
{
	member->mute_audio = mute;
	return 0;
}

int member_set_video_mute(struct ast_conf_member * member, int mute)
{
	member->mute_video = mute;
	return 0;
}

static int process_incoming(struct ast_conf_member *member, struct ast_frame *f)
{
	int ret = 0;

	if (f->frametype == AST_FRAME_DTMF)
	{
		// In Asterisk 1.4 AST_FRAME_DTMF is equivalent to
		// AST_FRAME_DTMF_END
		if (member->dtmf_switch)
		{
			switch (f->subclass)
			{
			case '0':
				member->requested_video_id = 0;
				break;
			case '1':
				member->requested_video_id = 1;
				break;
			case '2':
				member->requested_video_id = 2;
				break;
			case '3':
				member->requested_video_id = 3;
				break;
			case '4':
				member->requested_video_id = 4;
				break;
			case '5':
				member->requested_video_id = 5;
				break;
			case '6':
				member->requested_video_id = 6;
				break;
			case '7':
				member->requested_video_id = 7;
				break;
			case '8':
				member->requested_video_id = 8;
				break;
			case '9':
				member->requested_video_id = 9;
				break;
			case '*':
				if ( !member->mute_video && !member->mute_audio )
				{
					member_set_audio_mute(member, 1);
					member_set_video_mute(member, 1);
				}
				else if ( member->mute_video && member->mute_audio )
				{
					member_set_audio_mute(member, 0);
					member_set_video_mute(member, 0);
				}
				break;
			}
			member->video_switch = 1;
		}
		if (member->dtmf_relay)
		{
			// output to manager...
			manager_event(
				EVENT_FLAG_CALL,
				"ConferenceDTMF",
				"ConferenceName: %s\r\n"
				"Channel: %s\r\n"
				"CallerID: %s\r\n"
				"CallerIDName: %s\r\n"
				"Key: %c\r\n",
				member->conf_name,
				member->channel_name,
				member->callerid ? member->callerid : "unknown",
				member->callername ? member->callername : "unknown",
				f->subclass
				) ;
		}
		if (!member->dtmf_switch && !member->dtmf_relay)
		{
			// relay this to the listening channels
			queue_incoming_dtmf_frame( member, f );
		}
	}
	else if (f->frametype == AST_FRAME_DTMF_BEGIN)
	{
		if (!member->dtmf_switch && !member->dtmf_relay)
		{
			// relay this to the listening channels
			queue_incoming_dtmf_frame( member, f );
		}
	}
	else if ( f->frametype == AST_FRAME_VOICE && !member->mute_audio )
	{
		int silent_frame = 0;

		member->frames_in++;

#if ( SILDET == 2 )
		//
		// make sure we have a valid dsp and frame type
		//
		if ( member->dsp && f->subclass == AST_FORMAT_SLINEAR &&
				f->datalen == AST_CONF_FRAME_DATA_SIZE )
		{
			// send the frame to the preprocessor
			if ( speex_preprocess( member->dsp, f->data, NULL ) == 0 )
			{
				// we ignore the preprocessor's outcome if we've seen voice frames
				// in within the last AST_CONF_SKIP_SPEEX_PREPROCESS frames
				if ( member->ignore_speex_count > 0 )
				{
					--member->ignore_speex_count ;
				}
				else
				{
					silent_frame = 1 ;
				}
			}
			else
			{
				// voice detected, reset skip count
				member->ignore_speex_count = AST_CONF_SKIP_SPEEX_PREPROCESS ;
			}
		}
#endif
		if ( !silent_frame )
			queue_incoming_audio_frame( member, f );
	}
	else if ( f->frametype == AST_FRAME_VIDEO && !member->mute_video )
	{
		queue_incoming_video_frame( member, f );
	}
	else if ( f->frametype == AST_FRAME_CONTROL &&
			f->subclass == AST_CONTROL_HANGUP )
	{
		ast_log(LOG_WARNING, "Unexpected hangup frame, channel => %s\n",
				member->channel_name);
		ret = -1;
	}
	else if ( f->frametype == AST_FRAME_CONTROL &&
			f->subclass == AST_CONTROL_VIDUPDATE )
	{
		// say we have switched to cause a FIR to be sent to the sender
		member->video_switch = 1;
	}
	else if ( f->frametype == AST_FRAME_TEXT && member->does_text )
	{
		if ( strncmp(f->data, AST_CONF_CONTROL_CAMERA_DISABLED,
					strlen(AST_CONF_CONTROL_CAMERA_DISABLED)) == 0 )
		{
			manager_event(EVENT_FLAG_CALL,
			              "ConferenceCameraDisabled",
			              "ConferenceName: %s\r\nChannel: %s\r\n",
			              member->conf_name,
			              member->channel_name);
			member->no_camera = 1;
		}
		else if ( strncmp(f->data, AST_CONF_CONTROL_CAMERA_ENABLED,
					strlen(AST_CONF_CONTROL_CAMERA_ENABLED)) == 0 )
		{
			manager_event(EVENT_FLAG_CALL,
			              "ConferenceCameraEnabled",
			              "ConferenceName: %s\r\nChannel: %s\r\n",
			              member->conf_name,
			              member->channel_name);
			member->no_camera = 0;
		}
		else if ( strncmp(f->data, AST_CONF_CONTROL_STOP_VIDEO_TRANSMIT,
					strlen(AST_CONF_CONTROL_STOP_VIDEO_TRANSMIT)) == 0 )
		{
			manager_event(EVENT_FLAG_CALL,
			              "ConferenceStopVideoTransmit",
			              "ConferenceName: %s\r\nChannel: %s\r\n",
			              member->conf_name,
			              member->channel_name);
			member->norecv_video = 1;
		}
		else if ( strncmp(f->data, AST_CONF_CONTROL_START_VIDEO_TRANSMIT,
					strlen(AST_CONF_CONTROL_START_VIDEO_TRANSMIT)) == 0 )
		{
			manager_event(EVENT_FLAG_CALL,
			              "ConferenceStartVideoTransmit",
			              "ConferenceName: %s\r\nChannel: %s\r\n",
			              member->conf_name,
			              member->channel_name);
			member->norecv_video = 0;
		}
	}

	ast_frfree(f);

	return ret;
}

// get the next frame from the soundq;  must be called with member locked.
static struct ast_frame *get_next_soundframe(struct ast_conf_member * member)
{
	struct ast_frame * f = 0;
	struct ast_conf_soundq * sound;

	while ( (sound = member->soundq) &&
			!(f = ast_readframe(sound->stream)) )
	{
		// we're done with this sound; remove it from the queue, and try again
		ast_closestream(sound->stream);
		member->soundq = sound->next;
		manager_event(EVENT_FLAG_CALL,
				"ConferenceSoundComplete",
				"Channel: %s\r\n"
				"Sound: %s\r\n",
				member->channel_name,
				sound->name);
		ast_free(sound);
	}

	return f;
}

// Member must be locked prior to calling get_outgoing_frames().
static int get_outgoing_frames(struct ast_conf_member * member,
		struct conf_frame * frames[], const int frames_len)
{
	struct conf_frame * cf;
	int frame_count = 0;

	while ( (cf = framelist_pop_tail(&member->out_audio_framelist)) )
	{
		// If we are playing sounds, we can just replace the
		// frame with the next sound frame and send it instead.
		struct ast_frame * f = get_next_soundframe(member);
		if ( f )
		{
			f->delivery = cf->fr->delivery;
			frame_delete(cf);
			cf = frame_create(0, 0, f);
			ast_frfree(f);
		}

		frames[frame_count++] = cf;
		if ( frame_count == frames_len - 1 )
			goto bail;
	}

	while ( (cf = framelist_pop_tail(&member->out_video_framelist)) )
	{
		frames[frame_count++] = cf;
		if ( frame_count == frames_len - 1 )
			goto bail;
	}

	while ( (cf = framelist_pop_tail(&member->out_dtmf_framelist)) )
	{
		frames[frame_count++] = cf;
		if ( frame_count == frames_len - 1 )
			goto bail;
	}

	while ( (cf = framelist_pop_tail(&member->out_text_framelist)) )
	{
		frames[frame_count++] = cf;
		if ( frame_count == frames_len - 1 )
			goto bail;
	}

bail:
	return frame_count;
}

// Send each frame in frames list using ast_write(), updating the channel's
// write format as necessary. Note that the member does _not_ have to be
// locked, the channel object just needs to not be destroyed before this call
// completes.
static void write_outgoing_frames(struct ast_channel * chan,
		struct conf_frame * frames[], const int frame_count)
{
	int i;
	for ( i = 0; i < frame_count; ++i )
	{
		struct ast_frame * f = frames[i]->fr;

		switch ( f->frametype )
		{
		case AST_FRAME_VIDEO:
			if ( ast_write_video(chan, f) != 1 )
			{
				if ( !ast_check_hangup(chan) )
					ast_log(LOG_ERROR,
							"unable to write video "
							"frame, channel => %s\n",
							chan->name);
			}
			break;

		case AST_FRAME_VOICE:
			if ( !(f->subclass & chan->writeformat) )
			{
				const int new_format =
					(f->subclass & chan->nativeformats) ?
					chan->nativeformats : f->subclass;

				if ( ast_set_write_format(chan, new_format) < 0 )
					ast_log(LOG_ERROR, "unable to set write "
							"format to %d\n",
							new_format);
			}
			/* fall through on purpose */
		case AST_FRAME_TEXT:
		case AST_FRAME_DTMF:
		case AST_FRAME_DTMF_BEGIN:
			if ( ast_write(chan, f) )
			{
				if ( !ast_check_hangup(chan) )
					ast_log(LOG_ERROR,
							"unable to write frame, "
							"channel => %s\n",
							chan->name);
			}
			break;

		default:
			ast_log(LOG_ERROR,
					"attempt to write unknown frame type, "
					"channel => %s, frametype => %d\n",
					chan->name, f->frametype);
			break;
		}

		frame_delete(frames[i]);
	}
}

// main member thread function
int member_exec( struct ast_channel* chan, void* data )
{
	ast_log(LOG_DEBUG, "Begin processing member thread, channel => %s\n",
			chan->name);

	// If the call has not yet been answered, answer the call
	// Note: asterisk apps seem to check _state, but it seems like it's
	// safe to just call ast_answer. It will just do nothing if it is up.
	// it will also return -1 if the channel is a zombie, or has hung up.
	if ( ast_answer(chan) )
	{
		ast_log(LOG_WARNING, "unable to answer call\n");
		return -1;
	}

	struct ast_conf_member * member;
	if ( !(member = member_create(chan, data)) )
		return -1;

	struct ast_conference * conf;
	if ( !(conf = conference_join(member)) )
	{
		member_delete(member);
		return -1;
	}

	member_unlock(member);

	while ( 42 == 42 )
	{
		// The member and thus the member's channel object will not be
		// destroyed until after the remove_flag is set. Since the
		// remove_flag is only set in one place, outside this loop, we
		// have a guarantee that member->chan is okay to use inside
		// this loop even when member is unlocked.  This is desireable
		// so that potentially blocking ast_waitfor(), ast_read(), and
		// ast_write() operations can execute without the member lock
		// held and thus run in parallel with the conference thread.

		// Wait for incoming frames
		const int remaining_ms =
			ast_waitfor(member->chan, AST_CONF_WAITFOR_LATENCY);

		if ( remaining_ms < 0 )
		{
			// an error occured
			member_lock(member);
			ast_log(LOG_NOTICE,
				"error waiting for frame, channel => %s, error => %d\n",
				member->channel_name, remaining_ms);
			break;
		}
		else if ( remaining_ms == 0 )
		{
			// no frame has arrived yet
		}
		else if ( remaining_ms > 0 )
		{
			// a frame has come in before the latency timeout
			// was reached, so we process the frame

			struct ast_frame * f = ast_read( member->chan );

			member_lock(member);

			// We get a null frame if a hangup frame is read.
			if ( !f )
				break;

			if ( member->kick_flag )
			{
				ast_frfree(f);
				break;
			}

			if ( process_incoming(member, f) )
				break;

			member_unlock(member);
		}

		member_lock(member);
		if ( member->kick_flag )
			break;

		const int frames_len = 16;
		struct conf_frame * frames[frames_len];
		const int frame_count =
			get_outgoing_frames(member, frames, frames_len);

		member_unlock(member);

		write_outgoing_frames(member->chan, frames, frame_count);
	}

	ast_log(LOG_DEBUG, "end member event loop, time_entered => %ld\n",
			member->time_entered.tv_sec );

	member->remove_flag = 1;

	member_unlock(member);

	return -1;
}

// Allocates and initialized new, locked member.
struct ast_conf_member* member_create(struct ast_channel *chan, const char* data)
{
	struct ast_conf_member * member;
	if ( !(member = ast_calloc(1, sizeof(struct ast_conf_member))) )
		return NULL;

	ast_mutex_init(&member->lock);
	member_lock(member);

	member->video_start_timeout = AST_CONF_VIDEO_START_TIMEOUT;
	member->video_stop_timeout = AST_CONF_VIDEO_STOP_TIMEOUT;
	member->video_stop_broadcast_timeout = AST_CONF_VIDEO_STOP_BROADCAST_TIMEOUT;
	member->vad_prob_start = AST_CONF_PROB_START;
	member->vad_prob_continue = AST_CONF_PROB_CONTINUE;

	//
	// initialize member with passed data values
	//
	char argstr[256];

	strncpy( argstr, data, sizeof(argstr) - 1 );
	char *stringp = argstr;

	// parse the id
	char *token;
	if ( (token = strsep( &stringp, "/" )) != NULL )
	{
		member->conf_name = ast_strdup(token);
	}
	else
	{
		ast_log(LOG_ERROR, "unable to parse member id\n");
		ast_free(member);
		return NULL;
	}

	// parse the flags
	if ( (token = strsep(&stringp, "/")) != NULL )
		member->flags = ast_strdup(token);

	while ( (token = strsep(&stringp, "/")) != NULL )
	{
		static const char arg_priority[] = "priority";
		static const char arg_vad_prob_start[] = "vad_prob_start";
		static const char arg_vad_prob_continue[] = "vad_prob_continue";
		static const char arg_video_start_timeout[] = "video_start_timeout";
		static const char arg_video_stop_timeout[] = "video_stop_timeout";
		static const char arg_video_stop_broadcast_timeout[] =
			"video_stop_broadcast_timeout";

		char *value = token;
		const char *key = strsep(&value, "=");

		if ( key == NULL || value == NULL )
		{
			ast_log(LOG_WARNING, "Incorrect argument '%s', "
					"args => %s\n", token, data);
			continue;
		}
		if ( strncasecmp(key, arg_priority, sizeof(arg_priority) - 1) == 0 )
		{
			member->priority = strtol(value, (char **)NULL, 10);
		}
		else if ( strncasecmp(key, arg_vad_prob_start,
					sizeof(arg_vad_prob_start) - 1) == 0 )
		{
			member->vad_prob_start = strtof(value, (char **)NULL);
		}
		else if ( strncasecmp(key, arg_vad_prob_continue,
					sizeof(arg_vad_prob_continue) - 1) == 0 )
		{
			member->vad_prob_continue = strtof(value, (char **)NULL);
		}
		else if ( strncasecmp(key, arg_video_start_timeout,
					sizeof(arg_video_start_timeout) - 1) == 0 )
		{
			member->video_start_timeout = strtol(value, (char **)NULL, 10);
		}
		else if ( strncasecmp(key, arg_video_stop_timeout,
					sizeof(arg_video_stop_timeout) - 1) == 0 )
		{
			member->video_stop_timeout = strtol(value, (char **)NULL, 10);
		}
		else if ( !strncasecmp(key, arg_video_stop_broadcast_timeout,
					sizeof(arg_video_stop_broadcast_timeout) - 1) )
		{
			member->video_stop_broadcast_timeout = strtol(value, NULL, 10);
		}
		else
		{
			ast_log(LOG_WARNING, "unknown parameter %s with value %s\n",
					key, value);
		}
	}

	member->chan = chan;
	member->channel_name = ast_strdup(chan->name);
	member->callerid = ast_strdup(chan->cid.cid_num);
	member->callername = ast_strdup(chan->cid.cid_name);

	// start of day video ids
	member->requested_video_id = -1;
	member->video_switch = 1;
	member->id = -1;

	member->time_entered =
		member->last_in_dropped =
		member->last_out_dropped =
		member->last_state_change = ast_tvnow();

	//
	// parse passed flags
	//

	const char * flag = member->flags;

	while ( flag && *flag )
	{
		switch ( *flag )
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if ( member->requested_video_id < 0 )
				member->requested_video_id = *flag - (int)'0';
			else
				member->id = *flag - (int)'0';
			break;

		case 'C':
			member_set_video_mute(member, 1);
			break;
		case 'c':
			member->norecv_video = 1;
			break;
		case 'L':
			member_set_audio_mute(member, 1);
			break;
		case 'l':
			member->norecv_audio = 1;
			break;

			// speex preprocessing options
		case 'V':
			member->vad_flag = 1;
			break;
		case 'D':
			member->denoise_flag = 1;
			break;
		case 'A':
			member->agc_flag = 1;
			break;

			// dtmf/moderator/video switching options
		case 'X':
			member->dtmf_switch = 1;
			break;
		case 'R':
			member->dtmf_relay = 1;
			break;
		case 'S':
			member->vad_switch = 1;
			break;
		case 'F':
			member->force_vad_switch = 1;
			break;
		case 'M':
			member->ismoderator = 1;
			break;
		case 'N':
			member->no_camera = 1;
			break;
		case 't':
			member->does_text = 1;
			break;
		case 'z':
			member->vad_linger = 1;
			break;
		case 'o':
			member->does_chat_mode = 1;
			break;

			//Telephone connection
		case 'T':
			member->via_telephone = 1;
			break;

		default:
			ast_log(LOG_WARNING, "received invalid flag, "
					"chan => %s, flag => %c, flags => %s\n",
					chan->name, *flag, member->flags);
			break;
		}

		++flag;
	}

#if ( SILDET == 2 )
	// configure silence detection and preprocessing
	// if the user is coming in via the telephone,
	// and is not listen-only
	if ( member->via_telephone == 1 && member->type != 'L' )
	{
		// create a speex preprocessor
		member->dsp = speex_preprocess_state_init(
				AST_CONF_BLOCK_SAMPLES, AST_CONF_SAMPLE_RATE );

		if ( member->dsp == NULL )
		{
			ast_log(LOG_WARNING, "unable to initialize member dsp, "
					"channel => %s\n", chan->name);
		}
		else
		{
			ast_log(LOG_NOTICE, "member dsp initialized, "
					"channel => %s, v => %d, d => %d, "
					"a => %d\n",
					chan->name, member->vad_flag,
					member->denoise_flag, member->agc_flag);

			// set speex preprocessor options
			speex_preprocess_ctl(member->dsp,
					SPEEX_PREPROCESS_SET_VAD,
					&member->vad_flag);
			speex_preprocess_ctl(member->dsp,
					SPEEX_PREPROCESS_SET_DENOISE,
					&member->denoise_flag);
			speex_preprocess_ctl(member->dsp,
					SPEEX_PREPROCESS_SET_AGC,
					&member->agc_flag);
			speex_preprocess_ctl(member->dsp,
					SPEEX_PREPROCESS_SET_PROB_START,
					&member->vad_prob_start);
			speex_preprocess_ctl(member->dsp,
					SPEEX_PREPROCESS_SET_PROB_CONTINUE,
					&member->vad_prob_continue);

			ast_log(LOG_DEBUG, "speech_prob_start => %f, "
					"speech_prob_continue => %f\n",
					member->dsp->speech_prob_start,
					member->dsp->speech_prob_continue);
		}
	}
#endif

	//
	// read, write, and translation options
	//

	// set member's audio formats, taking dsp preprocessing into account
	// ( chan->nativeformats, AST_FORMAT_SLINEAR, AST_FORMAT_ULAW,
	// AST_FORMAT_GSM )
	member->read_format = member->dsp ?
		AST_FORMAT_SLINEAR :
		chan->nativeformats & AST_FORMAT_AUDIO_MASK;

	// translation paths ( ast_translator_build_path() returns null if
	// formats match )
	member->to_slinear = ast_translator_build_path(
			AST_FORMAT_SLINEAR, member->read_format);

	if ( !member->to_slinear )
		ast_log(LOG_WARNING, "failed to create to_slinear translator, "
				"read_format => 0x%08x, channel => %s\n",
				member->read_format, member->channel_name);

	member->from_slinear = ast_translator_build_path(
			member->chan->nativeformats & AST_FORMAT_AUDIO_MASK,
			AST_FORMAT_SLINEAR);

	if ( !member->from_slinear )
		ast_log(LOG_WARNING, "failed to create from_slinear translator, "
				"read_format => 0x%08x, channel => %s\n",
				member->chan->nativeformats,
				member->channel_name);

	// index for converted_frames array
	switch ( member->chan->nativeformats & AST_FORMAT_AUDIO_MASK )
	{
		case AST_FORMAT_SLINEAR:
			member->write_format_index = AC_SLINEAR_INDEX;
			break;

		case AST_FORMAT_ULAW:
			member->write_format_index = AC_ULAW_INDEX;
			break;

		case AST_FORMAT_ALAW:
			member->write_format_index = AC_ALAW_INDEX;
			break;

		case AST_FORMAT_GSM:
			member->write_format_index = AC_GSM_INDEX;
			break;

		case AST_FORMAT_SPEEX:
			member->write_format_index = AC_SPEEX_INDEX;
			break;

#ifdef AC_USE_G729A
		case AST_FORMAT_G729A:
			member->write_format_index = AC_G729A_INDEX;
			break;
#endif

		default:
			member->write_format_index = 0;
	}

	// index for converted_frames array
	switch ( member->read_format )
	{
		case AST_FORMAT_SLINEAR:
			member->read_format_index = AC_SLINEAR_INDEX;
			break;

		case AST_FORMAT_ULAW:
			member->read_format_index = AC_ULAW_INDEX;
			break;

		case AST_FORMAT_ALAW:
			member->read_format_index = AC_ALAW_INDEX;
			break;

		case AST_FORMAT_GSM:
			member->read_format_index = AC_GSM_INDEX;
			break;

		case AST_FORMAT_SPEEX:
			member->read_format_index = AC_SPEEX_INDEX;
			break;

#ifdef AC_USE_G729A
		case AST_FORMAT_G729A:
			member->read_format_index = AC_G729A_INDEX;
			break;
#endif

		default:
			member->read_format_index = 0;
	}

	// smoother defaults.
	member->smooth_multiple = 1;
	member->smooth_size_in = -1;
	member->smooth_size_out = -1;

	switch (member->read_format)
	{
		/* these assumptions may be incorrect */
		case AST_FORMAT_ULAW:
		case AST_FORMAT_ALAW:
			member->smooth_size_in  = 160; //bytes
			member->smooth_size_out = 160; //samples
			break;
		case AST_FORMAT_GSM:
			/*
			member->smooth_size_in  = 33; //bytes
			member->smooth_size_out = 160;//samples
			*/
			break;
		case AST_FORMAT_SPEEX:
		case AST_FORMAT_G729A:
			/* this assumptions are wrong
			member->smooth_multiple = 2;   // for testing, force to dual frame
			member->smooth_size_in  = 39;  // bytes
			member->smooth_size_out = 160; // samples
			*/
			break;
		case AST_FORMAT_SLINEAR:
			member->smooth_size_in  = 320; //bytes
			member->smooth_size_out = 160; //samples
			break;
		default:
			break;
	}

	if ( member->smooth_size_in > 0 )
	{
		member->in_smoother = ast_smoother_new(member->smooth_size_in);
		ast_log(LOG_DEBUG, "created smoother(%d) for %d\n",
				member->smooth_size_in , member->read_format);
	}

	if ( ast_set_read_format(member->chan, member->read_format) < 0 )
	{
		ast_log(LOG_ERROR, "unable to set read format to %d\n",
				member->read_format);
		member_delete(member);
		return 0;
	}

	ast_log(LOG_DEBUG, "created member, type => %c, priority => %d, "
			"readformat => %d\n",
			member->type, member->priority, chan->readformat);

	return member;
}

// Member must be locked prior to calling member_add_soundq
int member_add_soundq(struct ast_conf_member * member, const char * filename,
		int mute)
{
	struct ast_conf_soundq * newsound =
		ast_calloc(1, sizeof(struct ast_conf_soundq));

	if ( !newsound )
		return -1;

	newsound->stream = ast_openstream(member->chan, filename, 0);

	if ( !newsound->stream )
	{
		ast_free(newsound);
		return -1;
	}

	// This hack disallows asterisk from deallocating the stream. We
	// manage the stream via the member's soundq.
	member->chan->stream = 0;

	newsound->muted = mute;

	ast_copy_string(newsound->name, filename, sizeof(newsound->name));

	// Append to end of list
	struct ast_conf_soundq ** queue = &member->soundq;
	while ( *queue )
		queue = &(*queue)->next;
	*queue = newsound;

	return 0;
}

// Member must be locked prior to calling member_close_soundq().
int member_close_soundq(struct ast_conf_member * member)
{
	struct ast_conf_soundq * sound = member->soundq;
	while ( sound )
	{
		struct ast_conf_soundq * next_sound = sound->next;
		if ( ast_closestream(sound->stream) )
			ast_log(LOG_WARNING, "failed ast_closestream() "
					"channel => %s name => %s\n",
					member->channel_name, sound->name);
		ast_free(sound);
		sound = next_sound;
	}

	member->soundq = 0;

	return 0;
}

// Member must be locked prior to calling member_delete().
void member_delete( struct ast_conf_member* member )
{
	// If member is driving another member, make sure its speaker count is correct
	if ( member->driven_member && member->speaking_state == 1 )
	{
		member_lock(member->driven_member);
		member_decrement_speaker_count(member->driven_member);
		member_unlock(member->driven_member);
	}

	//
	// delete the member's frames
	//

	struct conf_frame* cf;

	while ( (cf = framelist_pop_tail(&member->in_audio_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->in_video_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->in_dtmf_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->out_audio_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->out_video_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->out_dtmf_framelist)) )
		frame_delete(cf);

	while ( (cf = framelist_pop_tail(&member->out_text_framelist)) )
		frame_delete(cf);

#if ( SILDET == 2 )
	if ( member->dsp != NULL )
	{
		ast_log(LOG_DEBUG, "destroying member preprocessor, name => %s\n",
			member->channel_name);
		speex_preprocess_state_destroy( member->dsp ) ;
	}
#endif

	ast_log(LOG_DEBUG, "freeing member translator paths, name => %s\n",
		member->channel_name);

	// free the mixing translators
	ast_translator_free_path( member->to_slinear ) ;
	ast_translator_free_path( member->from_slinear ) ;

	ast_smoother_free(member->in_smoother);

	member_close_soundq(member);

	ast_log(LOG_DEBUG, "freeing member channel name, name => %s\n",
		member->channel_name);

	// Note that we cast away const here, but since we are
	// destroying the whole object it is okay.
	ast_free((char *)member->flags);
	ast_free((char *)member->channel_name);
	ast_free((char *)member->conf_name);
	ast_free((char *)member->callerid);
	ast_free((char *)member->callername);

	member_unlock(member);
	ast_mutex_destroy(&member->lock);

	ast_free(member);
}

// Member must be locked prior to calling get_incoming_frame().
static struct conf_frame* get_incoming_frame( struct ast_conf_member *member )
{
#ifdef AST_CONF_CACHE_LAST_FRAME
	if ( framelist_len(&member->in_audio_framelist) == 0 )
	{
		//
		// repeat last frame a couple times to smooth transition
		//

		// nothing to do if there's no cached frame
		if ( !member->cached_audio_frame )
			return NULL;

		member->okay_to_cache_last = 0;

		if ( member->in_frames_repeat_last >= AST_CONF_CACHE_LAST_FRAME )
		{
			// already used this frame AST_CONF_CACHE_LAST_FRAME times

			// reset repeat count
			member->in_frames_repeat_last = 0;

			// clear the cached frame
			frame_delete(member->cached_audio_frame);
			member->cached_audio_frame = NULL;

			return NULL;
		}
		else
		{
			ast_log(LOG_DEBUG,
					"repeating cached frame, channel => %s, "
					"in_frames_repeat_last => %d\n",
					member->channel_name,
					member->in_frames_repeat_last);

			member->in_frames_repeat_last++;

			// return a copy of the cached frame
			return frame_copy(member->cached_audio_frame);
		}
	}
	else if ( !member->okay_to_cache_last &&
			framelist_len(&member->in_audio_framelist) >= 3 )
	{
		ast_log(LOG_DEBUG,
				"enabling cached frame, channel => %s, "
				"incoming => %d, outgoing => %d\n",
				member->channel_name,
				framelist_len(&member->in_audio_framelist),
				framelist_len(&member->out_audio_framelist));

		member->okay_to_cache_last = 1;
	}
#else
	if ( member->in_frames_count == 0 )
		return NULL;
#endif // AST_CONF_CACHE_LAST_FRAME

	struct conf_frame * cfr =
		framelist_pop_tail(&member->in_audio_framelist);

#ifdef AST_CONF_CACHE_LAST_FRAME
	// copy frame if queue is now empty
	if ( framelist_len(&member->in_audio_framelist) == 0 &&
			member->okay_to_cache_last )
	{
		// reset repeat count
		member->in_frames_repeat_last = 0;

		// clear cached frame
		if ( member->cached_audio_frame )
		{
			frame_delete(member->cached_audio_frame);
			member->cached_audio_frame = NULL;
		}

		// cache new frame
		member->cached_audio_frame = frame_copy(cfr);
	}
#endif // AST_CONF_CACHE_LAST_FRAME

	return cfr;
}

static int
queue_incoming_video_frame(struct ast_conf_member* member, const struct ast_frame* fr)
{
	if (!member->first_frame_received)
	{
		// nat=yes will be correct now
		member->first_frame_received = 1;
		member->video_switch = 1;
	}

	if ( framelist_len(&member->in_video_framelist) >=
			AST_CONF_MAX_VIDEO_QUEUE )
	{
		ast_log(LOG_WARNING,
			"unable to queue incoming VIDEO frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_video_framelist),
			framelist_len(&member->out_video_framelist));
		return -1;
	}

	return framelist_push_head(&member->in_video_framelist, fr, member);
}

int queue_incoming_dtmf_frame( struct ast_conf_member* member, const struct ast_frame* fr )
{
	if ( framelist_len(&member->in_dtmf_framelist) >=
			AST_CONF_MAX_DTMF_QUEUE )
	{
		ast_log(LOG_WARNING,
			"unable to queue incoming DTMF frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_dtmf_framelist),
			framelist_len(&member->out_dtmf_framelist));
		return -1;
	}

	return framelist_push_head(&member->in_dtmf_framelist, fr, member);
}

// Member must be locked prior to calling queue_incoming_audio_frame().
static int
queue_incoming_audio_frame(struct ast_conf_member* member, struct ast_frame* fr)
{
	const unsigned int in_framelist_len =
		framelist_len(&member->in_audio_framelist);

	if ( in_framelist_len > member->in_frames_needed &&
	     in_framelist_len > AST_CONF_QUEUE_DROP_THRESHOLD )
	{
		// time since last dropped frame
		const long diff = ast_tvdiff_ms(ast_tvnow(), member->last_in_dropped);

		// number of milliseconds which must pass between frame drops
		// ( 15 frames => -100ms, 10 frames => 400ms,
		// 5 frames => 900ms, 0 frames => 1400ms, etc. )
		const long time_limit = 1000 - ((in_framelist_len -
					AST_CONF_QUEUE_DROP_THRESHOLD) * 100);

		if ( diff >= time_limit )
		{
			// count sequential drops
			member->sequential_drops++;

			ast_log(LOG_WARNING,
					"dropping frame from input buffer, "
					"channel => %s, incoming => %d, outgoing => %d\n",
					member->channel_name,
					framelist_len(&member->in_audio_framelist),
					framelist_len(&member->out_audio_framelist));

			member->frames_in_dropped++;
			member->since_dropped = 0;

			frame_delete(framelist_pop_tail(
						&member->in_audio_framelist));

			member->last_in_dropped = ast_tvnow();
		}
	}

	if ( framelist_len(&member->in_audio_framelist) >= AST_CONF_MAX_QUEUE )
	{
		// if we have to drop frames, we'll drop new frames because
		// it's easier ( and doesn't matter much anyway ).

		member->sequential_drops++;

		ast_log(LOG_WARNING,
			"unable to queue incoming frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_audio_framelist),
			framelist_len(&member->out_audio_framelist));

		member->frames_in_dropped++;
		member->since_dropped = 0;

		return -1;
	}

	member->sequential_drops = 0;
	member->since_dropped++;

	if ( member->in_smoother )
	{
		ast_smoother_feed(member->in_smoother, fr);

		struct ast_frame *sfr;
		while ( (sfr = ast_smoother_read( member->in_smoother)) )
		{
			if ( framelist_push_head(&member->in_audio_framelist,
						sfr, member) )
				return -1;
		}
	}
	else
	{
		if ( framelist_push_head(&member->in_audio_framelist, fr,
					member) )
			return -1;
	}

	return 0;
}

static int
__queue_outgoing_frame(struct ast_conf_member* member,
		const struct ast_frame* fr, struct timeval delivery)
{
	member->frames_out++;

	// we have to drop frames, so we'll drop new frames
	// because it's easier ( and doesn't matter much anyway ).
	if ( framelist_len(&member->out_audio_framelist) >= AST_CONF_MAX_QUEUE )
	{
		ast_log(LOG_WARNING,
			"unable to queue outgoing frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_audio_framelist),
			framelist_len(&member->out_audio_framelist));
		member->frames_out_dropped++;
		return -1;
	}

	if ( framelist_push_head(&member->out_audio_framelist, fr, member) )
		return -1;

	// set delivery timestamp
	struct conf_frame * cfr =
		framelist_peek_head(&member->out_audio_framelist);
	cfr->fr->delivery = delivery;

	return 0;
}

static int
queue_outgoing_frame( struct ast_conf_member* member,
		const struct ast_frame* fr, struct timeval delivery )
{
	if ( !member->out_packer && member->smooth_multiple > 1 &&
			member->smooth_size_out > 0 )
	{
		member->out_packer = ast_packer_new(
				member->smooth_multiple * member->smooth_size_out);
	}

	if ( !member->out_packer )
	{
		return __queue_outgoing_frame( member, fr, delivery ) ;
	}
	else
	{
		struct ast_frame *sfr;
		int exitval = 0;
		ast_packer_feed( member->out_packer , fr );
		while ( (sfr = ast_packer_read(member->out_packer)) )
		{
			if ( __queue_outgoing_frame(member, sfr, delivery) )
				exitval = -1;
		}

		return exitval;
	}
}

int member_queue_outgoing_video(struct ast_conf_member * member,
		const struct ast_frame * fr)
{
	member->video_frames_out++;

	if ( framelist_len(&member->out_video_framelist) >=
			AST_CONF_MAX_VIDEO_QUEUE )
	{
		ast_log(LOG_WARNING,
			"unable to queue outgoing VIDEO frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_video_framelist),
			framelist_len(&member->out_video_framelist));
		member->video_frames_out_dropped++;
		return -1;
	}

	if ( framelist_push_head(&member->out_video_framelist, fr, member) )
		return -1;

	struct conf_frame * cfr =
		framelist_peek_head(&member->out_video_framelist);
	cfr->fr->delivery.tv_sec = 0;
	cfr->fr->delivery.tv_usec = 0;

	return 0;
}

int member_queue_outgoing_dtmf(struct ast_conf_member* member, const struct ast_frame* fr)
{
	member->dtmf_frames_out++;

	if ( framelist_len(&member->out_dtmf_framelist) >=
				AST_CONF_MAX_DTMF_QUEUE )
	{
		ast_log(LOG_WARNING,
			"unable to queue outgoing DTMF frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			framelist_len(&member->in_dtmf_framelist),
			framelist_len(&member->out_dtmf_framelist));

		member->dtmf_frames_out_dropped++;
		return -1;
	}

	return framelist_push_head(&member->out_dtmf_framelist, fr, member);
}

// Member must be locked prior to calling queue_outgoing_text_frame().
static int queue_outgoing_text_frame( struct ast_conf_member* member,
		const struct ast_frame* fr)
{
	member->text_frames_out++;

	if ( framelist_len(&member->out_text_framelist) >=
				AST_CONF_MAX_TEXT_QUEUE )
	{
		// we have to drop frames, so we'll drop new frames
		// because it's easier ( and doesn't matter much anyway ).
		ast_log(LOG_WARNING,
			"unable to queue outgoing text frame, channel => %s, "
			"incoming => %d, outgoing => %d\n",
			member->channel_name,
			0,
			framelist_len(&member->out_text_framelist));

		member->text_frames_out_dropped++;
		return -1;
	}

	return framelist_push_head(&member->out_text_framelist, fr, member);
}

//
// manager functions
//

void member_notify_state_change(struct ast_conf_member * member)
{
	if ( member->speaking_state_notify )
	{
		manager_event(EVENT_FLAG_CALL,
				"ConferenceState",
				"Channel: %s\r\n"
				"State: %s\r\n",
				member->channel_name,
				member->speaking_state == 1 ?
				"speaking" : "silent");

		member->speaking_state_notify = 0;
	}
}

static int queue_frame_for_listener(
	struct ast_conf_member* member,
	struct conf_frame* frame,
	const struct timeval delivery_time,
	struct ast_trans_pvt * from_slinear_path)
{
	// loop over spoken frames looking for member's appropriate match
	while ( frame && frame->member && frame->member != member )
		frame = frame->next;

	if ( !frame )
	{
		queue_silent_frame(member, delivery_time);
	}
	else if ( !frame->fr )
	{
		ast_log(LOG_ERROR, "unknown error queueing frame for listener, "
				"frame->fr => NULL, channel => %s\n",
				member->channel_name);
		return -1;
	}
	else
	{
		// first, try for a pre-converted frame
		struct ast_frame *qf =
			frame->converted[ member->write_format_index ];

		if ( !qf )
		{
			if ( !(qf = ast_frdup(frame->fr)) )
			{
				ast_log(LOG_ERROR, "unable to duplicate frame\n");
				return -1;
			}

			// convert using the conference's translation path
			qf = frame_convert_from_slinear(from_slinear_path, qf);

			if ( !qf )
			{
				ast_log(LOG_ERROR, "unable to translate outgoing "
						"listener frame, channel => %s\n",
						member->channel_name);
				return -1;
			}

			// store the converted frame
			// ( the frame will be free'd next time through the loop )
			frame->converted[ member->write_format_index ] = qf;
		}

		queue_outgoing_frame(member, qf, delivery_time);
	}
	
	return 0;
}


static int queue_frame_for_speaker(
	struct ast_conf_member* member,
	struct conf_frame* frame,
	const struct timeval delivery_time)
{
	// loop over spoken frames looking for member's appropriate match
	while ( frame && frame->member != member )
		frame = frame->next;

	if ( !frame )
	{
		// queue a silent frame
		queue_silent_frame(member, delivery_time);
	}
	else if ( !frame->fr )
	{
		ast_log(LOG_ERROR, "queue_frame_for_speaker: unable to queue "
				"speaker frame with null data, channel => %s\n",
				member->channel_name);
		queue_silent_frame(member, delivery_time);
	}
	else if ( frame->fr->subclass & member->chan->nativeformats )
	{
		// frame is already in correct format, so just queue it
		queue_outgoing_frame(member, frame->fr, delivery_time);
	}
	else
	{
		// Convert frame to member's write format. Use ast_frdup() to
		// make sure the translator's copy sticks around.
		struct ast_frame* qf =
			frame_convert_from_slinear(member->from_slinear,
				ast_frdup(frame->fr));

		if ( !qf )
		{
			ast_log(LOG_ERROR, "unable to translate outgoing "
					"speaker frame, channel => %s\n",
					member->channel_name);
			return -1;
		}

		queue_outgoing_frame(member, qf, delivery_time);

		// free the translator's copy
		ast_frfree(qf);
	}

	return 0;
}


static int queue_silent_frame(struct ast_conf_member* member,
		const struct timeval delivery_time)
{
	static struct conf_frame* silent_frame = NULL;

	if ( !silent_frame && !(silent_frame = frame_get_silent()) )
	{
		ast_log(LOG_WARNING, "unable to initialize static silent frame\n");
		return -1;
	}

	// get the appropriate silent frame
	struct ast_frame * qf =
		silent_frame->converted[ member->write_format_index ];

	if ( !qf )
	{
		// We need to do this to avoid echo on the speaker's line.
		// Translators seem to be single-purpose, i.e. they can't be
		// used simultaneously for multiple audio streams.

		struct ast_trans_pvt* trans = ast_translator_build_path(
				member->chan->nativeformats & AST_FORMAT_AUDIO_MASK,
				AST_FORMAT_SLINEAR);

		if ( !trans )
		{
			ast_log(LOG_ERROR, "unable to create translator for "
					"silent frame, channel => %s\n",
					member->channel_name);
			return -1;
		}

		// Attempt several times to get a silent frame to make sure we
		// provide the translator with enough data.
		int c;
		for ( c = 0 ; c < 5 && !qf; ++c )
			qf = ast_translate(trans, silent_frame->fr, 0);

		if ( qf )
		{
			// Isolate the frame so we can keep it around after
			// trans is free'd.
			qf = ast_frisolate(qf);

			// cache the new, isolated frame
			silent_frame->converted[member->write_format_index] = qf;
		}

		ast_translator_free_path(trans);
	}

	// queue the frame if it's not null, otherwise there was an error
	if ( !qf )
	{
		ast_log(LOG_ERROR, "unable to translate outgoing silent frame, "
				"channel => %s\n", member->channel_name );
		return -1;
	}

	queue_outgoing_frame(member, qf, delivery_time);

	return 0;
}

// Member must be locked prior to calling member_process_outgoing_frames().
int member_process_outgoing_frames(struct ast_conf_member *member,
		struct conf_frame *send_frames,
		const struct timeval delivery_time,
		struct ast_trans_pvt * from_slinear_path)
{
	if ( member->norecv_audio )
		return 0;

	if ( member->local_speaking_state )
		return queue_frame_for_speaker(member, send_frames,
				delivery_time);
	else
		return queue_frame_for_listener(member, send_frames,
				delivery_time, from_slinear_path);
}

// Functions that will increase and decrease speaker_count in a secure way,
// locking the member mutex if required. Will also set speaking_state flag.
// Returns the previous speaking state.
int member_increment_speaker_count(struct ast_conf_member *member)
{
	int old_state = member->speaking_state;

	member->speaker_count++;
	member->speaking_state = 1;

	ast_log(LOG_DEBUG, "Increment speaker count: id=%d, count=%d\n",
			member->id, member->speaker_count);

	// If this is a state change, update the timestamp
	if ( old_state == 0 )
	{
		member->speaking_state_notify = 1;
		member->last_state_change = ast_tvnow();
	}

	return old_state;
}

int member_decrement_speaker_count(struct ast_conf_member *member)
{
	int old_state = member->speaking_state;

	if ( member->speaker_count > 0 )
		member->speaker_count--;
	if ( member->speaker_count == 0 )
		member->speaking_state = 0;

	ast_log(LOG_DEBUG, "Decrement speaker count: id=%d, count=%d\n",
			member->id, member->speaker_count);

	// If this is a state change, update the timestamp
	if ( old_state == 1 && member->speaking_state == 0 )
	{
		member->speaking_state_notify = 1;
		member->last_state_change = ast_tvnow();
	}

	return old_state;
}

// Member must be locked prior to calling member_process_spoken_frames().
void member_process_spoken_frames(
		struct ast_conf_member *member,
		struct conf_frame **spoken_frames,
		long time_diff,
		int *listener_count,
		int *speaker_count)
{
	// tell member the number of frames we're going to need
	// ( used to help dropping algorithm )
	member->in_frames_needed = (time_diff / AST_CONF_FRAME_INTERVAL) - 1;

	// non-listener member should have frames,
	// unless silence detection dropped them
	struct conf_frame * cfr = get_incoming_frame(member);

	// handle retrieved frames
	if ( !cfr || !cfr->fr )
	{
		if ( cfr && !cfr->fr )
			ast_log(LOG_WARNING, "bogus incoming frame, channel => %s\n",
				       	member->channel_name);

		// Decrement speaker count for us and for driven members
		// This happens only for the first missed frame, since we want to
		// decrement only on state transitions
		if ( member->local_speaking_state == 1 )
		{
			member_decrement_speaker_count(member);
			member->local_speaking_state = 0;
			// If we're driving another member, decrement its
			// speaker count as well
			if ( member->driven_member )
			{
				member_lock(member->driven_member);
				member_decrement_speaker_count(member->driven_member);
				member_unlock(member->driven_member);
			}
		}

		// count the listeners
		(*listener_count)++ ;
	}
	else
	{
		// append the frame to the list of spoken frames
		if ( *spoken_frames != NULL )
		{
			// add new frame to end of list
			cfr->next = *spoken_frames ;
			(*spoken_frames)->prev = cfr ;
		}

		// point the list at the new frame
		*spoken_frames = cfr ;

		// Increment speaker count for us and for driven members
		// This happens only on the first received frame, since we
		// want to increment only on state transitions
		if ( member->local_speaking_state == 0 )
		{
			member_increment_speaker_count(member);
			member->local_speaking_state = 1;

			// If we're driving another member, increment its
			// speaker count as well
			if ( member->driven_member )
			{
				member_lock(member->driven_member);
				member_increment_speaker_count(member->driven_member);
				member_unlock(member->driven_member);
			}
		}

		// count the speakers
		(*speaker_count)++ ;
	}
}

// Member start and stop video methods
// Member must be locked prior to calling member_start_video().
void member_start_video(struct ast_conf_member *member)
{
	if ( member->video_started ||
			member->mute_video ||
			member->via_telephone )
		return;

	member_send_text_message(member, AST_CONF_CONTROL_START_VIDEO);
	member->video_started = 1;
}

void member_stop_video(struct ast_conf_member *member)
{
	if ( !member->video_started )
		return;

	member_send_text_message(member, AST_CONF_CONTROL_STOP_VIDEO);
	member->video_started = 0;
}

void member_update_video_broadcast(struct ast_conf_member * member,
		struct timeval now)
{
	const int input_frames_available =
		framelist_len(&member->in_video_framelist);

	if ( !input_frames_available &&
	     member->video_broadcast_active &&
	     ast_tvdiff_ms(now, member->last_video_frame_time) >
	     member->video_stop_broadcast_timeout
	   )
	{
		member->video_broadcast_active = 0;
		manager_event(EVENT_FLAG_CALL,
				"ConferenceVideoBroadcastOff",
				"ConferenceName: %s\r\nChannel: %s\r\n",
				member->conf_name,
				member->channel_name
				);
	}
	else if ( input_frames_available )
	{
		member->last_video_frame_time = now;
		if ( !member->video_broadcast_active )
		{
			member->video_broadcast_active = 1;
			manager_event(EVENT_FLAG_CALL,
				"ConferenceVideoBroadcastOn",
				"ConferenceName: %s\r\nChannel: %s\r\n",
				member->conf_name,
				member->channel_name
				);
		}
	}
}

// Creates a text frame and sends it to a given member
// Member must be locked prior to calling member_send_text_message().
// Returns 0 on success, -1 on failure
int member_send_text_message(struct ast_conf_member *member, const char *text)
{
	if ( member->does_text )
	{
		struct ast_frame * f = frame_create_text(text);
		if ( !f || queue_outgoing_text_frame(member, f))
			return -1;
		ast_frfree(f);
	}

	return 0;
}

