
#include <sys/time.h>

#define NULL (0)

/* Unix-specific functions */

void os_init(void)
{
}

void os_millisleep(long ms)
{
	struct timespec req;

        req.tv_nsec=10*1000*1000;  /* 10 ms */
        req.tv_sec=0;

        /* yes, it can return early.  We don't care */
        nanosleep(&req,NULL);

}
