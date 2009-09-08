
/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 * Copyright (C) 2003, 2004 HorizonLive.com, Inc.
 * Copyright (C) 2005, 2006 HorizonWimba, Inc.
 * Copyright (C) 2007 Wimba, Inc.
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * Video Conferencing support added by
 * Neil Stratford <neils@vipadia.com>
 * Copyright (C) 2005, 2005 Vipadia Limited
 *
 * VAD driven video conferencing, text message support
 * and miscellaneous enhancements added by
 * Mihai Balea <mihai at hates dot ms>
 *
 * This program may be modified and distributed under the
 * terms of the GNU General Public License. You should have received
 * a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc.
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "app_conference.h"
#include "frame.h"
#include "framelist.h"

struct conf_frame *
framelist_pop_tail(struct ast_conf_framelist * list)
{
	if ( list->len == 0 || list->len <= AST_CONF_MIN_QUEUE )
		return 0;

	struct conf_frame * cfr = list->tail;

	if ( list->tail == list->head )
	{
		list->head = 0;
		list->tail = 0;
	}
	else
	{
		list->tail = list->tail->prev;
		if ( list->tail )
			list->tail->next = 0;
	}

	cfr->next = cfr->prev = 0;

	--list->len;

	return cfr;
}

int
framelist_push_head(struct ast_conf_framelist * list,
		const struct ast_frame * fr,
		struct ast_conf_member * member)
{
	struct conf_frame * cfr = frame_create(member, list->head, fr);

	if ( !cfr )
		return -1;

	if ( !list->head )
		list->head = list->tail = cfr;
	else
		list->head = cfr;

	++list->len;

	return 0;
}

