/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Echo application -- play back what you hear to evaluate latency
 * 
 * Copyright (C) 1999, Mark Spencer
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

#include <asterisk/lock.h>
#include <asterisk/file.h>
#include <asterisk/logger.h>
#include <asterisk/channel.h>
#include <asterisk/pbx.h>
#include <asterisk/module.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#include "onering.h"

static char *tdesc = "Simple Echo Application";

static char *app = "Onering";

static char *synopsis = "delayed Echo audio read back to the user";

static char *descrip = 
"  Echo():  Echo audio read from channel back to the channel. Returns 0\n"
"if the user exits with the '#' key, or -1 if the user hangs up.\n";

STANDARD_LOCAL_USER;

LOCAL_USER_DECL;

static int echo_exec(struct ast_channel *chan, void *data)
{
	int res=-1;
	struct localuser *u;
	struct ast_frame *f,*of;
	struct ast_onering* ring;
	int cache=0;
	LOCAL_USER_ADD(u);
	ast_set_write_format(chan, AST_FORMAT_SLINEAR);
	ast_set_read_format(chan, AST_FORMAT_SLINEAR);
	/* Do our thing here */
	ring = ast_onering_new();
	while(ast_waitfor(chan, -1) > -1) {
		f = ast_read(chan);
		if (!f)
			break;
		if (f->frametype == AST_FRAME_VOICE) {
			if (ast_onering_write(ring, f)) {
			    break;
			} else {
			    cache++;
			}
			if (cache > 10) {
			    of=ast_onering_read(ring,f->samples/2);
			    ast_write(chan,of);
			    of=ast_onering_read(ring,f->samples/2);
			    ast_write(chan,of);
			    cache--;
			}
		} else if (f->frametype == AST_FRAME_DTMF) {
			if (f->subclass == '#') {
				res = 0;
				break;
			} else
				if (ast_write(chan, f))
					break;
		}
		ast_frfree(f);
	}
	LOCAL_USER_REMOVE(u);
	return res;
}

int unload_module(void)
{
	STANDARD_HANGUP_LOCALUSERS;
	return ast_unregister_application(app);
}

int load_module(void)
{
	return ast_register_application(app, echo_exec, synopsis, descrip);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	int res;
	STANDARD_USECOUNT(res);
	return res;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
