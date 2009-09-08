
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

#ifndef APP_CONF_PACKER_H_
#define APP_CONF_PACKER_H_

struct ast_packer;
struct ast_frame;

struct ast_packer *ast_packer_new(int bytes);
void ast_packer_set_flags(struct ast_packer *, int flags);
int ast_packer_get_flags(struct ast_packer *);
void ast_packer_free(struct ast_packer *);
void ast_packer_reset(struct ast_packer *, int bytes);
int ast_packer_feed(struct ast_packer *, const struct ast_frame *);
struct ast_frame *ast_packer_read(struct ast_packer *);

#endif
