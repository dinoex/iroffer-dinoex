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

#ifdef USE_GEOIP
void geoip_new_connection(transfer *const tr);
#ifndef WITHOUT_HTTP
unsigned int http_check_geoip(ir_uint32 remoteip);
unsigned int http_check_geoip6(struct in6_addr *remoteip);
#endif /* WITHOUT_HTTP */
void geoip_shutdown(void);
#endif /* USE_GEOIP */

/* End of File */
