
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

#ifndef _APP_CONF_FRAME_H
#define _APP_CONF_FRAME_H

//
// includes
//

#include "app_conference.h"
#include "conference.h"
#include "member.h"

//
// struct declarations
//

struct conf_frame 
{
	// frame audio data
	struct ast_frame* fr ;
	
	// array of converted versions for listeners
	struct ast_frame* converted[ AC_SUPPORTED_FORMATS ] ;
	
	// pointer to the frame's owner
	struct ast_conf_member* member ; // who sent this frame
	
	// frame meta data
//	struct timeval timestamp ;
//	unsigned long cycleid ;
//	int priority ;
	
	// linked-list pointers
	struct conf_frame* next ;
	struct conf_frame* prev ;
	
	// should this frame be preserved
	short static_frame ;
	
	// pointer to mixing buffer
	char* mixed_buffer ;
} ;

//
// function declarations
//

// mixing 
struct conf_frame* mix_frames( struct conf_frame* frames_in, int speaker_count, int listener_count ) ;

struct conf_frame* mix_multiple_speakers( struct conf_frame* frames_in, int speakers, int listeners ) ;
struct conf_frame* mix_single_speaker( struct conf_frame* frames_in ) ;

// frame creation and deletion
struct conf_frame* create_conf_frame( struct ast_conf_member* member, struct conf_frame* next ) ;
struct conf_frame* delete_conf_frame( struct conf_frame* cf ) ;

// convert frame functions
struct ast_frame* convert_frame_to_slinear( struct ast_trans_pvt* trans, struct ast_frame* fr ) ;
struct ast_frame* convert_frame_from_slinear( struct ast_trans_pvt* trans, struct ast_frame* fr ) ;
struct ast_frame* convert_frame( struct ast_trans_pvt* trans, struct ast_frame* fr ) ;

// slinear frame functions
struct ast_frame* create_slinear_frame( char* data ) ;
void mix_slinear_frames( char* dst, const char* src, int samples ) ;

// silent frame functions
struct conf_frame* get_silent_frame( void ) ;
struct ast_frame* get_silent_slinear_frame( void ) ;

#endif
