
#include "iaxclient_lib.h"

#include <windows.h>
#include <winbase.h>

#include <stdio.h>

/* Win-doze doenst have gettimeofday(). This sux. So, what we did is
provide some gettimeofday-like functionality that works for our purposes.
In the main(), we take a sample of the system tick counter (into startuptime).
This function returns the relative time since program startup, more or less,
which is certainly good enough for our purposes. */

/*
	changed 'struct timezone*' to 'void*' since
	timezone is defined as a long in MINGW and caused compile-time warnings.
	this should be okay since we don't use the passed value. 
*/

/* 
 * static variables 
 */

/* for original, lores_gettimeofday() */
static long startuptime;

/* for new, hires_gettimeofday() */
static __int64 freq, start ;
static int hires_gettimeofday_inited = 0 ;
static int hires_gettimeofday_available = 0 ;

/* 
 * functions implementations
 */

void gettimeofday( struct timeval* tv, void* tz )
{
	if ( hires_gettimeofday_available == 1 )
	{
		/* http://sourceforge.net/mailarchive/message.php?msg_id=7252124 */
		
		__int64 time ;
		double elapsed ;
	
		QueryPerformanceCounter( (LARGE_INTEGER*)( &time ) ) ;
	
		elapsed = (double)(time - start) / (double)freq ;
	
		tv->tv_sec = (long)( elapsed ) ;
		tv->tv_usec = (long)( ( elapsed - tv->tv_sec ) * 1000000 ) ;
	}
	else
	{
		long l = startuptime + GetTickCount();
	
		tv->tv_sec = l / 1000;
		tv->tv_usec = (l % 1000) * 1000;
	}

	return ;
}

void os_init(void)
{
	time_t t;
#ifdef IAXC_IAX2
	WSADATA wsd;

	if(WSAStartup(0x0101,&wsd))
	{   // Error message?
	    exit(1);
	}
#endif

	/* initialize lores gettimeofday() implementation */
	time(&t);
	startuptime = ((t % 86400) * 1000) - GetTickCount();

	/* try to initialized hires gettimeofday() implementation */
	if ( hires_gettimeofday_inited == 0 )
	{
		/* flag hires_gettimeofday as initialized */
		hires_gettimeofday_inited = 1 ;
		
		if ( 
			QueryPerformanceFrequency( (LARGE_INTEGER*)( &freq ) ) == TRUE 
			&& QueryPerformanceCounter( (LARGE_INTEGER*)( &start ) ) == TRUE
		)
		{
			fprintf( stderr, "%s: using hires gettimeofday() implementation\n", __FILE__ ) ; 
			fflush( stderr ) ;
		
			/* flag hires_gettimeofday as available */
			hires_gettimeofday_available = 1 ;
		}
		else
		{
			fprintf( stderr, "%s: using lores gettimeofday() implementation\n", __FILE__ ) ; 
			fflush( stderr ) ;
		}
	}
}

/* yes, it could have just been a #define, but that makes linking trickier */
void iaxc_millisleep(long ms)
{
	Sleep(ms);
}
