
// $Id: conference.h 884 2007-06-27 14:56:21Z sbalea $

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

#ifndef _APP_CONF_CONFERENCE_H
#define _APP_CONF_CONFERENCE_H

#include "app_conference.h"

//
// struct declarations
//

struct ast_conf_stats
{
	// conference name
	char name[128] ;

	// accounting data
	unsigned long frames_in ;
	unsigned long frames_out ;
	unsigned long frames_mixed ;

	struct timeval time_entered ;
};


struct ast_conference
{
	// conference name
	const char name[128] ;

	// single-linked list of members in conference
	struct ast_conf_member* memberlist ;
	int membercount ;

	// id of the default video source
	// If nobody is talking and video is unlocked, we use this source
	int default_video_source_id;

	// id of the current video source
	// this changes according to VAD rules and lock requests
	int current_video_source_id;

	// Video source locked flag, 1 -> locked, 0 -> unlocked
	short video_locked;

	// conference thread id
	pthread_t conference_thread ;

	// conference data mutex
	ast_mutex_t lock ;

	// pointer to next conference in single-linked list
	struct ast_conference* next ;

	// pointer to translation paths
	struct ast_trans_pvt* from_slinear_paths[ AC_SUPPORTED_FORMATS ] ;

	// conference stats
	struct ast_conf_stats stats ;

	// the conference does chat mode: special treatment for situations with 1 and 2 members
	short does_chat_mode;

	// chat mode is on;
	short chat_mode_on;
	
	// 1 => on, 0 => off
	short debug_flag ;
} ;


struct ast_conference* conference_join(struct ast_conf_member* member);

int conference_end(const char *name, int hangup);

// called by app_confernce.c:load_module()
void conference_init(void);

int conference_get_count(void);

int conference_show_list(int fd, const char* name);
int conference_manager_show_list(struct mansession *s, const struct message *m);
int conference_show_stats(int fd);
int conference_kick_member(const char * conf_name, int user_id);
int conference_kick_channel(const char * conf_name, const char * channel_name);
int conference_kick_all(void);
int conference_set_mute_member(const char * conf_name, int user_id, int mute);
int conference_set_mute_channel(const char * channel_name, int mute);

int conference_set_view_stream(const char * conf_name,
		int channel_id, int stream_id,
		const char * channel_name, const char * stream_name);

int conference_get_stats(struct ast_conf_stats* stats, int requested);

int conference_lock_video(const char * conf_name, int member_id,
		const char * channel_name);

int conference_unlock_video(const char *conf_name);

int conference_set_default_video(const char * conf_name, int member_id,
		const char * channel_name);

int conference_set_video_mute(const char * conf_name, int member_id,
		const char * channel_name, int mute);

int conference_send_text(const char * conf_name, int member_id,
		const char * channel_name, const char * text);

int conference_broadcast_text(const char *conf_name, const char *text);

int conference_set_video_drive(const char *conf_name,
		int src_member_id, int dst_member_id,
		const char * src_channel, const char * dst_channel);

int conference_play_sound(const char *channel_name, const char *file, int mute);

int conference_stop_sound(const char *channel_name);

int conference_set_debug(const char* name, int state);

#endif
