
// $Id$

/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 * Copyright (C) 2003, 2004 HorizonLive.com, Inc.
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * Video Conferencing support added by 
 * Neil Stratford <neils@vipadia.com>
 * Copyright (C) 2005, 2005 Vipadia Limited
 *
 * This program may be modified and distributed under the 
 * terms of the GNU Public License.
 *
 */

#ifndef _APP_CONF_MEMBER_H
#define _APP_CONF_MEMBER_H

//
// includes
//

#include "app_conference.h"
#include "common.h"

//
// struct declarations
//

struct ast_conf_soundq 
{
	char name[256];
	struct ast_filestream *stream; // the stream
	int muted; // should incoming audio be muted while we play?
	struct ast_conf_soundq *next;
};

struct ast_conf_member 
{
	ast_mutex_t lock ; // member data mutex
  
	struct ast_channel* chan ; // member's channel  
	char* channel_name ; // member's channel name

	// values passed to create_member () via *data
	int priority ;	// highest priority gets the channel
	char* flags ;	// raw member-type flags
	char type ;		// L = ListenOnly, M = Moderator, S = Standard (Listen/Talk)
	char* id ;		// member id
	
	char *callerid;
	char *callername;
	
	// video conference params
	int video_id;
	int initial_id;
	int req_video_id;
	
	// muting options - this member will not be heard/seen
	int mute_audio;
	int mute_video;

	// this member will not hear/see
	int norecv_audio;
	int norecv_video;

	// is this persion a moderator? 
	int ismoderator;

	// determine by flags and channel name	
	char connection_type ; // T = telephone, X = iaxclient, S = sip
	
	// vad voice probability thresholds
	float vad_prob_start ;
	float vad_prob_continue ;
	
	// ready flag
	short ready_for_outgoing ;
	
	// input frame queue
	conf_frame* inFrames ;
	conf_frame* inFramesTail ;	
	unsigned int inFramesCount ;
	conf_frame* inVideoFrames ;
	conf_frame* inVideoFramesTail ;	
	unsigned int inVideoFramesCount ;
	conf_frame* inDTMFFrames ;
	conf_frame* inDTMFFramesTail ;	
	unsigned int inDTMFFramesCount ;

	// input/output smoother
	struct ast_smoother *inSmoother;
	struct ast_packer *outPacker;
	int smooth_size_in;
	int smooth_size_out;
	int smooth_multiple;

	// frames needed by conference_exec
	unsigned int inFramesNeeded ;
	unsigned int inVideoFramesNeeded ;

	// used when caching last frame
	conf_frame* inFramesLast ;
	unsigned int inFramesRepeatLast ;
	unsigned short okayToCacheLast ;
		
	// LL output frame queue
	conf_frame* outFrames ;
	conf_frame* outFramesTail ;	
	unsigned int outFramesCount ;
	conf_frame* outVideoFrames ;
	conf_frame* outVideoFramesTail ;	
	unsigned int outVideoFramesCount ;
	conf_frame* outDTMFFrames ;
	conf_frame* outDTMFFramesTail ;	
	unsigned int outDTMFFramesCount ;

	// LL video switched flag
	short conference;

	// switch by dtmf?
	short dtmf_switch;
	// relay dtmf to manager?
	short dtmf_relay;
	// initial nat delay flag
	short first_frame_received;

	// time we last dropped a frame
	struct timeval last_in_dropped ;
	struct timeval last_out_dropped ;
	
	// ( not currently used )
	// int samplesperframe ; 
	
	// used for determining need to mix frames
	// and for management interface notification
	short speaking_state_prev ;
	short speaking_state_notify ;
	short speaking_state ;
	
	// pointer to next member in single-linked list	
	struct ast_conf_member* next ;
	
	// accounting values
	unsigned long frames_in ; 
	unsigned long frames_in_dropped ;
	unsigned long frames_out ;
	unsigned long frames_out_dropped ;

	unsigned long video_frames_in ; 
	unsigned long video_frames_in_dropped ;
	unsigned long video_frames_out ;
	unsigned long video_frames_out_dropped ;

	unsigned long dtmf_frames_in ; 
	unsigned long dtmf_frames_in_dropped ;
	unsigned long dtmf_frames_out ;
	unsigned long dtmf_frames_out_dropped ;

	// for counting sequentially dropped frames
	unsigned int sequential_drops ;
	unsigned long since_dropped ;

	// start time
	struct timeval time_entered ;
	struct timeval lastsent_timeval ;
		
	// flag indicating we should remove this member
	short remove_flag ;
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
	int write_format ;
	int read_format ;

	int write_format_index ;
	int read_format_index ;
	
	// member frame translators
	struct ast_trans_pvt* to_slinear ;
	struct ast_trans_pvt* from_slinear ;

	// For playing sounds
	struct ast_conf_soundq *soundq;
	struct ast_conf_soundq *videoq;
} ;

struct conf_member 
{
	struct ast_conf_member* realmember ;
	struct conf_member* next ;
} ;

//
// function declarations
//

int member_exec( struct ast_channel* chan, void* data ) ;

struct ast_conf_member* check_active_video( int video_id, struct ast_conference *conf );

struct ast_conf_member* create_member( struct ast_channel* chan, const char* data ) ;
struct ast_conf_member* delete_member( struct ast_conf_member* member ) ;

// incoming queue
int queue_incoming_frame( struct ast_conf_member* member, struct ast_frame* fr ) ;
int queue_incoming_video_frame( struct ast_conf_member* member, const struct ast_frame* fr ) ;
int queue_incoming_dtmf_frame( struct ast_conf_member* member, const struct ast_frame* fr ) ;
conf_frame* get_incoming_frame( struct ast_conf_member* member ) ;
conf_frame* get_incoming_video_frame( struct ast_conf_member* member ) ;
conf_frame* get_incoming_dtmf_frame( struct ast_conf_member* member ) ;

// outgoing queue
int queue_outgoing_frame( struct ast_conf_member* member, const struct ast_frame* fr, struct timeval delivery ) ;
int __queue_outgoing_frame( struct ast_conf_member* member, const struct ast_frame* fr, struct timeval delivery ) ;
conf_frame* get_outgoing_frame( struct ast_conf_member* member ) ;

int queue_outgoing_video_frame( struct ast_conf_member* member, const struct ast_frame* fr, struct timeval delivery ) ;
conf_frame* get_outgoing_video_frame( struct ast_conf_member* member ) ;
int queue_outgoing_dtmf_frame( struct ast_conf_member* member, const struct ast_frame* fr, struct timeval delivery ) ;
conf_frame* get_outgoing_dtmf_frame( struct ast_conf_member* member ) ;

void send_state_change_notifications( struct ast_conf_member* member ) ;


void member_process_spoken_frames(struct ast_conference* conf, 
				  struct ast_conf_member *member,
				  struct conf_frame **spoken_frames,
				  long time_diff,
				 int *listener_count,
				 int *speaker_count);

void member_process_outgoing_frames(struct ast_conference* conf, 
				    struct ast_conf_member *member,
				    struct conf_frame *send_frames);

//
// packer functions
// 

struct ast_packer;

extern struct ast_packer *ast_packer_new(int bytes);
extern void ast_packer_set_flags(struct ast_packer *packer, int flags);
extern int ast_packer_get_flags(struct ast_packer *packer);
extern void ast_packer_free(struct ast_packer *s);
extern void ast_packer_reset(struct ast_packer *s, int bytes);
extern int ast_packer_feed(struct ast_packer *s, const struct ast_frame *f);
extern struct ast_frame *ast_packer_read(struct ast_packer *s);
#endif
