/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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

void telnet_close_listen(void);
int telnet_setup_listen(void);
void telnet_reash_listen(void);
int telnet_select_listen(int highests);
void telnet_done_select(void);

/* End of File */
