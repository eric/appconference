
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
 * This program may be modified and distributed under the 
 * terms of the GNU Public License.
 */

#ifndef _APP_CONF_MEMBER_H
#define _APP_CONF_MEMBER_H

//
// includes
//

#include "app_conference.h"
#include "conference.h"
#include "frame.h"

//
// struct declarations
//

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
	
	// vad voice probability thresholds
	float vad_prob_start ;
	float vad_prob_continue ;
	
	// ready flag
	short ready_for_outgoing ;
	
	// input frame queue
	struct conf_frame* inFrames ;
	struct conf_frame* inFramesTail ;	
	int inFramesCount ;
	
	// output frame queue
	struct conf_frame* outFrames ;
	struct conf_frame* outFramesTail ;	
	int outFramesCount ;
	
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

	// for counting sequentially dropped frames
	unsigned int sequential_drops ;
	unsigned long since_dropped ;

	// start time
	struct timeval time_entered ;
		
	// flag indicating we should remove this member
	short remove_flag ;

#if ( SILDET == 2 )
	// pointer to speex preprocessor dsp
	SpeexPreprocessState *dsp ;
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

struct ast_conf_member* create_member( struct ast_channel* chan, const char* data ) ;
struct ast_conf_member* delete_member( struct ast_conf_member* member ) ;

// incoming queue
int queue_incoming_frame( struct ast_conf_member* member, struct ast_frame* fr ) ;
struct conf_frame* get_incoming_frame( struct ast_conf_member* member ) ;

// outgoing queue
int queue_outgoing_frame( struct ast_conf_member* member, struct ast_frame* fr ) ;
struct conf_frame* get_outgoing_frame( struct ast_conf_member* member ) ;

void send_state_change_notifications( struct ast_conf_member* member ) ;

#endif
