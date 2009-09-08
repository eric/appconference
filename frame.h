
// $Id: frame.h 746 2006-12-11 20:12:12Z sbalea $

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

#ifndef _APP_CONF_FRAME_H
#define _APP_CONF_FRAME_H

#include "app_conference.h"
#include "conf_frame.h"

struct conf_frame* frame_mix_frames(struct conf_frame* frames_in,
		int speaker_count, int listener_count);

struct conf_frame* frame_create(struct ast_conf_member* member,
		struct conf_frame* next, const struct ast_frame* fr);
struct conf_frame* frame_delete(struct conf_frame* cf);
struct conf_frame* frame_copy(struct conf_frame* src);

struct ast_frame* frame_convert_from_slinear(struct ast_trans_pvt* trans,
		struct ast_frame* fr);

struct ast_frame* frame_create_text(const char *text);

struct conf_frame* frame_get_silent(void);

#endif
