
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

#ifndef _APP_CONF_CONFERENCE_H
#define _APP_CONF_CONFERENCE_H

//
// includes
//

#include "app_conference.h"
#include "member.h"
#include "frame.h"

//
// struct declarations
//

struct ast_conference 
{
	ast_mutex_t lock ; // conference data mutex
	
	char name[80] ; // conference name
	
	// single-linked list of members in conference
	struct ast_conf_member* memberlist ;
	int membercount ;
	
	// audio thread mutexes
	pthread_t audiothread ;
	ast_mutex_t threadlock ;
	
	// notification thread mutexes
	pthread_t notification_thread ;
	ast_mutex_t notification_thread_lock ;
	
	// pointer to next conference in single-linked list
	struct ast_conference* next ;
	
	// accounting data
	long frames_in ;
	long frames_out ;
	long frames_mixed ;
	
	struct timeval time_entered ; 

	// pointer to translation paths
	struct ast_trans_pvt* from_slinear_paths[ AC_SUPPORTED_FORMATS ] ;
} ;

//
// function declarations
//

void conference_exec( struct ast_conference* conf ) ;

int queue_frame_for_listener( struct ast_conference* conf, struct ast_conf_member* member, struct conf_frame* frame ) ;
int queue_frame_for_speaker( struct ast_conference* conf, struct ast_conf_member* member, struct conf_frame* frame ) ;
int queue_silent_frame( struct ast_conference* conf, struct ast_conf_member* member ) ;

void remove_conf( struct ast_conference* conf ) ;
struct ast_conference* find_conf( char* name ) ;
struct ast_conference* create_conf( char* name, struct ast_conf_member* member ) ;

struct ast_conference* setup_member_conference( struct ast_conf_member* member ) ;
void add_member( struct ast_conf_member* member, struct ast_conference* conf ) ;
int remove_member( struct ast_conf_member* member, struct ast_conference* conf ) ;

// called by app_confernce.c:load_module()
void init_conflock( void ) ;

#endif
