#include "iaxclient_lib.h"
#include <windows.h>

/* Win-doze doenst have gettimeofday(). This sux. So, what we did is
provide some gettimeofday-like functionality that works for our purposes.
In the main(), we take a sample of the system tick counter (into startuptime).
This function returns the relative time since program startup, more or less,
which is certainly good enough for our purposes. */

static long startuptime;

void gettimeofday(struct timeval *tv, struct timezone *tz)
{
	long l = startuptime + GetTickCount();

	tv->tv_sec = l / 1000;
	tv->tv_usec = (l % 1000) * 1000;
	return;
}

void os_init(void)
{
	time_t t;
	time(&t);
	startuptime = ((t % 86400) * 1000) - GetTickCount();
}

/* yes, it could have just been a #define, but that makes linking trickier */
void iaxc_millisleep(long ms)
{
	Sleep(ms);
}
