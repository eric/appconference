
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
 *
 */

#ifndef _APP_CONF_CLI_H
#define _APP_CONF_CLI_H

//
// includes
//

#include "app_conference.h"
#include "common.h"

//
// function declarations
//

int conference_show_stats( int fd, int argc, char *argv[] ) ;
int conference_show_stats_name( int fd, const char* name ) ;

int conference_restart( int fd, int argc, char *argv[] );

int conference_debug( int fd, int argc, char *argv[] ) ;
int conference_no_debug( int fd, int argc, char *argv[] ) ;

int conference_list( int fd, int argc, char *argv[] ) ;
int conference_kick( int fd, int argc, char *argv[] ) ;
int conference_kickchannel( int fd, int argc, char *argv[] ) ;

int conference_mute( int fd, int argc, char *argv[] ) ;
int conference_unmute( int fd, int argc, char *argv[] ) ;
int conference_mutechannel( int fd, int argc, char *argv[] ) ;
int conference_unmutechannel( int fd, int argc, char *argv[] ) ;
int conference_viewstream( int fd, int argc, char *argv[] ) ;
int conference_viewchannel( int fd, int argc, char *argv[] ) ;

int conference_play_sound( int fd, int argc, char *argv[] ) ;
int conference_stop_sounds( int fd, int argc, char *argv[] ) ;

int conference_play_video( int fd, int argc, char *argv[] ) ;
int conference_stop_videos( int fd, int argc, char *argv[] ) ;

int conference_end( int fd, int argc, char *argv[] ) ;

int conference_lock( int fd, int argc, char *argv[] ) ;
int conference_lockchannel( int fd, int argc, char *argv[] ) ;
int conference_unlock( int fd, int argc, char *argv[] ) ;

int conference_set_default(int fd, int argc, char *argv[] ) ;
int conference_set_defaultchannel(int fd, int argc, char *argv[] ) ;

int conference_video_mute(int fd, int argc, char *argv[] ) ;
int conference_video_mutechannel(int fd, int argc, char *argv[] ) ;
int conference_video_unmute(int fd, int argc, char *argv[] ) ;
int conference_video_unmutechannel(int fd, int argc, char *argv[] ) ;

int conference_text( int fd, int argc, char *argv[] ) ;
int conference_textchannel( int fd, int argc, char *argv[] ) ;
int conference_textbroadcast( int fd, int argc, char *argv[] ) ;

int conference_drive( int fd, int argc, char *argv[] ) ;
int conference_drivechannel(int fd, int argc, char *argv[] );

int manager_conference_end(struct mansession *s, const struct message *m);

void register_conference_cli( void ) ;
void unregister_conference_cli( void ) ;


#endif
