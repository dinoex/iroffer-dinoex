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

void curl_startup(void);
void curl_shutdown(void);
#ifdef USE_CURL
extern int fetch_started;
void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);
void fetch_cancel(int num);
void fetch_perform(void);
void start_fetch_url(const userinput *const u);
void dinoex_dcl(const userinput *const u);
void dinoex_dcld(const userinput *const u);
int fetch_is_running(const char *file);
#endif /* USE_CURL */

/* End of File */
