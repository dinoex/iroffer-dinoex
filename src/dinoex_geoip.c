/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_geoip.h"

#include <ctype.h>

#ifdef USE_GEOIP

#include <GeoIP.h>

#define GEOIP_FLAGS GEOIP_MEMORY_CACHE

static GeoIP *gi = NULL;

static char *check_geoip(unsigned long remoteip)
{
  static char hostname[20];
  static char code[20];
  const char *result;

  if (gi == NULL) {
    if (gdata.geoipdatabase != NULL)
      gi = GeoIP_open(gdata.geoipdatabase, GEOIP_FLAGS);
    else
      gi = GeoIP_new(GEOIP_FLAGS);
  }

  if (gi == NULL) {
    code[0] = 0;
    return code;
  }

  snprintf(hostname, sizeof(hostname), "%lu.%lu.%lu.%lu",
            remoteip>>24, (remoteip>>16) & 0xFF, (remoteip>>8) & 0xFF, remoteip & 0xFF );
  result = GeoIP_country_code_by_addr(gi, hostname);
  if (result == NULL) {
    code[0] = 0;
    return code;
  }

  code[0] = tolower(result[0]);
  code[1] = tolower(result[1]);
  code[2] = 0;
  return code;
}
#endif /* USE_GEOIP */

/* check an IRC download against the GeoIP database */
void geoip_new_connection(transfer *const tr)
{
#ifdef USE_GEOIP
  const char *country;
  const char *group;
  char *msg;
#endif /* USE_GEOIP */

  if (tr->con.family != AF_INET)
    return;

#ifdef USE_GEOIP
  country = check_geoip(tr->remoteip);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "GeoIP [%02i:%s on %s]: Info %ld.%ld.%ld.%ld -> %s)",
            tr->id, tr->nick, gdata.networks[ tr->net ].name,
            tr->remoteip>>24, (tr->remoteip>>16) & 0xFF,
            (tr->remoteip>>8) & 0xFF, tr->remoteip & 0xFF,
            country);

  if (irlist_size(&gdata.geoipexcludegroup)) {
    for (group = (char *)irlist_get_head(&gdata.geoipexcludegroup);
         group;
         group = irlist_get_next(group)) {
      if (tr->xpack->group == NULL)
        continue;
      if (strcasecmp(tr->xpack->group, group) == 0)
        return;
    }
  }
  if (irlist_size(&gdata.geoipcountry)) {
    if (!verifyshell(&gdata.geoipcountry, country)) {
      if (!verifyshell(&gdata.geoipexcludenick, tr->nick)) {
         msg = mycalloc(maxtextlength);
         snprintf(msg, maxtextlength, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
         t_closeconn(tr, msg, 0);
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                 "IP from other country (%s) detected", country);
         mydelete(msg);
         return;
      }
    }
  }
  if (irlist_size(&gdata.nogeoipcountry)) {
    if (verifyshell(&gdata.nogeoipcountry, country)) {
      if (!verifyshell(&gdata.geoipexcludenick, tr->nick)) {
         msg = mycalloc(maxtextlength);
         snprintf(msg, maxtextlength, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
         t_closeconn(tr, msg, 0);
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                 "IP from other country (%s) detected", country);
         mydelete(msg);
         return;
      }
    }
  }
#endif /* USE_GEOIP */
}

#ifdef USE_GEOIP
#ifndef WITHOUT_HTTP
/* check a HTTP connection against the GeoIP database */
unsigned int http_check_geoip(unsigned long remoteip)
{
  const char *country;

  country = check_geoip(remoteip);
  if (irlist_size(&gdata.geoipcountry)) {
    if (!verifyshell(&gdata.geoipcountry, country)) {
      return 1;
    }
  }
  if (irlist_size(&gdata.nogeoipcountry)) {
    if (verifyshell(&gdata.nogeoipcountry, country)) {
      return 1;
    }
  }
  return 0;
}
#endif /* WITHOUT_HTTP */
#endif /* USE_GEOIP */

/* close GeoIP */
void geoip_shutdown(void)
{
#ifdef USE_GEOIP
  if (gi != NULL) {
    GeoIP_delete(gi);
    gi = NULL;
  }
#endif /* USE_GEOIP */
}

/* End of File */
