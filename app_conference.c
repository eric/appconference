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

#include "asterisk.h"

// SVN revision number, provided by make
#ifndef REVISION
#define REVISION "unknown"
#endif

static char *revision = REVISION;

ASTERISK_FILE_VERSION(__FILE__, REVISION)

#include "app_conference.h"
#include "member.h"
#include "conference.h"
#include "cli.h"

/*
 * a conference has n + 1 threads, where n is the number of
 * members and 1 is a conference thread which sends audio
 * back to the members.
 *
 * each member thread reads frames from the channel and
 * add's them to the member's frame queue.
 *
 * the conference thread reads frames from each speaking members
 * queue, mixes them, and then re-queues them for the member thread
 * to send back to the user.
 */

static const char app[] = "Conference";
static const char synopsis[] = "Channel Independent Conference";
static const char descrip[] = "Channel Independent Conference Application";

static int app_conference_main(struct ast_channel* chan, void* data)
{
	int res ;
	struct ast_module_user *u ;

	u = ast_module_user_add(chan);

	// call member thread function
	res = member_exec( chan, data ) ;

	ast_module_user_remove(u);

	return res ;
}

static int unload_module( void )
{
	ast_log( LOG_NOTICE, "unloading app_conference module\n" ) ;

	ast_module_user_hangup_all();

	unregister_conference_cli() ;

	return ast_unregister_application( app ) ;
}

static int load_module( void )
{
	ast_log( LOG_NOTICE, "Loading app_conference module, revision=%s\n", revision) ;

	conference_init();

	register_conference_cli() ;

	return ast_register_application( app, app_conference_main, synopsis, descrip ) ;
}

#define AST_MODULE "Conference"
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY,
		"Channel Independent Conference Application");
#undef AST_MODULE

