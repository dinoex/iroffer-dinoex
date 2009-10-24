/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2009 Dirk Meyer
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

void geoip_new_connection(transfer *const tr);
#ifdef USE_GEOIP
#ifndef WITHOUT_HTTP
int http_check_geoip(unsigned long remoteip);
#endif /* WITHOUT_HTTP */
#endif /* USE_GEOIP */
void geoip_shutdown(void);

/* End of File */
