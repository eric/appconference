
// $Id: conference.c 886 2007-08-06 14:33:34Z bcholew $

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
#include "asterisk/utils.h"

#include "conference.h"
#include "frame.h"
#include "member.h"

//
// static variables
//

// single-linked list of current conferences
struct ast_conference *conflist = NULL ;

// mutex for synchronizing access to conflist
//static ast_mutex_t conflist_lock = AST_MUTEX_INITIALIZER ;
AST_MUTEX_DEFINE_STATIC(conflist_lock);

static int conference_count = 0 ;

// Forward funtcion declarations
static void do_vad_switching(struct ast_conference *);
static void do_video_switching(struct ast_conference *, int new_id);
static void remove_member(struct ast_conference *, struct ast_conf_member *);

static inline int conference_list_lock(void)
{
	return ast_mutex_lock(&conflist_lock);
}

static inline int conference_list_unlock(void)
{
	return ast_mutex_unlock(&conflist_lock);
}

static inline int conference_lock(struct ast_conference * conf)
{
	return ast_mutex_lock(&conf->lock);
}

static inline int conference_unlock(struct ast_conference * conf)
{
	return ast_mutex_unlock(&conf->lock);
}

static struct ast_conference * conference_iter_locked(struct ast_conference * conf)
{
	struct ast_conference * conf_next;

	if ( conf )
	{
		conf_next = conf->next;
		conference_unlock(conf);
	}
	else
	{
		conf_next = conflist;
	}

	if ( conf_next )
		conference_lock(conf_next);

	return conf_next;
}

// Find a conference of the given name in the conference list. The conference
// list must be locked prior to calling find_conf_locked(). This function
// relies on the guarantee that the 'name' and 'next' properties of the
// conference are (1) immutable and (2) will remain valid as long as the
// conference list lock is held.
static struct ast_conference * find_conf(const char * conf_name)
{
	struct ast_conference * conf = conflist;
	while ( conf && strncmp(conf->name, conf_name, sizeof(conf->name)) )
		conf = conf->next;
	return conf;
}

// increment a timeval by ms milliseconds
static void add_milliseconds(struct timeval* tv, long ms)
{
	const long million = 1000000;

	// add the milliseconds to the microseconds
	tv->tv_usec += ms * 1000;

	// calculate the number of seconds to increment
	const long s = tv->tv_usec / million;

	// adjust the microseconds
	tv->tv_usec -= s * million;

	// increment the seconds
	tv->tv_sec += s;
}

static void check_thread_frequency(const char * conf_name,
		int * tf_count, struct timeval * tf_base)
{
	if ( ++(*tf_count) == AST_CONF_FRAMES_PER_SECOND )
	{
		const struct timeval tf_curr = ast_tvnow();

		const long tf_diff = ast_tvdiff_ms(tf_curr, *tf_base);

		const float tf_frequency = (float)tf_diff / (float)(*tf_count);

		if ( tf_frequency <= (float)(AST_CONF_FRAME_INTERVAL - 1) ||
		     tf_frequency >= (float)(AST_CONF_FRAME_INTERVAL + 1) )
		{
			ast_log(LOG_WARNING,
					"processed frame frequency variation, "
					"name => %s, tf_count => %d, "
					"tf_diff => %ld, tf_frequency => %2.4f\n",
					conf_name, *tf_count,
					tf_diff, tf_frequency);
		}

		*tf_base = tf_curr;
		*tf_count = 0;
	}
}

static int set_conf_video_lock(struct ast_conference * conf,
		struct ast_conf_member * member)
{
	if ( member && !member->mute_video )
	{
		conf->video_locked = 1;
		do_video_switching(conf, member->id);
		manager_event(EVENT_FLAG_CALL,
				"ConferenceLock",
				"ConferenceName: %s\r\n"
				"Channel: %s\r\n",
				conf->name,
				member->channel_name);
	}
	else if ( !member )
	{
		conf->video_locked = 0;
		manager_event(EVENT_FLAG_CALL,
				"ConferenceUnlock",
				"ConferenceName: %s\r\n",
				conf->name);
		do_video_switching(conf, conf->default_video_source_id);
	}
	else
	{
		return -1;
	}

	return 0;
}

// Conference and conflist must be locked prior to calling remove_conf().
static int remove_conf( struct ast_conference *conf )
{
	if (conf->debug_flag)
	{
		ast_log(LOG_NOTICE, "removing conference, "
				"count => %d, name => %s\n",
				conf->membercount, conf->name);
	}

	// Remove conf from conflist
	if ( conf == conflist )
	{
		conflist = conf->next;
	}
	else
	{
		struct ast_conference * conf_ptr = conflist;
		while ( conf_ptr->next && conf_ptr->next != conf )
			conf_ptr = conf_ptr->next;

		if ( !conf_ptr->next )
		{
			ast_log(LOG_WARNING,
					"failed to remove conf, name => %s\n",
					conf->name);
			return -1;
		}

		conf_ptr->next = conf->next;
	}

	--conference_count;
	return 0;
}

// Conference must be locked prior to calling destroy_conf().
static int destroy_conf( struct ast_conference * conf)
{
	int i;
	for ( i = 0; i < AC_SUPPORTED_FORMATS; ++i )
	{
		// free the translation paths
		if ( conf->from_slinear_paths[i] != NULL )
		{
			ast_translator_free_path(conf->from_slinear_paths[i]);
			conf->from_slinear_paths[i] = NULL;
		}
	}

	// report accounting information
	if (conf->debug_flag)
	{
		// calculate time in conference
		// total time converted to seconds
		const long tt = ast_tvdiff_ms(ast_tvnow(),
				conf->stats.time_entered) / 1000;

		ast_log(LOG_NOTICE, "conference accounting, fi => %ld, "
				"fo => %ld, fm => %ld, tt => %ld\n",
				conf->stats.frames_in,
				conf->stats.frames_out,
				conf->stats.frames_mixed,
				tt);

		ast_log(LOG_DEBUG, "removed conference, name => %s\n",
				conf->name);
	}

	conference_unlock(conf);
	ast_mutex_destroy(&conf->lock);

	ast_free(conf);

	return 0;
}

// Conference must be locked prior to calling mux_conference_video().
static void mux_conference_video(struct ast_conference * conf)
{
	const struct timeval tvnow = ast_tvnow();

	if ( conf->does_chat_mode && conf->membercount == 1)
	{
		// In chat mode with one member, the video is
		// reflected back to the one member.
		conf->chat_mode_on = 1;

		struct ast_conf_member * m1 = conf->memberlist;

		member_lock(m1);

		if ( !member_valid(m1) )
		{
			member_unlock(m1);
			return;
		}

		member_start_video(m1);
		member_update_video_broadcast(m1, tvnow);

		struct conf_frame * cfr;
		while ( (cfr = framelist_pop_tail(&m1->in_video_framelist)) )
		{
			member_queue_outgoing_video(m1, cfr->fr);
			frame_delete(cfr);
		}

		member_unlock(m1);

	}
	else if ( conf->does_chat_mode && conf->membercount == 2 )
	{
		// In chat mode with two members, each member sees
		// the other member's video.
		conf->chat_mode_on = 1;

		struct ast_conf_member * m1 = conf->memberlist;
		struct ast_conf_member * m2 = m1->next;

		member_lock(m1);
		member_lock(m2);

		if ( !member_valid(m1) || !member_valid(m2) )
		{
			member_unlock(m2);
			member_unlock(m1);
			return;
		}

		member_start_video(m1);
		member_start_video(m2);

		member_update_video_broadcast(m1, tvnow);
		member_update_video_broadcast(m2, tvnow);

		struct conf_frame * cfr;
		while ( (cfr = framelist_pop_tail(&m1->in_video_framelist)) )
		{
			member_queue_outgoing_video(m2, cfr->fr);
			frame_delete(cfr);
		}

		while ( (cfr = framelist_pop_tail(&m2->in_video_framelist)) )
		{
			member_queue_outgoing_video(m1, cfr->fr);
			frame_delete(cfr);
		}

		member_unlock(m2);
		member_unlock(m1);
	}
	else
	{
		// Generic conference handling (chat mode disabled or
		// more than 2 members)

		if ( conf->chat_mode_on )
		{
			// If we were previously in chat mode, turn it off and
			// send STOPVIDEO commands to everybody except the
			// current source, if any.
			conf->chat_mode_on = 0;
			struct ast_conf_member * member = 0;
			while ( (member = member_iter_locked(conf->memberlist,
							member)) )
			{
				if ( member->id != conf->current_video_source_id )
					member_stop_video(member);
			}
		}

		// loop over the incoming frames and send to all outgoing
		// TODO: this is an O(n^2) algorithm. Can we speed it
		// up without sacrificing per-member switching?

		struct ast_conf_member * video_src_member = 0;
		while ( (video_src_member = member_iter_locked(
						conf->memberlist,
						video_src_member)) )
		{
			member_update_video_broadcast(video_src_member, tvnow);

			struct conf_frame * cfr;
			while ( (cfr = framelist_pop_tail(
					&video_src_member->in_video_framelist)) )
			{
				struct ast_conf_member * member = 0;
				while ( (member = member_iter_locked(
								conf->memberlist,
								member)) )
				{
					if ( member->norecv_video )
					{
						// skip members that are not
						// supposed to receive video
					}
					else if ( conf->video_locked )
					{
						// Always send video from the locked source
						if ( conf->current_video_source_id == video_src_member->id )
						{
							member_queue_outgoing_video(member, cfr->fr);
						}
					}
					else
					{
						// If the member has vad switching disabled
						// and dtmf switching enabled, use that
						if ( member->dtmf_switch &&
						     !member->vad_switch &&
						     member->requested_video_id == video_src_member->id
						   )
						{
							member_queue_outgoing_video(member, cfr->fr);
						}
						else
						{
							// If no dtmf switching, then do VAD switching
							// The VAD switching decision code should make
							// sure that our video source is legit
							if ( (conf->current_video_source_id == video_src_member->id) ||
							     (conf->current_video_source_id < 0 &&
							      conf->default_video_source_id == video_src_member->id
							     )
							   )
							{
								member_queue_outgoing_video(member, cfr->fr);
							}
						}
					}
				}
				frame_delete(cfr);
			}
		}
	}
}

//
// main conference function
//

static void conference_exec( struct ast_conference *conf )
{
	ast_log(LOG_DEBUG, "Entered conference_exec, name => %s\n", conf->name);

	// timer timestamps
	struct timeval delivery_time, notify_time, tf_base;
	delivery_time = notify_time = tf_base = ast_tvnow();

	int since_last_slept = 0 ;

	// count to AST_CONF_FRAMES_PER_SECOND
	int tf_count = 0 ;

	//
	// main conference thread loop
	//

	while ( 42 == 42 )
	{
		struct ast_conf_member * member;

		// calculate difference in timestamps
		const long time_diff = ast_tvdiff_ms(ast_tvnow(), delivery_time);

		// calculate time we should sleep
		const long time_sleep = AST_CONF_FRAME_INTERVAL - time_diff;

		if ( time_sleep > 0 )
		{
			// sleep for sleep_time ( as milliseconds )
			usleep( time_sleep * 1000 ) ;

			// reset since last slept counter
			since_last_slept = 0 ;

			continue ;
		}
		else
		{
			// long sleep warning
			if ( since_last_slept == 0 &&
					time_diff > AST_CONF_CONFERENCE_SLEEP * 2 )
			{
				ast_log(LOG_WARNING,
					"long scheduling delay, time_diff => %ld, "
					"AST_CONF_FRAME_INTERVAL => %d\n",
					time_diff, AST_CONF_FRAME_INTERVAL);
			}

			// increment times since last slept
			++since_last_slept ;

			// sleep every other time
			if ( since_last_slept % 2 )
				usleep(1);
		}

		add_milliseconds(&delivery_time, AST_CONF_FRAME_INTERVAL);

		check_thread_frequency(conf->name, &tf_count, &tf_base);

		// If the conference is empty, remove it and break the loop.

		// The whole point of the funny locking is to avoid locking the
		// conference list every time through the loop. Doing so would
		// cause this to be a serialization point for all conference
		// threads!

		conference_lock(conf);

		if ( conf->membercount == 0 )
		{
			// To avoid deadlock, unlock conference, then lock
			// list, relock conf, and check count again.
			conference_unlock(conf);

			conference_list_lock();
			conference_lock(conf);

			if ( conf->membercount == 0 )
			{
				remove_conf(conf);
				// Do not need to unlock destroyed conference
				destroy_conf(conf);
				conference_list_unlock();
				break;
			}

			conference_unlock(conf);
			conference_list_unlock();
		}
		else
		{
			conference_unlock(conf);
		}

		//-----------------//
		// INCOMING FRAMES //
		//-----------------//

		conference_lock(conf);

		if ( conf->membercount == 0 )
		{
			conference_unlock(conf);
			continue; // We'll check again at the top of the loop
		}

		int speaker_count = 0;
		int listener_count = 0;

		struct conf_frame * spoken_frames = NULL;

		member = conf->memberlist;
		while ( member )
		{
			member_lock(member);
			struct ast_conf_member * member_next = member->next;

			if ( member->remove_flag )
			{
				remove_member(conf, member);
			}
			else
			{
				if ( member_valid(member) )
					member_process_spoken_frames(
							member,
							&spoken_frames,
							time_diff,
							&listener_count,
							&speaker_count);

				member_unlock(member);
			}

			member = member_next;
		}

		//---------------//
		// MIXING FRAMES //
		//---------------//

		// mix frames and get batch of outgoing frames
		struct conf_frame * send_frames =
			frame_mix_frames(spoken_frames, speaker_count, listener_count);

		// if there are frames, count them as one incoming frame
		if ( send_frames )
			conf->stats.frames_in++;

		//-----------------//
		// OUTGOING FRAMES //
		//-----------------//

		member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			if ( member->video_switch )
			{
				member->video_switch = 0;

				struct ast_conf_member * src_member = 0;
				while ( (src_member = member_iter_locked(
								conf->memberlist,
								src_member)) )
				{
					if ( src_member->id == member->requested_video_id )
					{
						ast_indicate(src_member->chan,
								AST_CONTROL_VIDUPDATE);
						member_unlock(src_member);
						break;
					}
				}
			}

			if ( member_process_outgoing_frames(member,
						send_frames,
						delivery_time,
						conf->from_slinear_paths[
						member->write_format_index]) )
			{
				ast_log(LOG_WARNING, "member_process_outgoing_frames"
						" failed, kicking channel => %s\n",
						member->channel_name);
				member_kick(member);
			}
		}

		//-------//
		// VIDEO //
		//-------//

		mux_conference_video(conf);

		//------//
		// DTMF //
		//------//

		struct ast_conf_member * src_member = 0;
		while ( (src_member = member_iter_locked(conf->memberlist,
						src_member)) )
		{
			struct conf_frame * cfr;
			while ( (cfr = framelist_pop_tail(
					&src_member->in_dtmf_framelist)) )
			{
				member = 0;
				while ( (member = member_iter_locked(
								conf->memberlist,
								member)) )
				{
					if ( member != src_member && cfr->fr )
					{
						member_queue_outgoing_dtmf(member,
								cfr->fr);
					}
				}
				frame_delete(cfr);
			}
		}

		//---------//
		// CLEANUP //
		//---------//

		// clean up send frames
		while ( send_frames != NULL )
		{
			// accouting: count all frames and mixed frames
			if ( send_frames->member == NULL )
				conf->stats.frames_out++ ;
			else
				conf->stats.frames_mixed++ ;

			// delete the frame
			send_frames = frame_delete( send_frames ) ;
		}

		// Periodically notify the manager of state changes
		// we piggyback on this for VAD switching logic
		if ( ast_tvdiff_ms(ast_tvnow(), notify_time) >=
				AST_CONF_NOTIFICATION_SLEEP )
		{
			// Do VAD switching logic.
			// We need to do this here since
			// member_notify_state_change resets the flags
			do_vad_switching(conf);

			member = 0;
			while ( (member = member_iter_locked(conf->memberlist,
							member)) )
			{
				member_notify_state_change(member);
			}

			add_milliseconds(&notify_time,
					AST_CONF_NOTIFICATION_SLEEP);
		}

		conference_unlock(conf);
	}

	ast_log(LOG_DEBUG, "exit conference_exec\n");

	pthread_exit( NULL ) ;
}

//
// manange conference functions
//

// called by app_conference.c:load_module()
void conference_init( void )
{
	ast_mutex_init( &conflist_lock ) ;
}

// This function should be called with conflist_lock held.
// Returns new conference in the locked state.
static struct ast_conference* create_conf(const char *conf_name)
{
	struct ast_conference * conf;

	if ( !(conf = ast_calloc(1, sizeof(struct ast_conference))) )
		return NULL;

	conf->conference_thread = -1;

	conf->default_video_source_id = -1;
	conf->current_video_source_id = -1;

	// record start time
	conf->stats.time_entered = ast_tvnow();

	// copy conf_name to conference, cast away const just this once.
	strncpy((char *)conf->name, conf_name, sizeof(conf->name) - 1);
	strncpy(conf->stats.name, conf_name, sizeof(conf->name) - 1);

	// initialize mutexes
	ast_mutex_init(&conf->lock);
	conference_lock(conf);

	// build translation paths
	conf->from_slinear_paths[ AC_SLINEAR_INDEX ] = NULL ;
	conf->from_slinear_paths[ AC_ULAW_INDEX ] =
		ast_translator_build_path( AST_FORMAT_ULAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_ALAW_INDEX ] =
		ast_translator_build_path( AST_FORMAT_ALAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_GSM_INDEX ] =
		ast_translator_build_path( AST_FORMAT_GSM, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_SPEEX_INDEX ] =
		ast_translator_build_path( AST_FORMAT_SPEEX, AST_FORMAT_SLINEAR ) ;
#ifdef AC_USE_G729A
	conf->from_slinear_paths[ AC_G729A_INDEX ] =
		ast_translator_build_path( AST_FORMAT_G729A, AST_FORMAT_SLINEAR ) ;
#endif

	return conf;
}

// Conference must be locked prior to calling start_conf
static int start_conf(struct ast_conference * conf)
{
	// spawn thread for new conference, using conference_exec
	if ( ast_pthread_create(&conf->conference_thread, NULL,
				(void*)conference_exec, conf) )
	{
		ast_log(LOG_ERROR, "unable to start conference thread for "
				"conference %s\n", conf->name ) ;
		conf->conference_thread = -1 ;
		return -1;
	}

	// detach the thread so it does not leak
	pthread_detach(conf->conference_thread);

	return 0;
}

// Conference and conflist must be locked prior to calling add_conf
static int add_conf(struct ast_conference * conf)
{
	// prepend new conference to conflist
	conf->next = conflist;
	conflist = conf;
	++conference_count;
	return 0;
}

// get a video ID for this member
// must have the conf lock when calling this
static int get_new_id( struct ast_conference *conf )
{
	struct ast_conf_member * othermember = conf->memberlist;
	int newid = 0;
	while (othermember)
	{
		if (othermember->id == newid)
		{
			newid++;
			othermember = conf->memberlist;
		}
		else
		{
			othermember = othermember->next;
		}
	}
	return newid;
}

int conference_end(const char *name, int hangup )
{
	int ret = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(name);
	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			// request hangup or just kick
			if (hangup)
				ast_softhangup(member->chan, 1);
			else
				member_kick(member);
		}

		conference_unlock(conf);
	}
	else
	{
		ast_log( LOG_WARNING, "could not find conference\n" ) ;
		ret = -1;
	}

	conference_list_unlock();

	return ret;
}

// The conference, conference list, and member must all be locked prior to
// calling add_member.
static void add_member(struct ast_conference *conf, struct ast_conf_member *member)
{
	// get an ID for this member
	member->id = get_new_id( conf );

	// update conference stats
	conf->membercount++;

	// The conference sets chat mode according to the latest member chat flag
	conf->does_chat_mode = member->does_chat_mode;

	// check if we're supposed to do chat_mode, and if so, start video on the client
	if ( conf->does_chat_mode && conf->membercount <= 2 )
	{
		member_start_video(member);
		conf->chat_mode_on = 1;
	}

	if ( member->mute_video )
		member_stop_video(member);

	// The requested video id may have been pre-selected in member_create
	if ( member->requested_video_id < 0 )
	{
		// want to watch the last person to 0 or 1 (for now)
		member->requested_video_id = member->id > 0 ? 0 : 1;
	}

	// Put member at head of list
	member->next = conf->memberlist;
	conf->memberlist = member;
}

// Conference and member must be locked prior to calling remove_member().
static void remove_member(struct ast_conference * conf,
		struct ast_conf_member * member)
{
	const int member_is_moderator = member->ismoderator;
	const int member_id = member->id;

	struct ast_conf_member *member_prev = NULL ;
	struct ast_conf_member *member_ptr = conf->memberlist;
	while ( member_ptr )
	{
		if ( member_ptr == member )
		{
			// calculate time in conference (in seconds)
			const long tt = ast_tvdiff_ms(ast_tvnow(),
					member->time_entered) / 1000;

			// if this is the first member in the linked-list,
			// skip over the first member in the list, else
			// point the previous 'next' to the current 'next',
			// thus skipping the current member in the list
			if ( !member_prev )
				conf->memberlist = member->next;
			else
				member_prev->next = member->next;

			// update conference stats
			--conf->membercount;

			if ( conf->default_video_source_id == member->id )
				conf->default_video_source_id = -1;

			if ( conf->current_video_source_id == member->id )
			{
				if ( conf->video_locked )
					set_conf_video_lock(conf, 0);
				else
					do_video_switching(conf, conf->default_video_source_id);
			}

			if ( member->video_broadcast_active )
			{
				manager_event(EVENT_FLAG_CALL,
					"ConferenceVideoBroadcastOff",
					"ConferenceName: %s\r\nChannel: %s\r\n",
					conf->name,
					member->channel_name
					);
			}

			manager_event(
				EVENT_FLAG_CALL,
				"ConferenceLeave",
				"ConferenceName: %s\r\n"
				"Member: %d\r\n"
				"Channel: %s\r\n"
				"CallerID: %s\r\n"
				"CallerIDName: %s\r\n"
				"Duration: %ld\r\n"
				"Count: %d\r\n",
				conf->name,
				member->id,
				member->channel_name,
				member->callerid,
				member->callername,
				tt, conf->membercount);

			member_ptr = member->next;

			member_delete(member);
		}
		else
		{
			member_lock(member_ptr);

			// If member is driven by the currently visited member,
			// break the association. Note that we have to do this
			// even if member_ptr is invalid (kicked or removable).
			if ( member_ptr->driven_member == member )
				member_ptr->driven_member = NULL;

			if ( member_valid(member_ptr) )
			{
				if ( member_ptr->requested_video_id == member_id )
					member_ptr->video_switch = 1;

				// If member is a moderator, we end the
				// conference by kicking the remaining members.
				if ( member_is_moderator )
					member_kick(member_ptr);
			}

			member_prev = member_ptr;
			member_unlock(member_ptr);
			member_ptr = member_ptr->next;
		}
	}
}

// Member must be locked prior to calling conference_join().
struct ast_conference* conference_join( struct ast_conf_member* member )
{
	conference_list_lock();

	// look for an existing conference
	struct ast_conference * conf = find_conf(member->conf_name);

	if ( !conf )
	{
		// unable to find an existing conference
		// Note that create_conf() returns a locked conference.
		conf = create_conf(member->conf_name);
		if ( !conf )
		{
			ast_log(LOG_ERROR, "unable to create requested conference\n");
			return 0;
		}

		add_member(conf, member);

		if ( start_conf(conf) )
		{
			destroy_conf(conf);
			return 0;
		}
		add_conf(conf);
	}
	else
	{
		// existing conference found, add new member to the conference
		conference_lock(conf);
		add_member(conf, member);
	}

	const int membercount = conf->membercount;

	conference_unlock(conf);
	conference_list_unlock();

	manager_event(
		EVENT_FLAG_CALL,
		"ConferenceJoin",
		"ConferenceName: %s\r\n"
		"Member: %d\r\n"
		"Channel: %s\r\n"
		"CallerID: %s\r\n"
		"CallerIDName: %s\r\n"
		"Count: %d\r\n",
		member->conf_name,
		member->id,
		member->channel_name,
		member->callerid ? member->callerid : "unknown",
		member->callername ? member->callername : "unknown",
		membercount
		);

	return conf ;
}

// returns: -1 => error, 0 => debugging off, 1 => debugging on
// state: on => 1, off => 0, toggle => -1
int conference_set_debug( const char* conf_name, int state )
{
	if ( !conf_name )
		return -1;

	int new_state = -1;

	conference_list_lock();

	struct ast_conference *conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		if ( state == -1 )
			conf->debug_flag = !conf->debug_flag;
		else
			conf->debug_flag = state ? 1 : 0;

		new_state = conf->debug_flag;

		conference_unlock(conf);
	}

	conference_list_unlock();

	return new_state;
}

int conference_get_count( void )
{
	return conference_count ;
}

int conference_show_stats(int fd)
{
	if ( !conflist )
		return 0;

	ast_cli( fd, "%-20s %s\n", "Name", "Members") ;

	conference_list_lock();

	struct ast_conference *conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		ast_cli(fd, "%-20s %3d\n", conf->name, conf->membercount);
	}

	conference_list_unlock();

	return 1;
}

int conference_show_list(int fd, const char *conf_name)
{
	conference_list_lock();

	struct ast_conference *conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		ast_cli(fd, "Chat mode is %s\n", conf->chat_mode_on ? "ON" : "OFF");

		struct ast_conf_member * member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			ast_cli(fd, "User #: %d  Channel: %s Flags:"
					"%s%s%s%s%s" /* flags */
					"%s%s%s%s%s" /* flags */
					"%s%s%s%s%s" /* flags */
					"%s%s"       /* flags */
					" "
					"%s%s%s",    /* default, current, locked */
					member->id, member->channel_name,

					member->mute_video       ? "C" : "",
					member->norecv_video     ? "c" : "",
					member->mute_audio       ? "L" : "",
					member->norecv_audio     ? "l" : "",
					member->vad_flag         ? "V" : "",

					member->denoise_flag     ? "D" : "",
					member->agc_flag         ? "A" : "",
					member->dtmf_switch      ? "X" : "",
					member->dtmf_relay       ? "R" : "",
					member->vad_switch       ? "S" : "",

					member->ismoderator      ? "M" : "",
					member->no_camera        ? "N" : "",
					member->does_text        ? "t" : "",
					member->via_telephone    ? "T" : "",
					member->vad_linger       ? "z" : "",

					member->does_chat_mode   ? "o" : "",
					member->force_vad_switch ? "F" : "",

					member->id == conf->default_video_source_id ?
						"Default " : "",

					member->id == conf->current_video_source_id ?
						"Showing " : "",

					member->id == conf->current_video_source_id &&
						conf->video_locked ? "Locked " : ""
				);
			if ( member->driven_member )
				ast_cli(fd, "Driving:%s(%d)\n",
						member->driven_member->channel_name,
						member->driven_member->id);
			else
				ast_cli(fd, "\n");
		}
		conference_unlock(conf);
	}
	conference_list_unlock();

	return 1;
}

/* Dump list of conference info */
int conference_manager_show_list(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	const char *conf_name = astman_get_header(m, "Conference");
	char id_text[256] = "";

	astman_send_ack(s, m, "Conference list will follow");

	// no conferences exist
	if ( conflist == NULL )
		ast_log(LOG_NOTICE, "conflist has not yet been initialized,"
				" name => %s\n", conf_name );

	if ( !ast_strlen_zero(id) )
		snprintf(id_text, sizeof(id_text), "ActionID: %s\r\n", id);

	conference_list_lock();

	struct ast_conference *conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			astman_append(s, "Event: ConferenceEntry\r\n"
					"ConferenceName: %s\r\n"
					"Member: %d\r\n"
					"Channel: %s\r\n"
					"CallerID: %s\r\n"
					"CallerIDName: %s\r\n"
					"Muted: %s\r\n"
					"VideoMuted: %s\r\n"
					"Default: %s\r\n"
					"Current: %s\r\n"
					"%s"
					"\r\n",
					conf->name,
					member->id,
					member->channel_name,
					member->callerid ?
						member->callerid : "unknown",
					member->callername ?
						member->callername : "unknown",
					member->mute_audio ? "YES" : "NO",
					member->mute_video ? "YES" : "NO",
					member->id == conf->default_video_source_id ?
						"YES" : "NO",
					member->id == conf->current_video_source_id ?
						"YES" : "NO",
					id_text);
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	astman_append(s,
		"Event: ConferenceListComplete\r\n"
		"%s\r\n", id_text);

	return RESULT_SUCCESS;
}

int conference_kick_member(const char* conf_name, int user_id)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, user_id, 0);

		if ( member )
		{
			member_kick(member);
			member_unlock(member);
			res = 1;
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

int conference_kick_channel(const char *conf_name, const char *channel_name)
{
	conference_list_lock();

	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, -1, channel_name);

		if ( member )
		{
			member_kick(member);

			member_unlock(member);
			conference_unlock(conf);
			conference_list_unlock();
			return 1;
		}
	}

	conference_list_unlock();
	return 0;
}

int conference_kick_all(void)
{
	conference_list_lock();

	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		struct ast_conf_member * member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			member_kick(member);
		}
	}

	conference_list_unlock();

	return 0;
}

int conference_set_mute_member(const char* conf_name, int user_id, int mute)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, user_id, 0);

		if ( member )
		{
			member_set_audio_mute(member, mute);
			member_unlock(member);
			res = 1;
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

int conference_set_view_stream(const char* conf_name, int channel_id, int stream_id,
		const char* channel_name, const char* stream_name)
{
	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( !conf )
	{
		conference_list_unlock();
		return 0;
	}

	conference_lock(conf);

	struct ast_conf_member * member =
		member_find_locked(conf->memberlist,
				stream_id, stream_name);

	if ( member )
	{
		stream_id = member->id;
		member_unlock(member);

		member = member_find_locked(conf->memberlist,
				channel_id, channel_name);

		if ( member )
		{
			member->requested_video_id = stream_id;
			member->video_switch = 1;
			member_unlock(member);
		}
	}

	conference_unlock(conf);
	conference_list_unlock();
	return member ? 1 : 0;
}

int conference_get_stats(struct ast_conf_stats * stats, int requested)
{
	conference_list_lock();

	// compare the number of requested to the number of available conferences
	requested = conference_get_count() < requested ?
		conference_get_count() : requested;

	int count = 0;
	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		if ( count < requested )
		{
			// copy stats struct to array
			stats[count] = conf->stats;
			++count;
		}
	}

	conference_list_unlock();

	return count;
}

// All the VAD-based video switching magic happens here
// This function should be called inside conference_exec
// The conference mutex should be locked, we don't have to do it here
static void do_vad_switching(struct ast_conference *conf)
{
	if ( conf->video_locked )
		return;

	struct timeval current_time = ast_tvnow();
	long longest_speaking = 0;
	struct ast_conf_member *longest_speaking_member = NULL;
	int current_silent = 0;
	int current_linger = 0;
	int current_no_video = 0;
	int current_force_switch = 0;
	int default_no_video = 0;
	int default_force_switch = 0;

	// Scan the member list looking for the longest speaking member. We
	// also check if the currently speaking member has been silent for a
	// while. Also, we check for camera disabled or video muted members.
	// We say that a member is speaking after his speaking state has been
	// on for at least AST_CONF_VIDEO_START_TIMEOUT ms. We say that a
	// member is silent after his speaking state has been off for at least
	// AST_CONF_VIDEO_STOP_TIMEOUT ms.
	struct ast_conf_member * member = 0;
	while ( (member = member_iter_locked(conf->memberlist, member)) )
	{
		// If a member connects via telephone, they don't have video
		if ( member->via_telephone )
			continue;

		// We check for no VAD switching, video-muted or camera
		// disabled. If yes, this member will not be considered as a
		// candidate for switching. If this is the currently speaking
		// member, then mark it so we force a switch.
		if ( !member->vad_switch )
			continue;

		// Extract the linger and force switch flags of the current
		// video source.
		if ( member->id == conf->current_video_source_id )
		{
			current_linger = member->vad_linger;
			current_force_switch = member->force_vad_switch;
		}

		if ( member->id == conf->default_video_source_id )
			default_force_switch = member->force_vad_switch;

		if ( member->no_camera || member->mute_video )
		{
			if ( member->id == conf->default_video_source_id )
				default_no_video = 1;

			if ( member->id == conf->current_video_source_id )
				current_no_video = 1;
			else if ( !member->force_vad_switch )
				continue;
		}

		// Check if current speaker has been silent for a while
		if ( member->id == conf->current_video_source_id &&
		     !member->speaking_state &&
		     ast_tvdiff_ms(current_time, member->last_state_change) > member->video_stop_timeout )
		{
			current_silent = 1;
		}

		// Find a candidate to switch to by looking for the longest
		// speaking member. We exclude the current video source from
		// the search.
		if ( member->id != conf->current_video_source_id && member->speaking_state == 1 )
		{
			long speak_time = ast_tvdiff_ms(current_time, member->last_state_change);
			if ( speak_time > member->video_start_timeout && speak_time > longest_speaking )
			{
				longest_speaking = speak_time;
				longest_speaking_member = member;
			}
		}
	}

	// We got our results, now let's make a decision. If the currently
	// speaking member has been marked as silent, then we take the longest
	// speaking member. If no member is speaking, but the current member
	// has the vad_linger flag set, we stay put, otherwise we go to
	// default. If there's no default, we blank. As a policy we don't want
	// to switch away from a member that is speaking however, we might need
	// to refine this to avoid a situation when a member has a low noise
	// threshold or its VAD is simply stuck.
	if (
	     (conf->current_video_source_id < 0) ||
	     (current_silent && !current_linger) ||
	     (current_silent && longest_speaking_member != NULL ) ||
	     (current_no_video && !current_force_switch)
	   )
	{
		int new_id;

		if ( longest_speaking_member )
			// Somebody is talking, switch to that member
			new_id = longest_speaking_member->id;
		else if ( conf->default_video_source_id >= 0 &&
		          (!default_no_video || default_force_switch)
		        )
			// No talking, but we have a default that can send video
			new_id = conf->default_video_source_id;
		else
			// No default, switch to empty (-1)
			new_id = -1;

		do_video_switching(conf, new_id);
	}
}

int conference_lock_video(const char *conf_name, int member_id, const char * channel_name)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member =
			member_find_locked(conf->memberlist,
					member_id, channel_name);

		if ( member )
		{
			res = !set_conf_video_lock(conf, member);
			member_unlock(member);
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

int conference_unlock_video(const char *conf_name)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);
		res = !set_conf_video_lock(conf, 0);
		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

int conference_set_default_video(const char *conf_name, int member_id,
		const char * channel_name)
{
	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( !conf )
	{
		conference_list_unlock();
		return 0;
	}

	conference_lock(conf);

	if ( member_id < 0 && !channel_name )
	{
		conf->default_video_source_id = -1;
		manager_event(EVENT_FLAG_CALL,
				"ConferenceDefault",
				"ConferenceName: %s\r\n"
				"Channel: empty\r\n",
				conf->name);
		conference_unlock(conf);
		conference_list_unlock();
		return 1;
	}

	int res = 0;

	struct ast_conf_member * member =
		member_find_locked(conf->memberlist, member_id, channel_name);

	if ( member )
	{
		// We do not allow video muted members or members that
		// do not support VAD switching to become defaults
		if ( !member->mute_video && member->vad_switch )
		{
			conf->default_video_source_id = member->id;
			manager_event(EVENT_FLAG_CALL,
					"ConferenceDefault",
					"ConferenceName: %s\r\n"
					"Channel: %s\r\n",
					conf->name,
					member->channel_name);
			res = 1;
		}

		member_unlock(member);
	}

	conference_unlock(conf);
	conference_list_unlock();

	return res;
}

int conference_set_video_mute(const char * conf_name, int member_id,
		const char * channel_name, int mute)
{
	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( !conf )
	{
		conference_list_unlock();
		return 0;
	}

	conference_lock(conf);

	int res = 0;

	struct ast_conf_member * member =
		member_find_locked(conf->memberlist, member_id, channel_name);

	if ( member )
	{
		member_set_video_mute(member, mute);
		member_unlock(member);

		manager_event(EVENT_FLAG_CALL,
				mute ?
				"ConferenceVideoMute" : "ConferenceVideoUnmute",
				"ConferenceName: %s\r\n"
				"Channel: %s\r\n",
				conf->name,
				member->channel_name);

		if ( member->id == conf->current_video_source_id )
			do_video_switching(conf, conf->default_video_source_id);

		res = 1;
	}

	conference_unlock(conf);
	conference_list_unlock();

	return res;
}

int conference_send_text(const char * conf_name, int member_id,
		const char * channel_name, const char * text)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member =
			member_find_locked(conf->memberlist,
					member_id, channel_name);

		if ( member )
		{
			res = !member_send_text_message(member, text);
			member_unlock(member);
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

int conference_broadcast_text(const char *conf_name, const char *text)
{
	int res = 0;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( conf )
	{
		conference_lock(conf);

		struct ast_conf_member * member = 0;
		while ( (member = member_iter_locked(conf->memberlist, member)) )
		{
			if ( !member_send_text_message(member, text) )
				res = 1;
		}

		conference_unlock(conf);
	}

	conference_list_unlock();

	return res;
}

// Associates two members
// Drives VAD-based video switching of dst_member from audio from src_member
// This can be used when a member participates in a video conference but
// talks using a telephone (simulcast) connection
int conference_set_video_drive(const char *conf_name,
		int src_member_id, int dst_member_id,
		const char * src_channel, const char * dst_channel)
{
	if ( !conf_name )
		return -1;

	conference_list_lock();

	struct ast_conference * conf = find_conf(conf_name);

	if ( !conf )
	{
		conference_list_unlock();
		return 0;
	}

	conference_lock(conf);

	struct ast_conf_member * src = 0;
	struct ast_conf_member * dst = 0;

	struct ast_conf_member * member = conf->memberlist;
	while ( member )
	{
		if ( member_match(member, src_member_id, src_channel) )
			src = member;

		if ( member_match(member, dst_member_id, dst_channel) )
			dst = member;

		member = member->next;
	}

	int res = 0;

	if ( !src )
		goto bail;

	member_lock(src);

	if ( !member_valid(src) )
	{
		member_unlock(src);
		goto bail;
	}

	res = 1;

	if ( dst )
	{
		src->driven_member = dst;
		if ( src->speaking_state == 1 )
		{
			member_lock(src->driven_member);
			member_increment_speaker_count(src->driven_member);
			member_unlock(src->driven_member);
		}
	}
	else if ( dst_member_id < 0 && !dst_channel && src->driven_member )
	{
		// Make sure the driven member's speaker count is correct
		if ( src->speaking_state == 1 )
		{
			member_lock(src->driven_member);
			member_decrement_speaker_count(src->driven_member);
			member_unlock(src->driven_member);
		}
		src->driven_member = NULL;
	}

	member_unlock(src);

bail:
	conference_unlock(conf);
	conference_list_unlock();
	return res;
}

// Switches video source. Sends a manager event as well as a text message
// notifying members of a video switch. The notification is sent to the
// current member and to the new member.
static void do_video_switching(struct ast_conference *conf, int new_id)
{
	// No need to do anything if the current member is the
	// same as the new member.
	if ( new_id == conf->current_video_source_id )
		return;

	// During chat mode, we don't actually switch members. However, we keep
	// track of who is supposed to be current speaker so we can switch to
	// that member once we get out of chat mode. We also send VideoSwitch
	// events so anybody monitoring the AMI can keep track of this.
	struct ast_conf_member * new_member = 0;
	struct ast_conf_member * member = 0;
	while ( (member = member_iter_locked(conf->memberlist, member)) )
	{
		if ( member->id == conf->current_video_source_id )
		{
			if ( !conf->chat_mode_on )
				member_stop_video(member);
		}
		if ( member->id == new_id )
		{
			if ( !conf->chat_mode_on )
				member_start_video(member);
			new_member = member;
		}
	}

	manager_event(EVENT_FLAG_CALL,
			"ConferenceVideoSwitch",
			"ConferenceName: %s\r\nChannel: %s\r\n",
			conf->name,
			new_member ? new_member->channel_name : "empty");

	conf->current_video_source_id = new_id;
}

int conference_set_mute_channel(const char * channel_name, int mute)
{
	int ret = -1;

	conference_list_lock();

	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, -1, channel_name);

		if ( member )
		{
			ret = member_set_audio_mute(member, mute);
			member_unlock(member);
			conference_unlock(conf);
			break;
		}
	}

	conference_list_unlock();

	return ret;
}

int conference_play_sound(const char *channel_name, const char *file, int mute)
{
	conference_list_lock();

	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, -1, channel_name);

		if ( member )
		{
			int ret = member_add_soundq(member, file, mute);

			member_unlock(member);
			conference_unlock(conf);
			conference_list_unlock();
			return !ret;
		}
	}

	conference_list_unlock();
	return 0;
}

int conference_stop_sound(const char *channel_name)
{
	conference_list_lock();

	struct ast_conference * conf = 0;
	while ( (conf = conference_iter_locked(conf)) )
	{
		struct ast_conf_member * member =
			member_find_locked(conf->memberlist, -1, channel_name);
		if ( member )
		{
			member_close_soundq(member);

			member_unlock(member);
			conference_unlock(conf);
			conference_list_unlock();
			return 1;
		}
	}

	conference_list_unlock();
	return 0;
}

