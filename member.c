
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

#include "member.h"

//
// main member thread function
//

int member_exec( struct ast_channel* chan, void* data )
{
//	struct timeval start, end ;
//	gettimeofday( &start, NULL ) ;

	struct ast_conference *conf ;
	struct ast_conf_member *member ;

	struct ast_frame *f ; // frame received from ast_read()
	conf_frame* cf ; // frame read from the output queue

	int left = 0 ;
	int res;
	
	ast_log( AST_CONF_DEBUG, "[ $Revision$ ] begin processing member thread, channel => %s\n", chan->name ) ;
	
	// 
	// If the call has not yet been answered, answer the call
	// Note: asterisk apps seem to check _state, but it seems like it's safe
	// to just call ast_answer.  It will just do nothing if it is up.
	// it will also return -1 if the channel is a zombie, or has hung up.
	//
	
	res = ast_answer( chan ) ;
	if ( res ) 
	{
		ast_log( LOG_ERROR, "unable to answer call\n" ) ;
		return -1 ;
	}

	//
	// create a new member for the conference
 	//
	
//	ast_log( AST_CONF_DEBUG, "creating new member, id => %s, flags => %s, p => %s\n", 
//		id, flags, priority ) ;
	
	member = create_member( chan, (const char*)( data ) ) ; // flags, atoi( priority ) ) ;
	
	// unable to create member, return an error
	if ( member == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to create member\n" ) ;
		return -1 ;
	} 

	//
	// setup asterisk read/write formats
	//
	
	ast_log( AST_CONF_DEBUG, "CHANNEL INFO, CHANNEL => %s, DNID => %s, CALLER_ID => %s, ANI => %s\n", 
		chan->name, chan->dnid, chan->callerid, chan->ani ) ;

	ast_log( AST_CONF_DEBUG, "CHANNEL CODECS, CHANNEL => %s, NATIVE => %d, READ => %d, WRITE => %d\n", 
		chan->name, chan->nativeformats, member->read_format, member->write_format ) ;

	if ( ast_set_read_format( chan, member->read_format ) < 0 )
	{
		ast_log( LOG_ERROR, "unable to set read format to signed linear\n" ) ;
		delete_member( member ) ;
		return -1 ;
	} 

	// for right now, we'll send everything as slinear
	if ( ast_set_write_format( chan, member->write_format ) < 0 ) // AST_FORMAT_SLINEAR, chan->nativeformats
	{
		ast_log( LOG_ERROR, "unable to set write format to signed linear\n" ) ;
		delete_member( member ) ;
		return -1 ;
	}

	//
	// setup a conference for the new member
	//

	conf = start_conference( member ) ;
		
	if ( conf == NULL )
	{
		ast_log( LOG_ERROR, "unable to setup member conference\n" ) ;
		delete_member( member) ;
		return -1 ;
	}
		
	//
	// variables for checking thread frequency
	//

	// frequency of frames received
	int fr_count = 0 ;
	long fr_diff = 0 ;
	float fr_frequency = 0.0 ;

	struct timeval fr_base, fr_curr ;
	gettimeofday( &fr_base, NULL ) ;

	// frequency of frames sent
	int fs_count = 0 ;
	long fs_diff = 0 ;
	float fs_rate = 0.0 ;

	struct timeval fs_base, fs_curr ;
	gettimeofday( &fs_base, NULL ) ;

/*
	// !!! TESTING !!!
	char* incoming_fn = malloc( 512 ) ;
	snprintf( incoming_fn, 512, "/tmp/ac_%s_%ld.gsm", chan->dnid, fr_base.tv_usec ) ;

	// !!! TESTING !!!
	FILE* incoming_fh = ( member->read_format == 2 ) 
		? fopen( incoming_fn, "wb" )
		: NULL 
	;

	// !!! TESTING !!!
	if ( incoming_fh == NULL )
	{
		ast_log( AST_CONF_DEBUG, "incoming_fh is null, incoming_fn => %s\n", incoming_fn ) ;
	}
	else
	{
		ast_log( AST_CONF_DEBUG, "incoming_fh is not null, incoming_fn => %s\n", incoming_fn ) ;
	}

	// !!! TESTING !!!
	free( incoming_fn ) ;
*/

	//
	// process loop for new member ( this runs in it's own thread
	//
	
	ast_log( AST_CONF_DEBUG, "begin member event loop, channel => %s\n", chan->name ) ;
	
	// timer timestamps
	struct timeval base, curr ;
	gettimeofday( &base, NULL ) ;

	// number of frames to ignore speex_preprocess()
	int ignore_speex_count = 0 ;

	// silence detection flag
	int silent_frame = 0 ;

	// tell conference_exec we're ready for frames
	member->ready_for_outgoing = 1 ;
	while ( 42 == 42 )
	{
		// make sure we have a channel to process
		if ( chan == NULL )
		{
			ast_log( LOG_NOTICE, "member channel has closed\n" ) ;
			break ;
		}

		//-----------------//
		// INCOMING FRAMES //
		//-----------------//
		
		// wait for an event on this channel
		left = ast_waitfor( chan, AST_CONF_WAITFOR_LATENCY ) ;

		// ast_log( AST_CONF_DEBUG, "received event on channel, name => %s, rest => %d\n", chan->name, rest ) ;
		
		if ( left < 0 )
		{
			// an error occured	
			ast_log( 
				LOG_NOTICE, 
				"an error occured waiting for a frame, channel => %s, error => %d\n", 
				chan->name, left
			) ;
		}
		else if ( left == 0 )
		{
			// no frame has arrived yet
			// ast_log( LOG_NOTICE, "no frame available from channel, channel => %s\n", chan->name ) ;
		}
		else if ( left > 0 ) 
		{
			// a frame has come in before the latency timeout 
			// was reached, so we process the frame

			f = ast_read( chan ) ;
			
			if ( f == NULL ) 
			{
				ast_log( LOG_NOTICE, "unable to read from channel, channel => %s\n", chan->name ) ;
				break ;
			}

			if ( member->type == 'L' )
			{
				// this is a listen-only user, ignore the frame
				ast_frfree( f ) ;
				f = NULL ;
			}
			else if ( f->frametype == AST_FRAME_VOICE ) 
			{			
				// reset silence detection flag
				silent_frame = 0 ;
				
				// accounting: count the incoming frame
				member->frames_in++ ;

/*
				// !!! TESTING !!!
				if ( incoming_fh != NULL )
				{
					fwrite( f->data, f->datalen, 1, incoming_fh ) ;
					fflush( incoming_fh ) ;
				}
*/
						
#if ( SILDET == 2 )
				// 
				// make sure we have a valid dsp and frame type
				//
				if ( 
					member->dsp != NULL
					&& f->subclass == AST_FORMAT_SLINEAR 
					&& f->datalen == AST_CONF_FRAME_DATA_SIZE
				)
				{
					// send the frame to the preprocessor
#if 1
					int spx_ret;
					TIMELOG(spx_ret = speex_preprocess( member->dsp, f->data, NULL ), 3, "speex_preprocess"); 
					if ( spx_ret == 0 )
#else
					if ( speex_preprocess( member->dsp, f->data, NULL ) == 0 )
#endif
					{
						//
						// we ignore the preprocessor's outcome if we've seen voice frames 
						// in within the last AST_CONF_SKIP_SPEEX_PREPROCESS frames
						//
						if ( ignore_speex_count > 0 )
						{
							// ast_log( AST_CONF_DEBUG, "ignore_speex_count => %d\n", ignore_speex_count ) ;
						
							// skip speex_preprocess(), and decrement counter
							--ignore_speex_count ;
						}
						else
						{
							// set silent_frame flag
							silent_frame = 1 ;
						}
					}
					else
					{
						// voice detected, reset skip count
						ignore_speex_count = AST_CONF_SKIP_SPEEX_PREPROCESS ;
					}
				}
#endif

				// used to debug drop off in received frames
//				struct timeval tv ;
//				gettimeofday( &tv, NULL ) ;

				if ( silent_frame == 1 ) 
				{
					// ignore silent frames
//					ast_log( AST_CONF_DEBUG, "RECEIVED SILENT FRAME, channel => %s, frames_in => %ld, s => %ld, ms => %ld\n", 
//						member->channel_name, member->frames_in, tv.tv_sec, tv.tv_usec ) ;
				} 
				else 
				{				
					// queue a non-silent frame for mixing

//					ast_log( AST_CONF_DEBUG, "RECEIVED VOICE FRAME, channel => %s, frames_in => %ld, s => %ld, ms => %ld\n", 
//						member->channel_name, member->frames_in, tv.tv_sec, tv.tv_usec ) ;

					// acquire member lock
					ast_mutex_lock( &member->lock ) ;

					//
					// queue up the voice frame so the conference 
					// thread can mix them before sending
					//
							
//					struct ast_frame* af = ast_frdup( f ) ;
					
//					if ( queue_incoming_frame( member, af ) != 0 )
					if ( queue_incoming_frame( member, f ) != 0 )
					{
						// free the duplicated frame, if we can't queue it
						// ast_log( LOG_NOTICE, "dropped incoming frame, channel => %s\n", chan->name ) ;
						// ast_frfree( af ) ;
						// af = NULL ;
					}
					else
					{
						// everything is going ok
					}

					// release member mutex
					ast_mutex_unlock( &member->lock ) ;
				}

				// free the original frame
				ast_frfree( f ) ;
				f = NULL ;

				//
				// check frequency of received frames
				//
				
				if ( ++fr_count >= AST_CONF_FRAMES_PER_SECOND )
				{
					// update current timestamp	
					gettimeofday( &fr_curr, NULL ) ;

					// compute timestamp difference
					fr_diff = usecdiff( &fr_curr, &fr_base ) ;
		
					// compute sampling frequency
					fr_frequency = ( float )( fr_diff ) / ( float )( fr_count ) ;
		
					if ( 
						( fr_frequency <= ( float )( AST_CONF_FRAME_INTERVAL - AST_CONF_INTERVAL_WARNING ) )
						|| ( fr_frequency >= ( float )( AST_CONF_FRAME_INTERVAL + AST_CONF_INTERVAL_WARNING ) )
					)
					{
						ast_log( 
							LOG_WARNING, 
							"received frame frequency variation, channel => %s, fr_count => %d, fr_diff => %ld, fr_frequency => %2.2f\n",
							member->channel_name, fr_count, fr_diff, fr_frequency
						) ;
					}

					// reset values 
					fr_base = fr_curr ;
					fr_count = 0 ;
				}
			}
			else if ( 
				f->frametype == AST_FRAME_CONTROL
				&& f->subclass == AST_CONTROL_HANGUP 
			) 
			{
				// hangup received
				
				// free the frame 
				ast_frfree( f ) ;
				f = NULL ;
				
				// break out of the while ( 42 == 42 )
				break ;
			}
			else
			{
				// undesirables
				ast_frfree( f ) ;
				f = NULL ;
			}
		}
		
		//-----------------//
		// OUTGOING FRAMES //
		//-----------------//

		// update the current timestamps
		gettimeofday( &curr, NULL ) ;


		for(;;)
		{	
			// acquire member mutex and grab a frame.
			ast_mutex_lock( &member->lock ) ;
			cf = get_outgoing_frame( member ) ;
			ast_mutex_unlock( &member->lock ) ;

			// if there's no frames exit the loop.
			if(!cf) break;
			
			
			// !!! TESTING !!!
			if ( 	
				cf->fr->delivery.tv_sec < member->lastsent_timeval.tv_sec 
				|| ( 
					cf->fr->delivery.tv_sec == member->lastsent_timeval.tv_sec 
					&& cf->fr->delivery.tv_usec <= member->lastsent_timeval.tv_usec 
				)
			)
			{
				ast_log( LOG_WARNING, "queued frame timestamped in the past, %ld.%ld <= %ld.%ld\n",
					cf->fr->delivery.tv_sec, cf->fr->delivery.tv_usec, 
					member->lastsent_timeval.tv_sec, member->lastsent_timeval.tv_usec ) ;
			}
			member->lastsent_timeval = cf->fr->delivery ;


#if 1
			TIMELOG( ast_write( member->chan, cf->fr ), 10, "member: ast_write");
#else
			// send the voice frame
			if ( ast_write( member->chan, cf->fr ) == 0 )
			{
				// struct timeval tv ;
				// gettimeofday( &tv, NULL ) ;
				// ast_log( AST_CONF_DEBUG, "SENT VOICE FRAME, channel => %s, frames_out => %ld, s => %ld, ms => %ld\n", 
				//	member->channel_name, member->frames_out, tv.tv_sec, tv.tv_usec ) ;
			}
			else
			{
				// log 'dropped' outgoing frame
				ast_log( LOG_ERROR, "unable to write voice frame to channel, channel => %s\n", member->channel_name ) ;

				// accounting: count dropped outgoing frames
				member->frames_out_dropped++ ;
			}
#endif


			// clean up frame
			delete_conf_frame( cf ) ;

			//
			// check frequency of sent frames
			//
			
			if ( ++fs_count >= AST_CONF_FRAMES_PER_SECOND )
			{
				// update current timestamp	
				gettimeofday( &fs_curr, NULL ) ;
	
				// compute timestamp difference
				fs_diff = usecdiff( &fs_curr, &fs_base ) ;
	
				// compute sampling frequency
				fs_rate = ( float )( fs_diff ) / ( float )( fs_count ) ;
	
				if ( 
					( fs_rate <= ( float )( AST_CONF_FRAME_INTERVAL - AST_CONF_INTERVAL_WARNING ) )
					|| ( fs_rate >= ( float )( AST_CONF_FRAME_INTERVAL + AST_CONF_INTERVAL_WARNING ) )
				)
				{
					ast_log( 
						LOG_WARNING, 
						"sent frames frequency variation, channel => %s, fs_count => %d, fs_diff => %ld, fs_rate => %2.2f ( ms/fr )\n",
						member->channel_name, fs_count, fs_diff, fs_rate 
					) ;
				}
	
				// reset values 
				fs_base = fs_curr ;
				fs_count = 0 ;
			}			
		}

		// back to process incoming frames
		continue ;
	}

	ast_log( AST_CONF_DEBUG, "end member event loop, time_entered => %ld\n", member->time_entered.tv_sec ) ;
	
	//
	// clean up
	//

/*
	// !!! TESTING !!!	
	if ( incoming_fh != NULL )
		fclose( incoming_fh ) ;
*/

	if ( member != NULL ) member->remove_flag = 1 ;

//	gettimeofday( &end, NULL ) ;
//	int expected_frames = ( int )( floor( (double)( usecdiff( &end, &start ) / AST_CONF_FRAME_INTERVAL ) ) ) ;
//	ast_log( AST_CONF_DEBUG, "expected_frames => %d\n", expected_frames ) ;

	return -1 ;
}

//
// manange member functions
//

struct ast_conf_member* create_member( struct ast_channel *chan, const char* data ) 
{
	//
	// check input
	//
	
	if ( chan == NULL )
	{
		ast_log( LOG_ERROR, "unable to create member with null channel\n" ) ;
		return NULL ;
	}
	
	if ( chan->name == NULL )
	{
		ast_log( LOG_ERROR, "unable to create member with null channel name\n" ) ;
		return NULL ;
	}
	
	//
	// allocate memory for new conference member
	//

	struct ast_conf_member *member = malloc( sizeof( struct ast_conf_member ) ) ;
	
	if ( member == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to malloc ast_conf_member\n" ) ;
		return NULL ;
	}
	
	// initialize mutex
	ast_mutex_init( &member->lock ) ;

	//
	// initialize member with passed data values
	//
	
	char argstr[80] ;
	char *stringp, *token ;

	// copy the passed data
	strncpy( argstr, data, sizeof(argstr) - 1 ) ;

	// point to the copied data
	stringp = argstr ;
	
	ast_log( AST_CONF_DEBUG, "attempting to parse passed params, stringp => %s\n", stringp ) ;
	
	// parse the id
	if ( ( token = strsep( &stringp, "/" ) ) != NULL )
	{
		member->id = malloc( strlen( token ) + 1 ) ;
		strcpy( member->id, token ) ;
	}
	else
	{
		ast_log( LOG_ERROR, "unable to parse member id\n" ) ;
		free( member ) ;
		return NULL ;
	}

	// parse the flags
	if ( ( token = strsep( &stringp, "/" ) ) != NULL )
	{
		member->flags = malloc( strlen( token ) + 1 ) ;
		strcpy( member->flags, token ) ;
	}
	else
	{
		// make member->flags something 
		member->flags = malloc( sizeof( char ) ) ;
		memset( member->flags, 0x0, sizeof( char ) ) ;
	}
	
	// parse the priority
	member->priority = ( token = strsep( &stringp, "/" ) ) != NULL
		? atoi( token ) 
		: 0
	;

	// parse the vad_prob_start
	member->vad_prob_start = ( token = strsep( &stringp, "/" ) ) != NULL
		? atof( token ) 
		: AST_CONF_PROB_START
	;
	
	// parse the vad_prob_continue
	member->vad_prob_continue = ( token = strsep( &stringp, "/" ) ) != NULL
		? atof( token ) 
		: AST_CONF_PROB_CONTINUE
	;

	// debugging
	ast_log( 
		AST_CONF_DEBUG, 
		"parsed data params, id => %s, flags => %s, priority => %d, vad_prob_start => %f, vad_prob_continue => %f\n",
		member->id, member->flags, member->priority, member->vad_prob_start, member->vad_prob_continue
	) ;

	//
	// initialize member with default values
	//

	// keep pointer to member's channel
	member->chan = chan ;
	
	// copy the channel name
	member->channel_name = malloc( strlen( chan->name ) + 1 ) ;
	strcpy( member->channel_name, chan->name ) ;
			
	// ( default can be overridden by passed flags )
	member->type = 'L' ;

	// ready flag
	member->ready_for_outgoing = 0 ;

	// incoming frame queue
	member->inFrames = NULL ;
	member->inFramesTail = NULL ;
	member->inFramesCount = 0 ;

	member->inFramesRepeatLast = 0 ;
	member->inFramesLast = NULL ; 

	// outgoing frame queue
	member->outFrames = NULL ;
	member->outFramesTail = NULL ;
	member->outFramesCount = 0 ;

	// ( not currently used )
	// member->samplesperframe = AST_CONF_BLOCK_SAMPLES ;

	// used for determining need to mix frames
	// and for management interface notification
	member->speaking_state_prev = 0 ;
	member->speaking_state_notify = 0 ;
	member->speaking_state = 0 ;

	// linked-list pointer
	member->next = NULL ;
	
	// account data
	member->frames_in = 0 ;
	member->frames_in_dropped = 0 ;
	member->frames_out = 0 ;
	member->frames_out_dropped = 0 ;

	// for counting sequentially dropped frames
	member->sequential_drops = 0 ;
	member->since_dropped = 0 ;

	// flags
	member->remove_flag = 0 ;

	// record start time
	gettimeofday( &member->time_entered, NULL ) ;

	// init dropped frame timestamps
	gettimeofday( &member->last_in_dropped, NULL ) ;
	gettimeofday( &member->last_out_dropped, NULL ) ;

	//
	// parse passed flags
	//
	
	// silence detection flags w/ defaults
	int vad_flag = 0 ;
	int denoise_flag = 0 ;
	int agc_flag = 0 ;
	
	// is this member using the telephone?
	int via_telephone = 0 ;
	
	// temp pointer to flags string
	char* flags = member->flags ;

	for ( int i = 0 ; i < strlen( flags ) ; ++i )
	{
		// allowed flags are M, L, S, V, D, A
		switch ( flags[i] )
		{
			// call via telephone
			case 'T':
				via_telephone = 1 ;
				break ;
		
			// member types ( last flag wins )
			case 'M':
				member->type = 'M' ;
				break ;
			case 'L':
				member->type = 'L' ;
				break ;
			case 'S':
				member->type = 'S' ;
				break ;

			// speex preprocessing options
			case 'V':
				vad_flag = 1 ;
				break ;
			case 'D':
				denoise_flag = 1 ;
				break ;
			case 'A':
				agc_flag = 1 ;
				break ;

			default:
				ast_log( LOG_WARNING, "received invalid flag, chan => %s, flag => %c\n", 
					chan->name, flags[i] ) ;			
				break ;
		}
	}

	// set the dsp to null so silence detection is disabled by default
	member->dsp = NULL ;

#if ( SILDET == 2 )
	//
	// configure silence detection and preprocessing
	// if the user is coming in via the telephone, 
	// and is not listen-only
	//
	if ( 
		via_telephone == 1 
		&& member->type != 'L'
	)
	{
		// create a speex preprocessor
		member->dsp = speex_preprocess_state_init( AST_CONF_BLOCK_SAMPLES, AST_CONF_SAMPLE_RATE ) ;
		
		if ( member->dsp == NULL ) 
		{
			ast_log( LOG_WARNING, "unable to initialize member dsp, channel => %s\n", chan->name ) ;
		}
		else
		{
			ast_log( LOG_NOTICE, "member dsp initialized, channel => %s, v => %d, d => %d, a => %d\n", 
				chan->name, vad_flag, denoise_flag, agc_flag ) ;
		
			// set speex preprocessor options
			speex_preprocess_ctl( member->dsp, SPEEX_PREPROCESS_SET_VAD, &vad_flag ) ;
			speex_preprocess_ctl( member->dsp, SPEEX_PREPROCESS_SET_DENOISE, &denoise_flag ) ;
			speex_preprocess_ctl( member->dsp, SPEEX_PREPROCESS_SET_AGC, &agc_flag ) ;

			speex_preprocess_ctl( member->dsp, SPEEX_PREPROCESS_SET_PROB_START, &member->vad_prob_start ) ;
			speex_preprocess_ctl( member->dsp, SPEEX_PREPROCESS_SET_PROB_CONTINUE, &member->vad_prob_continue ) ;
			
			ast_log( AST_CONF_DEBUG, "speech_prob_start => %f, speech_prob_continue => %f\n", 
				member->dsp->speech_prob_start, member->dsp->speech_prob_continue ) ;
		}
	}
#endif

	//
	// set connection type
	//

	if ( via_telephone == 1 )
	{
		member->connection_type = 'T' ;
	}
	else if ( strncmp( member->channel_name, "SIP", 3 ) == 0 )
	{
		member->connection_type = 'S' ;
	}
	else // default to iax
	{
		member->connection_type = 'X' ;
	}

	//
	// read, write, and translation options
	//

	// set member's audio formats, taking dsp preprocessing into account
	// ( chan->nativeformats, AST_FORMAT_SLINEAR, AST_FORMAT_ULAW, AST_FORMAT_GSM )
	member->read_format = ( member->dsp == NULL ) ? chan->nativeformats : AST_FORMAT_SLINEAR ;
	member->write_format = chan->nativeformats ;
	
	// translation paths ( ast_translator_build_path() returns null if formats match )
	member->to_slinear = ast_translator_build_path( AST_FORMAT_SLINEAR, member->read_format ) ;
	member->from_slinear = ast_translator_build_path( member->write_format, AST_FORMAT_SLINEAR ) ;
	
	ast_log( AST_CONF_DEBUG, "AST_FORMAT_SLINEAR => %d\n", AST_FORMAT_SLINEAR ) ;
	
	// index for converted_frames array
	switch ( member->write_format )
	{
		case AST_FORMAT_SLINEAR:
			member->write_format_index = AC_SLINEAR_INDEX ;
			break ;
			
		case AST_FORMAT_ULAW:
			member->write_format_index = AC_ULAW_INDEX ;
			break ;
			
		case AST_FORMAT_GSM: 
			member->write_format_index = AC_GSM_INDEX ;
			break ;			
		
		default: 
			member->write_format_index = 0 ;
	}

	// index for converted_frames array
	switch ( member->read_format )
	{
		case AST_FORMAT_SLINEAR:
			member->read_format_index = AC_SLINEAR_INDEX ;
			break ;
			
		case AST_FORMAT_ULAW:
			member->read_format_index = AC_ULAW_INDEX ;
			break ;
			
		case AST_FORMAT_GSM: 
			member->read_format_index = AC_GSM_INDEX ;
			break ;			
		
		default: 
			member->read_format_index = 0 ;
	}
	
	//
	// finish up
	//
		
	ast_log( AST_CONF_DEBUG, "created member, type => %c, priority => %d, readformat => %d\n", 	
		member->type, member->priority, chan->readformat ) ;

	return member ;
}

struct ast_conf_member* delete_member( struct ast_conf_member* member ) 
{
	// !!! NO RETURN TEST !!!
	// do { sleep(1) ; } while (1) ;

	// !!! CRASH TEST !!!
	// *((int *)0) = 0;

	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to the delete null member\n" ) ;
		return NULL ;
	}
	
	//
	// clean up member flags
	//
	
	if ( member->flags != NULL )
	{
		// !!! DEBUGING !!!	
		ast_log( AST_CONF_DEBUG, "freeing member flags, name => %s\n", 
			member->channel_name ) ;
		free( member->flags ) ;
	}
	
	//
	// delete the members frames
	//

	conf_frame* cf ;

	// !!! DEBUGING !!!	
	ast_log( AST_CONF_DEBUG, "deleting member input frames, name => %s\n", 
		member->channel_name ) ;

	// incoming frames
	cf = member->inFrames ;
	
	while ( cf != NULL )
	{
		cf = delete_conf_frame( cf ) ;
	}
	
	// !!! DEBUGING !!!	
	ast_log( AST_CONF_DEBUG, "deleting member output frames, name => %s\n", 
		member->channel_name ) ;

	// outgoing frames
	cf = member->outFrames ;
	
	while ( cf != NULL )
	{
		cf = delete_conf_frame( cf ) ;
	}

#if ( SILDET == 2 )
	if ( member->dsp != NULL )
	{
		// !!! DEBUGING !!!	
		ast_log( AST_CONF_DEBUG, "destroying member preprocessor, name => %s\n", 
			member->channel_name ) ;
		speex_preprocess_state_destroy( member->dsp ) ;
	}
#endif

	// !!! DEBUGING !!!	
	ast_log( AST_CONF_DEBUG, "freeing member translator paths, name => %s\n", 
		member->channel_name ) ;

	// free the mixing translators
	ast_translator_free_path( member->to_slinear ) ;
	ast_translator_free_path( member->from_slinear ) ;

	// get a pointer to the next 
	// member so we can return it
	struct ast_conf_member* nm = member->next ;
	
	// !!! DEBUGING !!!	
	ast_log( AST_CONF_DEBUG, "freeing member channel name, name => %s\n", 
		member->channel_name ) ;

	// free the member's copy for the channel name
	free( member->channel_name ) ;
	
	// !!! DEBUGING !!!	
	ast_log( AST_CONF_DEBUG, "freeing member\n" ) ;

	// free the member's memory
	free( member ) ;
	member = NULL ;
	
	return nm ;
}

//
// incoming frame functions
//

conf_frame* get_incoming_frame( struct ast_conf_member *member )
{
	//
	// sanity checks
	//
	
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to get frame from null member\n" ) ;
		return NULL ;
	}
 
 	//
 	// repeat last frame a couple times to smooth transition
 	//
 	
#ifdef AST_CONF_CACHE_LAST_FRAME
	if ( member->inFramesCount == 0 )
	{
		// nothing to do if there's no cached frame
		if ( member->inFramesLast == NULL )
			return NULL ;

		// turn off 'okay to cache' flag
		member->okayToCacheLast = 0 ;

		if ( member->inFramesRepeatLast >= AST_CONF_CACHE_LAST_FRAME )
		{
			// alreay used this frame AST_CONF_CACHE_LAST_FRAME times
		
			// reset repeat count
			member->inFramesRepeatLast = 0 ;
			
			// clear the cached frame
			delete_conf_frame( member->inFramesLast ) ;
			member->inFramesLast = NULL ;
			
			// return null
			return NULL ;
		}
		else
		{			 
			ast_log( AST_CONF_DEBUG, "repeating cached frame, channel => %s, inFramesRepeatLast => %d\n",
				member->channel_name, member->inFramesRepeatLast ) ;
		
			// increment counter
			member->inFramesRepeatLast++ ;
			
			// return a copy of the cached frame
			return copy_conf_frame( member->inFramesLast ) ;
		}		
	}
	else if ( member->okayToCacheLast == 0 && member->inFramesCount >= 3 ) 
	{
		ast_log( AST_CONF_DEBUG, "enabling cached frame, channel => %s, incoming => %d, outgoing => %d\n",
			member->channel_name, member->inFramesCount, member->outFramesCount ) ;
	
		// turn on 'okay to cache' flag
		member->okayToCacheLast = 1 ;
	}
#else
	if ( member->inFramesCount == 0 )
		return NULL ;
#endif // AST_CONF_CACHE_LAST_FRAME

	//
	// return the next frame in the queue
	//
	
	conf_frame* cfr = NULL ;

	// get first frame in line
	cfr = member->inFramesTail ;

	// if it's the only frame, reset the queu,
	// else, move the second frame to the front
	if ( member->inFramesTail == member->inFrames )
	{
		member->inFramesTail = NULL ;
		member->inFrames = NULL ;
	} 
	else 
	{
		// move the pointer to the next frame
		member->inFramesTail = member->inFramesTail->prev ;

		// reset it's 'next' pointer
		if ( member->inFramesTail != NULL ) 
			member->inFramesTail->next = NULL ;
	}

	// separate the conf frame from the list
	cfr->next = NULL ;
	cfr->prev = NULL ;

	// decriment frame count
	member->inFramesCount-- ;
	
#ifdef AST_CONF_CACHE_LAST_FRAME
	// copy frame if queue is now empty
	if ( 
		member->inFramesCount == 0 
		&& member->okayToCacheLast == 1 
	)
	{
		// reset repeat count
		member->inFramesRepeatLast = 0 ;

		// clear cached frame
		if ( member->inFramesLast != NULL )
		{
			delete_conf_frame( member->inFramesLast ) ;
			member->inFramesLast = NULL ;
		}
		
		// cache new frame  
		member->inFramesLast = copy_conf_frame( cfr ) ;
	}
#endif // AST_CONF_CACHE_LAST_FRAME
	
	return cfr ;
}

int queue_incoming_frame( struct ast_conf_member* member, const struct ast_frame* fr ) 
{
	//
	// sanity checks
	//

	// check on frame
	if ( fr == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to queue null frame\n" ) ;
		return -1 ;
	}
	
	// check on member
	if ( member == NULL )
	{
		ast_log( LOG_ERROR, "unable to queue frame for null member\n" ) ;
		return -1 ;
	}

	//
	// drop a frame if we've filled half the buffer
	// ( no more than once per AST_CONF_QUEUE_DROP_TIME_LIMIT ms )
	//

	if ( member->inFramesCount > member->inFramesNeeded )
	{
		if ( member->inFramesCount > AST_CONF_QUEUE_DROP_THRESHOLD ) 
		{
			struct timeval curr ;
			gettimeofday( &curr, NULL ) ;

			// time since last dropped frame			
			long diff = usecdiff( &curr, &member->last_in_dropped ) ;

			// number of milliseconds which must pass between frame drops
			// ( 15 frames => -100ms, 10 frames => 400ms, 5 frames => 900ms, 0 frames => 1400ms, etc. )
			long time_limit = 1000 - ( ( member->inFramesCount - AST_CONF_QUEUE_DROP_THRESHOLD ) * 100 ) ;
			
			if ( diff >= time_limit )
			{
				// count sequential drops
				member->sequential_drops++ ;
	
				ast_log( 
					AST_CONF_DEBUG,
					"dropping frame from input buffer, channel => %s, incoming => %d, outgoing => %d\n",
					member->channel_name, member->inFramesCount, member->outFramesCount
				) ;
				
				// accounting: count dropped incoming frames
				member->frames_in_dropped++ ;
	
				// reset frames since dropped
				member->since_dropped = 0 ;
	
				// delete the frame
				delete_conf_frame( get_incoming_frame( member ) ) ;
				
				gettimeofday( &member->last_in_dropped, NULL ) ;
			}
			else
			{
/*
				ast_log( 
					AST_CONF_DEBUG,
					"input buffer larger than drop threshold, channel => %s, incoming => %d, outgoing => %d\n",
					member->channel_name, member->inFramesCount, member->outFramesCount
				) ;
*/
			}
		}
	}

	//
	// if we have to drop frames, we'll drop new frames
	// because it's easier ( and doesn't matter much anyway ).
	//
	
	if ( member->inFramesCount >= AST_CONF_MAX_QUEUE ) 
	{
		// count sequential drops
		member->sequential_drops++ ;
	
		ast_log( 
			AST_CONF_DEBUG,
			"unable to queue incoming frame, channel => %s, incoming => %d, outgoing => %d\n",
			member->channel_name, member->inFramesCount, member->outFramesCount
		) ;

		// accounting: count dropped incoming frames
		member->frames_in_dropped++ ;

		// reset frames since dropped
		member->since_dropped = 0 ;
		
		return -1 ;
	} 

	// reset sequential drops 
	member->sequential_drops = 0 ;
	
	// increment frames since dropped
	member->since_dropped++ ;

	//
	// create new conf frame from passed data frame
	//
	
	// ( member->inFrames may be null at this point )
	conf_frame* cfr = create_conf_frame( member, member->inFrames, fr ) ;
	
	if ( cfr == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to malloc conf_frame\n" ) ;
		return -1 ;
	}
	
	// copy frame data pointer to conf frame
	// cfr->fr = fr ;

	//
	// add new frame to speaking members incoming frame queue
	// ( i.e. save this frame data, so we can distribute it in conference_exec later )
	//

	if ( member->inFrames == NULL )
	{
		// this is the first frame in the buffer
		member->inFramesTail = cfr ;
		member->inFrames = cfr ;
	}
	else
	{
		// put the new frame at the head of the list
		member->inFrames = cfr ;
	}
	
	// increment member frame count
	member->inFramesCount++ ;
	
	// return success
	return 0 ;
}

//
// outgoing frame functions
//

conf_frame* get_outgoing_frame( struct ast_conf_member *member )
{
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to get frame from null member\n" ) ;
		return NULL ;
	}

	conf_frame* cfr ;

	// ast_log( AST_CONF_DEBUG, "getting member frames, count => %d\n", member->outFramesCount ) ;
 
	if ( member->outFramesCount > AST_CONF_MIN_QUEUE ) 
	{
		cfr = member->outFramesTail ;
	
		// if it's the only frame, reset the queu,
		// else, move the second frame to the front
		if ( member->outFramesTail == member->outFrames )
		{
			member->outFrames = NULL ;
			member->outFramesTail = NULL ;
		} 
		else 
		{
			// move the pointer to the next frame
			member->outFramesTail = member->outFramesTail->prev ;

			// reset it's 'next' pointer
			if ( member->outFramesTail != NULL ) 
				member->outFramesTail->next = NULL ;
		}

		// separate the conf frame from the list
		cfr->next = NULL ;
		cfr->prev = NULL ;

		// decriment frame count
		member->outFramesCount-- ;
		
		return cfr ;
	} 

	return NULL ;
}

int queue_outgoing_frame( struct ast_conf_member* member, const struct ast_frame* fr, struct timeval delivery ) 
{
	// check on frame
	if ( fr == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to queue null frame\n" ) ;
		return -1 ;
	}

	// check on member
	if ( member == NULL )
	{
		ast_log( LOG_ERROR, "unable to queue frame for null member\n" ) ;
		return -1 ;
	}

	// accounting: count the number of outgoing frames for this member
	member->frames_out++ ;	
	
/*
	//
	// drop a frame if we've filled half the buffer
	//

	if ( member->outFramesCount > AST_CONF_QUEUE_DROP_THRESHOLD ) 
	{
		struct timeval curr ;
		gettimeofday( &curr, NULL ) ;
		
		long diff = usecdiff( &curr, &member->last_out_dropped ) ;
		
		if ( diff >= AST_CONF_QUEUE_DROP_TIME_LIMIT )
		{
			ast_log( 
				AST_CONF_DEBUG,
				"dropping frame from output buffer, channel => %s, incoming => %d, outgoing => %d\n",
				member->channel_name, member->inFramesCount, member->outFramesCount
			) ;

			// accounting: count dropped outgoing frames
			member->frames_out_dropped++ ;

			// delete the frame
			delete_conf_frame( get_outgoing_frame( member ) ) ;
			
			gettimeofday( &member->last_out_dropped, NULL ) ;
		}
	}
*/

	//
	// we have to drop frames, so we'll drop new frames
	// because it's easier ( and doesn't matter much anyway ).
	//
	if ( member->outFramesCount >= AST_CONF_MAX_QUEUE ) 
	{
		ast_log( 
			AST_CONF_DEBUG,
			"unable to queue outgoing frame, channel => %s, incoming => %d, outgoing => %d\n",
			member->channel_name, member->inFramesCount, member->outFramesCount
		) ;

		// accounting: count dropped outgoing frames
		member->frames_out_dropped++ ;

		return -1 ;
	}

	//
	// create new conf frame from passed data frame
	//
	
	conf_frame* cfr = create_conf_frame( member, member->outFrames, fr ) ;
	
	if ( cfr == NULL ) 
	{
		ast_log( LOG_ERROR, "unable to create new conf frame\n" ) ;

		// accounting: count dropped outgoing frames
		member->frames_out_dropped++ ;

		return -1 ;
	}

	// set delivery timestamp
	cfr->fr->delivery = delivery ;

	//
	// add new frame to speaking members incoming frame queue
	// ( i.e. save this frame data, so we can distribute it in conference_exec later )
	//

	if ( member->outFrames == NULL )
	{
		// this is the first frame in the buffer
		member->outFramesTail = cfr ;
		member->outFrames = cfr ;
	}
	else
	{
		// put the new frame at the head of the list
		member->outFrames = cfr ;
	}
	
	// increment member frame count
	member->outFramesCount++ ;
	
	// return success
	return 0 ;
}

//
// manager functions
//

void send_state_change_notifications( struct ast_conf_member* member )
{
	// ast_log( AST_CONF_DEBUG, "sending state change notification\n" ) ;

	// loop through list of members, sending state changes
	while ( member != NULL )
	{
		// has the state changed since last time through this loop?
		if ( member->speaking_state_notify != member->speaking_state_prev )
		{
			manager_event(
				EVENT_FLAG_CALL, 
				"ConferenceState", 
				"Channel: %s\r\n"
				"State: %s\r\n",
				member->channel_name, 
				( ( member->speaking_state_notify == 1 ) ? "speaking" : "silent" )
			) ;

			ast_log( AST_CONF_DEBUG, "member state changed, channel => %s, state => %d, incoming => %d, outgoing => %d\n",
				member->channel_name, member->speaking_state_notify, member->inFramesCount, member->outFramesCount ) ;

			// remember current state
			member->speaking_state_prev = member->speaking_state_notify ;
			
			// we do not reset the speaking_state_notify flag here
		}

		// reset notification flag so that 
		member->speaking_state_notify = 0 ;

		// move the pointer to the next member
		member = member->next ;
	}
	
	return ;
}

//
// meta-info accessors functions
//

short memberIsPhoneClient( struct ast_conf_member* member )
{
	if ( member == NULL ) 
		return 0 ;
		
	return ( member->connection_type == 'T' ) ? 1 : 0 ;
}

short memberIsIaxClient( struct ast_conf_member* member )
{
	if ( member == NULL ) 
		return 0 ;
		
	return ( member->connection_type == 'X' ) ? 1 : 0 ;
}

short memberIsSIPClient( struct ast_conf_member* member )
{
	if ( member == NULL ) 
		return 0 ;
		
	return ( member->connection_type == 'S' ) ? 1 : 0 ;
}

short memberIsModerator( struct ast_conf_member* member )
{
	if ( member == NULL ) 
		return 0 ;
		
	return ( member->type == 'M' ) ? 1 : 0 ;
}

short memberIsListener( struct ast_conf_member* member )
{
	if ( member == NULL ) 
		return 0 ;
		
	return ( member->type == 'L' ) ? 1 : 0 ;
}


