
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

#include "asterisk/autoconfig.h"
#include "conference.h"
#include "asterisk/utils.h"

//
// static variables
//

// single-linked list of current conferences
struct ast_conference *conflist = NULL ;

// mutex for synchronizing access to conflist
//static ast_mutex_t conflist_lock = AST_MUTEX_INITIALIZER ;
AST_MUTEX_DEFINE_STATIC(conflist_lock);

// mutex for synchronizing calls to start_conference() and remove_conf()
//static ast_mutex_t start_stop_conf_lock = AST_MUTEX_INITIALIZER ;
AST_MUTEX_DEFINE_STATIC(start_stop_conf_lock);

static int conference_count = 0 ;


//
// main conference function
//

void conference_exec( struct ast_conference *conf ) 
{

	struct ast_conf_member *member, *video_source_member, *dtmf_source_member;;
	struct conf_frame *cfr, *spoken_frames, *send_frames;
	
	// count number of speakers, number of listeners
	int speaker_count ;
	int listener_count ;
	
	ast_log( AST_CONF_DEBUG, "[ $Revision$ ] entered conference_exec, name => %s\n", conf->name ) ;
	
	// timer timestamps
	struct timeval base, curr, notify ;
	gettimeofday( &base, NULL ) ;
	gettimeofday( &notify, NULL ) ;

	// holds differences of curr and base
	long time_diff = 0 ;
	long time_sleep = 0 ;
	
	int since_last_slept = 0 ;
	
	//
	// variables for checking thread frequency
	//

	// count to AST_CONF_FRAMES_PER_SECOND
	int tf_count = 0 ;
	long tf_diff = 0 ;
	float tf_frequency = 0.0 ;

	struct timeval tf_base, tf_curr ;
	gettimeofday( &tf_base, NULL ) ;

	//
	// main conference thread loop
	//

 
	while ( 42 == 42 )
	{
		// update the current timestamp
		gettimeofday( &curr, NULL ) ;

		// calculate difference in timestamps
		time_diff = usecdiff( &curr, &base ) ;

		// calculate time we should sleep
		time_sleep = AST_CONF_FRAME_INTERVAL - time_diff ;
		
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
			if ( 
				since_last_slept == 0
				&& time_diff > AST_CONF_CONFERENCE_SLEEP * 2 
			)
			{
				ast_log( 
					AST_CONF_DEBUG, 
					"long scheduling delay, time_diff => %ld, AST_CONF_FRAME_INTERVAL => %d\n",
					time_diff, AST_CONF_FRAME_INTERVAL 
				) ;
			}

			// increment times since last slept
			++since_last_slept ;

			// sleep every other time
			if ( since_last_slept % 2 )
				usleep( 0 ) ;
		}

		// adjust the timer base ( it will be used later to timestamp outgoing frames )
		add_milliseconds( &base, AST_CONF_FRAME_INTERVAL ) ;
		
		//
		// check thread frequency
		//
	
		if ( ++tf_count >= AST_CONF_FRAMES_PER_SECOND )
		{
			// update current timestamp	
			gettimeofday( &tf_curr, NULL ) ;

			// compute timestamp difference
			tf_diff = usecdiff( &tf_curr, &tf_base ) ;

			// compute sampling frequency
			tf_frequency = ( float )( tf_diff ) / ( float )( tf_count ) ;

			if ( 
				( tf_frequency <= ( float )( AST_CONF_FRAME_INTERVAL - 1 ) )
				|| ( tf_frequency >= ( float )( AST_CONF_FRAME_INTERVAL + 1 ) )
			)
			{
				ast_log( 
					LOG_WARNING, 
					"processed frame frequency variation, name => %s, tf_count => %d, tf_diff => %ld, tf_frequency => %2.4f\n",
					conf->name, tf_count, tf_diff, tf_frequency
				) ;
			}

			// reset values 
			tf_base = tf_curr ;
			tf_count = 0 ;
		}

		//-----------------//
		// INCOMING FRAMES //
		//-----------------//

		// ast_log( AST_CONF_DEBUG, "PROCESSING FRAMES, conference => %s, step => %d, ms => %ld\n", 
		//	conf->name, step, ( base.tv_usec / 20000 ) ) ;

		// acquire conference mutex
		TIMELOG(ast_mutex_lock( &conf->lock ),1,"conf thread conf lock");

		// update the current delivery time
		conf->delivery_time = base ;
		
		//
		// loop through the list of members 
		// ( conf->memberlist is a single-linked list )
		//

		// ast_log( AST_CONF_DEBUG, "begin processing incoming audio, name => %s\n", conf->name ) ;

		// reset speaker and listener count
		speaker_count = 0 ;
		listener_count = 0 ;
		
		// get list of conference members
		member = conf->memberlist ;

		// reset pointer lists
		spoken_frames = NULL ;

		// reset video source
		video_source_member = NULL;

                // reset dtmf source
		dtmf_source_member = NULL;

		// loop over member list to retrieve queued frames
		while ( member != NULL )
		{
			member_process_spoken_frames(conf,member,&spoken_frames,time_diff,
						     &listener_count, &speaker_count);

			// adjust our pointer to the next inline
			member = member->next ;
		} 

		//
		// break, if we have no more members
		//

		if ( conf->membercount == 0 ) 
		{
			if (conf->debug_flag)
			{
			ast_log( LOG_NOTICE, "removing conference, count => %d, name => %s\n", conf->membercount, conf->name ) ;
			}
			remove_conf( conf ) ; // stop the conference
			break ; // break from main processing loop
		}

		// ast_log( AST_CONF_DEBUG, "finished processing incoming audio, name => %s\n", conf->name ) ;


		//---------------//
		// MIXING FRAMES //
		//---------------//

		// mix frames and get batch of outgoing frames
		send_frames = mix_frames( spoken_frames, speaker_count, listener_count ) ;

		// accounting: if there are frames, count them as one incoming frame
		if ( send_frames != NULL )
		{
			// set delivery timestamp 
			//set_conf_frame_delivery( send_frames, base ) ;
//			ast_log ( LOG_WARNING, "base = %d,%d: conf->delivery_time = %d,%d\n",base.tv_sec,base.tv_usec, conf->delivery_time.tv_sec, conf->delivery_time.tv_usec);

			// ast_log( AST_CONF_DEBUG, "base => %ld.%ld %d\n", base.tv_sec, base.tv_usec, ( int )( base.tv_usec / 1000 ) ) ;

			conf->stats.frames_in++ ;
		}
			
		//-----------------//
		// OUTGOING FRAMES //
		//-----------------//

		//
		// loop over member list to queue outgoing frames
		//
		for ( member = conf->memberlist ; member != NULL ; member = member->next ) 
		{
			member_process_outgoing_frames(conf, member, send_frames);

		}

		//-------//
		// VIDEO //
		//-------//

		// loop over the incoming frames and send to all outgoing
		// TODO: this is an O(n^2) algorithm. Can we speed it up without sacrificing per-member switching?
		for (video_source_member = conf->memberlist; 
		     video_source_member != NULL; 
		     video_source_member = video_source_member->next)
		{
			while ((cfr = get_incoming_video_frame( video_source_member )))
			{
				for (member = conf->memberlist; member != NULL; member = member->next)
				{
					// skip members that are not ready or are not supposed to receive video
					if ( !member->ready_for_outgoing || member->norecv_video ) 
						continue ;
					
					if ( member->vad_switch )
					{
						// VAD-based video switching takes precedence
						if ( conf->default_video_source_id == video_source_member->video_id )
							queue_outgoing_video_frame(member, cfr->fr, conf->delivery_time);
					} else if ( member->req_video_id == video_source_member->video_id )
					{
						// If VAD switching is disabled, then we check for DTMF switching
						queue_outgoing_video_frame(member, cfr->fr, conf->delivery_time);
					}
				}
				// Garbage collection
				delete_conf_frame(cfr);
			}
		}

                //------//
		// DTMF //
		//------//

		// loop over the incoming frames and send to all outgoing
		for (dtmf_source_member = conf->memberlist; dtmf_source_member != NULL; dtmf_source_member = dtmf_source_member->next)
		  {
		    while ((cfr = get_incoming_dtmf_frame( dtmf_source_member )))
		      {
			      for (member = conf->memberlist; member != NULL; member = member->next)
			      {
				      // skip members that are not ready
				      if ( member->ready_for_outgoing == 0 ) {
					      continue ;
				      }
				      
				      if (member != dtmf_source_member)
				      {
 					      // Send the latest frame
					      queue_outgoing_dtmf_frame(member, cfr->fr, conf->delivery_time);
				      }
			      }
			      // Garbage collection
			      delete_conf_frame(cfr);
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
			send_frames = delete_conf_frame( send_frames ) ;
		}

		//
		// notify the manager of state changes every 500 milliseconds
		// we piggyback on this for VAD switching logic
		//
		
		if ( ( usecdiff( &curr, &notify ) / AST_CONF_NOTIFICATION_SLEEP ) >= 1 )
		{
			// Do VAD switching logic
			// We need to do this here since send_state_change_notifications
			// resets the flags
			conf->default_video_source_id = do_VAD_switching(conf);
						
			// send the notifications
			send_state_change_notifications( conf->memberlist ) ;
		
			// increment the notification timer base
			add_milliseconds( &notify, AST_CONF_NOTIFICATION_SLEEP ) ;
		}

		// release conference mutex
		ast_mutex_unlock( &conf->lock ) ;
		
		// !!! TESTING !!!
		// usleep( 1 ) ;
	} 
	// end while ( 42 == 42 )

	//
	// exit the conference thread
	// 
	
	ast_log( AST_CONF_DEBUG, "exit conference_exec\n" ) ;

	// exit the thread
	pthread_exit( NULL ) ;

	return ;
}

//
// manange conference functions
//

// called by app_conference.c:load_module()
void init_conference( void ) 
{
	ast_mutex_init( &start_stop_conf_lock ) ;
	ast_mutex_init( &conflist_lock ) ;
}

struct ast_conference* start_conference( struct ast_conf_member* member ) 
{
	// check input
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to handle null member\n" ) ;
		return NULL ;
	}

	struct ast_conference* conf = NULL ;

	// acquire mutex
	ast_mutex_lock( &start_stop_conf_lock ) ;

	// look for an existing conference
	ast_log( AST_CONF_DEBUG, "attempting to find requested conference\n" ) ;
	conf = find_conf( member->id ) ;
	
	// unable to find an existing conference, try to create one
	if ( conf == NULL )
	{
		// create a new conference
		ast_log( AST_CONF_DEBUG, "attempting to create requested conference\n" ) ;

		// create the new conference with one member
		conf = create_conf( member->id, member ) ;

		// return an error if create_conf() failed
		if ( conf == NULL ) 
		{
			ast_log( LOG_ERROR, "unable to find or create requested conference\n" ) ;
			ast_mutex_unlock( &start_stop_conf_lock ) ; // release mutex
			return NULL ;
		}
	}
	else
	{
		//
		// existing conference found, add new member to the conference
		//
		// once we call add_member(), this thread
		// is responsible for calling delete_member()
		//
		add_member( member, conf ) ;
			}

	// release mutex
	ast_mutex_unlock( &start_stop_conf_lock ) ;

	return conf ;
}


struct ast_conference* find_conf( const char* name ) 
{	
	// no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return NULL ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (char*)&(conf->name), name, 80 ) == 0 )
		{
			// found conf name match 
			ast_log( AST_CONF_DEBUG, "found conference in conflist, name => %s\n", name ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	if ( conf == NULL )
	{
		ast_log( AST_CONF_DEBUG, "unable to find conference in conflist, name => %s\n", name ) ;
	}

	return conf ;
}

struct ast_conference* create_conf( char* name, struct ast_conf_member* member )
{
	ast_log( AST_CONF_DEBUG, "entered create_conf, name => %s\n", name ) ;	

	//
	// allocate memory for conference
	//

	struct ast_conference *conf = malloc( sizeof( struct ast_conference ) ) ;
	
	if ( conf == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to malloc ast_conference\n" ) ;
		return NULL ;
	}

	//
	// initialize conference
	//
	
	conf->next = NULL ;
	conf->memberlist = NULL ;

	conf->membercount = 0 ;
	conf->conference_thread = -1 ;

	conf->debug_flag = 0 ;

	conf->video_id_count = 0;
	
	conf->default_video_source_id = 0;

	// zero stats
	memset(	&conf->stats, 0x0, sizeof( ast_conference_stats ) ) ;
	
	// record start time
	gettimeofday( &conf->stats.time_entered, NULL ) ;

	// copy name to conference
	strncpy( (char*)&(conf->name), name, sizeof(conf->name) - 1 ) ;
	strncpy( (char*)&(conf->stats.name), name, sizeof(conf->name) - 1 ) ;
	
	// initialize mutexes
	ast_mutex_init( &conf->lock ) ;
	
	// build translation paths	
	conf->from_slinear_paths[ AC_SLINEAR_INDEX ] = NULL ;
	conf->from_slinear_paths[ AC_ULAW_INDEX ] = ast_translator_build_path( AST_FORMAT_ULAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_ALAW_INDEX ] = ast_translator_build_path( AST_FORMAT_ALAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_GSM_INDEX ] = ast_translator_build_path( AST_FORMAT_GSM, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_SPEEX_INDEX ] = ast_translator_build_path( AST_FORMAT_SPEEX, AST_FORMAT_SLINEAR ) ;

	// add the initial member
	add_member( member, conf ) ;
	
	//
	// prepend new conference to conflist
	//

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	conf->next = conflist ;
	conflist = conf ;

	ast_log( AST_CONF_DEBUG, "added new conference to conflist, name => %s\n", name ) ;

	//
	// spawn thread for new conference, using conference_exec( conf )
	//

	// acquire conference mutexes
	ast_mutex_lock( &conf->lock ) ;
	
	if ( ast_pthread_create( &conf->conference_thread, NULL, (void*)conference_exec, conf ) == 0 ) 
	{
		// detach the thread so it doesn't leak
		pthread_detach( conf->conference_thread ) ;
	
		// release conference mutexes
		ast_mutex_unlock( &conf->lock ) ;

		ast_log( AST_CONF_DEBUG, "started conference thread for conference, name => %s\n", conf->name ) ;
	}
	else
	{
		ast_log( LOG_ERROR, "unable to start conference thread for conference %s\n", conf->name ) ;
		
		conf->conference_thread = -1 ;

		// release conference mutexes
		ast_mutex_unlock( &conf->lock ) ;

		// clean up conference
		free( conf ) ;
		conf = NULL ;
	}

	// count new conference 
	if ( conf != NULL )
		++conference_count ;

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return conf ;
}

void remove_conf( struct ast_conference *conf )
{
  int c;

	// ast_log( AST_CONF_DEBUG, "attempting to remove conference, name => %s\n", conf->name ) ;

	struct ast_conference *conf_current = conflist ;
	struct ast_conference *conf_temp = NULL ;

	// acquire mutex
	ast_mutex_lock( &start_stop_conf_lock ) ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	// loop through list of conferences
	while ( conf_current != NULL ) 
	{
		// if conf_current point to the passed conf,
		if ( conf_current == conf ) 
		{
			if ( conf_temp == NULL ) 
			{
				// this is the first conf in the list, so we just point 
				// conflist past the current conf to the next
				conflist = conf_current->next ;
			}
			else 
			{
				// this is not the first conf in the list, so we need to
				// point the preceeding conf to the next conf in the list
				conf_temp->next = conf_current->next ;
			}

			//
			// do some frame clean up
			//
		
			for ( c = 0 ; c < AC_SUPPORTED_FORMATS ; ++c )
			{				
				// free the translation paths
				if ( conf_current->from_slinear_paths[ c ] != NULL )
				{
					ast_translator_free_path( conf_current->from_slinear_paths[ c ] ) ;
					conf_current->from_slinear_paths[ c ] = NULL ;
				}
			}

			// calculate time in conference
			struct timeval time_exited ;
			gettimeofday( &time_exited, NULL ) ;
			
			// total time converted to seconds
			long tt = ( usecdiff( &time_exited, &conf_current->stats.time_entered ) / 1000 ) ;
	
			// report accounting information
			if (conf->debug_flag)
			{
				ast_log( LOG_NOTICE, "conference accounting, fi => %ld, fo => %ld, fm => %ld, tt => %ld\n",
					 conf_current->stats.frames_in, conf_current->stats.frames_out, conf_current->stats.frames_mixed, tt ) ;
				
				ast_log( AST_CONF_DEBUG, "removed conference, name => %s\n", conf_current->name ) ;
			}

			ast_mutex_unlock( &conf_current->lock ) ;
			
			free( conf_current ) ;
			conf_current = NULL ;
			
			break ;
		}

		// save a refence to the soon to be previous conf
		conf_temp = conf_current ;
		
		// move conf_current to the next in the list
		conf_current = conf_current->next ;
	}
	
	// count new conference 
	--conference_count ;

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;
	
	// release mutex
	ast_mutex_unlock( &start_stop_conf_lock ) ;

	return ;
}

int get_new_video_id( struct ast_conference *conf )
{
	// must have the conf lock when calling this 
	int newid;
	struct ast_conf_member *othermember;
	// get a video ID for this member
	newid = 0;
	othermember = conf->memberlist;
	while (othermember)
	{
	    if (othermember->video_id == newid) 
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


int end_conference( struct ast_conference* conf ) 
{
	if ( conf == NULL ) {
		ast_log( LOG_WARNING, "null conference passed\n" ) ;
		return -1 ;
	}
	
	// acquire the conference lock
	ast_mutex_lock( &conf->lock ) ;

	// get list of conference members
	struct ast_conf_member* member = conf->memberlist ;

	// loop over member list and request hangup
	while ( member != NULL )
	{
		// acquire member mutex and request hangup
		ast_mutex_lock( &member->lock ) ;
		ast_softhangup( member->chan, 1 ) ;
		ast_mutex_unlock( &member->lock ) ;
		
		// go on to the next member
		// ( we have the conf lock, so we know this is okay )
		member = member->next ;
	}
	
	// release the conference lock
	ast_mutex_unlock( &conf->lock ) ;	
		
	return 0 ;
}

//
// member-related functions
//

void add_member( struct ast_conf_member *member, struct ast_conference *conf ) 
{
        int newid, last_video_id;
        struct ast_conf_member *othermember;
				int count;

	if ( conf == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to add member to NULL conference\n" ) ;
		return ;
	}
	
	// acquire the conference lock
	ast_mutex_lock( &conf->lock ) ;

	if (member->mute_video == 0)
	{
		if (member->video_id < 0)
		{
			// get a video ID for this member
			newid = get_new_video_id( conf );
			member->video_id = newid;
		}
		else
		{
			// boot anyone who has this id already
			othermember = conf->memberlist;
			while (othermember)
			{
				if (othermember->video_id == member->video_id)
					othermember->video_id = -1;
				othermember = othermember->next;
			}
		}
	}
	else
	{
		member->video_id = -1;
	}

	// set a long term id
	int new_initial_id = 0;
	othermember = conf->memberlist;
	while (othermember)
	{
		if (othermember->initial_id >= new_initial_id)
			new_initial_id++;
		
		othermember = othermember->next;
	}
	member->initial_id = new_initial_id;


	ast_log( AST_CONF_DEBUG, "new video id %d\n", newid) ;      

	if (conf->memberlist) last_video_id = conf->memberlist->video_id;
	else last_video_id = 0;

	if (member->req_video_id < 0) // otherwise pre-selected in create_member
	{
		// want to watch the last person to 0 or 1 (for now)
		if (member->video_id > 0) member->req_video_id = 0;
		else member->req_video_id = 1;
	}

	member->next = conf->memberlist ; // next is now list
	conf->memberlist = member ; // member is now at head of list

	// update conference stats
	count = count_member( member, conf, 1 ) ;

	ast_log( AST_CONF_DEBUG, "member added to conference, name => %s\n", conf->name ) ;
	
	// release the conference lock
	ast_mutex_unlock( &conf->lock ) ;	

	return ;
}

int remove_member( struct ast_conf_member* member, struct ast_conference* conf ) 
{
	// check for member
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to remove NULL member\n" ) ;
		return -1 ;
	}

	// check for conference
	if ( conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to remove member from NULL conference\n" ) ;
		return -1 ;
	}

	//
	// loop through the member list looking
	// for the requested member
	//

	ast_mutex_lock( &conf->lock );

	struct ast_conf_member *member_list = conf->memberlist ;
	struct ast_conf_member *member_temp = NULL ;
	
	int count = -1 ; // default return code

	while ( member_list != NULL ) 
	{
		// set conference to send no_video to anyone who was watching us
		ast_mutex_lock( &member_list->lock ) ;
		if (member_list->req_video_id == member->video_id)
		{
			member_list->conference = 1;
		}
		ast_mutex_unlock( &member_list->lock ) ;
		member_list = member_list->next ;		
	}

	

	member_list = conf->memberlist ;

	while ( member_list != NULL ) 
	{
		if ( member_list == member ) 
		{

			//
			// log some accounting information
			//

			// calculate time in conference
			struct timeval time_exited ;
			gettimeofday( &time_exited, NULL ) ;
			long tt = ( usecdiff( &time_exited, &member->time_entered ) / 1000 ) ; // convert to seconds

			if (conf->debug_flag)
			{
				ast_log( 
					LOG_NOTICE, 
					"member accounting, channel => %s, te => %ld, fi => %ld, fid => %ld, fo => %ld, fod => %ld, tt => %ld\n",
					member->channel_name,
					member->time_entered.tv_sec, member->frames_in, member->frames_in_dropped, 
					member->frames_out, member->frames_out_dropped, tt 
					) ;
			}

			//
			// if this is the first member in the linked-list,
			// skip over the first member in the list, else
			//
			// point the previous 'next' to the current 'next',
			// thus skipping the current member in the list	
			//	
			if ( member_temp == NULL )
				conf->memberlist = member->next ;
			else 
				member_temp->next = member->next ;

			// update conference stats
			count = count_member( member, conf, 0 ) ;

			// output to manager...
			manager_event(
				EVENT_FLAG_CALL,
				"ConferenceLeave",
				"SwitchName: %s\r\n"
				"Member: %d\r\n"
				"Channel: %s\r\n"
				"CallerID: %s\r\n"
				"CallerIDName: %s\r\n"
				"Duration: %ld\r\n"
				"Count: %d\r\n",
				conf->name,
				member->video_id,
				member->channel_name,
				member->callerid,
				member->callername,
				tt, count
			) ;

			// delete the member
			delete_member( member ) ;
			
			ast_log( AST_CONF_DEBUG, "removed member from conference, name => %s, remaining => %d\n", conf->name, conf->membercount ) ;
			
			//break ;
		}
		else
		{
			// Mihai: we don't want to kick the conference when one moderator leaves
			// if member is a moderator, we end the conference when they leave
			//if (member->ismoderator) 
			//{
			//	ast_mutex_lock( &member_list->lock ) ;
			//	member_list->kick_flag = 1;
			//	ast_mutex_unlock( &member_list->lock ) ;
			//}
		}
		
		
		// save a pointer to the current member,
		// and then point to the next member in the list
		member_temp = member_list ;
		member_list = member_list->next ;
	}
	ast_mutex_unlock( &conf->lock );

	// return -1 on error, or the number of members 
	// remaining if the requested member was deleted
	return count ;
}

int count_member( struct ast_conf_member* member, struct ast_conference* conf, short add_member )
{
	if ( member == NULL || conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to count member\n" ) ;
		return -1 ;
	}

	short delta = ( add_member == 1 ) ? 1 : -1 ;

	// increment member count
	conf->membercount += delta ;

	return conf->membercount ;
}

//
// queue incoming frame functions
//




//
// get conference stats
//

//
// returns: -1 => error, 0 => debugging off, 1 => debugging on
// state: on => 1, off => 0, toggle => -1
//
int set_conference_debugging( const char* name, int state )
{
	if ( name == NULL )
		return -1 ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	int new_state = -1 ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
			// lock conference
			// ast_mutex_lock( &(conf->lock) ) ;

			// toggle or set the state
			if ( state == -1 )
				conf->debug_flag = ( conf->debug_flag == 0 ) ? 1 : 0 ;
			else
				conf->debug_flag = ( state == 0 ) ? 0 : 1 ;

			new_state = conf->debug_flag ;

			// unlock conference
			// ast_mutex_unlock( &(conf->lock) ) ;

			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;
	
	return new_state ;
}


int get_conference_count( void ) 
{
	return conference_count ;
}

int show_conference_stats ( int fd )
{
        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized.\n") ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	ast_cli( fd, "%-20.20s  %-40.40s\n", "Name", "Members") ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		ast_cli( fd, "%-20.20s %3d\n", conf->name, conf->membercount ) ;		
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return 1 ;
}

int show_conference_list ( int fd, const char *name )
{
	struct ast_conf_member *member;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
		        // do the biz
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    ast_cli( fd, "User #: %d  ", member->video_id ) ;	
			    ast_cli( fd, "Channel: %s ", member->channel_name ) ;
			    if (member->mute_audio == 1)
				ast_cli ( fd, "Muted ");
      			    ast_cli( fd, "\n");
			    member = member->next;
			  }
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return 1 ;
}

/* Dump list of conference info */
int manager_conference_list( struct mansession *s, struct message *m )
{
	char *id = astman_get_header(m,"ActionID");
	char *conffilter = astman_get_header(m,"Conference");
	char idText[256] = "";
	struct ast_conf_member *member;

	astman_send_ack(s, m, "Conference list will follow");

  // no conferences exist
	if ( conflist == NULL ) 
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", conffilter );;
	
	if (!ast_strlen_zero(id)) {
		snprintf(idText,256,"ActionID: %s\r\n",id);
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), conffilter, 80 ) == 0 )
		{
			// do the biz
			member = conf->memberlist ;
			while (member != NULL)
			  {
					astman_append(s, "Event: ConferenceEntry\r\n"
						"SwitchName: %s\r\n"	
						"Member: %d\r\n"	
						"Channel: %s\r\n"
						"CallerID: %s\r\n"
						"CallerIDName: %s\r\n"
						"Muted: %s\r\n"
						"%s"
						"\r\n",
						conf->name,
						member->video_id,
						member->channel_name,
						member->chan->cid.cid_num ? member->chan->cid.cid_num : "unknown",
						member->chan->cid.cid_name ? member->chan->cid.cid_name : "unknown",
						member->mute_audio ? "YES" : "NO",
						idText);
			    member = member->next;
			  }
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	astman_append(s,
		"Event: ConferenceListComplete\r\n"
		"%s"
		"\r\n",idText);

	return RESULT_SUCCESS;
}

int kick_member (  const char* confname, int user_id)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz	
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->video_id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->kick_flag = 1;
				      //ast_soft_hangup(member->chan);
				      ast_mutex_unlock( &member->lock ) ;

				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int kick_all ( void )
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized\n" ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		// do the biz	
		ast_mutex_lock( &conf->lock ) ;
		member = conf->memberlist ;
		while (member != NULL)
		{
			ast_mutex_lock( &member->lock ) ;
			member->kick_flag = 1;
			ast_mutex_unlock( &member->lock ) ;
			member = member->next;
		}
		ast_mutex_unlock( &conf->lock ) ;
		break ;
	
	conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int mute_member (  const char* confname, int user_id)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->video_id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 1;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;		
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int mute_channel (  const char* confname, const char* user_chan)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
				  if (strncasecmp( member->channel_name, user_chan, 80 ) == 0)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 1;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;		
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int unmute_member (  const char* confname, int user_id)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->video_id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 0;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int unmute_channel (const char* confname, const char* user_chan)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			   if (strncasecmp( member->channel_name, user_chan, 80 ) == 0)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 0;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int viewstream_switch ( const char* confname, int user_id, int stream_id )
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			{
				if (member->video_id == user_id)
				{
					// switch the video
					ast_mutex_lock( &member->lock ) ;
					
					member->req_video_id = stream_id; 
					member->conference = 1;
	
					ast_mutex_unlock( &member->lock ) ; 
					res = 1;			      
				}
				member = member->next;
			}
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int viewchannel_switch ( const char* confname, const char* userchan, const char* streamchan )
{
  struct ast_conf_member *member;
  int res = 0;
  int stream_id = -1;

        // no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			{
				if (strncasecmp( member->channel_name, streamchan, 80 ) == 0)
				{
					stream_id = member->video_id;
				}
				member = member->next;
			}
			ast_mutex_unlock( &conf->lock ) ;
			if (stream_id >= 0) 
			{
				// do the biz
				ast_mutex_lock( &conf->lock ) ;
				member = conf->memberlist ;
				while (member != NULL)
				{
					if (strncasecmp( member->channel_name, userchan, 80 ) == 0)
					{
						// switch the video
						ast_mutex_lock( &member->lock ) ;
						
						member->req_video_id = stream_id; 
						member->conference = 1;
						
						ast_mutex_unlock( &member->lock ) ; 
						res = 1;			      
					}
					member = member->next;
				}
				ast_mutex_unlock( &conf->lock ) ;
			}
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int get_conference_stats( ast_conference_stats* stats, int requested )
{	
	// no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialize\n" ) ;
		return 0 ;
	}
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	// compare the number of requested to the number of available conferences
	requested = ( get_conference_count() < requested ) ? get_conference_count() : requested ;

	//
	// loop through conf list
	//

	struct ast_conference* conf = conflist ;
	int count = 0 ;
	
	while ( count <= requested && conf != NULL ) 
	{
		// copy stats struct to array
		stats[ count ] = conf->stats ; 
		
		conf = conf->next ;
		++count ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return count ;
}

int get_conference_stats_by_name( ast_conference_stats* stats, const char* name )
{	
	// no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return 0 ;
	}
	
	// make sure stats is null
	stats = NULL ;
	
	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
			// copy stats for found conference
			*stats = conf->stats ;
			break ;
		}
	
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return ( stats == NULL ) ? 0 : 1 ;
}

struct ast_conf_member *find_member ( char *chan, int lock) 
{
	struct ast_conf_member *found = NULL;
	struct ast_conf_member *member;	
	struct ast_conference *conf;

	ast_mutex_lock( &conflist_lock ) ;

	conf = conflist;

	// loop through conf list
	while ( conf != NULL && !found ) 
	{
		// lock conference
		ast_mutex_lock( &conf->lock );

		member = conf->memberlist ;

		while (member != NULL) 
		{
		    if(!strcmp(member->channel_name, chan)) {
			found = member;
			if(lock) 
			    ast_mutex_lock(&member->lock);
			break;
		    }
		    member = member->next;
		}

		// unlock conference
		ast_mutex_unlock( &conf->lock );

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return found;
}

unsigned long timeval_to_millis(struct timeval tv)
{
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// All the VAD-based video switching magic happens here
// Returns the new video source id
int do_VAD_switching(struct ast_conference *conf)
{
	struct ast_conf_member *member;
	
	for ( member = conf->memberlist ;
	      member != NULL ;
	      member = member->next )
	{
		if ( member->mute_video ) continue;
		// Has the state changed since last time through this loop?
		if ( member->speaking_state_notify != member->speaking_state_prev )
		{
			fprintf(stderr, "Mihai: member %s has changed state to %s at timestamp %ld\n", 
				member->channel_name, 
				((member->speaking_state_notify == 1 ) ? "speaking" : "silent"),
				timeval_to_millis(member->last_state_change)
			       );
			
		}
	}
	return conf->default_video_source_id;
}

