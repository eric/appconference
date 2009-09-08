
// $Id: cli.c 884 2007-06-27 14:56:21Z sbalea $

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
#include "cli.h"
#include "conference.h"
#include "member.h"

static int cli_conference_restart( int fd, int argc, char *argv[] )
{
	if ( argc < 2 )
		return RESULT_SHOWUSAGE ;

	conference_kick_all();
	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_restart =
{
	{ "conference", "restart", NULL },
	cli_conference_restart,
	"restart a conference",
	"usage: conference restart\n"
	"       kick all users in all conferences\n"
};

static int cli_conference_debug( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	const char * conf_name = argv[2] ;

   	// get the new state
	int state = 0 ;

	if ( argc == 3 )
	{
		// no state specified, so toggle it
		state = -1 ;
	}
	else
	{
		if ( strncasecmp( argv[3], "on", 4 ) == 0 )
			state = 1 ;
		else if ( strncasecmp( argv[3], "off", 3 ) == 0 )
			state = 0 ;
		else
			return RESULT_SHOWUSAGE ;
	}

	int new_state = conference_set_debug( conf_name, state ) ;

	if ( new_state == 1 )
	{
		ast_cli(fd, "enabled conference debugging, name => %s, "
				"new_state => %d\n", conf_name, new_state);
	}
	else if ( new_state == 0 )
	{
		ast_cli(fd, "disabled conference debugging, name => %s, "
				"new_state => %d\n", conf_name, new_state);
	}
	else
	{
		ast_cli(fd, "\nunable to set debugging state, name => %s\n\n",
				conf_name);
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_debug =
{
	{ "conference", "debug", NULL },
	cli_conference_debug,
	"enable debugging for a conference",
	"usage: conference debug <conference name> [ on | off ]\n"
	"       enable debugging for a conference\n"
};

static int cli_conference_show_stats_name( int fd, const char* name )
{
	// not implemented yet
	return RESULT_SUCCESS ;
}

static int cli_conference_show_stats( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	// get count of active conferences
	int count = conference_get_count() ;

	ast_cli( fd, "\n\nCONFERENCE STATS, ACTIVE( %d )\n\n", count ) ;

	// if zero, go no further
	if ( count <= 0 )
		return RESULT_SUCCESS ;

	//
	// get the conference stats
	//

	// array of stats structs
	struct ast_conf_stats stats[ count ] ;

	// get stats structs
	count = conference_get_stats( stats, count ) ;

	// make sure we were able to fetch some
	if ( count <= 0 )
	{
		ast_cli( fd, "!!! error fetching conference stats, available => %d !!!\n", count ) ;
		return RESULT_SUCCESS ;
	}

	//
	// output the conference stats
	//

	// output header
	ast_cli( fd, "%-20.20s  %-40.40s\n", "Name", "Stats") ;
	ast_cli( fd, "%-20.20s  %-40.40s\n", "----", "-----") ;

	struct ast_conf_stats* s = NULL ;

	int i;

	for ( i = 0 ; i < count ; ++i )
	{
		s = &(stats[i]) ;

		// output this conferences stats
		ast_cli( fd, "%-20.20s\n", (char*)( &(s->name) )) ;
	}

	ast_cli( fd, "\n" ) ;

	//
	// drill down to specific stats
	//

	if ( argc == 4 )
	{
		// show stats for a particular conference
		cli_conference_show_stats_name( fd, argv[3] ) ;
	}

	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_show_stats =
{
	{ "conference", "show", "stats", NULL },
	cli_conference_show_stats,
	"show conference stats",
	"usage: conference show stats\n"
	"       display stats for active conferences.\n"
};

static int cli_conference_list( int fd, int argc, char *argv[] )
{
	int index;

	if ( argc < 2 )
		return RESULT_SHOWUSAGE ;

	if (argc >= 3)
	{
		for (index = 2; index < argc; index++)
		{
			// get the conference name
			const char* name = argv[index] ;
			conference_show_list( fd, name );
		}
	}
	else
	{
		conference_show_stats(fd);
	}
	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_list =
{
	{ "conference", "list", NULL },
	cli_conference_list,
	"list members of a conference",
	"usage: conference list {<conference name>}\n"
	"       list members of a conference\n"
};

static int cli_conference_kick( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char* conf_name = argv[2];

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	if ( conference_kick_member( conf_name, member_id ) )
		ast_cli( fd, "User #: %d kicked\n", member_id);

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_kick =
{
	{ "conference", "kick", NULL },
	cli_conference_kick,
	"kick member from a conference",
	"usage: conference kick <conference> <member id>\n"
	"       kick member <member id> from conference <conference>\n"
};

static int cli_conference_kickchannel( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	const char *conf_name = argv[2] ;
	const char *channel_name = argv[3];

	if ( !conference_kick_channel( conf_name, channel_name ) )
	{
		ast_cli(fd, "Cannot kick channel %s in conference %s\n",
				channel_name, conf_name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_kickchannel =
{
	{ "conference", "kickchannel", NULL },
	cli_conference_kickchannel,
	"kick channel from conference",
	"usage: conference kickchannel <conference name> <channel>\n"
	"       kick channel from conference\n"
};

static int cli_conference_mute( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	// get the conference name
	const char* conf_name = argv[2];

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	if ( conference_set_mute_member( conf_name, member_id, 1 ) )
		ast_cli(fd, "User #: %d muted\n", member_id);

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_mute =
{
	{ "conference", "mute", NULL },
	cli_conference_mute,
	"mute member in a conference",
	"usage: conference mute <conference name> <member id>\n"
	"       mute member in a conference\n"
};

static int cli_conference_mutechannel( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	const char * channel_name = argv[2];

	if ( conference_set_mute_channel(channel_name, 1) )
	{
		ast_cli(fd, "Failed to mute channel %s\n", channel_name);
		return RESULT_FAILURE;
	}

	ast_cli( fd, "Channel #: %s muted\n", channel_name) ;
	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_mutechannel =
{
	{ "conference", "mutechannel", NULL },
	cli_conference_mutechannel,
	"mute channel in a conference",
	"usage: conference mutechannel <channel>\n"
	"       mute channel in a conference\n"
};

static int cli_conference_viewstream( int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	const char* conf_name = argv[2] ;

	int member_id, stream_id;
	sscanf(argv[3], "%d", &member_id);
	sscanf(argv[4], "%d", &stream_id);

	if ( conference_set_view_stream(conf_name, member_id, stream_id,
				NULL, NULL) )
		ast_cli(fd, "User #: %d viewing %d\n",
				member_id, stream_id);

	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_viewstream =
{
	{ "conference", "viewstream", NULL },
	cli_conference_viewstream,
	"switch view in a conference",
	"usage: conference viewstream <conference name> <member id> <stream no>\n"
	"       member <member id> will receive video stream <stream no>\n"
};

static int cli_conference_viewchannel( int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	const char * conf_name = argv[2];
	const char * channel_name = argv[3];
	const char * stream_name = argv[4];

	if ( conference_set_view_stream(conf_name, -1, -1,
				channel_name, stream_name) )
		ast_cli(fd, "Channel #: %s viewing %s\n",
				channel_name, stream_name);

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_viewchannel =
{
	{ "conference", "viewchannel", NULL },
	cli_conference_viewchannel,
	"switch channel in a conference",
	"usage: conference viewchannel <conference name> <dest channel> <src channel>\n"
	"       channel <dest channel> will receive video stream <src channel>\n"
};

static int cli_conference_unmute( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	const char* conf_name = argv[2] ;

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	if ( conference_set_mute_member( conf_name, member_id, 0 ) )
		ast_cli(fd, "User #: %d unmuted\n", member_id);

	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_unmute =
{
	{ "conference", "unmute", NULL },
	cli_conference_unmute,
	"unmute member in a conference",
	"usage: conference unmute <conference name> <member id>\n"
	"       unmute member in a conference\n"
};

static int cli_conference_unmutechannel( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	const char * channel_name = argv[2];

	if ( conference_set_mute_channel(channel_name, 0) )
	{
		ast_cli(fd, "Failed to unmute channel %s\n", channel_name);
		return RESULT_FAILURE;
	}

	ast_cli( fd, "Channel #: %s unmuted\n", channel_name);

	return RESULT_SUCCESS ;
}

static struct ast_cli_entry cli_unmutechannel =
{
	{ "conference", "unmutechannel", NULL },
	cli_conference_unmutechannel,
	"unmute channel in a conference",
	"usage: conference unmutechannel <channel>\n"
	"       unmute channel in a conference\n"
};

static int cli_conference_play_sound( int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	const char * channel_name = argv[3];
	const char * file_name = argv[4];
	int mute = 0;

	if (argc > 5 && !strcmp(argv[5], "mute"))
		mute = 1;

	if ( !conference_play_sound(channel_name, file_name, mute) )
	{
		ast_cli(fd, "Sound playback failed\n");
		return RESULT_FAILURE;
	}

	ast_cli(fd, "Playing sound %s to member %s %s\n",
			file_name, channel_name, mute ? "with mute" : "");

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_play_sound =
{
	{ "conference", "play", "sound", NULL },
	cli_conference_play_sound,
	"play a sound to a conference member",
	"usage: conference play sound <channel-id> <sound-file> [mute]\n"
	"       play sound <sound-file> to conference member <channel-id>.\n"
	"       If mute is specified, all other audio is muted while the "
	"sound is played back.\n"
};

static int cli_conference_stop_sounds( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	const char * channel_name = argv[3];

	if ( !conference_stop_sound(channel_name) )
	{
		ast_cli(fd, "Sound stop failed for member %s\n", channel_name);
		return RESULT_FAILURE;
	}

	ast_cli(fd, "Stopped sounds to member %s\n", channel_name);

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_stop_sounds =
{
	{ "conference", "stop", "sounds", NULL },
	cli_conference_stop_sounds,
	"stop sounds for a conference member",
	"usage: conference stop sounds <channel-id>\n"
	"       stop sounds for conference member <channel-id>.\n"
};

static int cli_conference_end( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	const char* conf_name = argv[2] ;

	if ( conference_end( conf_name, 1 ) != 0 )
	{
		ast_cli( fd, "unable to end the conference, name => %s\n",
				conf_name);
		return RESULT_SHOWUSAGE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_end =
{
	{ "conference", "end", NULL },
	cli_conference_end,
	"stops a conference",
	"usage: conference end <conference name>\n"
	"       ends a conference.\n"
};


//
// E.BUU - Manager conference end. Additional option to just kick everybody out
// without hangin up channels
//
static int manager_conference_end(struct mansession *s, const struct message *m)
{
	const char *confname = astman_get_header(m, "Conference");
	int hangup = 1;

	const char * h = astman_get_header(m, "Hangup");
	if (h)
	{
		hangup = atoi(h);
	}

	ast_log( LOG_NOTICE, "Terminating conference %s on manager's request. "
			"Hangup: %s.\n", confname, hangup ? "YES" : "NO" );
        if ( conference_end( confname, hangup ) != 0 )
        {
		ast_log( LOG_ERROR, "manager end conf: unable to terminate "
				"conference %s.\n", confname );
		astman_send_error(s, m, "Failed to terminate\r\n");
		return RESULT_FAILURE;
	}

	astman_send_ack(s, m, "Conference terminated");
	return RESULT_SUCCESS;
}

static int cli_conference_video_lock( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char * conf_name = argv[2];
	int member;
	sscanf(argv[3], "%d", &member);

	if ( !conference_lock_video(conf_name, member, NULL) )
	{
		ast_cli(fd, "Locking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_lock =
{
	{ "conference", "lock", NULL },
	cli_conference_video_lock,
	"locks incoming video to a member",
	"usage: conference lock <conference name> <member id>\n"
	"       locks incoming video stream for conference <conference name> "
	"to member <member id>\n"
};

static int cli_conference_video_lockchannel( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	const char *channel_name = argv[3];

	if ( !conference_lock_video(conf_name, -1, channel_name) )
	{
		ast_cli(fd, "Locking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_lockchannel =
{
	{ "conference", "lockchannel", NULL },
	cli_conference_video_lockchannel,
	"locks incoming video to a channel",
	"usage: conference lockchannel <conference name> <channel>\n"
	"       locks incoming video stream for conference <conference name> "
	"to channel <channel>\n"
};

static int cli_conference_video_unlock( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE;

	const char * conf_name = argv[2];

	if ( !conference_unlock_video(conf_name) )
	{
		ast_cli(fd, "Unlocking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_unlock =
{
	{ "conference", "unlock", NULL },
	cli_conference_video_unlock,
	"unlocks conference",
	"usage: conference unlock <conference name>\n"
	"       unlocks conference <conference name>\n"
};

static int cli_conference_set_default(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	if ( !conference_set_default_video(conf_name, member, NULL) )
	{
		ast_cli(fd, "Setting default video id failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_set_default =
{
	{ "conference", "set", "default", NULL },
	cli_conference_set_default,
	"sets default video source",
	"usage: conference set default <conference name> <member id>\n"
	"       sets the default video source for conference <conference name>"
	" to member <member id>\n"
	"       Use a negative value for member if you want to clear the default\n"
};

static int cli_conference_set_defaultchannel(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	const char *channel_name = argv[4];

	if ( !conference_set_default_video(conf_name, -1, channel_name) )
	{
		ast_cli(fd, "Setting default video id failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_set_defaultchannel =
{
	{ "conference", "set", "defaultchannel", NULL },
	cli_conference_set_defaultchannel,
	"sets default video source channel",
	"usage: conference set defaultchannel <conference name> <channel>\n"
	"       sets the default video source channel for conference "
	"<conference name> to channel <channel>\n"
};

static int cli_conference_video_mute(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	if ( !conference_set_video_mute(conf_name, member, NULL, 1) )
	{
		ast_cli(fd, "Muting video from member %d failed\n", member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_video_mute =
{
	{ "conference", "video", "mute", NULL },
	cli_conference_video_mute,
	"mutes video from a member",
	"usage: conference video mute <conference name> <member id>\n"
	"       mutes video from member <member id> in conference <conference name>\n"
};

static int cli_conference_video_unmute(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	if ( !conference_set_video_mute(conf_name, member, NULL, 0) )
	{
		ast_cli(fd, "Unmuting video from member %d failed\n", member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_video_unmute =
{
	{ "conference", "video", "unmute", NULL },
	cli_conference_video_unmute,
	"unmutes video from a member",
	"usage: conference video unmute <conference name> <member id>\n"
	"       unmutes video from member <member id> in conference <conference name>\n"
};

static int cli_conference_video_mutechannel(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	const char *channel_name = argv[4];

	if ( !conference_set_video_mute(conf_name, -1, channel_name, 1) )
	{
		ast_cli(fd, "Muting video from channel %s failed\n",
				channel_name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_video_mutechannel =
{
	{ "conference", "video", "mutechannel", NULL },
	cli_conference_video_mutechannel,
	"mutes video from a channel",
	"usage: conference video mutechannel <conference name> <channel>\n"
	"       mutes video from channel <channel> in conference <conference name>\n"
};

static int cli_conference_video_unmutechannel(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[3];
	const char *channel_name = argv[4];

	if ( !conference_set_video_mute(conf_name, -1, channel_name, 0) )
	{
		ast_cli(fd, "Unmuting video from channel %s failed\n",
				channel_name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_video_unmutechannel =
{
	{ "conference", "video", "unmutechannel", NULL },
	cli_conference_video_unmutechannel,
	"unmutes video from a channel",
	"usage: conference video unmutechannel <conference name> <channel>\n"
	"       unmutes video from channel <channel> in conference <conference name>\n"
};

static int cli_conference_text(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	int member;
	sscanf(argv[3], "%d", &member);
	const char *text = argv[4];

	if ( !conference_send_text(conf_name, member, NULL, text) )
	{
		ast_cli(fd, "Sending a text message to member %d failed\n",
				member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_text =
{
	{ "conference", "text", NULL },
	cli_conference_text,
	"sends a text message to a member",
	"usage: conference text <conference name> <member id> <text>\n"
	"        Sends text message <text> to member <member id> in "
	"conference <conference name>\n"
};

static int cli_conference_textchannel(int fd, int argc, char *argv[] )
{
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	const char *channel_name = argv[3];
	const char *text = argv[4];

	if ( !conference_send_text(conf_name, -1, channel_name, text) )
	{
		ast_cli(fd, "Sending a text message to channel %s failed\n",
				channel_name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_textchannel =
{
	{ "conference", "textchannel", NULL },
	cli_conference_textchannel,
	"sends a text message to a channel",
	"usage: conference textchannel <conference name> <channel> <text>\n"
	"        Sends text message <text> to channel <channel> in conference"
	" <conference name>\n"
};

static int cli_conference_textbroadcast(int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	const char *text = argv[3];

	if ( !conference_broadcast_text(conf_name, text) )
	{
		ast_cli(fd, "Sending a text broadcast to conference %s failed\n",
				conf_name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_textbroadcast =
{
	{ "conference", "textbroadcast", NULL },
	cli_conference_textbroadcast,
	"sends a text message to all members in a conference",
	"usage: conference textbroadcast <conference name> <text>\n"
	"        Sends text message <text> to all members in conference <conference name>\n"
};

//
// Associate two members
// Audio from the source member will drive VAD based video switching for the
// destination member. If the destination member is missing or negative, break
// any existing association.
//
static int cli_conference_drive(int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	int src_member = -1;
	int dst_member = -1;
	sscanf(argv[3], "%d", &src_member);
	if ( argc > 4 )
		sscanf(argv[4], "%d", &dst_member);

	if ( !conference_set_video_drive(conf_name, src_member, dst_member,
				NULL, NULL) )
	{
		ast_cli(fd, "Pairing members %d and %d failed\n", src_member, dst_member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_drive =
{
	{ "conference", "drive", NULL },
	cli_conference_drive,
	"pairs two members to drive VAD-based video switching",
	"usage: conference drive <conference name> <source member> [destination member]\n"
	"        Drives VAD video switching of <destination member> using "
	"audio from <source member> in conference <conference name>\n"
	"        If destination is missing or negative, break existing association\n"
};

//
// Associate two channels
// Audio from the source channel will drive VAD based video switching for the
// destination channel. If the destination channel is missing, break any
// existing association.
//

static int cli_conference_drivechannel(int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conf_name = argv[2];
	const char *src_channel = argv[3];
	const char *dst_channel = NULL;
	if ( argc > 4 )
		dst_channel = argv[4];

	if ( !conference_set_video_drive(conf_name, -1, -1,
				src_channel, dst_channel) )
	{
		ast_cli(fd, "Pairing channels %s and %s failed\n",
				src_channel, dst_channel);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_drivechannel =
{
	{ "conference", "drivechannel", NULL },
	cli_conference_drivechannel,
	"pairs two channels to drive VAD-based video switching",
	"usage: conference drive <conference name> <source channel> [destination channel]\n"
	"        Drives VAD video switching of <destination member> using"
	" audio from <source channel> in conference <conference channel>\n"
	"        If destination is missing, break existing association\n"
};


static struct ast_cli_entry * cli_functions[] =
{
	&cli_restart,
	&cli_debug,
	&cli_show_stats,
	&cli_list,
	&cli_kick,
	&cli_kickchannel,
	&cli_mute,
	&cli_mutechannel,
	&cli_viewstream,
	&cli_viewchannel,
	&cli_unmute,
	&cli_unmutechannel,
	&cli_play_sound,
	&cli_stop_sounds,
	&cli_end,
	&cli_lock,
	&cli_lockchannel,
	&cli_unlock,
	&cli_set_default,
	&cli_set_defaultchannel,
	&cli_video_mute,
	&cli_video_unmute,
	&cli_video_mutechannel,
	&cli_video_unmutechannel,
	&cli_text,
	&cli_textchannel,
	&cli_textbroadcast,
	&cli_drive,
	&cli_drivechannel,
	0
};

void register_conference_cli( void )
{
	int i;
	for ( i = 0; cli_functions[i]; ++i )
		ast_cli_register(cli_functions[i]);

	ast_manager_register( "ConferenceList", 0, conference_manager_show_list,
			"Conference List" );
	ast_manager_register( "ConferenceEnd", EVENT_FLAG_CALL,
			manager_conference_end, "Terminate a conference" );

}

void unregister_conference_cli( void )
{
	int i;
	for ( i = 0; cli_functions[i] != 0; ++i )
		ast_cli_unregister(cli_functions[i]);
	ast_manager_unregister( "ConferenceList" );
	ast_manager_unregister( "ConferenceEnd" );
}

