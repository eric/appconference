
#include "iaxclient_lib.h"

#include <windows.h>
#include <winbase.h>

#include <stdio.h>

/* Win-doze doesnt have gettimeofday(). This sux. So, what we did is
provide some gettimeofday-like functionality that works for our purposes. */

/*
	changed 'struct timezone*' to 'void*' since
	timezone is defined as a long in MINGW and caused compile-time warnings.
	this should be okay since we don't use the passed value. 
*/


/* 
 * functions implementations
 */

void gettimeofday( struct timeval* tv, void* tz )
{
	struct _timeb curSysTime;
	_ftime(curSysTime);
	tv->tv_sec = curSysTime.time;
	tv->tv_usec = curSysTime.millitm * 1000;

	return ;
}

void os_init(void)
{
#ifdef IAXC_IAX2
	WSADATA wsd;

	if(WSAStartup(0x0101,&wsd))
	{   // Error message?
	    exit(1);
	}
#endif
}

/* yes, it could have just been a #define, but that makes linking trickier */
void iaxc_millisleep(long ms)
{
	Sleep(ms);
}
