/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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
unsigned int telnet_setup_listen(void);
void telnet_reash_listen(void);
int telnet_select_fdset(int highests);
void telnet_perform(void);

/* End of File */
