/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2011 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.dinoex.net/
 *
 * $Id$
 *
 */

unsigned int dcc_host_password(dccchat_t *chat, char *passwd);
void chat_writestatus(void);
int chat_select_fdset(int highests);
void chat_perform(void);

/* End of File */
