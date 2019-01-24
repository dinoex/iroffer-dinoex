/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2019 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.net/
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
#include "dinoex_maxminddb.h"

#include <ctype.h>

#ifdef USE_MAXMINDDB

#include <maxminddb.h>

typedef struct {
  const char *gi;
  MMDB_s mmdb;
  time_t loaded;
  char code[8];
} ir_maxminddb;

static ir_maxminddb maxminddb_data = { NULL, {}, 0 };

static time_t maxminddb_time(const char *name)
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

static void maxminddb_close(ir_maxminddb *maxminddb)
{
  if (maxminddb->gi != NULL) {
    MMDB_close(&(maxminddb->mmdb));
    maxminddb->gi = NULL;
  }
}

static void maxminddb_open(ir_maxminddb *maxminddb, const char *maxmindfile)
{
  int rc;

  if (maxminddb->gi != NULL) {
    if (maxminddb->loaded != maxminddb_time(maxmindfile)) {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "File '%s' has changed", maxmindfile);
      /* reload on change */
      maxminddb_close(maxminddb);
    }
  }
  if (maxminddb->gi == NULL) {
    maxminddb->loaded = maxminddb_time(maxmindfile);
    rc = MMDB_open(maxmindfile, MMDB_MODE_MMAP, &(maxminddb->mmdb));
    if (rc != MMDB_SUCCESS) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "error reading '%s'", maxmindfile);
      return;
    }
    maxminddb_data.gi = "on"; /* NOTRANSLATE */
  }
}

static const char *maxminddb_return_null(ir_maxminddb *maxminddb)
{
  maxminddb->code[0] = 0;
  return maxminddb->code;
}

static const char *maxminddb_return_reusult(ir_maxminddb *maxminddb, const char *result)
{
  if (result == NULL)
    return maxminddb_return_null(maxminddb);

  maxminddb->code[0] = tolower(result[0]);
  maxminddb->code[1] = tolower(result[1]);
  maxminddb->code[2] = 0;
  return maxminddb->code;
}

static const char *check_maxminddb_socket(const struct sockaddr *const sa, const char *hostname)
{
  int rc;
  MMDB_lookup_result_s result;
  MMDB_entry_data_s entry_data;

  maxminddb_open(&maxminddb_data, gdata.maxminddb);
  if (maxminddb_data.gi == NULL)
    return maxminddb_return_null(&maxminddb_data);

  result = MMDB_lookup_sockaddr(&(maxminddb_data.mmdb), sa, &rc);
  if (rc != MMDB_SUCCESS) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "error libmaxminddb '%s': %s",
             hostname, MMDB_strerror(rc));
    return maxminddb_return_null(&maxminddb_data);
  }
  if (!result.found_entry) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "no libmaxminddb entry for '%s'",
             hostname);
    return maxminddb_return_null(&maxminddb_data);
  }
  rc = MMDB_get_value(&result.entry, &entry_data,
                      "country", "iso_code", NULL); /* NOTRANSLATE */
  if (rc != MMDB_SUCCESS) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "error entry libmaxminddb '%s': %s",
             hostname, MMDB_strerror(rc));
    return maxminddb_return_null(&maxminddb_data);
  }
  if (!entry_data.has_data) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "empty entry libmaxminddb '%s'",
             hostname);
    return maxminddb_return_null(&maxminddb_data);
  }
  return maxminddb_return_reusult(&maxminddb_data, entry_data.utf8_string);
}

static void maxminddb_deny(transfer *const tr, const char *country)
{
  char *msg;

  msg = mymalloc(maxtextlength);
  snprintf(msg, maxtextlength, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
  t_closeconn(tr, msg, 0);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
           "IP from other country (%s) detected", country);
  mydelete(msg);
}

/* check an IRC download against the GeoIP database */
void maxminddb_new_connection(transfer *const tr)
{
  const char *country;
  const char *group;

  if (gdata.maxminddb == NULL)
    return;

  country = check_maxminddb_socket(&(tr->con.remote.sa), tr->con.remoteaddr);
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

  if (verifyshell(&gdata.geoipexcludenick, tr->nick))
    return;

  if (no_verifyshell(&gdata.geoipcountry, country)) {
    maxminddb_deny(tr, country);
    return;
  }
  if (verifyshell(&gdata.nogeoipcountry, country)) {
    maxminddb_deny(tr, country);
    return;
  }
}

#ifndef WITHOUT_HTTP
static unsigned int http_check_country(const char *country)
{
  if (no_verifyshell(&gdata.geoipcountry, country))
    return 1;

  if (verifyshell(&gdata.nogeoipcountry, country))
    return 1;

  return 0;
}

/* check a HTTP connection against the GeoIP database */
unsigned int http_check_maxminddb(const struct sockaddr *const sa, const char *hostname)
{
  const char *country;

  if (gdata.http_geoip == 0)
    return 0;

  country = check_maxminddb_socket(sa, hostname);
  return http_check_country(country);
}
#endif /* WITHOUT_HTTP */

/* close GeoIP */
void maxminddb_shutdown(void)
{
  maxminddb_close(&maxminddb_data);
}

#endif /* USE_MAXMINDDB */

/* End of File */
