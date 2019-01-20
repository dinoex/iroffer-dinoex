/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2019 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from https://iroffer.net/
 *
 * $Id$
 *
 */

#ifdef USE_MAXMINDDB
void maxminddb_new_connection(transfer *const tr);
#ifndef WITHOUT_HTTP
unsigned int http_check_maxminddb(const struct sockaddr *const sa, const char *hostname);
#endif /* WITHOUT_HTTP */
void maxminddb_shutdown(void);
#endif /* USE_MAXMINDDB */

/* End of File */
