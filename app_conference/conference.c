
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

#include "conference.h"

//
// static variables
//

// main app_conference mutex
static ast_mutex_t conflock = AST_MUTEX_INITIALIZER ;

// single-linked list of current conferences
static struct ast_conference *conflist = NULL ;

//
// main conference function
//

void conference_exec( struct ast_conference *conf ) 
{

	struct ast_conf_member *member, *temp_member ;
	struct conf_frame *cfr, *spoken_frames, *send_frames ;
	
	// count number of speakers, number of listeners
	int speaker_count ;
	int listener_count ;
	
	ast_log( AST_CONF_DEBUG, "entered conference_exec, name => %s\n", conf->name ) ;
	
	// timer timestamps
	struct timeval base, curr, notify ;
	gettimeofday( &base, NULL ) ;
	gettimeofday( &notify, NULL ) ;

	// holds differences of curr and base
	long time_diff = 0 ;
	
	// number of frames we need to process to keep up
	int step = 0 ;

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

		//
		// determine if it's time to process new frames
		// ( skip time comparison if we're behind a step )
		//
		if ( step <= 0 )
		{
			// calculate difference in timestamps
			time_diff = usecdiff( &curr, &base ) ;

			// determine number of frames we should have processed since last time through
			// ( use floor, since we only want interger values >= 1 )
			step = ( int )( floor( (double)( time_diff / AST_CONF_SAMPLE_FREQUENCY ) ) ) ;

			// if the number of frames is not postive, sleep for a bit
			if ( step <= 0 )
			{
				// convert constant to microseconds for usleep()
				usleep( AST_CONF_CONFERENCE_SLEEP * 1000 ) ;
				continue ;
			}
						
			// adjust the timer base
			add_milliseconds( &base, ( AST_CONF_SAMPLE_FREQUENCY * step ) ) ;
		}
	
		// decrement step to mark reading of frame
		--step ;
		
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
				( tf_frequency <= ( float )( AST_CONF_SAMPLE_FREQUENCY - 1 ) )
				|| ( tf_frequency >= ( float )( AST_CONF_SAMPLE_FREQUENCY + 1 ) )
			)
			{
				ast_log( 
					LOG_WARNING, 
					"processed frame frequency variation, name => %s, tf_count => %d, tf_diff => %ld, tf_frequency => %2.4f\n",
					conf->name, tf_count, tf_diff, tf_frequency
				) ;
			}

			// reset values 
			copy_timeval( &tf_base, &tf_curr ) ;
			tf_count = 0 ;
		}

		//-----------------//
		// INCOMING FRAMES //
		//-----------------//

		// ast_log( AST_CONF_DEBUG, "PROCESSING FRAMES, conference => %s, step => %d, ms => %ld\n", 
		//	conf->name, step, ( base.tv_usec / 20000 ) ) ;

		// acquire conference mutex
		ast_mutex_lock( &conf->lock ) ;

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

		// loop over member list to retrieve queued frames
		while ( member != NULL )
		{
			// acquire member mutex
			ast_mutex_lock( &member->lock ) ;

			// check for dead members
			if ( member->remove_flag == 1 ) 
			{
				ast_log( LOG_NOTICE, "found member slated for removal, channel => %s\n", member->channel_name ) ;
				temp_member = member->next ;
				remove_member( member, conf ) ;
				member = temp_member ;
				continue ;
			}
			
			// get speaking member's audio frames,
			if ( member->type == 'L' )
			{
				// listeners never have frames
				cfr = NULL ;
			}
			else
			{
				// non-listener member should have frames,
				// unless silence detection dropped them
				cfr = get_incoming_frame( member ) ;
			}

			// handle retrieved frames
			if ( cfr == NULL ) 
			{
				// this member is listen-only, or has not spoken
				// ast_log( AST_CONF_DEBUG, "silent member, channel => %s\n", member->channel_name ) ;
				
				// mark member as silent
				member->speaking_state = 0 ;
				
				// count the listeners
				++listener_count ;
			}
			else 
			{
				// this speaking member has spoken
				// ast_log( AST_CONF_DEBUG, "speaking member, channel => %s\n", member->channel_name ) ;
				
				// append the frame to the list of spoken frames
				if ( spoken_frames != NULL ) 
				{
					// add new frame to end of list
					cfr->next = spoken_frames ;
					spoken_frames->prev = cfr ;
				}

				// point the list at the new frame
				spoken_frames = cfr ;
				
				// mark member as speaker
				member->speaking_state = 1 ;
				member->speaking_state_notify = 1 ;
				
				// count the speakers
				++speaker_count ;
			}

			// release member mutex
			ast_mutex_unlock( &member->lock ) ;

			// adjust our pointer to the next inline
			member = member->next ;
		} 

		//
		// break, if we have no more members
		//

		if ( conf->membercount == 0 ) 
		{
			ast_log( LOG_NOTICE, "removing conference, count => %d, name => %s\n", conf->membercount, conf->name ) ;
			remove_conf( conf ) ; // remove the conference
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
			conf->frames_in++ ;
			
		//-----------------//
		// OUTGOING FRAMES //
		//-----------------//

		//
		// queue send frames
		//
		
		// ast_log( AST_CONF_DEBUG, "begin queueing outgoing audio, name => %s\n", conf->name ) ;
		
		//
		// loop over member list to queue outgoing frames
		//
		for ( member = conf->memberlist ; member != NULL ; member = member->next ) 
		{			
			// skip members that are not ready
			if ( member->ready_for_outgoing == 0 )
				continue ;
		
			if ( member->speaking_state == 0 )
			{
				// queue listener frame
				queue_frame_for_listener( conf, member, send_frames ) ;
			}
			else
			{
				// queue speaker frame
				queue_frame_for_speaker( conf, member, send_frames ) ;
			}
		}

		// ast_log( AST_CONF_DEBUG, "end queueing outgoing audio, name => %s\n", conf->name ) ;

		//---------//
		// CLEANUP //
		//---------//

		// clean up send frames		
		while ( send_frames != NULL )
		{		
			// accouting: count all frames and mixed frames
			if ( send_frames->member == NULL )
				conf->frames_out++ ;
			else
				conf->frames_mixed++ ;
			
			// delete the frame
			send_frames = delete_conf_frame( send_frames ) ;
		}

		//
		// notify the manager of state changes every 500 milliseconds
		//
		
		if ( ( usecdiff( &curr, &notify ) / AST_CONF_NOTIFICATION_SLEEP ) >= 1 )
		{
			// send the notifications
			send_state_change_notifications( conf->memberlist ) ;
		
			// increment the notification timer base
			add_milliseconds( &notify, AST_CONF_NOTIFICATION_SLEEP ) ;
		}

		// release conference mutex
		ast_mutex_unlock( &conf->lock ) ;
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
void init_conflock( void ) 
{
	ast_mutex_init( &conflock ) ;
}

struct ast_conference* setup_member_conference( struct ast_conf_member* member ) 
{
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to handle null member\n" ) ;
		return NULL ;
	}

	struct ast_conference* conf = NULL ;

	// acquire module mutex
	ast_mutex_lock( &conflock ) ;

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
			ast_mutex_unlock( &conflock ) ; // release module mutex
			return NULL ;
		}
	}
	else
	{
		// existing conference found, add new member to the conference
	
		// acquire the conference lock
		ast_mutex_lock( &conf->lock ) ;
	
		// add the member to the conference
		//
		// once we call add_member(), this thread
		// is responsible for calling delete_member()
		//
		add_member( member, conf ) ;
		
		// release the conference lock
		ast_mutex_unlock( &conf->lock ) ;	
	}

	// release module mutex
	ast_mutex_unlock( &conflock ) ;

	return conf ;
}


struct ast_conference* find_conf( char* name ) 
{	
	// no conferences exist
	if ( conflist == NULL ) 
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return NULL ;
	}
	
	struct ast_conference *conf = conflist ;
	
	// loop through conf list
	while ( conf != NULL ) 
	{
		if ( strncasecmp( (char*)&(conf->name), name, 80 ) == 0 )
		{
			// found conf name match 
			ast_log( AST_CONF_DEBUG, "found conference in conflist, name => %s\n", name ) ;
			return conf ;
		}
	
		conf = conf->next ;
	}

	ast_log( AST_CONF_DEBUG, "unable to find conference in conflist, name => %s\n", name ) ;

	return NULL ;
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
	conf->audiothread = -1 ;

	// account data
	conf->frames_in = 0 ;
	conf->frames_out = 0 ;
	conf->frames_mixed = 0 ;
	
	// record start time
	gettimeofday( &conf->time_entered, NULL ) ;

	// copy name to conference
	strncpy( (char*)&(conf->name), name, sizeof(conf->name) - 1 ) ;
	
	// initialize mutexes
	ast_mutex_init( &conf->lock ) ;
	ast_mutex_init( &conf->threadlock ) ;
	
	// build translation paths	
	conf->from_slinear_paths[ AC_SLINEAR_INDEX ] = NULL ;
	conf->from_slinear_paths[ AC_ULAW_INDEX ] = ast_translator_build_path( AST_FORMAT_ULAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_GSM_INDEX ] = ast_translator_build_path( AST_FORMAT_GSM, AST_FORMAT_SLINEAR ) ;

	// add the initial member
	add_member( member, conf ) ;
	
	//
	// prepend new conference to conflist
	//

	conf->next = conflist ;
	conflist = conf ;

	ast_log( AST_CONF_DEBUG, "added new conference to conflist, name => %s\n", name ) ;

	//
	// spawn thread for new conference, using conference_exec( conf )
	//

	// acquire conference mutexes
	ast_mutex_lock( &conf->lock ) ;
	ast_mutex_lock( &conf->threadlock ) ;
	
	if ( pthread_create( &conf->audiothread, NULL, (void*)conference_exec, conf ) == 0 ) 
	{
		// detach the thread so it doesn't leak
		pthread_detach( conf->audiothread ) ;
	
		// release conference mutexes
		ast_mutex_unlock( &conf->threadlock ) ;
		ast_mutex_unlock( &conf->lock ) ;

		ast_log( AST_CONF_DEBUG, "started audiothread for conference, name => %s\n", conf->name ) ;
	}
	else
	{
		ast_log( LOG_ERROR, "unable to start audiothread for conference %s\n", conf->name ) ;
		
		conf->audiothread = -1 ;

		// release conference mutexes
		ast_mutex_unlock( &conf->threadlock ) ;
		ast_mutex_unlock( &conf->lock ) ;

		// clean up conference
		free( conf ) ;
		conf = NULL ;
	}

	return conf ;
}


void remove_conf( struct ast_conference *conf )
{
	// ast_log( AST_CONF_DEBUG, "attempting to remove conference, name => %s\n", conf->name ) ;

	// acquire module mutex
	ast_mutex_lock( &conflock ) ;

	struct ast_conference *conf_current = conflist ;
	struct ast_conference *conf_temp = NULL ;

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

			// terminate the notification thread
			// pthread_join( (pthread_t)&(conf->notification_thread), NULL ) ;

			//
			// do some frame clean up
			//
		
			for ( int c = 0 ; c < AC_SUPPORTED_FORMATS ; ++c )
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
			long tt = ( usecdiff( &time_exited, &conf_current->time_entered ) / 1000 ) ;
	
			// report accounting information
			ast_log( LOG_NOTICE, "conference accounting, fi => %ld, fo => %ld, fm => %ld, tt => %ld\n",
				conf_current->frames_in, conf_current->frames_out, conf_current->frames_mixed, tt ) ;

			ast_log( AST_CONF_DEBUG, "removed conference, name => %s\n", conf_current->name ) ;

			ast_mutex_unlock( &conf_current->lock ) ;
			
			free( conf_current ) ;
			conf_current = NULL ;

			// release module mutex
			ast_mutex_unlock( &conflock ) ;
			
			return ;
		}

		// save a refence to the soon to be previous conf
		conf_temp = conf_current ;
		
		// move conf_current to the next in the list
		conf_current = conf_current->next ;
	}
	
	// ast_log( AST_CONF_DEBUG, "unable to remove conference, name => %s\n", conf->name ) ;
	
	// release module mutex
	ast_mutex_unlock( &conflock ) ;
	
	return ;
}

//
// member-related functions
//

void add_member( struct ast_conf_member *member, struct ast_conference *conf ) 
{
	if ( conf == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to add member to NULL conference\n" ) ;
		return ;
	}
	
	member->next = conf->memberlist ; // next is now list
	conf->memberlist = member ; // member is now at head of list

	// increment member count
	conf->membercount++ ;

	ast_log( AST_CONF_DEBUG, "member added to conference, name => %s\n", conf->name ) ;
	
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

	struct ast_conf_member *member_list = conf->memberlist ;
	struct ast_conf_member *member_temp = NULL ;
	
	int count = -1 ; // default return code

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

			ast_log( 
				LOG_NOTICE, 
				"member accounting, channel => %s, te => %ld, fi => %ld, fid => %ld, fo => %ld, fod => %ld, tt => %ld\n",
				member->channel_name,
				member->time_entered.tv_sec, member->frames_in, member->frames_in_dropped, 
				member->frames_out, member->frames_out_dropped, tt 
			) ;

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

			// delete the member
			delete_member( member ) ;

			conf->membercount-- ;
			count = conf->membercount ; // so we can return the new count
			
			ast_log( AST_CONF_DEBUG, "removed member from conference, name => %s, remaining => %d\n", conf->name, conf->membercount ) ;
			
			break ;
		}
		
		// save a pointer to the current member,
		// and then point to the next member in the list
		member_temp = member_list ;
		member_list = member_list->next ;
	}
	
	// return -1 on error, or the number of members 
	// remaining if the requested member was deleted
	return count ;
}

//
// queue incoming frame functions
//

int queue_frame_for_speaker( 
	struct ast_conference* conf, 
	struct ast_conf_member* member, 
	struct conf_frame* frame
)
{
	//
	// check inputs
	//
	
	if ( conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to queue listener frame with null conference\n" ) ;
		return -1 ;
	}
	
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to queue listener frame with null member\n" ) ;
		return -1 ;
	}
	
	//
	// loop over spoken frames looking for member's appropriate match
	//
	
	short found_flag = 0 ;
	struct ast_frame* qf ;
	
	for ( ; frame != NULL ; frame = frame->next ) 
	{
		if ( frame->member != member )
			continue ;

		// acquire member lock
		ast_mutex_lock( &member->lock ) ;
		
		// copy the frame before converting it
		qf = ast_frdup( frame->fr ) ;

		if ( qf == NULL )
		{
			ast_log( LOG_WARNING, "unable to dupicate frame\n" ) ;
			continue ;
		}

		// convert frame, if necessary
		if ( qf->subclass == member->write_format )
		{
			// frame is already in correct format
		}
		else
		{
			// convert frame to member's write format
			qf = convert_frame_from_slinear( member->from_slinear, qf ) ;
		}
		
		if ( qf != NULL )
		{
			if ( queue_outgoing_frame( member, qf ) != 0 )
			{
				// free the frame, if we can't queue it
				ast_frfree( qf ) ;
				qf = NULL ;
			}
		}
		else
		{
			ast_log( LOG_WARNING, "unable to translate outgoing speaker frame, channel => %s\n", member->channel_name ) ;
		}

		// release member lock
		ast_mutex_unlock( &member->lock ) ;
		
		// set found flag
		found_flag = 1 ;
		
		// we found the frame, skip to the next member
		break ;
	}
	
	// queue a silent frame
	if ( found_flag == 0 )
		queue_silent_frame( conf, member ) ;
	
	return 0 ;
}

int queue_frame_for_listener( 
	struct ast_conference* conf, 
	struct ast_conf_member* member, 
	struct conf_frame* frame
)
{
	//
	// check inputs
	//
	
	if ( conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to queue listener frame with null conference\n" ) ;
		return -1 ;
	}
	
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to queue listener frame with null member\n" ) ;
		return -1 ;
	}

	//
	// loop over spoken frames looking for member's appropriate match
	//

	short found_flag = 0 ;
	struct ast_frame* qf ;

	for ( ; frame != NULL ; frame = frame->next ) 
	{
		// we're looking for a null or matching member
		if ( frame->member != NULL && frame->member != member )
			continue ;
	
		if ( frame->fr == NULL )
		{
			ast_log( LOG_WARNING, "unknown error queueing frame for listener, frame->fr == NULL\n" ) ;
			continue ;
		}

		// acquire member lock
		ast_mutex_lock( &member->lock ) ;

		// first, try for a pre-converted frame
		qf = frame->converted[ member->write_format_index ] ;
	
		// convert ( and store ) the frame
		if ( qf == NULL )
		{		
			// make a copy of the slinear version of the frame
			qf = ast_frdup( frame->fr ) ;	
			
			if ( qf == NULL )
			{
				ast_log( LOG_WARNING, "unable to duplicate frame\n" ) ;
				continue ;
			}
			
			// convert using the conference's translation path
			qf = convert_frame_from_slinear( conf->from_slinear_paths[ member->write_format_index ], qf ) ;
			
			// store the converted frame
			// ( the frame will be free'd next time through the loop )
			frame->converted[ member->write_format_index ] = qf ;
		}

		if ( qf != NULL )
		{
			// duplicate the frame before queue'ing it
			// ( since this member doesn't own this _shared_ frame )
			qf = ast_frdup( qf ) ;
			
			if ( queue_outgoing_frame( member, qf ) != 0 )
			{
				// free the new frame if it couldn't be queue'd
				ast_frfree( qf ) ;
				qf = NULL ;
			}
		}
		else
		{
			ast_log( LOG_WARNING, "unable to translate outgoing listener frame, channel => %s\n", member->channel_name ) ;
		}

		// release member lock
		ast_mutex_unlock( &member->lock ) ;
		
		// set found flag
		found_flag = 1 ;
		
		// break from for loop
		break ;
	}
	
	// queue a silent frame
	if ( found_flag == 0 )
		queue_silent_frame( conf, member ) ;

	return 0 ;
}

int queue_silent_frame( 
	struct ast_conference* conf, 
	struct ast_conf_member* member
)
{	
#ifdef APP_CONFERENCE_DEBUG
	//
	// check inputs
	//
	
	if ( conf == NULL )
	{
		ast_log( AST_CONF_DEBUG, "unable to queue silent frame for null conference\n" ) ;
		return -1 ;
	}
	
	if ( member == NULL )
	{
		ast_log( AST_CONF_DEBUG, "unable to queue silent frame for null member\n" ) ;
		return -1 ;
	}
#endif // APP_CONFERENCE_DEBUG

	//
	// initialize static variables
	//

	static struct conf_frame* silent_frame = NULL ;
	static struct ast_frame* qf = NULL ;

	if ( silent_frame == NULL )
	{
		if ( ( silent_frame = get_silent_frame() ) == NULL )
		{
			ast_log( LOG_WARNING, "unable to initialize static silent frame\n" ) ;
			return -1 ;
		}
	}	

	// acquire member lock
	ast_mutex_lock( &member->lock ) ;

	// get the appropriate silent frame
	qf = silent_frame->converted[ member->write_format_index ] ;
	
	if ( qf == NULL )
	{
		//
		// we need to do this to avoid echo on the speaker's line.
		// translators seem to be single-purpose, i.e. they
		// can't be used simultaneously for multiple audio streams
		//
	
		struct ast_trans_pvt* trans = ast_translator_build_path( member->write_format, AST_FORMAT_SLINEAR ) ;
	
		if ( trans != NULL )
		{
			// attempt ( five times ) to get a silent frame
			// to make sure we provice the translator with enough data 
			for ( int c = 0 ; c < 5 ; ++c )
			{
				// translate the frame
				qf = ast_translate( trans, silent_frame->fr, 0 ) ;
				
				// break if we get a frame
				if ( qf != NULL ) break ;
			}
			
			if ( qf != NULL )
			{
				// isolate the frame so we can keep it around after trans is free'd
				qf = ast_frisolate( qf ) ;
			
				// cache the new, isolated frame
				silent_frame->converted[ member->write_format_index ] = qf ;
			}
			
			ast_translator_free_path( trans ) ;
		}
	}
	
	//
	// queue the frame, if it's not null, 
	// otherwise there was an error
	//
	if ( qf != NULL )
	{
		// dup the frame befoe queue'ing it
		// ( we dup the frame because member_exec() will free it )
		qf = ast_frdup( qf ) ;
	
		if ( queue_outgoing_frame( member, qf ) != 0 )
		{
			// free the frame, if we can't queue it						
			ast_frfree( qf ) ;
			qf = NULL ;
		}
	}
	else
	{
		ast_log( LOG_ERROR, "unable to translate outgoing silent frame, channel => %s\n", member->channel_name ) ;
	}
	
	// release member lock
	ast_mutex_unlock( &member->lock ) ;

	return 0 ;
}
