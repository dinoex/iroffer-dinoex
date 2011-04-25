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

void t_start_dcc_send(transfer *tr);
void t_setup_dcc(transfer *tr);
transfer *create_transfer(xdcc *xd, const char *nick, const char *hostname);
void t_unlmited(transfer * const tr, const char *hostmask);
void t_notice_transfer(transfer * const tr, const char *msg, unsigned int pack, unsigned int queue);
unsigned int t_check_ip_access(transfer *const tr);
unsigned int t_find_transfer(const char *nick, const char *filename, const char *remoteip, const char *remoteport, const char *token);
unsigned int t_find_resume(const char *nick, const char *filename, const char *localport, const char *bytes, char *token);
unsigned int verify_acknowlede(transfer *tr);
const char *t_print_state(transfer *const tr);
int t_select_fdset(int highests, int changequartersec);
void t_perform(int changesec, int changequartersec);

/* End of File */
