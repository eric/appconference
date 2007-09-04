 
// $Id: app_conference.c 693 2006-11-15 22:29:26Z sbalea $

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
#include "app_conference.h"
#include "common.h"
#include "asterisk/module.h"

/*

a conference has n + 1 threads, where n is the number of 
members and 1 is a conference thread which sends audio
back to the members. 

each member thread reads frames from the channel and
add's them to the member's frame queue. 

the conference thread reads frames from each speaking members
queue, mixes them, and then re-queues them for the member thread
to send back to the user.

*/

//
// app_conference text descriptions
//

static char *tdesc = "Channel Independent Conference Application" ;
static char *app = "Conference" ;
static char *synopsis = "Channel Independent Conference" ;
static char *descrip = "  Conference():  returns 0\n"
"if the user exits with the '#' key, or -1 if the user hangs up.\n" ;

// SVN revision number, provided by Make
#ifdef REVISION
static char *revision = REVISION;
#else
static char *revision = "unknown";
#endif

//
// functions defined in asterisk/module.h
//


static int unload_module( void *mod )
{
	ast_log( LOG_NOTICE, "unloading app_conference module\n" ) ;

	ast_module_user_hangup_all();
	//STANDARD_HANGUP_LOCALUSERS ; // defined in asterisk/module.h

	// register conference cli functions
	unregister_conference_cli() ;

	return ast_unregister_application( app ) ;
}

static int load_module( void *mod )
{
	ast_log( LOG_NOTICE, "Loading app_conference module, revision=%s\n", revision) ;

	// intialize conference
	init_conference() ;

	// register conference cli functions
	register_conference_cli() ;

	return ast_register_application( app, app_conference_main, synopsis, descrip ) ;
}

static const char *description( void )
{
	return tdesc ;
}

#if 0
static int usecount( void *mod )
{
	int res;
	STANDARD_USECOUNT( res ) ; // defined in asterisk/module.h
	return res;
}
#endif

static char *key( )
{
	return ASTERISK_GPL_KEY ;
}



//
// main app_conference function
//

int app_conference_main( struct ast_channel* chan, void* data ) 
{
	int res = 0 ;
	struct ast_module_user *u ;
	
	// defined in asterisk/module.h
	//LOCAL_USER_ADD( u ) ; 
	u = ast_module_user_add(chan);

	// call member thread function
	res = member_exec( chan, data ) ;

	// defined in asterisk/module.h
	//LOCAL_USER_REMOVE( u ) ;
	ast_module_user_remove (u);

	return res ;
}

//
// utility functions
//

// now returns milliseconds
long usecdiff( struct timeval* timeA, struct timeval* timeB )
{
	long a_secs = timeA->tv_sec - timeB->tv_sec ;
	long b_secs = (long)( timeA->tv_usec / 1000 ) - (long)( timeB->tv_usec / 1000 ) ;
	long u_secs = ( a_secs * 1000 ) + b_secs ;
	return u_secs ;
}

// increment a timeval by ms milliseconds
void add_milliseconds( struct timeval* tv, long ms )
{
	// add the microseconds to the microseconds field
	tv->tv_usec += ( ms * 1000 ) ;

	// calculate the number of seconds to increment
	long s = ( tv->tv_usec / 1000000 ) ;

	// adjust the microsends field
	if ( s > 0 ) tv->tv_usec -= ( s * 1000000 ) ;

	// increment the seconds field
	tv->tv_sec += s ;

	return ;
}

static int reload(void *mod)
{
	return 0;	
}

#define AST_MODULE "conference"

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Conference");

//STD_MOD(MOD_1, reload, NULL, NULL);
