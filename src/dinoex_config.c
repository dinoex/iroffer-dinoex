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

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_irc.h"
#include "dinoex_admin.h"
#include "dinoex_http.h"
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

typedef struct {
  const char *name;
  void (*func)(char *var);
} config_func_typ;


const char *current_config;
long current_line;


static int config_bool_anzahl = 0;
static config_bool_typ config_parse_bool[] = {
{"auto_crc_check",         &gdata.auto_crc_check },
{"auto_default_group",     &gdata.auto_default_group },
{"auto_path_group",        &gdata.auto_path_group },
{"autoaddann_short",       &gdata.autoaddann_short },
{"balanced_queue",         &gdata.balanced_queue },
{"direct_config_access",   &gdata.direct_config_access },
{"direct_file_access",     &gdata.direct_file_access },
{"disablexdccinfo",        &gdata.disablexdccinfo },
{"dos_text_files",         &gdata.dos_text_files },
{"extend_status_line",     &gdata.extend_status_line },
#ifndef WITHOUT_BLOWFISH
{"fish_only",              &gdata.fish_only },
#endif /* WITHOUT_BLOWFISH */
{"getipfromserver",        &gdata.getipfromserver },
#ifdef USE_UPNP
{"getipfromupnp",          &gdata.getipfromupnp },
#endif /* USE_UPNP */
{"groupsincaps",           &gdata.groupsincaps },
{"hide_list_info",         &gdata.hide_list_info },
{"hidelockedpacks",        &gdata.hidelockedpacks },
{"hideos",                 &gdata.hideos },
{"holdqueue",              &gdata.holdqueue },
#ifndef WITHOUT_HTTP
{"http_geoip",             &gdata.http_geoip },
{"http_search",            &gdata.http_search },
#endif /* WITHOUT_HTTP */
{"ignoreduplicateip",      &gdata.ignoreduplicateip },
{"ignoreuploadbandwidth",  &gdata.ignoreuploadbandwidth },
{"include_subdirs",        &gdata.include_subdirs },
{"logmessages",            &gdata.logmessages },
{"lognotices",             &gdata.lognotices },
{"logstats",               &gdata.logstats },
{"mirc_dcc64",             &gdata.mirc_dcc64 },
{"need_voice",             &gdata.need_voice },
{"no_auto_rehash",         &gdata.no_auto_rehash },
{"no_duplicate_filenames", &gdata.no_duplicate_filenames },
{"no_find_trigger",        &gdata.no_find_trigger },
{"no_minspeed_on_free",    &gdata.no_minspeed_on_free },
{"no_status_chat",         &gdata.no_status_chat },
{"no_status_log",          &gdata.no_status_log },
{"noautorejoin",           &gdata.noautorejoin },
{"nocrc32",                &gdata.nocrc32 },
{"noduplicatefiles",       &gdata.noduplicatefiles },
{"nomd5sum",               &gdata.nomd5sum },
{"old_statefile",          &gdata.old_statefile },
{"passive_dcc",            &gdata.passive_dcc },
#ifndef WITHOUT_BLOWFISH
{"privmsg_encrypt",        &gdata.privmsg_encrypt },
#endif /* WITHOUT_BLOWFISH */
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
{"send_batch",             &gdata.send_batch },
{"show_date_added",        &gdata.show_date_added },
{"show_list_all",          &gdata.show_list_all },
{"spaces_in_filenames",    &gdata.spaces_in_filenames },
{"timestampconsole",       &gdata.timestampconsole },
#ifdef USE_UPNP
{"upnp_router",            &gdata.upnp_router },
#endif /* USE_UPNP */
{"verbose_crc32",          &gdata.verbose_crc32},
{"xdcclist_by_privmsg",    &gdata.xdcclist_by_privmsg},
{"xdcclist_grouponly",     &gdata.xdcclist_grouponly },
{"xdcclistfileraw",        &gdata.xdcclistfileraw },
{NULL, NULL }};


void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
dump_line(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vioutput(CALLTYPE_NORMAL, OUT_L, COLOR_NO_COLOR, format, args);
   va_end(args);
}

void dump_config_int2(const char *name, int val)
{
  dump_line("GDATA * " "%s: %d", name, val);
}

void dump_config_string2(const char *name, const char *val)
{
  dump_line("GDATA * " "%s: %s", name, val ? val : "<undef>" );
}

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

static int parse_bool_val(const char *key, const char *text)
{
  if (text == NULL) {
    return 1;
  }
  if (!strcmp("yes", text)) {
    return 1;
  }
  if (!strcmp("no", text)) {
    return 0;
  }
  outerror(OUTERROR_TYPE_WARN,
           "%s:%ld ignored '%s' because it has unknown args: '%s'",
           current_config, current_line, key, text);
  return -1;
}

static int set_config_bool(const char *key, const char *text)
{
  int i;
  int val;

  updatecontext();

  i = config_find_bool(key);
  if (i < 0)
    return 1;

  val = parse_bool_val(key, text);
  if (val != -1 ) {
    *(config_parse_bool[i].ivar) = val;
  }
  return 0;
}

static char *print_config_bool(const char *key)
{
  int i;
  const char *val;

  updatecontext();

  i = config_find_bool(key);
  if (i < 0)
    return NULL;

  val = (*(config_parse_bool[i].ivar) != 0) ? "yes" : "no";
  return mystrdup(val);
}

static void dump_config_bool(void)
{
  long i;

  for (i = 0L; config_parse_bool[i].name != NULL; i ++) {
    dump_config_int2(config_parse_bool[i].name, *(config_parse_bool[i].ivar));
  }
}


static int config_int_anzahl = 0;
static config_int_typ config_parse_int[] = {
{"adminlevel",              &gdata.adminlevel,              1, 5, 1 },
{"atfind",                  &gdata.atfind,                  0, 10, 1 },
{"autoadd_delay",           &gdata.autoadd_delay,           0, 65000, 1 },
{"autoadd_time",            &gdata.autoadd_time,            0, 65000, 1 },
{"autoignore_threshold",    &gdata.autoignore_threshold,    0, 600, 1 },
{"debug",                   &gdata.debug,                   0, 65000, 1 },
{"fileremove_max_packs",    &gdata.fileremove_max_packs,    0, 1000000, 1 },
{"hadminlevel",             &gdata.hadminlevel,             1, 5, 1 },
#ifndef WITHOUT_HTTP
{"http_port",               &gdata.http_port,               0, 65535, 1 },
#endif /* WITHOUT_HTTP */
{"idlequeuesize",           &gdata.idlequeuesize,           0, 1000000, 1 },
{"lowbdwth",                &gdata.lowbdwth,                0, 1000000, 1 },
{"max_find",                &gdata.max_find,                0, 65000, 1 },
{"max_uploads",             &gdata.max_uploads,             0, 65000, 1 },
{"max_upspeed",             &gdata.max_upspeed,             0, 1000000, 4 },
{"maxidlequeuedperperson",  &gdata.maxidlequeuedperperson,  1, 1000000, 1 },
{"maxqueueditemsperperson", &gdata.maxqueueditemsperperson, 1, 1000000, 1 },
{"maxtransfersperperson",   &gdata.maxtransfersperperson,   1, 1000000, 1 },
{"monitor_files",           &gdata.monitor_files,           1, 65000, 1 },
{"need_level",              &gdata.need_level,              0, 3, 1 },
{"new_trigger",             &gdata.new_trigger,             0, 1000000, 1 },
{"notifytime",              &gdata.notifytime,              0, 1000000, 1 },
{"overallmaxspeed",         &gdata.overallmaxspeed,         0, 1000000, 4 },
{"overallmaxspeeddayspeed", &gdata.overallmaxspeeddayspeed, 0, 1000000, 4 },
{"punishslowusers",         &gdata.punishslowusers,         0, 1000000, 1 },
{"queuesize",               &gdata.queuesize,               0, 1000000, 1 },
{"reconnect_delay",         &gdata.reconnect_delay,         0, 2000, 1 },
{"remove_dead_users",       &gdata.remove_dead_users,       0, 2, 1 },
{"restrictsend_delay",      &gdata.restrictsend_delay,      0, 2000, 1 },
{"restrictsend_timeout",    &gdata.restrictsend_timeout,    0, 600, 1 },
{"send_listfile",           &gdata.send_listfile,           -1, 1000000, 1 },
{"send_statefile_minute",   &gdata.send_statefile_minute,   0, 60, 1 },
{"smallfilebypass",         &gdata.smallfilebypass,         0, 1024*1024, 1024 },
{"start_of_month",          &gdata.start_of_month,          1, 31, 1 },
{"tcprangelimit",           &gdata.tcprangelimit,           1024, 65535, 1 },
{"tcprangestart",           &gdata.tcprangestart,           1024, 65530, 1 },
#ifndef WITHOUT_TELNET
{"telnet_port",             &gdata.telnet_port,             0, 65535, 1 },
#endif /* WITHOUT_TELNET */
{"waitafterjoin",           &gdata.waitafterjoin,           0, 2000, 1 },
{NULL, NULL, 0, 0, 0 }};

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

static int report_no_arg(const char *key, const char *text)
{
  if (text == NULL) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has no args.",
             current_config, current_line, key);
    return 1;
  }
  if (*text == 0) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has no args.",
             current_config, current_line, key);
    return 1;
  }
  return 0;
}

static int check_range(const char *key, const char *text, int *val, int min, int max)
{
  int rawval;
  char *endptr;

  if (report_no_arg(key, text))
    return 1;

  rawval = (int)strtol(text, &endptr, 0);
  if (endptr[0] != 0) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has invalid args: '%s'",
             current_config, current_line, key, text);
    return 1;
  }

  if ((rawval < min) || (rawval > max)) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld '%s': %d is out-of-range",
             current_config, current_line, key, rawval);
  }
  *val = between(min, rawval, max);
  return 0;
}

static int set_config_int(const char *key, const char *text)
{
  int i;
  int rawval;

  updatecontext();

  i = config_find_int(key);
  if (i < 0)
    return 1;

  if (check_range(key, text, &rawval, config_parse_int[i].min, config_parse_int[i].max))
    return 0;

  rawval *= config_parse_int[i].mult;
  *(config_parse_int[i].ivar) = rawval;
  return 0;
}

static char *print_config_int(const char *key)
{
  int i;
  char *val;

  updatecontext();

  i = config_find_int(key);
  if (i < 0)
    return NULL;

  val = mycalloc(maxtextlengthshort);
  snprintf(val, maxtextlengthshort, "%d", *(config_parse_int[i].ivar));
  return val;
}

static void dump_config_int(void)
{
  long i;

  for (i = 0L; config_parse_int[i].name != NULL; i ++) {
    dump_config_int2(config_parse_bool[i].name, *(config_parse_bool[i].ivar));
  }
}


/*  flags for strings
 0 -> literal string
 1 -> pathname
 4 -> adminpass
 */

static int config_string_anzahl = 0;
static config_string_typ config_parse_string[] = {
{"admin_job_file",          &gdata.admin_job_file,          1 },
{"adminpass",               &gdata.adminpass,               4 },
{"announce_seperator",      &gdata.announce_seperator,      0 },
{"autoadd_group",           &gdata.autoadd_group,           0 },
{"autoadd_sort",            &gdata.autoadd_sort,            0 },
{"autoaddann",              &gdata.autoaddann,              0 },
{"charset",                 &gdata.charset,                 0 },
{"creditline",              &gdata.creditline,              0 },
{"enable_nick",             &gdata.enable_nick,             0 },
#ifdef USE_GEOIP
{"geoipdatabase",           &gdata.geoipdatabase,           0 },
#endif /* USE_GEOIP */
{"group_seperator",         &gdata.group_seperator,         0 },
{"hadminpass",              &gdata.hadminpass,              4 },
{"headline",                &gdata.headline,                0 },
#ifndef WITHOUT_HTTP_ADMIN
{"http_admin",              &gdata.http_admin,              0 },
#endif /* WITHOUT_HTTP */
{"http_date",               &gdata.http_date,               0 },
#ifndef WITHOUT_HTTP
{"http_dir",                &gdata.http_dir,                1 },
#endif /* WITHOUT_HTTP */
{"local_vhost",             &gdata.local_vhost,             0 },
{"logfile",                 &gdata.logfile,                 1 },
{"logfile_httpd",           &gdata.logfile_httpd,           1 },
{"logfile_messages",        &gdata.logfile_messages,        1 },
{"logfile_notices",         &gdata.logfile_notices,         1 },
{"loginname",               &gdata.loginname,               0 },
{"nickserv_pass",           &gdata.nickserv_pass,           0 },
{"owner_nick",              &gdata.owner_nick,              0 },
{"pidfile",                 &gdata.pidfile,                 1 },
#ifndef WITHOUT_BLOWFISH
{"privmsg_fish",            &gdata.privmsg_fish,            0 },
#endif /* WITHOUT_BLOWFISH */
{"respondtochannellistmsg", &gdata.respondtochannellistmsg, 0 },
{"restrictprivlistmsg",     &gdata.restrictprivlistmsg,     0 },
#ifdef USE_RUBY
{"ruby_script",             &gdata.ruby_script,             1 },
#endif /* USE_RUBY */
{"send_statefile",          &gdata.send_statefile,          0 },
{"trashcan_dir",            &gdata.trashcan_dir,            1 },
{"uploaddir",               &gdata.uploaddir,               1 },
{"usenatip",                &gdata.usenatip,                0 },
{"user_modes",              &gdata.user_modes,              0 },
{"user_nick",               &gdata.config_nick,             0 },
{"user_realname",           &gdata.user_realname,           0 },
{"xdcclistfile",            &gdata.xdcclistfile,            1 },
{"xdccremovefile",          &gdata.xdccremovefile,          1 },
{"xdccxmlfile",             &gdata.xdccxmlfile,             1 },
{NULL, NULL, 0 }};

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

static int set_config_string(const char *key, char *text)
{
  int i;

  updatecontext();

  i = config_find_string(key);
  if (i < 0)
    return 1;

  if (report_no_arg(key, text))
    return 0;

  switch (config_parse_string[i].flags) {
  case 4:
     if (strchr(text, ' ') != NULL) {
       outerror(OUTERROR_TYPE_WARN,
                "%s:%ld ignored %s '%s' because it contains spaces",
                current_config, current_line, key, text);
       return 0;
     }
     checkadminpass2(text);
  case 1:
     convert_to_unix_slash(text);
     break;
  }
  mydelete(*(config_parse_string[i].svar));
  *(config_parse_string[i].svar) = mystrdup(text);
  return 0;
}

static char *print_config_string(const char *key)
{
  int i;
  const char *val;

  updatecontext();

  i = config_find_string(key);
  if (i < 0)
    return NULL;

  val = *(config_parse_string[i].svar);
  return mystrdup(val);
}

static void dump_config_string(void)
{
  long i;

  for (i = 0L; config_parse_string[i].name != NULL; i ++) {
    dump_config_string2(config_parse_string[i].name, *(config_parse_string[i].svar));
  }
}


/*  flags for lists
 0 -> literal string
 1 -> pathname
 2 -> hostmask
 3 -> admin hostmask
 5 -> netmask
 */

static int config_list_anzahl = 0;
static config_list_typ config_parse_list[] = {
{"adddir_exclude",          &gdata.adddir_exclude,          0 },
{"adminhost",               &gdata.adminhost,               3 },
{"autoadd_dir",             &gdata.autoadd_dirs,            0 },
{"autocrc_exclude",         &gdata.autocrc_exclude,         0 },
{"autoignore_exclude",      &gdata.autoignore_exclude,      2 },
{"downloadhost",            &gdata.downloadhost,            2 },
{"filedir",                 &gdata.filedir,                 1 },
#ifndef WITHOUT_BLOWFISH
{"fish_exclude_nick",       &gdata.fish_exclude_nick,       2 },
#endif /* WITHOUT_BLOWFISH */
#ifdef USE_GEOIP
{"geoipcountry",            &gdata.geoipcountry,            0 },
{"geoipexcludegroup",       &gdata.geoipexcludegroup,       0 },
{"geoipexcludenick",        &gdata.geoipexcludenick,        0 },
#endif /* USE_GEOIP */
{"hadminhost",              &gdata.hadminhost,              3 },
#ifndef WITHOUT_HTTP
{"http_allow",              &gdata.http_allow,              5 },
{"http_deny",               &gdata.http_deny,               5 },
{"http_vhost",              &gdata.http_vhost,              0 },
#endif /* WITHOUT_HTTP */
{"log_exclude_host",        &gdata.log_exclude_host,        2 },
{"log_exclude_text",        &gdata.log_exclude_text,        0 },
{"nodownloadhost",          &gdata.nodownloadhost,          2 },
#ifdef USE_GEOIP
{"nogeoipcountry",          &gdata.nogeoipcountry,          0 },
#endif /* USE_GEOIP */
#ifndef WITHOUT_TELNET
{"telnet_vhost",            &gdata.telnet_vhost,            0 },
#endif /* WITHOUT_TELNET */
{"unlimitedhost",           &gdata.unlimitedhost,           2 },
{"uploadhost",              &gdata.uploadhost,              2 },
{"weblist_info",            &gdata.weblist_info,            0 },
{"xdcc_allow",              &gdata.xdcc_allow,              5 },
{"xdcc_deny",               &gdata.xdcc_deny,               5 },
{NULL, NULL, 0 }};

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

static int get_netmask(char *text, int init)
{
  char *work;
  int newmask;

  work = strchr(text, '/');
  if (work != NULL) {
    *(work++) = 0;
    newmask = atoi(work);
    if (newmask < 0 )
      return -1;
    if (newmask > init )
      return -1;
    return newmask;
  }
  return init;
}

static int set_config_list(const char *key, char *text)
{
  int i;
  int j;
  int e;
  char *mask;
  ir_cidr_t *cidr;

  updatecontext();

  i = config_find_list(key);
  if (i < 0)
    return 1;

  if (report_no_arg(key, text))
    return 0;

  switch (config_parse_list[i].flags) {
  case 5:
    cidr = irlist_add(config_parse_list[i].list, sizeof(ir_cidr_t));
    if (strchr(text, ':') == NULL) {
      cidr->family = AF_INET;
      cidr->netmask = get_netmask(text, 32);
      e = inet_pton(cidr->family, text, &(cidr->remote.sin.sin_addr));
    } else {
      cidr->family = AF_INET6;
      cidr->netmask = get_netmask(text, 128);
      e = inet_pton(cidr->family, text, &(cidr->remote.sin6.sin6_addr));
    }
    cidr->remote.sa.sa_family = cidr->family;
    if (e != 1) {
       outerror(OUTERROR_TYPE_WARN,
                "%s:%ld ignored '%s' because it has invalid args: '%s'",
                current_config, current_line, key, text);
       irlist_delete(config_parse_list[i].list, cidr);
    }
    return 0;
  case 3:
     for (j=strlen(text)-1; j>=0; j--) {
       if (text[j] == '@') {
         j = 0;
       } else if ((text[j] != '*') && (text[j] != '?') && (text[j] != '#')) {
         break;
       }
     }
     if ((j<0) || (!strchr(text, '@'))) {
       outerror(OUTERROR_TYPE_WARN,
                "%s:%ld ignored %s '%s' because it's way too vague",
                current_config, current_line, config_parse_list[i].name, text);
       return 0;
     }
     /* FALLTHROUGH */
  case 2:
     if (strchr(text, ' ') != NULL) {
       outerror(OUTERROR_TYPE_WARN,
                "%s:%ld ignored %s '%s' because it contains spaces",
                current_config, current_line, key, text);
       return 0;
     }
     mask = hostmask_to_fnmatch(text);
     irlist_add_string(config_parse_list[i].list, mask);
     mydelete(mask);
     return 0;
  case 1:
     convert_to_unix_slash(text);
     break;
  }
  irlist_add_string(config_parse_list[i].list, text);
  return 0;
}

static void dump_config_list(void)
{
  ir_cidr_t *cidr;
  char *string;
  long i;
  char ip6[maxtextlengthshort];

  for (i = 0L; config_parse_list[i].name != NULL; i ++) {
    dump_line("GDATA * " "%s:",
            config_parse_list[i].name);
    switch (config_parse_list[i].flags) {
    case 5:
      for (cidr = irlist_get_head(config_parse_list[i].list);
           cidr;
           cidr = irlist_get_next(cidr)) {
        dump_line("  " ": %s %d", "family", cidr->family);
        dump_line("  " ": %s %d", "netmask", cidr->netmask);
        my_getnameinfo(ip6, maxtextlengthshort -1, &(cidr->remote.sa), 0);
        dump_line("  " ": %s %s", "remoteip", ip6);
      }
      break;
    default:
      for (string = irlist_get_head(config_parse_list[i].list);
           string;
           string = irlist_get_next(string)) {
        dump_line("  " ": %s", string);
      }
    }
  }
}


static void set_default_network_name(void)
{
  char *var;

  if (gdata.networks[gdata.networks_online].name != NULL) {
    if (gdata.networks[gdata.networks_online].name[0] != 0) {
      return;
    }
    mydelete(gdata.networks[gdata.networks_online].name);
  }

  var = mymalloc(10);
  snprintf(var, 10, "%d", gdata.networks_online + 1);
  gdata.networks[gdata.networks_online].name = mystrdup(var);
  return;
}

static int parse_channel_int(short *iptr, char **part, int i)
{
  char *tptr2;

  tptr2 = part[++i];
  if (!tptr2)
    return -1;
  *iptr = atoi(tptr2);
  mydelete(tptr2);
  return 1;
}

static int parse_channel_string(char **cptr, char **part, int i)
{
  char *tptr2;

  tptr2 = part[++i];
  if (!tptr2)
    return -1;
  *cptr = tptr2;
  return 1;
}

static int parse_channel_format(short *iptr, char *tptr2)
{
  if (!tptr2)
    return -1;

  if (!strcmp(tptr2, "full"))
    return 1;
  if (!strcmp(tptr2, "minimal")) {
    *iptr |= CHAN_MINIMAL;
    return 1;
  }
  if (!strcmp(tptr2, "summary")) {
    *iptr |= CHAN_SUMMARY;
    return 1;
  }
  return -1;
}

static int parse_channel_option(channel_t *cptr, char *tptr, char **part, int i)
{
  char *tptr2;
  int j;

  if (!strcmp(tptr, "-plist")) {
    return parse_channel_int(&(cptr->plisttime), part, i);
  }
  if (!strcmp(tptr, "-plistoffset")) {
    return parse_channel_int(&(cptr->plistoffset), part, i);
  }
  if (!strcmp(tptr, "-delay")) {
    return parse_channel_int(&(cptr->delay), part, i);
  }
  if (!strcmp(tptr, "-waitjoin")) {
    return parse_channel_int(&(cptr->waitjoin), part, i);
  }

  if (!strcmp(tptr, "-key")) {
    return parse_channel_string(&(cptr->key), part, i);
  }
#ifndef WITHOUT_BLOWFISH
  if (!strcmp(tptr, "-fish")) {
    return parse_channel_string(&(cptr->fish), part, i);
  }
#endif /* WITHOUT_BLOWFISH */
  if (!strcmp(tptr, "-pgroup")) {
    return parse_channel_string(&(cptr->pgroup), part, i);
  }
  if (!strcmp(tptr, "-joinmsg")) {
    return parse_channel_string(&(cptr->joinmsg), part, i);
  }
  if (!strcmp(tptr, "-headline")) {
    return parse_channel_string(&(cptr->headline), part, i);
  }
  if (!strcmp(tptr, "-listmsg")) {
    return parse_channel_string(&(cptr->listmsg), part, i);
  }
  if (!strcmp(tptr, "-rgroup")) {
    return parse_channel_string(&(cptr->rgroup), part, i);
  }

  if (!strcmp(tptr, "-noannounce")) {
    cptr->noannounce = 1;
    return 0;
  }
  if (!strcmp(tptr, "-notrigger")) {
    cptr->notrigger = 1;
    return 0;
  }

  if (!strcmp(tptr, "-pformat")) {
    tptr2 = part[++i];
    j = parse_channel_format(&(cptr->flags), tptr2);
    mydelete(tptr2);
    return j;
  }

  return -1;
}

#define MAX_CHANNEL_OPTIONS 20

static int parse_channel_options(channel_t *cptr, char *var)
{
  char *part[MAX_CHANNEL_OPTIONS];
  char *tptr;
  int i;
  int j;
  int m;

  m = get_argv(part, var, MAX_CHANNEL_OPTIONS);
  for (i=0; i<m; i++) {
    tptr = part[i];
    if (tptr == NULL)
      break;
    j = parse_channel_option(cptr, tptr, part, i);
    mydelete(tptr);
    if (j < 0 )
      return -1;
    i += j;
  }
  return 0;
}


static void c_autoadd_group_match(char *var)
{
  char *split;
  autoadd_group_t *ag;

  split = strchr(var, ' ');
  if (split == NULL) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has invalid args: '%s'",
             current_config, current_line, "autoadd_group_match", var);
    return;
  }

  *(split++) = 0;
  ag = irlist_add(&(gdata.autoadd_group_match), sizeof(autoadd_group_t));
  ag->a_group = mystrdup(var);
  ag->a_pattern = mystrdup(split);
}

static void c_autosendpack(char *var)
{
  char *part[3];
  int rawval;

  get_argv(part, var, 3);
  if (part[0] && part[1]) {
    autoqueue_t *aq;
    aq = irlist_add(&gdata.autoqueue, sizeof(autoqueue_t));
    rawval = atoi(part[0]);
    aq->pack = between(0, rawval, 100000);
    aq->word = part[1];
    aq->message = part[2];
  } else {
    mydelete(part[1]);
    mydelete(part[2]);
  }
  mydelete(part[0]);
}

static void c_channel(char *var)
{
  char *part[2];
  channel_t *cptr;
  channel_t *rch;

  updatecontext();

  get_argv(part, var, 2);
  if (part[0] == NULL)
    return;

  for (rch = irlist_get_head(&gdata.networks[gdata.networks_online].channels);
       rch;
       rch = irlist_get_next(rch)) {
    if (strcmp(rch->name, part[0]) == 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "could not add channel %s twice!", part[0]);
      mydelete(part[0]);
      mydelete(part[1]);
      return;
    }
  }

  cptr = irlist_add(&gdata.networks[gdata.networks_online].channels, sizeof(channel_t));
  cptr->name = part[0];
  caps(cptr->name);
  if (parse_channel_options(cptr, part[1]) < 0) {
     outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored channel '%s' on %s because it has invalid args: '%s'",
             current_config, current_line, part[0],
             gdata.networks[gdata.networks_online].name, part[1]);
  }

  if (cptr->plisttime && (cptr->plistoffset >= cptr->plisttime)) {
     outerror(OUTERROR_TYPE_WARN,
             "%s:%ld channel '%s' on %s: plistoffset must be less than plist time, ignoring offset",
             current_config, current_line, part[0], gdata.networks[gdata.networks_online].name);
     cptr->plistoffset = 0;
  }
  mydelete(part[1]);
}

static void c_channel_join_raw(char *var)
{
   irlist_add_string(&gdata.networks[gdata.networks_online].channel_join_raw, var);
}

static void c_connectionmethod(char *var)
{
  char *part[5];
  int m;

  mydelete(gdata.networks[gdata.networks_online].connectionmethod.host);
  mydelete(gdata.networks[gdata.networks_online].connectionmethod.password);
  mydelete(gdata.networks[gdata.networks_online].connectionmethod.vhost);
  bzero((char *) &gdata.networks[gdata.networks_online].connectionmethod, sizeof(connectionmethod_t));

  m = get_argv(part, var, 5);
  if (part[1]) {
    gdata.networks[gdata.networks_online].connectionmethod.host = part[1];
    part[1] = NULL;
  }
  if (part[2]) {
    gdata.networks[gdata.networks_online].connectionmethod.port = atoi(part[2]);
  }
  if (part[2]) {
    gdata.networks[gdata.networks_online].connectionmethod.password = part[3];
    part[2] = NULL;
  }
  if (part[4]) {
    gdata.networks[gdata.networks_online].connectionmethod.vhost = part[4];
    part[4] = NULL;
  }
  if (part[0] != NULL) {
    if (!strcmp(part[0], "direct")) {
      gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
    } else if (!strcmp(part[0], "ssl")) {
#ifdef USE_SSL
      gdata.networks[gdata.networks_online].connectionmethod.how = how_ssl;
#else
      gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "%s:%ld connectionmethod ssl not compiled, defaulting to direct",
               current_config, current_line);
#endif /* USE_SSL */
    } else if ((m >= 4) && !strcmp(part[0], "bnc")) {
      gdata.networks[gdata.networks_online].connectionmethod.how = how_bnc;
    } else if ((m == 2) && !strcmp(part[0], "wingate")) {
      gdata.networks[gdata.networks_online].connectionmethod.how = how_wingate;
    } else if ((m == 2) && !strcmp(part[0], "custom")) {
      gdata.networks[gdata.networks_online].connectionmethod.how = how_custom;
    } else {
      gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "%s:%ld Invalid connectionmethod %s in config file, defaulting to direct",
               current_config, current_line, part[0]);
    }
  }
  mydelete(part[0]);
  mydelete(part[1]);
  mydelete(part[2]);
  mydelete(part[3]);
  mydelete(part[4]);
}

static void c_disk_quota(char *var)
{
  gdata.disk_quota = atoull(var)*1024*1024;
}

static void c_getip_network(char *var)
{
  gdata.networks[gdata.networks_online].getip_net = get_network(var);
}

static void c_group_admin(char *var)
{
  group_admin_t *ga;
  char *data;
  int stufe = 0;
  int drop = 0;

  ga = irlist_add(&(gdata.group_admin), sizeof(group_admin_t));
  for (data = strtok(var, " ");
       data && (drop == 0);
       data = strtok(NULL, " ")) {
    if (data[0] == 0)
      continue;

    switch (++stufe) {
    case 1:
      ga->g_level = atoi(data);
      break;
    case 2:
      ga->g_host = mystrdup(data);
      break;
    case 3:
      ga->g_pass = mystrdup(data);
      break;
    case 4:
      ga->g_groups = mystrdup(data);
      break;
    default:
      drop ++;
      break;
    }
  }
  if (!ga->g_groups)
    drop ++;

  if (drop) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has unknown args: '%s'",
             current_config, current_line, "group_admin", var);
    mydelete(ga->g_host);
    mydelete(ga->g_pass);
    mydelete(ga->g_groups);
    ga = irlist_delete(&gdata.group_admin, ga);
  }
}

static void c_logrotate(char *var)
{
  if (!strcmp(var, "daily"))   gdata.logrotate =    24*60*60;
  if (!strcmp(var, "weekly"))  gdata.logrotate =  7*24*60*60;
  if (!strcmp(var, "monthly")) gdata.logrotate = 30*24*60*60;
}

static void c_local_vhost(char *var)
{
  mydelete(gdata.networks[gdata.networks_online].local_vhost);
  gdata.networks[gdata.networks_online].local_vhost = mystrdup(var);
}

static void c_mime_type(char *var)
{
  char *split;
  http_magic_t *mime;

  split = strchr(var, ' ');
  if (split == NULL) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has invalid args: '%s'",
             current_config, current_line, "mime_type", var);
    return;
  }

  *(split++) = 0;
  mime = irlist_add(&(gdata.mime_type), sizeof(http_magic_t));
  mime->m_ext = mystrdup(var);
  mime->m_mime = split;
}

static void c_need_level(char *var)
{
  const char *key = "need_level";
  int rawval;

  if (check_range(key, var, &rawval, 0, 3) == 0) {
    gdata.networks[gdata.networks_online].need_level = rawval;
  }
}

static void c_network(char *var)
{
  gdata.bracket = 0;
  if (gdata.networks_online == 0) {
     /* add new network only when server defined */
     if (irlist_size(&gdata.networks[gdata.networks_online].servers))
       gdata.networks_online ++;
  } else {
    gdata.networks_online ++;
  }
  if (gdata.networks_online >= MAX_NETWORKS) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored network '%s' because we have to many.",
             current_config, current_line, var);
    return;
  }
  gdata.networks[gdata.networks_online].net = gdata.networks_online;
  mydelete(gdata.networks[gdata.networks_online].name);
  if (var[0] != 0) {
    gdata.networks[gdata.networks_online].name = mystrdup(var);
  } else {
    set_default_network_name();
  }
}

static void c_nickserv_pass(char *var)
{
  mydelete(gdata.networks[gdata.networks_online].nickserv_pass);
  gdata.networks[gdata.networks_online].nickserv_pass = mystrdup(var);
}

static void c_noannounce(char *var)
{
  int val;

  val = parse_bool_val("noannounce", var);
  if (val != -1 ) {
    gdata.networks[gdata.networks_online].noannounce = val;
  }
}

static void c_overallmaxspeeddaydays(char *var)
{
  char *src;
  int i;

  gdata.overallmaxspeeddaydays = 0;
  src = var;
  for (i=0; (*src) && (i<8); i++) {
    switch (*(src++)) {
    case 'U':
      gdata.overallmaxspeeddaydays |= 0x01;
      break;
    case 'M':
      gdata.overallmaxspeeddaydays |= 0x02;
      break;
    case 'T':
      gdata.overallmaxspeeddaydays |= 0x04;
      break;
    case 'W':
      gdata.overallmaxspeeddaydays |= 0x08;
      break;
    case 'R':
      gdata.overallmaxspeeddaydays |= 0x10;
      break;
    case 'F':
      gdata.overallmaxspeeddaydays |= 0x20;
      break;
    case 'S':
      gdata.overallmaxspeeddaydays |= 0x40;
      break;
    }
  }
}

static void c_overallmaxspeeddaytime(char *var)
{
  char *part[2];
  int a = 0;
  int b = 0;

  if (get_argv(part, var, 2) == 2) {
    a = atoi(part[0]);
    b = atoi(part[1]);
    gdata.overallmaxspeeddaytimestart = between(0, a, 23);
    gdata.overallmaxspeeddaytimeend   = between(0, b, 23);
  }
  mydelete(part[0]);
  mydelete(part[1]);
}

static void c_periodicmsg(char *var)
{
  char *part[4];
  int tnum;
  int m;

  mydelete(gdata.periodicmsg_nick);
  mydelete(gdata.periodicmsg_msg);
  m = get_argv(part, var, 3);
  if (m != 3) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "%s:%ld ignored '%s' because it has invalid args: '%s'",
             current_config, current_line, "periodicmsg", var);
    mydelete(part[0]);
    mydelete(part[1]);
    mydelete(part[2]);
    return;
  }
  gdata.periodicmsg_nick = part[0];
  tnum = atoi(part[1]);
  mydelete(part[1]);
  gdata.periodicmsg_msg = part[2];
  gdata.periodicmsg_time = max2(1, tnum);
}

static void c_proxyinfo(char *var)
{
   irlist_add_string(&gdata.networks[gdata.networks_online].proxyinfo, var);
}

static void c_server(char *var)
{
  server_t *ss;
  char *part[3];

  set_default_network_name();
  get_argv(part, var, 3);
  if (part[0] == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "%s:%ld ignored '%s' because it has invalid args: '%s'",
             current_config, current_line, "server", var);
    mydelete(part[0]);
    mydelete(part[1]);
    mydelete(part[2]);
    return;
  }
  ss = irlist_add(&gdata.networks[gdata.networks_online].servers, sizeof(server_t));
  ss->hostname = part[0];
  ss->port = 6667;
  ss->password = part[2];
  if (part[1]) {
    ss->port = atoi(part[1]);
    mydelete(part[1]);
  }
}

static void c_server_connected_raw(char *var)
{
   irlist_add_string(&gdata.networks[gdata.networks_online].server_connected_raw, var);
}

static void c_server_join_raw(char *var)
{
   irlist_add_string(&gdata.networks[gdata.networks_online].server_join_raw, var);
}

static void c_slotsmax(char *var)
{
  int ival;

  ival = atoi(var);
  gdata.slotsmax = between(1, ival, MAXTRANS);
  if (gdata.slotsmax != ival) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld unable to have slotsmax of %d, using %d instead",
             current_config, current_line, ival, gdata.slotsmax);
  }
}

static void c_slow_privmsg(char *var)
{
  const char *key = "need_level";
  int rawval;

  if (check_range(key, var, &rawval, 0, 120) == 0) {
    gdata.networks[gdata.networks_online].slow_privmsg = rawval;
  }
}

static void c_statefile(char *var)
{
  int i;

  mydelete(gdata.statefile);
  gdata.statefile = mystrdup(var);
  convert_to_unix_slash(gdata.statefile);
  i = open(gdata.statefile, O_RDWR | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS );
  if (i >= 0)
    close(i);
}

static void c_transferlimits(char *var)
{
  char *part[NUMBER_TRANSFERLIMITS];
  int m;
  int i;
  int ival;

  bzero((char *)part, sizeof(part));
  m = get_argv(part, var, (int)NUMBER_TRANSFERLIMITS);
  for (i=0; i<m; i++) {
    if (part[i]) {
      ival = atoi(part[i]);
      gdata.transferlimits[i].limit = max2(0, ival);
      gdata.transferlimits[i].limit *= 1024 * 1024;
      mydelete(part[i]);
    }
  }
}

static void c_transfermaxspeed(char *var)
{
  float fval;

  fval = atof(var);
  gdata.transfermaxspeed = max2(0, fval);
}

static void c_transferminspeed(char *var)
{
  gdata.transferminspeed = atof(var);
}

static void c_uploadmaxsize(char *var)
{
  gdata.uploadmaxsize = atoull(var)*1024*1024;
}

static void c_uploadminspace(char *var)
{
  gdata.uploadminspace = atoull(var)*1024*1024;
}

static void c_usenatip(char *var)
{
  gnetwork_t *backup;

  backup = gnetwork;
  gnetwork = &(gdata.networks[gdata.networks_online]);
  gnetwork->natip = mystrdup(var);
  gnetwork->usenatip = 1;
  update_natip(var);
  gnetwork = backup;
}

static void c_user_modes(char *var)
{
  mydelete(gdata.networks[gdata.networks_online].user_modes);
  gdata.networks[gdata.networks_online].user_modes = mystrdup(var);
}

static void c_user_nick(char *var)
{
  mydelete(gdata.networks[gdata.networks_online].config_nick);
  gdata.networks[gdata.networks_online].config_nick = mystrdup(var);
}

static void c_bracket_open(char * UNUSED(var))
{
  gdata.bracket ++;
}

static void c_bracket_close(char *UNUSED(var))
{
  gdata.bracket --;
}

static int config_func_anzahl = 0;
static config_func_typ config_parse_func[] = {
{"autoadd_group_match",    c_autoadd_group_match },
{"autosendpack",           c_autosendpack },
{"channel",                c_channel },
{"channel_join_raw",       c_channel_join_raw },
{"connectionmethod",       c_connectionmethod },
{"disk_quota",             c_disk_quota },
{"getip_network",          c_getip_network },
{"group_admin",            c_group_admin },
{"local_vhost",            c_local_vhost },
{"logrotate",              c_logrotate },
{"mime_type",              c_mime_type },
{"need_level",             c_need_level },
{"network",                c_network },
{"nickserv_pass",          c_nickserv_pass },
{"noannounce",             c_noannounce },
{"overallmaxspeeddaydays", c_overallmaxspeeddaydays },
{"overallmaxspeeddaytime", c_overallmaxspeeddaytime },
{"periodicmsg",            c_periodicmsg },
{"proxyinfo",              c_proxyinfo },
{"server",                 c_server },
{"server_connected_raw",   c_server_connected_raw },
{"server_join_raw",        c_server_join_raw },
{"slotsmax",               c_slotsmax },
{"slow_privmsg",           c_slow_privmsg },
{"statefile",              c_statefile },
{"transferlimits",         c_transferlimits },
{"transfermaxspeed",       c_transfermaxspeed },
{"transferminspeed",       c_transferminspeed },
{"uploadmaxsize",          c_uploadmaxsize },
{"uploadminspace",         c_uploadminspace },
{"usenatip",               c_usenatip },
{"user_modes",             c_user_modes },
{"user_nick",              c_user_nick },
{"{",                      c_bracket_open },
{"}",                      c_bracket_close },
{NULL, NULL }};

static void config_sorted_func(void)
{
  long i;

  for (i = 0L; config_parse_func[i].name != NULL; i ++) {
    if (config_parse_func[i + 1].name == NULL)
      break;
    if (strcmp(config_parse_func[i].name, config_parse_func[i + 1].name) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             config_parse_func[i].name, config_parse_func[i + 1].name);
  };
  config_func_anzahl = i + 1;
}

static int config_find_func(const char *key)
{
  int how_far;
  int bin_mid;
  int bin_low;
  int bin_high;

  if (config_func_anzahl > 0L) {
    bin_low = 0;
    bin_high = config_func_anzahl - 1;
    while (bin_low <= bin_high) {
      bin_mid = (bin_low + bin_high) / 2;
      how_far = strcmp(config_parse_func[bin_mid].name, key);
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

static int set_config_func(const char *key, char *text)
{
  int i;

  updatecontext();

  i = config_find_func(key);
  if (i < 0)
    return 1;

  (*config_parse_func[i].func)(text);
  return 0;
}

static void parse_config_line(char **part)
{
  if (gdata.bracket == 0) {
    /* globals */
    if (set_config_bool(part[0], part[1]) == 0)
      return;

    if (set_config_int(part[0], part[1]) == 0)
      return;

    if (set_config_string(part[0], part[1]) == 0)
      return;

    if (set_config_list(part[0], part[1]) == 0)
      return;
  }
  if (set_config_func(part[0], part[1]) == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD,
          "%s:%ld ignored invalid line in config file: %s",
           current_config, current_line, part[0]);
}

void getconfig_set(const char *line)
{
  char *part[2];

  updatecontext();

  get_argv(part, line, 2);
  parse_config_line(part);
  mydelete(part[0]);
  mydelete(part[1]);
}

char *print_config_key(const char *key)
{
  char *val;

  val = print_config_bool(key);
  if (val != NULL)
    return val;

  val = print_config_int(key);
  if (val != NULL)
    return val;

  val = print_config_string(key);
  if (val != NULL)
    return val;

  return NULL;
}

void config_dump(void)
{
  dump_config_bool();
  dump_config_int();
  dump_config_string();
  dump_config_list();
}

void config_startup(void)
{
  config_sorted_bool();
  config_sorted_int();
  config_sorted_string();
  config_sorted_list();
  config_sorted_func();
  current_config = "";
  current_line = 0;
}

/* EOF */
