
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

#ifndef APP_CONF_FRAMELIST_H_
#define APP_CONF_FRAMELIST_H_

struct conf_frame;
struct ast_conf_member;

struct ast_conf_framelist
{
	unsigned int len;
	struct conf_frame * head;
	struct conf_frame * tail;
};

struct conf_frame *
framelist_pop_tail(struct ast_conf_framelist *);

int
framelist_push_head(struct ast_conf_framelist *, const struct ast_frame *,
		struct ast_conf_member *);

static inline struct conf_frame *
framelist_peek_head(struct ast_conf_framelist * list)
{
	return list->head;
}

static inline unsigned int
framelist_len(struct ast_conf_framelist * list)
{
	return list->len;
}

#endif
