/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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
#include "dinoex_config.h"

#include <ctype.h>

typedef struct {
  const char *name;
  int *ivar;
} config_bool_typ;

typedef struct {
  const char *name;
  int *ivar;
  int min;
  int max;
  int mult;
} config_int_typ;

typedef struct {
  const char *name;
  char **svar;
  int flags;
} config_string_typ;

typedef struct {
  const char *name;
  irlist_t *list;
  int flags;
} config_list_typ;


static int config_bool_anzahl = 0;
static config_bool_typ config_parse_bool[] = {
{"auto_crc_check",         &gdata.auto_crc_check },
{"auto_default_group",     &gdata.auto_default_group },
{"auto_path_group",        &gdata.auto_path_group },
{"autoaddann_short",       &gdata.autoaddann_short },
{"balanced_queue",         &gdata.balanced_queue },
{"direct_file_access",     &gdata.direct_file_access },
{"disablexdccinfo",        &gdata.disablexdccinfo },
{"extend_status_line",     &gdata.extend_status_line },
{"getipfromserver",        &gdata.getipfromserver },
{"groupsincaps",           &gdata.groupsincaps },
{"hide_list_info",         &gdata.hide_list_info },
{"hidelockedpacks",        &gdata.hidelockedpacks },
{"hideos",                 &gdata.hideos },
{"holdqueue",              &gdata.holdqueue },
{"ignoreduplicateip",      &gdata.ignoreduplicateip },
{"ignoreuploadbandwidth",  &gdata.ignoreuploadbandwidth },
{"include_subdirs",        &gdata.include_subdirs },
{"logmessages",            &gdata.logmessages },
{"lognotices",             &gdata.lognotices },
{"logstats",               &gdata.logstats },
{"need_voice",             &gdata.need_voice },
{"noautorejoin",           &gdata.noautorejoin },
{"nocrc32",                &gdata.nocrc32 },
{"noduplicatefiles",       &gdata.noduplicatefiles },
{"nomd5sum",               &gdata.nomd5sum },
{"quietmode",              &gdata.quietmode },
{"removelostfiles",        &gdata.removelostfiles },
{"respondtochannellist",   &gdata.respondtochannellist },
{"respondtochannelxdcc",   &gdata.respondtochannelxdcc },
{"restrictlist",           &gdata.restrictlist },
{"restrictprivlist",       &gdata.restrictprivlist },
{"restrictprivlistfull",   &gdata.restrictprivlistfull },
{"restrictprivlistmain",   &gdata.restrictprivlistmain },
{"restrictsend",           &gdata.restrictsend },
{"restrictsend_warning",   &gdata.restrictsend_warning },
{"spaces_in_filenames",    &gdata.spaces_in_filenames },
{"timestampconsole",       &gdata.timestampconsole },
{"xdcclist_by_privmsg",    &gdata.xdcclist_by_privmsg},
{"xdcclist_grouponly",     &gdata.xdcclist_grouponly },
{"xdcclistfileraw",        &gdata.xdcclistfileraw },
{NULL, NULL }};

static int config_int_anzahl = 0;
static config_int_typ config_parse_int[] = {
{"adminlevel",              &gdata.adminlevel,              1, 5, 1 },
{"atfind",                  &gdata.atfind,                  0, 10, 1 },
{"autoadd_delay",           &gdata.autoadd_delay,           0, 65000, 1 },
{"autoadd_time",            &gdata.autoadd_time,            0, 65000, 1 },
{"autoignore_threshold",    &gdata.autoignore_threshold,    0, 600, 1 },
{"debug",                   &gdata.debug,                   0, 65000, 1 },
{"hadminlevel",             &gdata.hadminlevel,             1, 5, 1 },
{"http_port",               &gdata.http_port,               0, 65535, 1 },
{"idlequeuesize",           &gdata.idlequeuesize,           0, 1000000, 1 },
{"lowbdwth",                &gdata.lowbdwth,                0, 1000000, 1 },
{"max_find",                &gdata.max_find,                0, 65000, 1 },
{"max_uploads",             &gdata.max_uploads,             0, 65000, 1 },
{"max_upspeed",             &gdata.max_upspeed,             0, 1000000, 4 },
{"maxidlequeuedperperson",  &gdata.maxidlequeuedperperson,  1, 1000000, 1 },
{"maxqueueditemsperperson", &gdata.maxqueueditemsperperson, 1, 1000000, 1 },
{"maxtransfersperperson",   &gdata.maxtransfersperperson,   1, 1000000, 1 },
{"monitor_files",           &gdata.monitor_files,           1, 65000, 1 },
{"need_level",              &gdata.need_level,              0, 3, 0 },
{"notifytime",              &gdata.notifytime,              0, 1000000, 1 },
{"overallmaxspeed",         &gdata.overallmaxspeed,         0, 1000000, 4 },
{"overallmaxspeeddayspeed", &gdata.overallmaxspeeddayspeed, 0, 1000000, 4 },
{"punishslowusers",         &gdata.punishslowusers,         0, 1000000, 1 },
{"queuesize",               &gdata.queuesize,               0, 1000000, 1 },
{"restrictsend_delay",      &gdata.restrictsend_delay,      0, 2000, 1 },
{"restrictsend_timeout",    &gdata.restrictsend_timeout,    0, 600, 1 },
{"send_statefile_minute",   &gdata.send_statefile_minute,   0, 60, 1 },
{"smallfilebypass",         &gdata.smallfilebypass,         0, 1024*1024, 1024 },
{"start_of_month",          &gdata.start_of_month,          1, 31, 1 },
{"tcprangelimit",           &gdata.tcprangelimit,           1024, 65535, 1 },
{"tcprangestart",           &gdata.tcprangestart,           1024, 65530, 1 },
{"waitafterjoin",           &gdata.waitafterjoin,           0, 2000, 1 },
{NULL, NULL }};

static int config_string_anzahl = 0;
static config_string_typ config_parse_string[] = {
{"admin_job_file",          &gdata.admin_job_file,          0 },
{"adminpass",               &gdata.adminpass,               4 },
{"autoadd_group",           &gdata.autoadd_group,           0 },
{"autoadd_sort",            &gdata.autoadd_sort,            0 },
{"autoaddann",              &gdata.autoaddann,              0 },
{"creditline",              &gdata.creditline,              0 },
{"enable_nick",             &gdata.enable_nick,             0 },
{"geoipdatabase",           &gdata.geoipdatabase,           0 },
{"hadminpass",              &gdata.hadminpass,              4 },
{"headline",                &gdata.headline,                0 },
{"http_admin",              &gdata.http_admin,              0 },
{"http_dir",                &gdata.http_dir,                1 },
{"logfile",                 &gdata.logfile,                 1 },
{"loginname",               &gdata.loginname,               0 },
{"nickserv_pass",           &gdata.nickserv_pass,           0 },
{"owner_nick",              &gdata.owner_nick,              0 },
{"pidfile",                 &gdata.pidfile,                 1 },
{"respondtochannellistmsg", &gdata.respondtochannellistmsg, 0 },
{"restrictprivlistmsg",     &gdata.restrictprivlistmsg,     0 },
{"send_statefile",          &gdata.send_statefile,          0 },
{"uploaddir",               &gdata.uploaddir,               1 },
{"user_modes",              &gdata.user_modes,              0 },
{"user_nick",               &gdata.config_nick,             0 },
{"user_realname",           &gdata.user_realname,           0 },
{"xdcclistfile",            &gdata.xdcclistfile,            1 },
{"xdccremovefile",          &gdata.xdccremovefile,          1 },
{NULL, NULL }};

static int config_list_anzahl = 0;
static config_list_typ config_parse_list[] = {
{"adddir_exclude",          &gdata.adddir_exclude,          0 },
{"adminhost",               &gdata.adminhost,               3 },
{"autoadd_dir",             &gdata.autoadd_dirs,            0 },
{"autocrc_exclude",         &gdata.autocrc_exclude,         0 },
{"autoignore_exclude",      &gdata.autoignore_exclude,      2 },
{"downloadhost",            &gdata.downloadhost,            2 },
{"filedir",                 &gdata.filedir,                 1 },
{"geoipcountry",            &gdata.geoipcountry,            0 },
{"geoipexcludenick",        &gdata.geoipexcludenick,        0 },
{"hadminhost",              &gdata.hadminhost,              3 },
{"nodownloadhost",          &gdata.nodownloadhost,          2 },
{"proxyinfo",               &gdata.proxyinfo,               0 },
{"unlimitedhost",           &gdata.unlimitedhost,           2 },
{"uploadhost",              &gdata.uploadhost,              2 },
{NULL, NULL }};


static void config_sorted_bool(void)
{
  long i;

  for (i = 0L; config_parse_bool[i].name != NULL; i ++) {
    if (config_parse_bool[i + 1].name == NULL)
      break;
    if (strcmp(config_parse_bool[i].name, config_parse_bool[i + 1].name) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             config_parse_bool[i].name, config_parse_bool[i + 1].name);
  };
  config_bool_anzahl = i + 1;
}

static int config_find_bool(const char *key)
{ 
  int how_far;
  int bin_mid;
  int bin_low;
  int bin_high;
  
  if (config_bool_anzahl > 0L) {
    bin_low = 0;
    bin_high = config_bool_anzahl - 1;
    while (bin_low <= bin_high) {
      bin_mid = (bin_low + bin_high) / 2;
      how_far = strcmp(config_parse_bool[bin_mid].name, key);
      if (how_far == 0)
        return bin_mid;
      if (how_far < 0)
        bin_low = bin_mid + 1;
      else
        bin_high = bin_mid - 1;
    }
  };
  return -1;
}

int set_config_bool(const char *key, const char *text)
{
  int i;

  updatecontext();

  i = config_find_bool(key);
  if (i < 0)
    return 1;

  if (*text == 0) {
    *(config_parse_bool[i].ivar) = 1;
    return 0;
  }
  if (!strcmp("yes", text)) {
    *(config_parse_bool[i].ivar) = 1;
    return 0;
  }
  if (!strcmp("no", text)) {
    *(config_parse_bool[i].ivar) = 0;
    return 0;
  }
  outerror(OUTERROR_TYPE_WARN,
           "ignored '%s' because it has unknown args: '%s'",
           key, text);
  return 0;
}


static void config_sorted_int(void)
{
  long i;

  for (i = 0L; config_parse_int[i].name != NULL; i ++) {
    if (config_parse_int[i + 1].name == NULL)
      break;
    if (strcmp(config_parse_int[i].name, config_parse_int[i + 1].name) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             config_parse_int[i].name, config_parse_int[i + 1].name);
  };
  config_int_anzahl = i + 1;
}

static int config_find_int(const char *key)
{
  int how_far;
  int bin_mid;
  int bin_low;
  int bin_high;

  if (config_int_anzahl > 0L) {
    bin_low = 0;
    bin_high = config_int_anzahl - 1;
    while (bin_low <= bin_high) {
      bin_mid = (bin_low + bin_high) / 2;
      how_far = strcmp(config_parse_int[bin_mid].name, key);
      if (how_far == 0)
        return bin_mid;
      if (how_far < 0)
        bin_low = bin_mid + 1;
      else
        bin_high = bin_mid - 1;
    }
  };
  return -1;
}


int set_config_int(const char *key, const char *text)
{
  int i;
  int rawval;
  char *endptr;

  updatecontext();

  i = config_find_int(key);
  if (i < 0)
    return 1;

  if (*text == 0) {
    outerror(OUTERROR_TYPE_WARN,
             "ignored '%s' because it has no args.",
             key);
    return 0;
  }

  rawval = (int)strtol(text, &endptr, 0);
  if (endptr[0] != 0) {
    outerror(OUTERROR_TYPE_WARN,
             "ignored '%s' because it has invalid args: '%s'",
             key, text);
    return 0;
  }

  if ((rawval < config_parse_int[i].min) || (rawval > config_parse_int[i].max)) {
    outerror(OUTERROR_TYPE_WARN,
             "'%s': %d is out-of-range",
             key, rawval);
  }
  rawval = between(config_parse_int[i].min, rawval, config_parse_int[i].max);
  rawval *= config_parse_int[i].mult;
  *(config_parse_int[i].ivar) = rawval;
  return 0;
}


static void config_sorted_string(void)
{
  long i;

  for (i = 0L; config_parse_string[i].name != NULL; i ++) {
    if (config_parse_string[i + 1].name == NULL)
      break;
    if (strcmp(config_parse_string[i].name, config_parse_string[i + 1].name) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             config_parse_string[i].name, config_parse_string[i + 1].name);
  };
  config_string_anzahl = i + 1;
}

static int config_find_string(const char *key)
{
  int how_far;
  int bin_mid;
  int bin_low;
  int bin_high;

  if (config_string_anzahl > 0L) {
    bin_low = 0;
    bin_high = config_string_anzahl - 1;
    while (bin_low <= bin_high) {
      bin_mid = (bin_low + bin_high) / 2;
      how_far = strcmp(config_parse_string[bin_mid].name, key);
      if (how_far == 0)
        return bin_mid;
      if (how_far < 0)
        bin_low = bin_mid + 1;
      else
        bin_high = bin_mid - 1;
    }
  };
  return -1;
}

int set_config_string(const char *key, char *text)
{
  int i;

  updatecontext();

  i = config_find_string(key);
  if (i < 0)
    return 1;

  if (*text == 0) {
    outerror(OUTERROR_TYPE_WARN,
             "ignored '%s' because it has no args.",
             key);
    mydelete(text);
    return 0;
  }
  switch (config_parse_string[i].flags) {
  case 4:
     if (strchr(text, ' ') != NULL) {
       outerror(OUTERROR_TYPE_WARN,
                "ignored %s '%s' because it contains spaces",
                key, text);
       return 0;
     }
     checkadminpass2(text);
  case 1:
     convert_to_unix_slash(text);
     break;
  }
  mydelete(*(config_parse_string[i].svar));
  *(config_parse_string[i].svar) = text;
  return 0;
}


static void config_sorted_list(void)
{
  long i;

  for (i = 0L; config_parse_list[i].name != NULL; i ++) {
    if (config_parse_list[i + 1].name == NULL)
      break;
    if (strcmp(config_parse_list[i].name, config_parse_list[i + 1].name) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             config_parse_list[i].name, config_parse_list[i + 1].name);
  };
  config_list_anzahl = i + 1;
}

static int config_find_list(const char *key)
{ 
  int how_far;
  int bin_mid;
  int bin_low;
  int bin_high;
  
  if (config_list_anzahl > 0L) {
    bin_low = 0;
    bin_high = config_list_anzahl - 1;
    while (bin_low <= bin_high) {
      bin_mid = (bin_low + bin_high) / 2;
      how_far = strcmp(config_parse_list[bin_mid].name, key);
      if (how_far == 0)
        return bin_mid;
      if (how_far < 0)
        bin_low = bin_mid + 1;
      else
        bin_high = bin_mid - 1;
    }
  };
  return -1;
}

int set_config_list(const char *key, char *text)
{
  int i;
  int j;
  char *pi;
  char *mask;
  regex_t *ah;

  updatecontext();

  i = config_find_list(key);
  if (i < 0)
    return 1;

  if (*text == 0) {
    outerror(OUTERROR_TYPE_WARN,
             "ignored '%s' because it has no args.",
             key);
    mydelete(text);
    return 0;
  }
  switch (config_parse_list[i].flags) {
  case 3:
     for (j=strlen(text)-1; j>=0; j--) {
       if (text[j] == '@') {
         j = 0;
       } else if ((text[j] != '*') && (text[j] != '?') && (text[j] != '#')) {
         break;
       }
     }
     if ((j<0) || (!strchr(text,'@'))) {
       outerror(OUTERROR_TYPE_WARN,
                "ignored %s '%s' because it's way too vague",
                config_parse_list[i].name, text);
       mydelete(text);
       return 0;
     }
     /* FALLTHROUGH */
  case 2:
     if (strchr(text, ' ') != NULL) {
       outerror(OUTERROR_TYPE_WARN,
                "ignored %s '%s' because it contains spaces",
                key, text);
       return 0;
     }
     caps(text);
     mask = hostmasktoregex(text);
     ah = irlist_add(config_parse_list[i].list, sizeof(regex_t));
     if (regcomp(ah, mask, REG_ICASE|REG_NOSUB)) {
         irlist_delete(config_parse_list[i].list, ah);
     }
     mydelete(mask);
     mydelete(text);
     return 0;
  case 1:
     convert_to_unix_slash(text);
     break;
  }
  pi = irlist_add(config_parse_list[i].list, strlen(text) + 1);
  strcpy(pi, text);
  mydelete(text);
  return 0;
}


void config_startup(void)
{
  config_sorted_bool();
  config_sorted_int();
  config_sorted_string();
  config_sorted_list();
}

/* EOF */
