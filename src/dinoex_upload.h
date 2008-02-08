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

int l_setup_file(upload * const l, struct stat *stp);
int l_setup_listen(upload * const l);
int l_setup_passive(upload * const l, char *token);
void l_setup_accept(upload * const l);
int l_select_fdset(int highests);

int file_uploading(const char *file);
void upload_start(char *nick, char *hostname, char *hostmask, char *filename, char *remoteip, char *remoteport, char *bytes, char *token);

/* End of File */
