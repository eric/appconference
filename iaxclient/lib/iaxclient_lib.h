#ifndef _iaxclient_lib_h
#define _iaxclient_lib_h

/* This is the internal include file for IAXCLIENT -- externally
 * accessible APIs should be declared in iaxclient.h */

#include "iaxclient.h"

#ifdef WIN32
#include "winpoop.h" // Win32 Support Functions
#include <winsock.h>
#include <process.h>
#include <stddef.h>
#include <stdlib.h>

#else
/* not win32 */
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "gsm.h"

#include "audio_portaudio.h"
#include "audio_encode.h"

#ifdef WIN32
#include "audio_win32.h"
#endif

/* os-dependent macros, etc */
#ifdef WIN32
#define os_millisleep(x) Sleep(x)
#else
void os_millisleep(long ms);
#endif


#define RBUFSIZE 256
#define MAXARGS 10
#define MAXARG 256
#define MAX_SESSIONS 4


#include "iax-client.h" // LibIAX functions

static struct peer *peers;

long iaxc_usecdiff( struct timeval *timeA, struct timeval *timeB );
void iaxc_handle_network_event(FILE *f, struct iax_event *e, struct peer *p);
void iaxc_service_network(int netfd, FILE *f);
#endif

