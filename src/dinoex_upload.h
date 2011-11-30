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

char *get_uploaddir(const char *hostmask);

int l_setup_file(upload * const l, struct stat *stp);
int l_setup_passive(upload * const l, char *token);
const char *l_print_state(upload * const l);
int l_select_fdset(int highests, int changequartersec);
void l_perform(int changesec);

int file_uploading(const char *file);
int invalid_upload(const char *nick, const char *hostmask, off_t len);
void upload_start(const char *nick, const char *hostname, const char *hostmask,
                  const char *filename, const char *remoteip, const char *remoteport, const char *bytes, char *token);

void clean_uploadhost(void);

/* End of File */
