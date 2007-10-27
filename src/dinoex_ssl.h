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

void ssl_startup(void);
void close_server(void);
int setup_ssl(void);
ssize_t readserver_ssl(void *buf, size_t nbytes);
ssize_t writeserver_ssl(const void *buf, size_t nbytes);

/* End of File */
