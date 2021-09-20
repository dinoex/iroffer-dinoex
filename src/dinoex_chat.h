/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2021 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from https://iroffer.net/
 *
 * SPDX-FileCopyrightText: 2004-2021 Dirk Meyer
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * $Id$
 *
 */

void chat_shutdown(dccchat_t *chat, int flush);
int chat_setup_out(const char *nick, const char *hostmask, const char *token,
               int use_ssl);
int chat_setup(const char *nick, const char *hostmask, const char *line,
               int use_ssl);
void chat_banner(dccchat_t *chat);
unsigned int dcc_host_password(dccchat_t *chat, char *passwd);
void chat_writestatus(void);
int chat_select_fdset(int highests);
void chat_perform(int changesec);
void chat_shutdown_all(void);

/* End of File */
