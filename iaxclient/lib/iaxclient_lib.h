#ifndef _iaxclient_lib_h
#define _iaxclient_lib_h

#ifdef WIN32

#include "winpoop.h" // Win32 Support Functions

#endif

#include <stdio.h>
#include <sys/types.h>
#include "gsm.h"

#define RBUFSIZE 256
#define MAXARGS 10
#define MAXARG 256
#define MAX_SESSIONS 4

// Define audio type constants
#define AUDIO_INTERNAL 0
#define AUDIO_EXTERNAL 1

#include "iax-client.h" // LibIAX functions

static struct peer *peers;

int initialize_client(int audType, FILE *file);
void shutdown_client();
int process_calls();
int service_audio();
void handle_network_event(FILE *f, struct iax_event *e, struct peer *p);
void client_call(FILE *f, char *num);
void client_answer_call(void); 
void client_dump_call(void);
void client_reject_call(void);
static struct peer *find_peer(struct iax_session *session);
void service_network(int netfd, FILE *f);
void do_iax_event(FILE *f);
int was_call_answered();
void external_audio_event(FILE *f, struct iax_event *e, struct peer *p);
void external_service_audio();
void *get_audio_data(int i);
#endif
