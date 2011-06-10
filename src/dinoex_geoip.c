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

typedef struct {
  GeoIP *gi;
  time_t loaded;
  char code[8];
} ir_geoip;

static ir_geoip geoip4 = { NULL, 0 };
#ifdef USE_GEOIP6
static ir_geoip geoip6 = { NULL, 0 };
#endif /* USE_GEOIP6 */

static time_t geoip_time(const char *name)
{
  struct stat st;

  if (name == NULL)
    return 0;

  if (stat(name, &st) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
	     "cannot access '%s', ignoring: %s",
             name, strerror(errno));
    return 0;
  }
  return st.st_mtime;
}

static void geoip_close(ir_geoip *geoip)
{
  if (geoip->gi != NULL) {
    GeoIP_delete(geoip->gi);
    geoip->gi = NULL;
  }
}

static void geoip_open(ir_geoip *geoip, const char *geoipdatabase)
{
  if (geoip->gi != NULL) {
    if (geoip->loaded != geoip_time(geoipdatabase)) {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "File '%s' has changed", geoipdatabase);
      /* reload on change */
      geoip_close(geoip);
    }
  }
  if (geoip->gi == NULL) {
    if (geoipdatabase == NULL) {
      geoip->gi = GeoIP_new(GEOIP_FLAGS);
      return;
    }
    geoip->loaded = geoip_time(geoipdatabase);
    geoip->gi = GeoIP_open(geoipdatabase, GEOIP_FLAGS);
  }
}

static const char *geoip_return_null(ir_geoip *geoip)
{
  geoip->code[0] = 0;
  return geoip->code;
}

static const char *geoip_return_reusult(ir_geoip *geoip, const char *result)
{
  if (result == NULL)
    return geoip_return_null(geoip);

  geoip->code[0] = tolower(result[0]);
  geoip->code[1] = tolower(result[1]);
  geoip->code[2] = 0;
  return geoip->code;
}

static const char *check_geoip(ir_uint32 remoteip)
{
  static char hostname[20];
  const char *result;

  geoip_open(&geoip4, gdata.geoipdatabase);
  if (geoip4.gi == NULL)
    return geoip_return_null(&geoip4);

  snprintf(hostname, sizeof(hostname), IPV4_PRINT_FMT,
           IPV4_PRINT_DATA(remoteip));
  result = GeoIP_country_code_by_addr(geoip4.gi, hostname);
  return geoip_return_reusult(&geoip4, result);
}

#ifdef USE_GEOIP6
static const char *check_geoip6(struct in6_addr *remoteip)
{
  const char *result;

  geoip_open(&geoip6, gdata.geoip6database);
  if (geoip6.gi == NULL)
    return geoip_return_null(&geoip6);

  result = GeoIP_country_code_by_ipnum_v6(geoip6.gi, *remoteip);
  return geoip_return_reusult(&geoip6, result);
}
#endif /* USE_GEOIP6 */

/* check an IRC download against the GeoIP database */
void geoip_new_connection(transfer *const tr)
{
  const char *country;
  const char *group;
  char *msg;

  if (tr->con.family != AF_INET) {
#ifdef USE_GEOIP6
    if (gdata.geoip6database == NULL)
      return;
    country = check_geoip6(&(tr->con.remote.sin6.sin6_addr));
#else
    return;
#endif /* USE_GEOIP6 */
  } else {
    country = check_geoip(tr->remoteip);
  }
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "GeoIP [%02i:%s on %s]: Info %s -> %s)",
            tr->id, tr->nick, gdata.networks[ tr->net ].name,
            tr->con.remoteaddr, country);
  tr->country = mystrdup(country);
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
         msg = mymalloc(maxtextlength);
         snprintf(msg, maxtextlength, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
         t_closeconn(tr, msg, 0);
         ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                 "IP from other country (%s) detected", country);
         mydelete(msg);
         return;
      }
    }
  }
  if (irlist_size(&gdata.nogeoipcountry)) {
    if (verifyshell(&gdata.nogeoipcountry, country)) {
      if (!verifyshell(&gdata.geoipexcludenick, tr->nick)) {
         msg = mymalloc(maxtextlength);
         snprintf(msg, maxtextlength, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
         t_closeconn(tr, msg, 0);
         ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                 "IP from other country (%s) detected", country);
         mydelete(msg);
         return;
      }
    }
  }
}

#ifndef WITHOUT_HTTP
static unsigned int http_check_country(const char *country)
{
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

/* check a HTTP connection against the GeoIP database */
unsigned int http_check_geoip(ir_uint32 remoteip)
{
  const char *country;

  if (gdata.http_geoip == 0)
    return 0;

  country = check_geoip(remoteip);
  return http_check_country(country);
}

#ifdef USE_GEOIP6
/* check a HTTP connection against the GeoIPv6 database */
unsigned int http_check_geoip6(struct in6_addr *remoteip)
{
  const char *country;

  if (gdata.http_geoip == 0)
    return 0;

  country = check_geoip6(remoteip);
  return http_check_country(country);
}
#endif /* USE_GEOIP6 */
#endif /* WITHOUT_HTTP */

/* close GeoIP */
void geoip_shutdown(void)
{
  geoip_close(&geoip4);
#ifdef USE_GEOIP6
  geoip_close(&geoip6);
#endif /* USE_GEOIP6 */
}

#endif /* USE_GEOIP */

/* End of File */
