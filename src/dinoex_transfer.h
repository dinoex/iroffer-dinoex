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
int t_check_ip_access(transfer *const tr);
int t_find_transfer(const char *nick, const char *filename, const char *remoteip, const char *remoteport, const char *token);
int t_find_resume(const char *nick, const char *filename, const char *localport, const char *bytes, char *token);
void t_connected(transfer *tr);
void t_check_duplicateip(transfer *const newtr);
int verify_acknowlede(transfer *tr);
int t_select_fdset(int highests, int changequartersec);

/* End of File */
