#ifndef _iaxclient_lib_h
#define _iaxclient_lib_h

/* This is the internal include file for IAXCLIENT -- externally
 * accessible APIs should be declared in iaxclient.h */


#ifdef WIN32
#include "winpoop.h" // Win32 Support Functions
#include <winsock.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>

#else
/* not win32 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#endif

#include <stdio.h>




/* os-dependent macros, etc */
#ifdef WIN32
#define THREAD HANDLE
#define THREADID unsigned
#define THREADCREATE(func, args, thread, id) \
(thread = (HANDLE)_beginthreadex(NULL, 0, func, (PVOID)args, 0, &id))
#define THREADCREATE_ERROR NULL
#define THREADFUNCDECL(func) unsigned __stdcall func(PVOID args)
#define THREADFUNCRET(r) int r = 0
#define THREADJOIN(t) WaitForSingleObject(t, INFINITE)
#define MUTEX CRITICAL_SECTION
#define MUTEXINIT(m) InitializeCriticalSection(m)
#define MUTEXLOCK(m) EnterCriticalSection(m)
#define MUTEXUNLOCK(m) LeaveCriticalSection(m)
#define MUTEXDESTROY(m) DeleteCriticalSection(m)

#else
#define THREAD pthread_t
#define THREADID unsigned /* unused for Posix Threads */
#define THREADCREATE(func, args, thread, id) \
pthread_create(&thread, NULL, func, args)
#define THREADCREATE_ERROR -1
#define THREADFUNCDECL(func) void * func(void *args)
#define THREADFUNCRET(r) void * r = 0
#define THREADJOIN(t) pthread_join(t, 0)
#define MUTEX pthread_mutex_t
#define MUTEXINIT(m) pthread_mutex_init(m, NULL) //TODO: check error
#define MUTEXLOCK(m) pthread_mutex_lock(m)
#define MUTEXUNLOCK(m) pthread_mutex_unlock(m)
#define MUTEXDESTROY(m) pthread_mutex_destroy(m)
#endif


#define RBUFSIZE 256
#define MAXARGS 10
#define MAXARG 256
#define MAX_SESSIONS 4
#define OUT_INTERVAL 20


#include "iax-client.h" // LibIAX functions
#include "gsm.h"

struct peer {
	int time;
	gsm gsmin;
	gsm gsmout;

	struct iax_session *session;
	struct peer *next;
};

long iaxc_usecdiff( struct timeval *timeA, struct timeval *timeB );
void iaxc_handle_network_event(FILE *f, struct iax_event *e, struct peer *p);
void iaxc_service_network(int netfd, FILE *f);



#include "iaxclient.h"
#include "audio_encode.h"
#include "audio_portaudio.h"

#ifdef WIN32
#include "audio_win32.h"
#endif


extern double iaxc_silence_threshold;
extern int iaxc_audio_output_mode;
extern iaxc_levels_callback_t iaxc_levels_callback;
extern iaxc_message_callback_t iaxc_error_callback;
extern iaxc_message_callback_t iaxc_status_callback;

/* external audio functions */
void iaxc_external_service_audio();

#endif

