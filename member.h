
// $Id: member.h 872 2007-03-05 23:43:10Z sbalea $

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

#ifndef _APP_CONF_MEMBER_H
#define _APP_CONF_MEMBER_H

#include "app_conference.h"
#include "framelist.h"

#if ( SILDET == 2 )
#include "libspeex/speex_preprocess.h"
#endif

struct conf_frame;
struct ast_conf_soundq;

struct ast_conf_member
{
	ast_mutex_t lock ; // member data mutex

	struct ast_channel* chan ; // member's channel

	// member's channel name
	const char * channel_name;

	// highest priority gets the channel
	int priority;

	// raw member-type flags
	const char * flags;

	// L = ListenOnly, M = Moderator, S = Standard (Listen/Talk)
	char type;

	// name of the conference that own this member
	const char * conf_name;

	const char * callerid;
	const char * callername;

	// voice flags
	int vad_flag;
	int denoise_flag;
	int agc_flag;
	int via_telephone;

	// video conference params
	int id;
	int requested_video_id;

	// muting options - this member will not be heard/seen
	int mute_audio;
	int mute_video;

	// this member will not hear/see
	int norecv_audio;
	int norecv_video;

	// this member does not have a camera
	int no_camera;

	// is this person a moderator?
	int ismoderator;

	// vad voice probability thresholds
	float vad_prob_start ;
	float vad_prob_continue ;

	struct ast_conf_framelist in_audio_framelist;
	struct ast_conf_framelist in_video_framelist;
	struct ast_conf_framelist in_dtmf_framelist;

	struct ast_conf_framelist out_audio_framelist;
	struct ast_conf_framelist out_video_framelist;
	struct ast_conf_framelist out_dtmf_framelist;
	struct ast_conf_framelist out_text_framelist;

	// input/output smoother
	struct ast_smoother *in_smoother;
	struct ast_packer *out_packer;
	int smooth_size_in;
	int smooth_size_out;
	int smooth_multiple;

	// frames needed by conference_exec
	unsigned int in_frames_needed ;

	// used when caching last frame
	struct conf_frame* cached_audio_frame ;
	unsigned int in_frames_repeat_last ;
	unsigned short okay_to_cache_last ;

	// LL video switched flag
	short video_switch;

	// switch video by VAD?
	short vad_switch;
	// do a VAD switch even if video is not enabled?
	short force_vad_switch;
	// if member is current speaker, video will stay on it when it becomes silent
	short vad_linger;
	// switch by dtmf?
	short dtmf_switch;
	// relay dtmf to manager?
	short dtmf_relay;
	// initial nat delay flag
	short first_frame_received;
	// does text messages?
	short does_text;
	// conference does chat mode (1 on 1 video when two members in conference)
	short does_chat_mode;

	// Timeouts for VAD based video switching (in ms)
	// Length of audio needed to decide that the member has started talking
	int video_start_timeout;
	// Length of silence needed to decide that the member has stopped talking
	int video_stop_timeout;
	// Length of time (in milliseconds) subsequent to last sent video frame 
	// to wait before sending ConferenceVideoBroadcastOff manager event.
	int video_stop_broadcast_timeout;

	// time we last dropped a frame
	struct timeval last_in_dropped ;
	struct timeval last_out_dropped ;

	// used for determining need to mix frames
	// and for management interface notification
	// and for VAD based video switching
	short speaking_state_notify ;
	short speaking_state ; // This flag will be true if this member or any of its drivers is speaking
	short local_speaking_state; // This flag will be true only if this member is speaking
	struct timeval last_state_change;
	int speaker_count; // Number of drivers (including this member) that are speaking

	// Stuff used to determine video broadcast state
	// This member's video is sent out to at least one member of the conference
	short video_broadcast_active;
	// Time when we last sent out a video frame from this member
	struct timeval last_video_frame_time;

	// Is the member supposed to be transmitting video?
	short video_started;

	// pointer to next member in single-linked list
	struct ast_conf_member* next ;

	// accounting values
	unsigned long frames_in ;
	unsigned long frames_in_dropped ;
	unsigned long frames_out ;
	unsigned long frames_out_dropped ;

	unsigned long video_frames_out ;
	unsigned long video_frames_out_dropped ;

	unsigned long dtmf_frames_out ;
	unsigned long dtmf_frames_out_dropped ;

	unsigned long text_frames_out ;
	unsigned long text_frames_out_dropped ;

	// for counting sequentially dropped frames
	unsigned int sequential_drops ;
	unsigned long since_dropped ;

	// start time
	struct timeval time_entered ;

	// flag indicating we should remove this member
	short remove_flag ;
	// flag indicating this member thread should finish
	short kick_flag ;

#if ( SILDET == 2 )
	// pointer to speex preprocessor dsp
	SpeexPreprocessState *dsp ;
        // number of frames to ignore speex_preprocess()
	int ignore_speex_count;
#else
	// placeholder when preprocessing is not enabled
	void* dsp ;
#endif

	// audio format this member is using
	int read_format ;

	int write_format_index ;
	int read_format_index ;

	// member frame translators
	struct ast_trans_pvt* to_slinear ;
	struct ast_trans_pvt* from_slinear ;

	// For playing sounds
	struct ast_conf_soundq *soundq;

	// Pointer to another member that will be driven from this member's audio
	struct ast_conf_member *driven_member;
} ;

//
// function declarations
//

int member_exec( struct ast_channel* chan, void* data ) ;

struct ast_conf_member* member_create( struct ast_channel *, const char* data );
void member_delete( struct ast_conf_member *);

static inline int member_lock(struct ast_conf_member * member)
{
	return ast_mutex_lock(&member->lock);
}

static inline int member_unlock(struct ast_conf_member * member)
{
	return ast_mutex_unlock(&member->lock);
}

static inline void member_kick(struct ast_conf_member * member)
{
	member->kick_flag = 1;
}

static inline int member_valid(const struct ast_conf_member * member)
{
	return !member->remove_flag && !member->kick_flag;
}

static inline int member_match(const struct ast_conf_member * member,
		const int id, const char * name)
{
	return (member->id == id && id >= 0) ||
		(name && strcmp(member->channel_name, name) == 0);
}

// This function is useful for iterating over a linked list of members. The
// member returned from this function is the next valid member in the list.
// The returned member is guaranteed to be either locked and valid, or null.
// The member_head argument should point to the head of the member list. The
// member argument should initially be 0 (null). The conference holding the
// member list must be locked prior to using member_iter_locked().
//
// Here is the intended loop construct:
//
//     struct ast_conf_member * m = 0;
//     while ( (m = member_iter_locked(conf->memberlist, m)) )
//     {
//        /* m is locked and valid inside the loop */
//
//        /* Note that if 'break' is used, m will remain locked
//         * outside the loop unless explicitly unlocked with
//         * member_unlock().
//         */
//
//        /* Note also that if 'continue' is used, m will be
//         * unlocked when member_iter_locked() is evaluated
//         * again.
//         */
//     }
//     /* m is null after the loop completes */
//
static inline struct ast_conf_member *
member_iter_locked(struct ast_conf_member * member_head,
		struct ast_conf_member * member)
{
	while ( 1 )
	{
		struct ast_conf_member * member_next;

		if ( member )
		{
			member_next = member->next;
			member_unlock(member);
		}
		else
		{
			member_next = member_head;
		}

		if ( !member_next )
			return 0;

		member_lock(member_next);
		if ( member_valid(member_next) )
			return member_next;
		else
			member = member_next;
	}
}

// This function finds a member in a member list based on id and/or name. The
// returned member is guaranteed to be either null or locked and valid. This
// function relies on the member list (conference) to be locked. We take
// advantage of the fact that the member's 'id' and 'channel_name' properties
// are okay to test/compare when the member is unlocked, but the conference is
// locked. Note that the member is [must be] locked prior to testing its
// validity. To avoid matching based on id, pass -1 for the id parameter. To
// avoid matching based on the name, pass null (0) for the name parameter.
static inline struct ast_conf_member * member_find_locked(
		struct ast_conf_member * member_head,
		int id, const char * name)
{
	struct ast_conf_member * member = member_head;

	while ( member && !member_match(member, id, name) )
		member = member->next;

	if ( member )
	{
		member_lock(member);
		if ( !member_valid(member) )
		{
			member_unlock(member);
			member = 0;
		}
	}

	return member;
}

int member_set_audio_mute(struct ast_conf_member *, int mute);
int member_set_video_mute(struct ast_conf_member *, int mute);

int member_send_text_message(struct ast_conf_member *, const char * text);

int member_queue_outgoing_video(struct ast_conf_member *, const struct ast_frame *);
int member_queue_outgoing_dtmf(struct ast_conf_member *, const struct ast_frame *);

void member_notify_state_change(struct ast_conf_member *);

int member_increment_speaker_count(struct ast_conf_member *);
int member_decrement_speaker_count(struct ast_conf_member *);

void member_process_spoken_frames(struct ast_conf_member *member,
		struct conf_frame **spoken_frames,
		long time_diff,
		int *listener_count,
		int *speaker_count);

int member_process_outgoing_frames(struct ast_conf_member *member,
				    struct conf_frame *send_frames,
				    const struct timeval delivery_time,
				    struct ast_trans_pvt * from_slinear_path);

void member_start_video(struct ast_conf_member *);
void member_stop_video(struct ast_conf_member *);
void member_update_video_broadcast(struct ast_conf_member *, struct timeval);

int member_close_soundq(struct ast_conf_member *);
int member_add_soundq(struct ast_conf_member *, const char * filename, int mute);

#endif
