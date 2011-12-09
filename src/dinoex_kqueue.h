/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2011-2011 Dirk Meyer
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

#ifdef USE_KQUEUE
void ir_kqueue_init(void);
void ir_kqueue_exit(void);
#endif /* USE_KQUEUE */

int ir_kqueue_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
void event_close(int handle);
void shutdown_close(int handle);

/* End of File */
