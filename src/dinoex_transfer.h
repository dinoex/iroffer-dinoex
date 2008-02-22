/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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

void t_setup_dcc(transfer *tr, const char *nick);
int t_find_transfer(char *nick, char *filename, char *remoteip, char *remoteport, char *token);
int t_find_resume(char *nick, char *filename, char *localport, char *bytes, char *token);
void t_connected(transfer *tr);
void t_check_duplicateip(transfer *const newtr);
int t_select_fdset(int highests, int changequartersec);

/* End of File */
