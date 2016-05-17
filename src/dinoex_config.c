/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2014 Dirk Meyer
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
#include "dinoex_jobs.h"
#include "dinoex_misc.h"
#include "dinoex_config.h"

#include <ctype.h>

typedef const char *(*config_name_t)(unsigned int i);

typedef struct {
  const char *name;
  unsigned int *ivar;
  unsigned int reset;
  unsigned int dummy;
} config_bool_typ;

typedef struct {
  const char *name;
  unsigned int *ivar;
  int min;
  int max;
  unsigned int mult;
  unsigned int reset;
} config_int_typ;

typedef struct {
  const char *name;
  char **svar;
  unsigned int flags;
  unsigned int dummy;
} config_string_typ;

typedef struct {
  const char *name;
  irlist_t *list;
  unsigned int flags;
  unsigned int dummy;
} config_list_typ;

typedef struct {
  const char *name;
  void (*func)(const char *key, char *var);
} config_func_typ;

typedef struct {
  const char *name;
  char *(*fprint)(void);
} config_fprint_typ;

typedef struct {
  const char *name;
  void (*fdump)(const char *key);
} config_fdump_typ;

const char *current_config;
unsigned long current_line;
unsigned int current_network;
unsigned int current_bracket;


void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
/* dump a string to logfile */
dump_line(const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vioutput(OUT_L, COLOR_NO_COLOR, format, args);
   va_end(args);
}

/* dump an bool variable to logfile */
static void dump_config_bool2(const char *name, unsigned int val)
{
  const char *text;

  text = "no"; /* NOTRANSLATE */
  for (;;) {
    if (val == 2)
      return;
    if (val == 0)
      break;
    text = "yes"; /* NOTRANSLATE */
    break;
  }
  dump_line("%s %s", name, text);
}

/* dump an bool variable to logfile if defined */
static void dump_config_bool3(const char *name, unsigned int val, unsigned int reset)
{
  if ((gdata.dump_all != 0) || (val != reset))
    dump_config_bool2(name, val);
}

/* dump an integer variable to logfile */
static void dump_config_int2(const char *name, unsigned int val)
{
  dump_line("%s %u", name, val);
}

/* dump an integer variable to logfile if defined */
static void dump_config_int3(const char *name, unsigned int val, unsigned int reset)
{
  if ((gdata.dump_all != 0) || (val != reset))
    dump_config_int2(name, val);
}

/* dump an float variable to logfile */
static void dump_config_float2(const char *name, float val)
{
  if ((gdata.dump_all != 0) || (val != 0.0))
    dump_line("%s %.5f", name, val); /* NOTRANSLATE */
}

/* dump an 64bit variable to logfile */
static void dump_config_long2(const char *name, ir_int64 val)
{
  if ((gdata.dump_all != 0) || (val != 0))
    dump_line("%s %" LLPRINTFMT "d", name, val); /* NOTRANSLATE */
}

/* dump an megabyte variable to logfile */
static void dump_config_mega2(const char *name, ir_int64 val)
{
  val /= (1024*1024);
  dump_config_long2(name, val);
}

/* dump a string variable to logfile */
static void dump_config_string2(const char *name, const char *val)
{
  dump_line("%s \"%s\"", /* NOTRANSLATE */
            name, val ? val : "<undef>" );
}

/* dump a string variable to logfile skip if not defined */
static void dump_config_string3(const char *name, const char *val)
{
  if ((gdata.dump_all != 0) || (val != NULL))
    dump_config_string2(name, val);
}

/* dump a list variable to logfile skip if not defined */
static void dump_config_list2(const char *name, const irlist_t *list)
{
  char *string;

  for (string = irlist_get_head(list);
       string;
       string = irlist_get_next(string)) {
    dump_config_string2(name, string);
  }
}

static char *print_config_long2(ir_int64 val)
{
  char *text;

  text = mymalloc(maxtextlengthshort);
  snprintf(text, maxtextlengthshort, "%" LLPRINTFMT "d", val); /* NOTRANSLATE */
  return text;
}

/* validate config and warn if password is not encrypted */
static void checkadminpass2(const char *key, const char *masterpass)
{
#ifndef NO_CRYPT
  unsigned int err=0;
  unsigned int i;

  updatecontext();

  if (!masterpass || strlen(masterpass) < 13U) ++err;

  for (i=0; !err && i<strlen(masterpass); ++i) {
    if (!((masterpass[i] >= 'a' && masterpass[i] <= 'z') ||
          (masterpass[i] >= 'A' && masterpass[i] <= 'Z') ||
          (masterpass[i] >= '0' && masterpass[i] <= '9') ||
          (masterpass[i] == '.') ||
          (masterpass[i] == '$') ||
          (masterpass[i] == '/')))
      ++err;
  }

  if (err) outerror(OUTERROR_TYPE_CRASH,
                    "%s:%ld %s is not encrypted!",
                    current_config, current_line, key);
#endif /* NO_CRYPT */
}

static int config_sorted_check(config_name_t config_name_f)
{
  const char *name1;
  const char *name2;
  unsigned int i;

  for (i = 0; ; ++i) {
    name2 = config_name_f(i + 1);
    if (name2 == NULL)
      break;
    name1 = config_name_f(i);
    if (name1 == NULL)
      break;
    if (strcmp(name1, name2) < 0)
      continue;

    outerror(OUTERROR_TYPE_CRASH, "Config structure failed %s <-> %s",
             name1, name2);
  }
  return (int)(i + 1);
}

static int config_find_typ(config_name_t config_name_f, const char *key, int bin_high)
{
  int how_far;
  int bin_mid;
  int bin_low;

  if (bin_high == 0)
    return -1;

  bin_high -= 1;
  bin_low = 0;
  while (bin_low <= bin_high) {
    bin_mid = (bin_low + bin_high) / 2;
    how_far = strcmp(config_name_f((unsigned int)bin_mid), key);
    if (how_far == 0)
      return bin_mid;
    if (how_far < 0)
      bin_low = bin_mid + 1;
    else
      bin_high = bin_mid - 1;
  };
  return -1;
}


static int config_bool_anzahl = 0;
static config_bool_typ config_parse_bool[] = {
{"announce_size",          &gdata.announce_size,           0 }, /* NOTRANSLATE */
{"auto_crc_check",         &gdata.auto_crc_check,          0 }, /* NOTRANSLATE */
{"auto_default_group",     &gdata.auto_default_group,      0 }, /* NOTRANSLATE */
{"auto_path_group",        &gdata.auto_path_group,         0 }, /* NOTRANSLATE */
{"autoaddann_short",       &gdata.autoaddann_short,        0 }, /* NOTRANSLATE */
{"balanced_queue",         &gdata.balanced_queue,          0 }, /* NOTRANSLATE */
{"direct_config_access",   &gdata.direct_config_access,    0 }, /* NOTRANSLATE */
{"direct_file_access",     &gdata.direct_file_access,      0 }, /* NOTRANSLATE */
{"disablexdccinfo",        &gdata.disablexdccinfo,         0 }, /* NOTRANSLATE */
#if defined(_OS_CYGWIN)
{"dos_text_files",         &gdata.dos_text_files,          1 }, /* NOTRANSLATE */
#else /* _OS_CYGWIN */
{"dos_text_files",         &gdata.dos_text_files,          0 }, /* NOTRANSLATE */
#endif /* _OS_CYGWIN */
{"dump_all",               &gdata.dump_all,                0 }, /* NOTRANSLATE */
{"extend_status_line",     &gdata.extend_status_line,      0 }, /* NOTRANSLATE */
#ifndef WITHOUT_BLOWFISH
{"fish_only",              &gdata.fish_only,               0 }, /* NOTRANSLATE */
#endif /* WITHOUT_BLOWFISH */
{"getipfromserver",        &gdata.getipfromserver,         0 }, /* NOTRANSLATE */
#ifdef USE_UPNP
{"getipfromupnp",          &gdata.getipfromupnp,           0 }, /* NOTRANSLATE */
#endif /* USE_UPNP */
{"groupsincaps",           &gdata.groupsincaps,            0 }, /* NOTRANSLATE */
{"hide_list_info",         &gdata.hide_list_info,          0 }, /* NOTRANSLATE */
{"hide_list_stop",         &gdata.hide_list_stop,          0 }, /* NOTRANSLATE */
{"hidelockedpacks",        &gdata.hidelockedpacks,         0 }, /* NOTRANSLATE */
{"hideos",                 &gdata.hideos,                  0 }, /* NOTRANSLATE */
{"holdqueue",              &gdata.holdqueue,               0 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"http_geoip",             &gdata.http_geoip,              0 }, /* NOTRANSLATE */
{"http_search",            &gdata.http_search,             0 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP */
{"ignoreuploadbandwidth",  &gdata.ignoreuploadbandwidth,   0 }, /* NOTRANSLATE */
{"include_subdirs",        &gdata.include_subdirs,         0 }, /* NOTRANSLATE */
{"logmessages",            &gdata.logmessages,             0 }, /* NOTRANSLATE */
{"lognotices",             &gdata.lognotices,              0 }, /* NOTRANSLATE */
{"logstats",               &gdata.logstats,                1 }, /* NOTRANSLATE */
{"mirc_dcc64",             &gdata.mirc_dcc64,              0 }, /* NOTRANSLATE */
{"need_voice",             &gdata.need_voice,              0 }, /* NOTRANSLATE */
{"no_auto_rehash",         &gdata.no_auto_rehash,          0 }, /* NOTRANSLATE */
{"no_duplicate_filenames", &gdata.no_duplicate_filenames,  0 }, /* NOTRANSLATE */
{"no_find_trigger",        &gdata.no_find_trigger,         0 }, /* NOTRANSLATE */
{"no_minspeed_on_free",    &gdata.no_minspeed_on_free,     0 }, /* NOTRANSLATE */
{"no_natural_sort",        &gdata.no_natural_sort,         0 }, /* NOTRANSLATE */
{"no_status_chat",         &gdata.no_status_chat,          0 }, /* NOTRANSLATE */
{"no_status_log",          &gdata.no_status_log,           0 }, /* NOTRANSLATE */
{"noautorejoin",           &gdata.noautorejoin,            0 }, /* NOTRANSLATE */
{"nocrc32",                &gdata.nocrc32,                 0 }, /* NOTRANSLATE */
{"noduplicatefiles",       &gdata.noduplicatefiles,        0 }, /* NOTRANSLATE */
{"nomd5sum",               &gdata.nomd5sum,                0 }, /* NOTRANSLATE */
{"old_statefile",          &gdata.old_statefile,           0 }, /* NOTRANSLATE */
{"passive_dcc",            &gdata.passive_dcc,             0 }, /* NOTRANSLATE */
{"passive_dcc_chat",       &gdata.passive_dcc_chat,        0 }, /* NOTRANSLATE */
#ifndef WITHOUT_BLOWFISH
{"privmsg_encrypt",        &gdata.privmsg_encrypt,         0 }, /* NOTRANSLATE */
#endif /* WITHOUT_BLOWFISH */
{"quietmode",              &gdata.quietmode,               0 }, /* NOTRANSLATE */
{"removelostfiles",        &gdata.removelostfiles,         0 }, /* NOTRANSLATE */
{"requeue_sends",          &gdata.requeue_sends,           0 }, /* NOTRANSLATE */
{"respondtochannellist",   &gdata.respondtochannellist,    0 }, /* NOTRANSLATE */
{"respondtochannelxdcc",   &gdata.respondtochannelxdcc,    0 }, /* NOTRANSLATE */
{"restrictlist",           &gdata.restrictlist,            0 }, /* NOTRANSLATE */
{"restrictprivlist",       &gdata.restrictprivlist,        0 }, /* NOTRANSLATE */
{"restrictprivlistfull",   &gdata.restrictprivlistfull,    0 }, /* NOTRANSLATE */
{"restrictprivlistmain",   &gdata.restrictprivlistmain,    0 }, /* NOTRANSLATE */
{"restrictsend",           &gdata.restrictsend,            0 }, /* NOTRANSLATE */
{"restrictsend_warning",   &gdata.restrictsend_warning,    0 }, /* NOTRANSLATE */
{"send_batch",             &gdata.send_batch,              0 }, /* NOTRANSLATE */
{"show_date_added",        &gdata.show_date_added,         0 }, /* NOTRANSLATE */
{"show_group_of_pack",     &gdata.show_group_of_pack,      0 }, /* NOTRANSLATE */
{"show_list_all",          &gdata.show_list_all,           0 }, /* NOTRANSLATE */
{"spaces_in_filenames",    &gdata.spaces_in_filenames,     0 }, /* NOTRANSLATE */
{"subdirs_delayed",        &gdata.subdirs_delayed,         0 }, /* NOTRANSLATE */
#if defined(_OS_CYGWIN)
{"tcp_nodelay",            &gdata.tcp_nodelay,             1 }, /* NOTRANSLATE */
#else /* _OS_CYGWIN */
{"tcp_nodelay",            &gdata.tcp_nodelay,             0 }, /* NOTRANSLATE */
#endif /* _OS_CYGWIN */
{"timestampconsole",       &gdata.timestampconsole,        0 }, /* NOTRANSLATE */
#ifdef USE_UPNP
{"upnp_router",            &gdata.upnp_router,             0 }, /* NOTRANSLATE */
#endif /* USE_UPNP */
{"verbose_crc32",          &gdata.verbose_crc32,           0 }, /* NOTRANSLATE */
{"xdcclist_by_privmsg",    &gdata.xdcclist_by_privmsg,     0 }, /* NOTRANSLATE */
{"xdcclist_grouponly",     &gdata.xdcclist_grouponly,      0 }, /* NOTRANSLATE */
{"xdcclistfileraw",        &gdata.xdcclistfileraw,         0 }, /* NOTRANSLATE */
{NULL, NULL, 0 }};

static const char *config_name_bool(unsigned int i)
{
  return config_parse_bool[i].name;
}

static int config_find_bool(const char *key)
{
  return config_find_typ(config_name_bool, key, config_bool_anzahl);
}

static int parse_bool_val(const char *key, const char *text)
{
  if (text == NULL) {
    return 1;
  }
  if (!strcmp("yes", text)) { /* NOTRANSLATE */
    return 1;
  }
  if (!strcmp("no", text)) { /* NOTRANSLATE */
    return 0;
  }
  outerror(OUTERROR_TYPE_WARN,
           "%s:%ld ignored '%s' because it has unknown args: '%s'",
           current_config, current_line, key, text);
  return -1;
}

static void parse_bool_to_ptr(unsigned int *ptr, const char *key, const char *text)
{
  int val;

  val = parse_bool_val(key, text);
  if (val >= 0) {
    *ptr = (unsigned int)val;
  }
}

static unsigned int set_config_bool(const char *key, const char *text)
{
  int i;

  updatecontext();

  i = config_find_bool(key);
  if (i < 0)
    return 1;

  parse_bool_to_ptr(config_parse_bool[i].ivar, key, text);
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

  val = (*(config_parse_bool[i].ivar) != 0) ? "yes" : "no"; /* NOTRANSLATE */
  return mystrdup(val);
}

static void dump_config_bool(void)
{
  unsigned int i;

  for (i = 0; config_parse_bool[i].name != NULL; ++i) {
    dump_config_bool3(config_parse_bool[i].name, *(config_parse_bool[i].ivar), config_parse_bool[i].reset);
  }
}

static void reset_config_bool(void)
{
  unsigned int i;

  for (i = 0; config_parse_bool[i].name != NULL; ++i) {
    *(config_parse_bool[i].ivar) = config_parse_bool[i].reset;
  }
}


static int config_int_anzahl = 0;
static config_int_typ config_parse_int[] = {
{"adddir_min_size",         &gdata.adddir_min_size,         0, 1024, 65000, 0 }, /* NOTRANSLATE */
{"adminlevel",              &gdata.adminlevel,              1, 5, 1, ADMIN_LEVEL_FULL }, /* NOTRANSLATE */
{"atfind",                  &gdata.atfind,                  0, 10, 1, 0 }, /* NOTRANSLATE */
{"autoadd_delay",           &gdata.autoadd_delay,           0, 65000, 1, 0 }, /* NOTRANSLATE */
{"autoadd_time",            &gdata.autoadd_time,            0, 65000, 1, 0 }, /* NOTRANSLATE */
{"autoignore_rate",         &gdata.autoignore_rate,         1, 100, 1, 10 }, /* NOTRANSLATE */
{"autoignore_threshold",    &gdata.autoignore_threshold,    0, 600, 1, 10 }, /* NOTRANSLATE */
{"debug",                   &gdata.debug,                   0, 65000, 1, XDCC_SEND_LIST }, /* NOTRANSLATE */
{"expire_logfiles",         &gdata.expire_logfiles,         0, 65000, 1, 0 }, /* NOTRANSLATE */
{"fileremove_max_packs",    &gdata.fileremove_max_packs,    0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"flood_protection_rate",   &gdata.flood_protection_rate,   1, 100, 1, 6 }, /* NOTRANSLATE */
{"hadminlevel",             &gdata.hadminlevel,             1, 5, 1, ADMIN_LEVEL_HALF }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"http_port",               &gdata.http_port,               0, 65535, 1, 0 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP */
{"idlequeuesize",           &gdata.idlequeuesize,           0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"ignore_duplicate_ip",     &gdata.ignore_duplicate_ip,     0, 24*31, 1, 0 }, /* NOTRANSLATE */
{"lowbdwth",                &gdata.lowbdwth,                0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"max_find",                &gdata.max_find,                0, 65000, 1, 0 }, /* NOTRANSLATE */
{"max_uploads",             &gdata.max_uploads,             0, 65000, 1, 0 }, /* NOTRANSLATE */
{"max_upspeed",             &gdata.max_upspeed,             0, 1000000, 4, 65000 }, /* NOTRANSLATE */
{"maxidlequeuedperperson",  &gdata.maxidlequeuedperperson,  1, 1000000, 1, 1 }, /* NOTRANSLATE */
{"maxqueueditemsperperson", &gdata.maxqueueditemsperperson, 1, 1000000, 1, 1 }, /* NOTRANSLATE */
{"maxtransfersperperson",   &gdata.maxtransfersperperson,   1, 1000000, 1, 1 }, /* NOTRANSLATE */
#if defined(_OS_CYGWIN)
{"monitor_files",           &gdata.monitor_files,           1, 65000, 1, 5 }, /* NOTRANSLATE */
#else /* _OS_CYGWIN */
{"monitor_files",           &gdata.monitor_files,           1, 65000, 1, 20 }, /* NOTRANSLATE */
#endif /* _OS_CYGWIN */
{"need_level",              &gdata.need_level,              0, 3, 1, 0 }, /* NOTRANSLATE */
{"new_trigger",             &gdata.new_trigger,             0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"notifytime",              &gdata.notifytime,              0, 1000000, 1, 5 }, /* NOTRANSLATE */
{"overallmaxspeed",         &gdata.overallmaxspeed,         0, 1000000, 4, 0 }, /* NOTRANSLATE */
{"overallmaxspeeddayspeed", &gdata.overallmaxspeeddayspeed, 0, 1000000, 4, 0 }, /* NOTRANSLATE */
{"punishslowusers",         &gdata.punishslowusers,         0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"queuesize",               &gdata.queuesize,               0, 1000000, 1, 0 }, /* NOTRANSLATE */
{"reconnect_delay",         &gdata.reconnect_delay,         0, 2000, 1, 0 }, /* NOTRANSLATE */
{"reminder_send_retry",     &gdata.reminder_send_retry,     0, 2, 1, 1 }, /* NOTRANSLATE */
{"remove_dead_users",       &gdata.remove_dead_users,       0, 2, 1, 0 }, /* NOTRANSLATE */
{"restrictsend_delay",      &gdata.restrictsend_delay,      0, 2000, 1, 0 }, /* NOTRANSLATE */
{"restrictsend_timeout",    &gdata.restrictsend_timeout,    0, 600, 1, 300 }, /* NOTRANSLATE */
{"send_statefile_minute",   &gdata.send_statefile_minute,   0, 60, 1, 0 }, /* NOTRANSLATE */
{"smallfilebypass",         &gdata.smallfilebypass,         0, 1024*1024, 1024, 0 }, /* NOTRANSLATE */
{"start_of_month",          &gdata.start_of_month,          1, 31, 1, 1 }, /* NOTRANSLATE */
{"status_time_dcc_chat",    &gdata.status_time_dcc_chat,    10, 2000, 1, 120 }, /* NOTRANSLATE */
#if defined(_OS_CYGWIN)
{"tcp_buffer_size",         &gdata.tcp_buffer_size,         0, 1024*1024, 1024, 372*1024 }, /* NOTRANSLATE */
#else /* _OS_CYGWIN */
{"tcp_buffer_size",         &gdata.tcp_buffer_size,         0, 1024*1024, 1024, 0 }, /* NOTRANSLATE */
#endif /* _OS_CYGWIN */
{"tcprangelimit",           &gdata.tcprangelimit,           1024, 65535, 1, 65535 }, /* NOTRANSLATE */
{"tcprangestart",           &gdata.tcprangestart,           1024, 65530, 1, 0 }, /* NOTRANSLATE */
#ifndef WITHOUT_TELNET
{"telnet_port",             &gdata.telnet_port,             0, 65535, 1, 0 }, /* NOTRANSLATE */
#endif /* WITHOUT_TELNET */
{"waitafterjoin",           &gdata.waitafterjoin,           0, 2000, 1, 200 }, /* NOTRANSLATE */
{NULL, NULL, 0, 0, 0, 0 }};

static const char *config_name_int(unsigned int i)
{
  return config_parse_int[i].name;
}

static int config_find_int(const char *key)
{
  return config_find_typ(config_name_int, key, config_int_anzahl);
}

static unsigned int report_no_arg(const char *key, const char *text)
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

static void invalid_args(const char *key, const char *text)
{
  outerror(OUTERROR_TYPE_WARN,
           "%s:%ld ignored '%s' because it has invalid args: '%s'",
           current_config, current_line, key, text);
}

static unsigned int check_range(const char *key, const char *text, int *val, int min, int max)
{
  int rawval;
  char *endptr;

  if (report_no_arg(key, text))
    return 1;

  rawval = (int)strtol(text, &endptr, 0);
  if (endptr[0] != 0) {
    invalid_args(key, text);
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

static unsigned int set_config_int(const char *key, const char *text)
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
  *(config_parse_int[i].ivar) = (unsigned int)rawval;
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

  val = mymalloc(maxtextlengthshort);
  snprintf(val, maxtextlengthshort, "%u", *(config_parse_int[i].ivar)); /* NOTRANSLATE */
  return val;
}

static void dump_config_int(void)
{
  unsigned int i;
  unsigned int rawval;

  for (i = 0; config_parse_int[i].name != NULL; ++i) {
    if (gdata.dump_all == 0) {
      if (config_parse_int[i].reset == XDCC_SEND_LIST)
        continue;
      if (*(config_parse_int[i].ivar) == config_parse_int[i].reset)
        continue;
    }
    rawval = *(config_parse_int[i].ivar) / config_parse_int[i].mult;
    dump_config_int2(config_parse_int[i].name, rawval);
  }
}

static void reset_config_int(void)
{
  unsigned int i;

  for (i = 0; config_parse_int[i].name != NULL; ++i) {
    if (config_parse_int[i].reset != XDCC_SEND_LIST)
      *(config_parse_int[i].ivar) = config_parse_int[i].reset;
  }
}


/*  flags for strings
 0 -> literal string
 1 -> pathname
 4 -> adminpass
 */

static int config_string_anzahl = 0;
static config_string_typ config_parse_string[] = {
{"admin_job_done_file",     &gdata.admin_job_done_file,     1 }, /* NOTRANSLATE */
{"admin_job_file",          &gdata.admin_job_file,          1 }, /* NOTRANSLATE */
{"adminpass",               &gdata.adminpass,               4 }, /* NOTRANSLATE */
{"announce_seperator",      &gdata.announce_seperator,      0 }, /* NOTRANSLATE */
{"announce_suffix_color",   &gdata.announce_suffix_color,   0 }, /* NOTRANSLATE */
{"autoadd_color",           &gdata.autoadd_color,           0 }, /* NOTRANSLATE */
{"autoadd_group",           &gdata.autoadd_group,           0 }, /* NOTRANSLATE */
{"autoadd_sort",            &gdata.autoadd_sort,            0 }, /* NOTRANSLATE */
{"autoaddann",              &gdata.autoaddann,              0 }, /* NOTRANSLATE */
{"charset",                 &gdata.charset,                 0 }, /* NOTRANSLATE */
{"creditline",              &gdata.creditline,              0 }, /* NOTRANSLATE */
{"download_completed_msg",  &gdata.download_completed_msg,  0 }, /* NOTRANSLATE */
{"enable_nick",             &gdata.enable_nick,             0 }, /* NOTRANSLATE */
#ifdef USE_GEOIP
#ifdef USE_GEOIP6
{"geoip6database",          &gdata.geoip6database,          0 }, /* NOTRANSLATE */
#endif /* USE_GEOIP6 */
{"geoipdatabase",           &gdata.geoipdatabase,           0 }, /* NOTRANSLATE */
#endif /* USE_GEOIP */
{"group_seperator",         &gdata.group_seperator,         0 }, /* NOTRANSLATE */
{"hadminpass",              &gdata.hadminpass,              4 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"http_access_log",         &gdata.http_access_log,         1 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP_ADMIN
{"http_admin",              &gdata.http_admin,              0 }, /* NOTRANSLATE */
{"http_admin_dir",          &gdata.http_admin_dir,          1 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP_ADMIN */
#endif /* WITHOUT_HTTP */
{"http_date",               &gdata.http_date,               0 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"http_dir",                &gdata.http_dir,                1 }, /* NOTRANSLATE */
{"http_forbidden",          &gdata.http_forbidden,          0 }, /* NOTRANSLATE */
{"http_index",              &gdata.http_index,              0 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP */
{"local_vhost",             &gdata.local_vhost,             0 }, /* NOTRANSLATE */
{"logfile",                 &gdata.logfile,                 1 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"logfile_httpd",           &gdata.logfile_httpd,           1 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP */
{"logfile_messages",        &gdata.logfile_messages,        1 }, /* NOTRANSLATE */
{"logfile_notices",         &gdata.logfile_notices,         1 }, /* NOTRANSLATE */
{"loginname",               &gdata.loginname,               0 }, /* NOTRANSLATE */
{"nickserv_pass",           &gdata.nickserv_pass,           0 }, /* NOTRANSLATE */
{"owner_nick",              &gdata.owner_nick,              0 }, /* NOTRANSLATE */
{"pidfile",                 &gdata.pidfile,                 1 }, /* NOTRANSLATE */
#ifndef WITHOUT_BLOWFISH
{"privmsg_fish",            &gdata.privmsg_fish,            0 }, /* NOTRANSLATE */
#endif /* WITHOUT_BLOWFISH */
{"respondtochannellistmsg", &gdata.respondtochannellistmsg, 0 }, /* NOTRANSLATE */
{"restrictprivlistmsg",     &gdata.restrictprivlistmsg,     0 }, /* NOTRANSLATE */
#ifdef USE_RUBY
{"ruby_script",             &gdata.ruby_script,             1 }, /* NOTRANSLATE */
#endif /* USE_RUBY */
{"send_statefile",          &gdata.send_statefile,          0 }, /* NOTRANSLATE */
{"trashcan_dir",            &gdata.trashcan_dir,            1 }, /* NOTRANSLATE */
{"uploaddir",               &gdata.uploaddir,               1 }, /* NOTRANSLATE */
{"usenatip",                &gdata.usenatip,                0 }, /* NOTRANSLATE */
{"user_modes",              &gdata.user_modes,              0 }, /* NOTRANSLATE */
{"user_nick",               &gdata.config_nick,             0 }, /* NOTRANSLATE */
{"user_realname",           &gdata.user_realname,           0 }, /* NOTRANSLATE */
{"xdcclistfile",            &gdata.xdcclistfile,            1 }, /* NOTRANSLATE */
{"xdccremovefile",          &gdata.xdccremovefile,          1 }, /* NOTRANSLATE */
{"xdccxmlfile",             &gdata.xdccxmlfile,             1 }, /* NOTRANSLATE */
{NULL, NULL, 0 }};

static const char *config_name_string(unsigned int i)
{
  return config_parse_string[i].name;
}

static int config_find_string(const char *key)
{
  return config_find_typ(config_name_string, key, config_string_anzahl);
}

static unsigned int set_config_string(const char *key, char *text)
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
     checkadminpass2(key, text);
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
  if (val == NULL)
    return NULL;

  return mystrdup(val);
}

static void dump_config_string(void)
{
  unsigned int i;

  for (i = 0; config_parse_string[i].name != NULL; ++i) {
    dump_config_string3(config_parse_string[i].name, *(config_parse_string[i].svar));
  }
}

static void reset_config_string(void)
{
  unsigned int i;

  for (i = 0; config_parse_string[i].name != NULL; ++i) {
    mydelete(*(config_parse_string[i].svar));
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
{"adddir_exclude",          &gdata.adddir_exclude,          0 }, /* NOTRANSLATE */
{"adddir_match",            &gdata.adddir_match,            0 }, /* NOTRANSLATE */
{"adminhost",               &gdata.adminhost,               3 }, /* NOTRANSLATE */
{"autoadd_dir",             &gdata.autoadd_dirs,            0 }, /* NOTRANSLATE */
{"autoaddann_mask",         &gdata.autoaddann_mask,         0 }, /* NOTRANSLATE */
{"autocrc_exclude",         &gdata.autocrc_exclude,         0 }, /* NOTRANSLATE */
{"autoignore_exclude",      &gdata.autoignore_exclude,      2 }, /* NOTRANSLATE */
{"downloadhost",            &gdata.downloadhost,            2 }, /* NOTRANSLATE */
{"filedir",                 &gdata.filedir,                 1 }, /* NOTRANSLATE */
#ifndef WITHOUT_BLOWFISH
{"fish_exclude_nick",       &gdata.fish_exclude_nick,       2 }, /* NOTRANSLATE */
#endif /* WITHOUT_BLOWFISH */
#ifdef USE_GEOIP
{"geoipcountry",            &gdata.geoipcountry,            0 }, /* NOTRANSLATE */
{"geoipexcludegroup",       &gdata.geoipexcludegroup,       0 }, /* NOTRANSLATE */
{"geoipexcludenick",        &gdata.geoipexcludenick,        0 }, /* NOTRANSLATE */
#endif /* USE_GEOIP */
{"hadminhost",              &gdata.hadminhost,              3 }, /* NOTRANSLATE */
{"headline",                &gdata.headline,                0 }, /* NOTRANSLATE */
#ifndef WITHOUT_HTTP
{"http_allow",              &gdata.http_allow,              5 }, /* NOTRANSLATE */
{"http_deny",               &gdata.http_deny,               5 }, /* NOTRANSLATE */
{"http_vhost",              &gdata.http_vhost,              0 }, /* NOTRANSLATE */
#endif /* WITHOUT_HTTP */
{"log_exclude_host",        &gdata.log_exclude_host,        2 }, /* NOTRANSLATE */
{"log_exclude_text",        &gdata.log_exclude_text,        0 }, /* NOTRANSLATE */
{"md5sum_exclude",          &gdata.md5sum_exclude,          0 }, /* NOTRANSLATE */
{"nodownloadhost",          &gdata.nodownloadhost,          2 }, /* NOTRANSLATE */
#ifdef USE_GEOIP
{"nogeoipcountry",          &gdata.nogeoipcountry,          0 }, /* NOTRANSLATE */
#endif /* USE_GEOIP */
#ifndef WITHOUT_TELNET
{"telnet_allow",            &gdata.telnet_allow,            5 }, /* NOTRANSLATE */
{"telnet_deny",             &gdata.telnet_deny,             5 }, /* NOTRANSLATE */
{"telnet_vhost",            &gdata.telnet_vhost,            0 }, /* NOTRANSLATE */
#endif /* WITHOUT_TELNET */
{"unlimitedhost",           &gdata.unlimitedhost,           2 }, /* NOTRANSLATE */
{"uploadhost",              &gdata.uploadhost,              2 }, /* NOTRANSLATE */
{"weblist_info",            &gdata.weblist_info,            0 }, /* NOTRANSLATE */
{"xdcc_allow",              &gdata.xdcc_allow,              5 }, /* NOTRANSLATE */
{"xdcc_deny",               &gdata.xdcc_deny,               5 }, /* NOTRANSLATE */
{NULL, NULL, 0 }};

static const char *config_name_list(unsigned int i)
{
  return config_parse_list[i].name;
}

static int config_find_list(const char *key)
{
  return config_find_typ(config_name_list, key, config_list_anzahl);
}

static unsigned int get_netmask(char *text, unsigned int init)
{
  char *work;
  int newmask;

  work = strchr(text, '/');
  if (work == NULL)
    return init;

  *(work++) = 0;
  newmask = atoi(work);
  if (newmask < 0 )
    return init;

  if (newmask > (int)init )
    return init;

  return (unsigned int )newmask;
}

static unsigned int set_config_list(const char *key, char *text)
{
  ssize_t j;
  int i;
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
      cidr->netmask = get_netmask(text, 32U);
      e = inet_pton(cidr->family, text, &(cidr->remote.sin.sin_addr));
    } else {
      cidr->family = AF_INET6;
      cidr->netmask = get_netmask(text, 128U);
      e = inet_pton(cidr->family, text, &(cidr->remote.sin6.sin6_addr));
    }
    cidr->remote.sa.sa_family = cidr->family;
    if (e != 1) {
       invalid_args(key, text);
       irlist_delete(config_parse_list[i].list, cidr);
    }
    return 0;
  case 3:
    for (j=strlen(text) - 1; j>=0; --j) {
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
  unsigned int i;
  unsigned int netmask;
  char ip6[maxtextlengthshort];

  for (i = 0; config_parse_list[i].name != NULL; ++i) {
    if (gdata.dump_all == 0) {
      if (irlist_size(config_parse_list[i].list) == 0)
        continue;
    }
    switch (config_parse_list[i].flags) {
    case 5:
      for (cidr = irlist_get_head(config_parse_list[i].list);
           cidr;
           cidr = irlist_get_next(cidr)) {
        my_getnameinfo(ip6, maxtextlengthshort - 1, &(cidr->remote.sa));
        netmask = 0;
        if (cidr->family == AF_INET) {
          netmask = 32;
        }
        if (cidr->family == AF_INET6) {
          netmask = 128;
        }
        if (cidr->netmask == netmask) {
          dump_line("%s %s", /* NOTRANSLATE */
                    config_parse_list[i].name, ip6);
        } else {
          dump_line("%s %s/%u", /* NOTRANSLATE */
                    config_parse_list[i].name, ip6, cidr->netmask);
        }
      }
      break;
    default:
      dump_config_list2(config_parse_list[i].name, config_parse_list[i].list);
    }
  }
}

static void reset_config_list(void)
{
  unsigned int i;

  for (i = 0; config_parse_list[i].name != NULL; ++i) {
    irlist_delete_all(config_parse_list[i].list);
  }
}


static void set_default_network_name(void)
{
  char *var;

  if (gdata.networks[current_network].name != NULL) {
    if (gdata.networks[current_network].name[0] != 0) {
      return;
    }
    mydelete(gdata.networks[current_network].name);
  }

  var = mymalloc(10);
  snprintf(var, 10, "%u", current_network + 1); /* NOTRANSLATE */
  gdata.networks[current_network].name = mystrdup(var);
  mydelete(var);
  return;
}

static int parse_channel_int(unsigned short *iptr, char **part, unsigned int i)
{
  char *tptr2;

  tptr2 = part[++i];
  if (!tptr2)
    return -1;
  *iptr = atoi(tptr2);
  mydelete(tptr2);
  return 1;
}

static int parse_channel_string(char **cptr, char **part, unsigned int i)
{
  char *tptr2;

  tptr2 = part[++i];
  if (!tptr2)
    return -1;
  *cptr = tptr2;
  return 1;
}

static int parse_channel_list(irlist_t *list, char **part, unsigned int i)
{
  char *tptr2;

  tptr2 = part[++i];
  if (!tptr2)
    return -1;
  irlist_add_string(list, tptr2);
  mydelete(tptr2);
  return 1;
}

static int parse_channel_format(unsigned short *iptr, char *tptr2)
{
  if (!tptr2)
    return -1;

  if (!strcmp(tptr2, "full")) /* NOTRANSLATE */
    return 1;
  if (!strcmp(tptr2, "minimal")) { /* NOTRANSLATE */
    *iptr |= CHAN_MINIMAL;
    return 1;
  }
  if (!strcmp(tptr2, "summary")) { /* NOTRANSLATE */
    *iptr |= CHAN_SUMMARY;
    return 1;
  }
  return -1;
}

static int parse_channel_option(channel_t *cptr, char *tptr, char **part, unsigned int i)
{
  char *tptr2;
  int j;

  if (!strcmp(tptr, "-plist")) { /* NOTRANSLATE */
    return parse_channel_int(&(cptr->plisttime), part, i);
  }
  if (!strcmp(tptr, "-plistoffset")) { /* NOTRANSLATE */
    return parse_channel_int(&(cptr->plistoffset), part, i);
  }
  if (!strcmp(tptr, "-delay")) { /* NOTRANSLATE */
    return parse_channel_int(&(cptr->delay), part, i);
  }
  if (!strcmp(tptr, "-waitjoin")) { /* NOTRANSLATE */
    return parse_channel_int(&(cptr->waitjoin), part, i);
  }

  if (!strcmp(tptr, "-key")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->key), part, i);
  }
#ifndef WITHOUT_BLOWFISH
  if (!strcmp(tptr, "-fish")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->fish), part, i);
  }
#endif /* WITHOUT_BLOWFISH */
  if (!strcmp(tptr, "-pgroup")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->pgroup), part, i);
  }
  if (!strcmp(tptr, "-joinmsg")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->joinmsg), part, i);
  }
  if (!strcmp(tptr, "-headline")) { /* NOTRANSLATE */
    return parse_channel_list(&(cptr->headline), part, i);
  }
  if (!strcmp(tptr, "-listmsg")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->listmsg), part, i);
  }
  if (!strcmp(tptr, "-rgroup")) { /* NOTRANSLATE */
    return parse_channel_string(&(cptr->rgroup), part, i);
  }

  if (!strcmp(tptr, "-noannounce")) { /* NOTRANSLATE */
    cptr->noannounce = 1;
    return 0;
  }
  if (!strcmp(tptr, "-notrigger")) { /* NOTRANSLATE */
    cptr->notrigger = 1;
    return 0;
  }
  if (!strcmp(tptr, "-plaintext")) { /* NOTRANSLATE */
    cptr->plaintext = 1;
    return 0;
  }

  if (!strcmp(tptr, "-pformat")) { /* NOTRANSLATE */
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
  unsigned int i;
  int j;
  unsigned int m;

  m = get_argv(part, var, MAX_CHANNEL_OPTIONS);
  for (i=0; i<m; ++i) {
    tptr = part[i];
    if (tptr == NULL)
      break;
    j = parse_channel_option(cptr, tptr, part, i);
    mydelete(tptr);
    if (j < 0 )
      return -1;
    i += (unsigned int)j;
  }
  return 0;
}


static void c_auth_name(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].auth_name);
  gdata.networks[current_network].auth_name = mystrdup(var);
}

static char *p_auth_name(void)
{
  return mystrdup(gnetwork->auth_name);
}

static void c_autoadd_group_match(const char *key, char *var)
{
  char *split;
  autoadd_group_t *ag;

  split = strchr(var, ' ');
  if (split == NULL) {
    invalid_args(key, var);
    return;
  }

  *(split++) = 0;
  ag = irlist_add(&(gdata.autoadd_group_match), sizeof(autoadd_group_t));
  ag->a_group = mystrdup(var);
  ag->a_pattern = mystrdup(split);
}

static void d_autoadd_group_match(const char *key)
{
  autoadd_group_t *ag;

  for (ag = irlist_get_head(&gdata.autoadd_group_match);
       ag;
       ag = irlist_get_next(ag)) {
    dump_line("%s %s \"%s\"", key, ag->a_group, ag->a_pattern); /* NOTRANSLATE */
  }
}

static void c_autosendpack(const char * UNUSED(key), char *var)
{
  char *part[3];
  unsigned int rawval;

  get_argv(part, var, 3);
  if (part[0] && part[1]) {
    autoqueue_t *aq;
    aq = irlist_add(&gdata.autoqueue, sizeof(autoqueue_t));
    rawval = atoi(part[0]);
    if (rawval == XDCC_SEND_LIST) {
      aq->pack = XDCC_SEND_LIST;
    } else {
      aq->pack = between(0, rawval, 100000);
    }
    aq->word = part[1];
    aq->message = part[2];
  } else {
    mydelete(part[1]);
    mydelete(part[2]);
  }
  mydelete(part[0]);
}

static void d_autosendpack(const char *key)
{
  autoqueue_t *aq;

  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_get_next(aq)) {
    dump_line("%s %u %s \"%s\"", key, aq->pack, aq->word, aq->message); /* NOTRANSLATE */
  }
}

static char *p_bandmax(void)
{
  char *text;

  text = mymalloc(maxtextlengthshort);
  snprintf(text, maxtextlengthshort, "%u.0kB/s", gdata.maxb / 4); /* NOTRANSLATE */
  return text;
}

static char *p_banduse(void)
{
  return get_current_bandwidth();
}

static void c_channel(const char * UNUSED(key), char *var)
{
  char *part[2];
  channel_t *cptr;
  channel_t *rch;

  updatecontext();

  get_argv(part, var, 2);
  if (part[0] == NULL)
    return;

  for (rch = irlist_get_head(&gdata.networks[current_network].channels);
       rch;
       rch = irlist_get_next(rch)) {
    if (strcmp(rch->name, part[0]) == 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "could not add channel %s twice!", part[0]);
      mydelete(part[0]);
      mydelete(part[1]);
      return;
    }
  }

  cptr = irlist_add(&gdata.networks[current_network].channels, sizeof(channel_t));
  cptr->name = part[0];
  caps(cptr->name);
  if (parse_channel_options(cptr, part[1]) < 0) {
     outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored channel '%s' on %s because it has invalid args: '%s'",
             current_config, current_line, part[0],
             gdata.networks[current_network].name, part[1]);
  }

  if (cptr->plisttime && (cptr->plistoffset >= cptr->plisttime)) {
     outerror(OUTERROR_TYPE_WARN,
             "%s:%ld channel '%s' on %s: plistoffset must be less than plist time, ignoring offset",
             current_config, current_line, part[0], gdata.networks[current_network].name);
     cptr->plistoffset = 0;
  }
  mydelete(part[1]);
}

static void c_channel_join_raw(const char *key, char *var)
{
  if (var == NULL) {
    invalid_args(key, var);
    return;
  }

  irlist_add_string(&gdata.networks[current_network].channel_join_raw, var);
}

static void c_connectionmethod(const char * UNUSED(key), char *var)
{
  char *part[5];
  unsigned int m;

  mydelete(gdata.networks[current_network].connectionmethod.host);
  mydelete(gdata.networks[current_network].connectionmethod.password);
  mydelete(gdata.networks[current_network].connectionmethod.vhost);
  bzero((char *) &gdata.networks[current_network].connectionmethod, sizeof(connectionmethod_t));

  m = get_argv(part, var, 5);
  if (part[1]) {
    gdata.networks[current_network].connectionmethod.host = part[1];
    part[1] = NULL;
  }
  if (part[2]) {
    gdata.networks[current_network].connectionmethod.port = atoi(part[2]);
  }
  if (part[3]) {
    gdata.networks[current_network].connectionmethod.password = part[3];
    part[3] = NULL;
  }
  if (part[4]) {
    gdata.networks[current_network].connectionmethod.vhost = part[4];
    part[4] = NULL;
  }
  if (part[0] != NULL) {
    if (!strcmp(part[0], "direct")) { /* NOTRANSLATE */
      gdata.networks[current_network].connectionmethod.how = how_direct;
    } else if (!strcmp(part[0], "ssl")) { /* NOTRANSLATE */
#ifdef USE_SSL
      gdata.networks[current_network].connectionmethod.how = how_ssl;
#else
      gdata.networks[current_network].connectionmethod.how = how_direct;
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "%s:%ld connectionmethod ssl not compiled, defaulting to direct",
               current_config, current_line);
#endif /* USE_SSL */
    } else if ((m >= 3) && !strcmp(part[0], "bnc")) { /* NOTRANSLATE */
      gdata.networks[current_network].connectionmethod.how = how_bnc;
    } else if ((m == 3) && !strcmp(part[0], "wingate")) { /* NOTRANSLATE */
      gdata.networks[current_network].connectionmethod.how = how_wingate;
    } else if ((m == 3) && !strcmp(part[0], "custom")) { /* NOTRANSLATE */
      gdata.networks[current_network].connectionmethod.how = how_custom;
    } else {
      gdata.networks[current_network].connectionmethod.how = how_direct;
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

static void c_disk_quota(const char * UNUSED(key), char *var)
{
  gdata.disk_quota = atoull(var)*1024*1024;
}

static char *p_disk_quota(void)
{
  return print_config_long2(gdata.disk_quota);
}

static off_t get_toffered(void)
{
  xdcc *xd;
  off_t toffered;

  toffered = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (hide_pack(xd))
      continue;

    toffered += xd->st_size;
  }
  return toffered;
}

static char *p_disk_space(void)
{
  return print_config_long2(get_toffered());
}

static char *p_disk_space_text(void)
{
  return sizestr(0, get_toffered());
}

static void d_disk_quota(const char *key)
{
  dump_config_mega2(key, gdata.disk_quota);
}

static void c_getip_network(const char *key, char *var)
{
  int net;

  net = get_network(var);
  if (net < 0) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has unknown args: '%s'",
             current_config, current_line, key, var);
  } else {
    gdata.networks[current_network].getip_net = (unsigned int)net;
  }
}

static char *p_getip_network(void)
{
  return print_config_long2((ir_int64)(gnetwork->getip_net));
}

static void c_group_admin(const char *key, char *var)
{
  group_admin_t *ga;
  char *data;
  unsigned int stufe = 0;
  unsigned int drop = 0;

  ga = irlist_add(&(gdata.group_admin), sizeof(group_admin_t));
  for (data = strtok(var, " "); /* NOTRANSLATE */
       data && (drop == 0);
       data = strtok(NULL, " ")) { /* NOTRANSLATE */
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
    case 5:
      ga->g_uploaddir = mystrdup(data);
      break;
    default:
      ++drop;
      break;
    }
  }
  if (!ga->g_groups)
    ++drop;

  if (drop) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored '%s' because it has unknown args: '%s'",
             current_config, current_line, key, var);
    mydelete(ga->g_host);
    mydelete(ga->g_pass);
    mydelete(ga->g_groups);
    mydelete(ga->g_uploaddir);
    ga = irlist_delete(&gdata.group_admin, ga);
  }
}

static void d_group_admin(const char *key)
{
  group_admin_t *ga;

  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_get_next(ga)) {

    if (ga->g_uploaddir != NULL) {
      dump_line("%s %u %s %s %s \"%s\"", /* NOTRANSLATE */
                key, ga->g_level, ga->g_host, ga->g_pass, ga->g_groups, ga->g_uploaddir);
    } else {
      dump_line("%s %u %s %s %s", /* NOTRANSLATE */
                key, ga->g_level, ga->g_host, ga->g_pass, ga->g_groups);
    }
  }
}

static char *p_idlequeueused(void)
{
  return print_config_long2((ir_int64)(irlist_size(&gdata.idlequeue)));
}

static void c_ignoreduplicateip(const char * key, char *var)
{
  int val;

  val = parse_bool_val(key, var);
  if (val < 0)
    return;

  if (val > 0) {
    gdata.ignore_duplicate_ip = 24;
  } else {
    gdata.ignore_duplicate_ip = 0;
  }
}

static char *p_ignoreduplicateip(void)
{
  return print_config_long2((ir_int64)(gdata.ignore_duplicate_ip));
}

static void c_logrotate(const char * UNUSED(key), char *var)
{
  unsigned int val;

  val = atoi(var);
  if (val > 0) {
    gdata.logrotate = val*60*60;
    return;
  }
  if (strcmp(var, "daily") == 0) { /* NOTRANSLATE */
    gdata.logrotate =    24*60*60;
    return;
  }
  if (strcmp(var, "weekly") == 0) { /* NOTRANSLATE */
    gdata.logrotate =  7*24*60*60;
    return;
  }
  if (strcmp(var, "monthly") == 0) { /* NOTRANSLATE */
    gdata.logrotate = 30*24*60*60;
    return;
  }
}

static char *p_logrotate(void)
{
  return print_config_long2((ir_int64)(gdata.logrotate));
}

static void d_logrotate(const char *key)
{
  if (gdata.logrotate == 30*24*60*60) {
    dump_config_string2(key, "monthly"); /* NOTRANSLATE */
    return;
  }
  if (gdata.logrotate ==  7*24*60*60) {
    dump_config_string2(key, "weekly"); /* NOTRANSLATE */
    return;
  }
  if (gdata.logrotate ==    24*60*60) {
    dump_config_string2(key, "daily"); /* NOTRANSLATE */
    return;
  }
  dump_config_int3(key, gdata.logrotate, 0);
}

static void c_local_vhost(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].local_vhost);
  gdata.networks[current_network].local_vhost = mystrdup(var);
}

static char *p_local_vhost(void)
{
  return mystrdup(gnetwork->local_vhost);
}

static void c_login_name(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].login_name);
  gdata.networks[current_network].login_name = mystrdup(var);
}

static char *p_login_name(void)
{
  return mystrdup(gnetwork->login_name);
}

static char *p_mainqueueused(void)
{
  return print_config_long2((ir_int64)(irlist_size(&gdata.mainqueue)));
}

static void c_mime_type(const char *key, char *var)
{
  char *split;
  http_magic_t *mime;

  split = strchr(var, ' ');
  if (split == NULL) {
    invalid_args(key, var);
    return;
  }

  *(split++) = 0;
  mime = irlist_add(&(gdata.mime_type), sizeof(http_magic_t));
  mime->m_ext = mystrdup(var);
  mime->m_mime = mystrdup(split);
}

static void d_mime_type(const char *key)
{
  http_magic_t *mime;

  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_get_next(mime)) {
    dump_line("%s %s %s", key, mime->m_ext, mime->m_mime); /* NOTRANSLATE */
  }
}

static void c_need_level(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, 0, 3) == 0) {
    gdata.networks[current_network].need_level = (unsigned int)rawval;
  }
}

static char *p_need_level(void)
{
  return print_config_long2((ir_int64)(gdata.need_level));
}

static void c_network(const char * UNUSED(key), char *var)
{
  char *bracket;
  unsigned int ss;

  current_bracket = 0;
  if (var[0] != 0) {
    /* check for opening bracket */
    bracket = strchr(var, ' ');
    if (bracket != NULL) {
      *(bracket++) = 0;
      if (strchr(bracket, '{') != NULL)
        ++(current_bracket);
    }
    /* check if the given network does exist */
    for (ss=0; ss < MAX_NETWORKS; ++ss) {
      if (ss > gdata.networks_online)
        break;
      if (gdata.networks[ss].name == NULL)
        continue;
      if (strcasecmp(gdata.networks[ss].name, var) != 0)
        continue;
      current_network = ss;
      return;
    }
  }
  if (gdata.networks_online == 0) {
     /* add new network only when server defined */
     if (irlist_size(&gdata.networks[gdata.networks_online].servers))
       ++(gdata.networks_online);
  } else {
    ++(gdata.networks_online);
  }
  if (gdata.networks_online > MAX_NETWORKS) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld ignored network '%s' because we have to many.",
             current_config, current_line, var);
    gdata.networks_online = MAX_NETWORKS;
    return;
  }
  current_network = gdata.networks_online;
  gdata.networks[current_network].net = current_network;
  mydelete(gdata.networks[current_network].name);
  if (var[0] != 0) {
    gdata.networks[current_network].name = mystrdup(var);
  } else {
    set_default_network_name();
  }
}

static void c_nickserv_pass(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].nickserv_pass);
  gdata.networks[current_network].nickserv_pass = mystrdup(var);
}

static char *p_nickserv_pass(void)
{
  return mystrdup(gnetwork->nickserv_pass);
}

static void c_noannounce(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].noannounce), key, var);
}

static char *p_noannounce(void)
{
  return print_config_long2((ir_int64)(gnetwork->noannounce));
}

static void c_offline(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].offline), key, var);
}

static char *p_offline(void)
{
  return print_config_long2((ir_int64)(gnetwork->offline));
}

static void c_overallmaxspeeddaydays(const char * UNUSED(key), char *var)
{
  char *src;
  unsigned int i;

  gdata.overallmaxspeeddaydays = 0;
  src = var;
  for (i=0; (*src) && (i<8); ++i) {
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

static char d_weekdays[8] = { 'U', 'M', 'T', 'W', 'R', 'F', 'S', 0 };

static void get_weekdays(char *src)
{
  unsigned int i;
  unsigned int mask;

  mask = 1;
  for (i=0; (i<7); ++i) {
    if ((gdata.overallmaxspeeddaydays & mask) != 0) {
      *(src++) = d_weekdays[i];
    }
    mask <<= 1;
  }
  *src = 0;
}

static char *p_overallmaxspeeddaydays(void)
{
  char val[8];

  get_weekdays(val);
  return mystrdup(val);
}

static void d_overallmaxspeeddaydays(const char *key)
{
  char val[8];

  if ((gdata.dump_all == 0) && (gdata.overallmaxspeeddaydays == 0x7F))
    return;

  get_weekdays(val);
  dump_config_string2(key, val);
}

/* read an uint value from text with bounds */
static unsigned int atoui_between(const char * text, int min, int max )
{
  int val;

  val = atoi(text);
  if (val < min)
    return (unsigned int)min;

  if (val > max)
    return (unsigned int)max;

  return (unsigned int)val;
}

static void c_overallmaxspeeddaytime(const char * UNUSED(key), char *var)
{
  char *part[2];

  if (get_argv(part, var, 2) == 2) {
    gdata.overallmaxspeeddaytimestart = atoui_between(part[0], 0, 23);
    gdata.overallmaxspeeddaytimeend   = atoui_between(part[1], 0, 23);
  }
  mydelete(part[0]);
  mydelete(part[1]);
}

static void d_overallmaxspeeddaytime(const char *key)
{
  if ((gdata.dump_all == 0) && (gdata.overallmaxspeeddaytimestart == 0) && (gdata.overallmaxspeeddaytimeend == 0))
    return;

  dump_line("%s %u %u", /* NOTRANSLATE */
            key, gdata.overallmaxspeeddaytimestart, gdata.overallmaxspeeddaytimeend);
}

static char *p_packsum(void)
{
  return print_config_long2((ir_int64)(irlist_size(&gdata.xdccs)));
}

static void c_periodicmsg(const char *key, char *var)
{
  char *part[4];
  periodicmsg_t *pm;
  unsigned int rawval;
  unsigned int m;

  m = get_argv(part, var, 3);
  if (m >= 3) {
    pm = irlist_add(&gdata.periodicmsg, sizeof(periodicmsg_t));
    rawval = atoi(part[1]);
    if (rawval < 1)
      rawval = 1;
    pm->pm_time = rawval;
    pm->pm_time = atoi(part[1]);
    pm->pm_net = current_network;
    pm->pm_nick = part[0];
    pm->pm_msg = part[2];
  } else {
    invalid_args(key, var);
    mydelete(part[2]);
    mydelete(part[0]);
  }
  mydelete(part[1]);
}

static void d_periodicmsg(const char *key, unsigned int net)
{
  periodicmsg_t *pm;

  for (pm = irlist_get_head(&gdata.periodicmsg);
       pm;
       pm = irlist_get_next(pm)) {
    if (pm->pm_net != net)
      continue;
    dump_line("%s %s %u \"%s\"", /* NOTRANSLATE */
              key, pm->pm_nick, pm->pm_time, pm->pm_msg);
  }
}

static void c_plaintext(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].plaintext), key, var);
}

static char *p_plaintext(void)
{
  return print_config_long2((ir_int64)(gnetwork->plaintext));
}

static void c_proxyinfo(const char *key, char *var)
{
  if (var == NULL) {
    invalid_args(key, var);
    return;
  }

  irlist_add_string(&gdata.networks[current_network].proxyinfo, var);
}

static void c_respondtochannellist(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].respondtochannellist), key, var);
}

static char *p_respondtochannellist(void)
{
  return print_config_long2((ir_int64)(gnetwork->respondtochannellist));
}

static void c_respondtochannelxdcc(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].respondtochannelxdcc), key, var);
}

static char *p_respondtochannelxdcc(void)
{
  return print_config_long2((ir_int64)(gnetwork->respondtochannelxdcc));
}

static void c_restrictlist(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].restrictlist), key, var);
}

static char *p_restrictlist(void)
{
  return print_config_long2((ir_int64)(gnetwork->restrictlist));
}

static void c_restrictsend(const char *key, char *var)
{
  parse_bool_to_ptr(&(gdata.networks[current_network].restrictsend), key, var);
}

static char *p_restrictsend(void)
{
  return print_config_long2((ir_int64)(gnetwork->restrictsend));
}

static void c_send_listfile(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, -1, 1000000))
    return;

  /* -1 is converted to XDCC_SEND_LIST */
  gdata.send_listfile = (unsigned int)rawval;
}

static char *p_send_listfile(void)
{
  return print_config_long2((ir_int64)(gdata.send_listfile));
}

static void c_server(const char *key, char *var)
{
  server_t *ss;
  char *part[3];

  set_default_network_name();
  get_argv(part, var, 3);
  if (part[0] == NULL) {
    invalid_args(key, var);
    mydelete(part[0]);
    mydelete(part[1]);
    mydelete(part[2]);
    return;
  }
  ss = irlist_add(&gdata.networks[current_network].servers, sizeof(server_t));
  ss->hostname = part[0];
  ss->port = 6667;
  ss->password = part[2];
  if (part[1]) {
    ss->port = atoi(part[1]);
    mydelete(part[1]);
  }
}

static void c_server_connect_timeout(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, CTIMEOUT, 2000) == 0) {
    gdata.networks[current_network].server_connect_timeout = (unsigned int)rawval;
  }
}

static void c_server_connected_raw(const char *key, char *var)
{
  if (var == NULL) {
    invalid_args(key, var);
    return;
  }

  irlist_add_string(&gdata.networks[current_network].server_connected_raw, var);
}

static void c_server_join_raw(const char *key, char *var)
{
  if (var == NULL) {
    invalid_args(key, var);
    return;
  }

  irlist_add_string(&gdata.networks[current_network].server_join_raw, var);
}

static void c_server_send_max(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, 300, 1200))
    return;

  gdata.networks[current_network].server_send_max = rawval;
}

static char *p_server_send_max(void)
{
  return print_config_long2((ir_int64)(gnetwork->server_send_max));
}

static void c_server_send_rate(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, 10, 300))
    return;

  gdata.networks[current_network].server_send_rate = rawval;
}

static char *p_server_send_rate(void)
{
  return print_config_long2((ir_int64)(gnetwork->server_send_rate));
}

static char *p_slotsfree(void)
{
  return print_config_long2((ir_int64)slotsfree());
}

static void c_slotsmax(const char * UNUSED(key), char *var)
{
  unsigned int ival;

  ival = atoi(var);
  gdata.slotsmax = between(1, ival, gdata.maxtrans);
  if (gdata.slotsmax != ival) {
    outerror(OUTERROR_TYPE_WARN,
             "%s:%ld unable to have slotsmax of %u, using %u instead",
             current_config, current_line, ival, gdata.slotsmax);
  }
}

static char *p_slotsmax(void)
{
  return print_config_long2((ir_int64)(gdata.slotsmax));
}

static void d_slotsmax(const char *key)
{
  dump_config_int3(key, gdata.slotsmax, 0);
}

static char *p_slotsused(void)
{
  return print_config_long2((ir_int64)(irlist_size(&gdata.trans)));
}

static void c_slow_privmsg(const char *key, char *var)
{
  int rawval;

  if (check_range(key, var, &rawval, 0, 120) == 0) {
    gdata.networks[current_network].slow_privmsg = (unsigned int)rawval;
  }
}

static char *p_slow_privmsg(void)
{
  return print_config_long2((ir_int64)(gnetwork->slow_privmsg));
}

static void c_statefile(const char * UNUSED(key), char *var)
{
  int i;

  mydelete(gdata.statefile);
  gdata.statefile = mystrdup(var);
  convert_to_unix_slash(gdata.statefile);

  /* create if not exist */
  i = open(gdata.statefile, O_RDWR | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS );
  if (i >= 0)
    close(i);
}

static char *p_statefile(void)
{
  return mystrdup(gdata.statefile);
}

static void d_statefile(const char *key)
{
  dump_config_string3(key, gdata.statefile);
}

static char *p_totaluptime(void)
{
  char *text;

  text = mymalloc(maxtextlengthshort);
  return getuptime(text, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
}

static void c_transferlimits(const char * UNUSED(key), char *var)
{
  char *part[NUMBER_TRANSFERLIMITS];
  unsigned int m;
  unsigned int i;
  unsigned int ival;

  bzero((char *)part, sizeof(part));
  m = get_argv(part, var, (int)NUMBER_TRANSFERLIMITS);
  for (i=0; i<m; ++i) {
    if (part[i]) {
      ival = atoi(part[i]);
      gdata.transferlimits[i].limit = ival;
      gdata.transferlimits[i].limit *= 1024 * 1024;
      mydelete(part[i]);
    }
  }
}

static void d_transferlimits(const char *key)
{
  unsigned int ii;
  ir_int64 val[NUMBER_TRANSFERLIMITS];

  for (ii=0; ii<NUMBER_TRANSFERLIMITS; ++ii) {
    val[ii] = gdata.transferlimits[ii].limit;
    val[ii] /= ( 1024 * 1024 );
  }
  dump_line("%s %" LLPRINTFMT "u %" LLPRINTFMT "u %" LLPRINTFMT "u", /* NOTRANSLATE */
            key, val[TRANSFERLIMIT_DAILY], val[TRANSFERLIMIT_WEEKLY], val[TRANSFERLIMIT_MONTHLY]);
}

static void c_transfermaxspeed(const char * UNUSED(key), char *var)
{
  float fval;

  fval = atof(var);
  gdata.transfermaxspeed = max2(0, fval);
}

static void d_transfermaxspeed(const char *key)
{
  dump_config_float2(key, gdata.transfermaxspeed);
}

static void c_transferminspeed(const char * UNUSED(key), char *var)
{
  gdata.transferminspeed = atof(var);
}

static void d_transferminspeed(const char *key)
{
  dump_config_float2(key, gdata.transferminspeed);
}

static char *p_transfereddaily(void)
{
  return sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
}

static char *p_transferedweekly(void)
{
  return sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
}

static char *p_transferedmonthly(void)
{
  return sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
}

static char *p_transferedtotal(void)
{
  return sizestr(0, gdata.totalsent);
}

static char *p_transferedtotalbytes(void)
{
  return print_config_long2(gdata.totalsent);
}

static void c_uploadmaxsize(const char * UNUSED(key), char *var)
{
  gdata.uploadmaxsize = atoull(var)*1024*1024;
}

static char *p_uploadmaxsize(void)
{
  return print_config_long2(gdata.uploadmaxsize);
}

static void d_uploadmaxsize(const char *key)
{
  dump_config_mega2(key, gdata.uploadmaxsize);
}

static void c_uploadminspace(const char * UNUSED(key), char *var)
{
  gdata.uploadminspace = atoull(var)*1024*1024;
}

static char *p_uploadminspace(void)
{
  return print_config_long2(gdata.uploadminspace);
}

static void d_uploadminspace(const char *key)
{
  dump_config_mega2(key, gdata.uploadminspace);
}

static char *p_uptime(void)
{
  char *text;

  text = mymalloc(maxtextlengthshort);
  return getuptime(text, 1, gdata.startuptime, maxtextlengthshort);
}

static void c_usenatip(const char * UNUSED(key), char *var)
{
  gnetwork_t *backup;

  backup = gnetwork;
  gnetwork = &(gdata.networks[current_network]);
  gnetwork->natip = mystrdup(var);
  gnetwork->usenatip = 1;
  update_natip(var);
  gnetwork = backup;
}

static char *p_usenatip(void)
{
  if (gnetwork->natip == NULL)
    return NULL;
  return mystrdup(gnetwork->natip);
}

static void c_user_modes(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].user_modes);
  gdata.networks[current_network].user_modes = mystrdup(var);
}

static char *p_user_modes(void)
{
  return mystrdup(gnetwork->user_modes);
}

static void c_user_nick(const char * UNUSED(key), char *var)
{
  mydelete(gdata.networks[current_network].config_nick);
  gdata.networks[current_network].config_nick = mystrdup(var);
}

static char *p_user_nick(void)
{
  return mystrdup(gnetwork->config_nick);
}

static char *p_version(void)
{
  return mystrdup( "iroffer-dinoex " VERSIONLONG ); /* NOTRANSLATE */
}

static char *p_features(void)
{
  char *text;

  text = mymalloc(maxtextlength);
  snprintf(text, maxtextlength, "iroffer-dinoex " VERSIONLONG FEATURES "%s%s",
           gdata.hideos ? "" : " - ",
           gdata.hideos ? "" : gdata.osstring);
  return text;
}

static void c_bracket_open(const char * UNUSED(key), char * UNUSED(var))
{
  ++current_bracket;
}

static void c_bracket_close(const char * UNUSED(key), char *UNUSED(var))
{
  --current_bracket;
}

static int config_func_anzahl = 0;
static config_func_typ config_parse_func[] = {
{"auth_name",              c_auth_name }, /* NOTRANSLATE */
{"autoadd_group_match",    c_autoadd_group_match }, /* NOTRANSLATE */
{"autosendpack",           c_autosendpack }, /* NOTRANSLATE */
{"channel",                c_channel }, /* NOTRANSLATE */
{"channel_join_raw",       c_channel_join_raw }, /* NOTRANSLATE */
{"connectionmethod",       c_connectionmethod }, /* NOTRANSLATE */
{"disk_quota",             c_disk_quota }, /* NOTRANSLATE */
{"getip_network",          c_getip_network }, /* NOTRANSLATE */
{"group_admin",            c_group_admin }, /* NOTRANSLATE */
{"ignoreduplicateip",      c_ignoreduplicateip }, /* NOTRANSLATE */
{"local_vhost",            c_local_vhost }, /* NOTRANSLATE */
{"login_name",             c_login_name }, /* NOTRANSLATE */
{"logrotate",              c_logrotate }, /* NOTRANSLATE */
{"mime_type",              c_mime_type }, /* NOTRANSLATE */
{"need_level",             c_need_level }, /* NOTRANSLATE */
{"network",                c_network }, /* NOTRANSLATE */
{"nickserv_pass",          c_nickserv_pass }, /* NOTRANSLATE */
{"noannounce",             c_noannounce }, /* NOTRANSLATE */
{"offline",                c_offline }, /* NOTRANSLATE */
{"overallmaxspeeddaydays", c_overallmaxspeeddaydays }, /* NOTRANSLATE */
{"overallmaxspeeddaytime", c_overallmaxspeeddaytime }, /* NOTRANSLATE */
{"periodicmsg",            c_periodicmsg }, /* NOTRANSLATE */
{"plaintext",              c_plaintext }, /* NOTRANSLATE */
{"proxyinfo",              c_proxyinfo }, /* NOTRANSLATE */
{"respondtochannellist",   c_respondtochannellist }, /* NOTRANSLATE */
{"respondtochannelxdcc",   c_respondtochannelxdcc }, /* NOTRANSLATE */
{"restrictlist",           c_restrictlist }, /* NOTRANSLATE */
{"restrictsend",           c_restrictsend }, /* NOTRANSLATE */
{"send_listfile",          c_send_listfile }, /* NOTRANSLATE */
{"server",                 c_server }, /* NOTRANSLATE */
{"server_connect_timeout", c_server_connect_timeout }, /* NOTRANSLATE */
{"server_connected_raw",   c_server_connected_raw }, /* NOTRANSLATE */
{"server_join_raw",        c_server_join_raw }, /* NOTRANSLATE */
{"server_send_max",        c_server_send_max }, /* NOTRANSLATE */
{"server_send_rate",       c_server_send_rate }, /* NOTRANSLATE */
{"slotsmax",               c_slotsmax }, /* NOTRANSLATE */
{"slow_privmsg",           c_slow_privmsg }, /* NOTRANSLATE */
{"statefile",              c_statefile }, /* NOTRANSLATE */
{"transferlimits",         c_transferlimits }, /* NOTRANSLATE */
{"transfermaxspeed",       c_transfermaxspeed }, /* NOTRANSLATE */
{"transferminspeed",       c_transferminspeed }, /* NOTRANSLATE */
{"uploadmaxsize",          c_uploadmaxsize }, /* NOTRANSLATE */
{"uploadminspace",         c_uploadminspace }, /* NOTRANSLATE */
{"usenatip",               c_usenatip }, /* NOTRANSLATE */
{"user_modes",             c_user_modes }, /* NOTRANSLATE */
{"user_nick",              c_user_nick }, /* NOTRANSLATE */
{"{",                      c_bracket_open }, /* NOTRANSLATE */
{"}",                      c_bracket_close }, /* NOTRANSLATE */
{NULL, NULL }};

static const char *config_name_func(unsigned int i)
{
  return config_parse_func[i].name;
}

static int config_find_func(const char *key)
{
  return config_find_typ(config_name_func, key, config_func_anzahl);
}

static int set_config_func(const char *key, char *text)
{
  int i;

  updatecontext();

  i = config_find_func(key);
  if (i < 0)
    return 1;

  (*config_parse_func[i].func)(config_parse_func[i].name, text);
  return 0;
}

static int config_fprint_anzahl = 0;
static config_fprint_typ config_parse_fprint[] = {
{"auth_name",              p_auth_name }, /* NOTRANSLATE */
{"bandmax",                p_bandmax }, /* NOTRANSLATE */
{"banduse",                p_banduse }, /* NOTRANSLATE */
{"disk_quota",             p_disk_quota }, /* NOTRANSLATE */
{"disk_space",             p_disk_space }, /* NOTRANSLATE */
{"disk_space_text",        p_disk_space_text }, /* NOTRANSLATE */
{"features",               p_features }, /* NOTRANSLATE */
{"getip_network",          p_getip_network }, /* NOTRANSLATE */
{"idlequeueused",          p_idlequeueused }, /* NOTRANSLATE */
{"ignoreduplicateip",      p_ignoreduplicateip }, /* NOTRANSLATE */
{"local_vhost",            p_local_vhost }, /* NOTRANSLATE */
{"login_name",             p_login_name }, /* NOTRANSLATE */
{"logrotate",              p_logrotate }, /* NOTRANSLATE */
{"mainqueueused",          p_mainqueueused }, /* NOTRANSLATE */
{"need_level",             p_need_level }, /* NOTRANSLATE */
{"nickserv_pass",          p_nickserv_pass }, /* NOTRANSLATE */
{"noannounce",             p_noannounce }, /* NOTRANSLATE */
{"offline",                p_offline }, /* NOTRANSLATE */
{"overallmaxspeeddaydays", p_overallmaxspeeddaydays }, /* NOTRANSLATE */
{"packsum",                p_packsum }, /* NOTRANSLATE */
{"plaintext",              p_plaintext }, /* NOTRANSLATE */
{"respondtochannellist",   p_respondtochannellist }, /* NOTRANSLATE */
{"respondtochannelxdcc",   p_respondtochannelxdcc }, /* NOTRANSLATE */
{"restrictlist",           p_restrictlist }, /* NOTRANSLATE */
{"restrictsend",           p_restrictsend }, /* NOTRANSLATE */
{"send_listfile",          p_send_listfile }, /* NOTRANSLATE */
{"server_send_max",        p_server_send_max }, /* NOTRANSLATE */
{"server_send_rate",       p_server_send_rate }, /* NOTRANSLATE */
{"slotsfree",              p_slotsfree }, /* NOTRANSLATE */
{"slotsmax",               p_slotsmax }, /* NOTRANSLATE */
{"slotsused",              p_slotsused }, /* NOTRANSLATE */
{"slow_privmsg",           p_slow_privmsg }, /* NOTRANSLATE */
{"statefile",              p_statefile }, /* NOTRANSLATE */
{"totaluptime",            p_totaluptime }, /* NOTRANSLATE */
{"transfereddaily",        p_transfereddaily }, /* NOTRANSLATE */
{"transferedmonthly",      p_transferedmonthly }, /* NOTRANSLATE */
{"transferedtotal",        p_transferedtotal }, /* NOTRANSLATE */
{"transferedtotalbytes",   p_transferedtotalbytes }, /* NOTRANSLATE */
{"transferedweekly",       p_transferedweekly }, /* NOTRANSLATE */
{"uploadmaxsize",          p_uploadmaxsize }, /* NOTRANSLATE */
{"uploadminspace",         p_uploadminspace }, /* NOTRANSLATE */
{"uptime",                 p_uptime }, /* NOTRANSLATE */
{"usenatip",               p_usenatip }, /* NOTRANSLATE */
{"user_modes",             p_user_modes }, /* NOTRANSLATE */
{"user_nick",              p_user_nick }, /* NOTRANSLATE */
{"version",                p_version }, /* NOTRANSLATE */
{NULL, NULL }};

static const char *config_name_fprint(unsigned int i)
{
  return config_parse_fprint[i].name;
}

static int config_find_fprint(const char *key)
{
  return config_find_typ(config_name_fprint, key, config_fprint_anzahl);
}

static char *print_config_fprint(const char *key)
{
  int i;

  updatecontext();

  i = config_find_fprint(key);
  if (i < 0)
    return NULL;

  return (*config_parse_fprint[i].fprint)();
}


static int config_fdump_anzahl = 0;
static config_fdump_typ config_parse_fdump[] = {
{"autoadd_group_match",    d_autoadd_group_match }, /* NOTRANSLATE */
{"autosendpack",           d_autosendpack }, /* NOTRANSLATE */
{"disk_quota",             d_disk_quota }, /* NOTRANSLATE */
{"group_admin",            d_group_admin }, /* NOTRANSLATE */
{"logrotate",              d_logrotate }, /* NOTRANSLATE */
{"mime_type",              d_mime_type }, /* NOTRANSLATE */
{"overallmaxspeeddaydays", d_overallmaxspeeddaydays }, /* NOTRANSLATE */
{"overallmaxspeeddaytime", d_overallmaxspeeddaytime }, /* NOTRANSLATE */
{"slotsmax",               d_slotsmax }, /* NOTRANSLATE */
{"statefile",              d_statefile }, /* NOTRANSLATE */
{"transferlimits",         d_transferlimits }, /* NOTRANSLATE */
{"transfermaxspeed",       d_transfermaxspeed }, /* NOTRANSLATE */
{"transferminspeed",       d_transferminspeed }, /* NOTRANSLATE */
{"uploadmaxsize",          d_uploadmaxsize }, /* NOTRANSLATE */
{"uploadminspace",         d_uploadminspace }, /* NOTRANSLATE */
{NULL, NULL }};

static const char *config_name_fdump(unsigned int i)
{
  return config_parse_fdump[i].name;
}

static void dump_config_fdump(void)
{
  server_t *ss;
  channel_t *ch;
  const char *how;
  char *buffer;
  char *line;
  size_t len;
  unsigned int si;
  unsigned int i;

  for (i = 0; config_parse_fdump[i].name != NULL; ++i) {
    if (config_parse_fdump[i].fdump != NULL)
      (*config_parse_fdump[i].fdump)(config_parse_fdump[i].name);
  }

  for (si=0; si<gdata.networks_online; ++si) {
    dump_line("%s \"%s\"", "network", gdata.networks[si].name); /* NOTRANSLATE */
    dump_line("{"); /* NOTRANSLATE */

    how = text_connectionmethod(gdata.networks[si].connectionmethod.how);
    for (;;) {
      if (gdata.networks[si].connectionmethod.vhost) {
        dump_line("%s %s %s %u \"%s\" %s", "connectionmethod", how, /* NOTRANSLATE */
                  gdata.networks[si].connectionmethod.host,
                  gdata.networks[si].connectionmethod.port,
                  gdata.networks[si].connectionmethod.password,
                  gdata.networks[si].connectionmethod.vhost);
        break;
      }
      if (gdata.networks[si].connectionmethod.password) {
         dump_line("%s %s %s %u \"%s\"", "connectionmethod",  how, /* NOTRANSLATE */
                    gdata.networks[si].connectionmethod.host,
                    gdata.networks[si].connectionmethod.port,
                    gdata.networks[si].connectionmethod.password);
        break;
      }
      if (gdata.networks[si].connectionmethod.host) {
        dump_line("%s %s %s %u", "connectionmethod", how, /* NOTRANSLATE */
                  gdata.networks[si].connectionmethod.host,
                  gdata.networks[si].connectionmethod.port);
        break;
      }
      dump_line("%s %s", "connectionmethod", how); /* NOTRANSLATE */
      break;
    }

    dump_config_list2("proxyinfo", &gdata.networks[si].proxyinfo); /* NOTRANSLATE */
    dump_config_list2("server_join_raw", &gdata.networks[si].server_join_raw); /* NOTRANSLATE */
    dump_config_list2("server_connected_raw", &gdata.networks[si].server_connected_raw); /* NOTRANSLATE */
    dump_config_list2("channel_join_raw", &gdata.networks[si].channel_join_raw); /* NOTRANSLATE */
    dump_config_string3("local_vhost", gdata.networks[si].local_vhost); /* NOTRANSLATE */
    if (gdata.networks[si].usenatip != 0) {
      if (gdata.networks[si].natip != NULL)
        dump_config_string2("usenatip", gdata.networks[si].natip); /* NOTRANSLATE */
    }
    dump_config_string3("nickserv_pass", gdata.networks[si].nickserv_pass); /* NOTRANSLATE */
    dump_config_string3("auth_name", gdata.networks[si].auth_name); /* NOTRANSLATE */
    dump_config_string3("login_name", gdata.networks[si].login_name); /* NOTRANSLATE */
    dump_config_string3("user_nick", gdata.networks[si].config_nick); /* NOTRANSLATE */
    dump_config_string3("user_modes", gdata.networks[si].user_modes); /* NOTRANSLATE */

    d_periodicmsg("periodicmsg", si); /* NOTRANSLATE */

    for (ss = irlist_get_head(&gdata.networks[si].servers);
         ss;
         ss = irlist_get_next(ss)) {
      if (ss->password != NULL) {
        dump_line("%s %s %u %s", "server", ss->hostname, ss->port, ss->password); /* NOTRANSLATE */
      } else {
        if (ss->port != 6667) {
          dump_line("%s %s %u", "server", ss->hostname, ss->port); /* NOTRANSLATE */
        } else {
          dump_line("%s %s", "server", ss->hostname); /* NOTRANSLATE */
        }
      }
    }

    for (ch = irlist_get_head(&(gdata.networks[si].channels));
         ch;
         ch = irlist_get_next(ch)) {
      buffer = mymalloc(maxtextlength);
      len = add_snprintf(buffer, maxtextlength, "%s", ch->name); /* NOTRANSLATE */
      if (ch->plisttime != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s %u", "-plist", ch->plisttime); /* NOTRANSLATE */
      if (ch->plistoffset != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s %u", "-plistoffset", ch->plistoffset); /* NOTRANSLATE */
      if (((ch->flags & (CHAN_SUMMARY|CHAN_MINIMAL)) != 0) || (ch->plisttime != 0))
        len += add_snprintf(buffer + len, maxtextlength - len, " %s %s", "-pformat", text_pformat(ch->flags)); /* NOTRANSLATE */
      if (ch->pgroup != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-pgroup", ch->pgroup); /* NOTRANSLATE */
      if (ch->key != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-key", ch->key); /* NOTRANSLATE */
      if (ch->delay != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s %u", "-delay", ch->delay); /* NOTRANSLATE */
      if (ch->noannounce != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s", "-noannounce"); /* NOTRANSLATE */
      if (ch->joinmsg != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-joinmsg", ch->joinmsg); /* NOTRANSLATE */
      for (line = irlist_get_head(&(ch->headline));
           line;
           line = irlist_get_next(line)) {
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-headline", line); /* NOTRANSLATE */
      }
#ifndef WITHOUT_BLOWFISH
      if (ch->fish != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-fish", ch->fish); /* NOTRANSLATE */
#endif /* WITHOUT_BLOWFISH */
      if (ch->listmsg != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-listmsg", ch->listmsg); /* NOTRANSLATE */
      if (ch->rgroup != NULL)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s \"%s\"", "-rgroup", ch->rgroup); /* NOTRANSLATE */
      if (ch->notrigger != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s", "-notrigger"); /* NOTRANSLATE */
      if (ch->plaintext != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s", "-plaintext"); /* NOTRANSLATE */
      if (ch->waitjoin != 0)
        len += add_snprintf(buffer + len, maxtextlength - len, " %s %u", "-waitjoin", ch->waitjoin); /* NOTRANSLATE */
      dump_line("%s %s", "channel", buffer); /* NOTRANSLATE */
      mydelete(buffer);
    }

    dump_config_bool3("need_voice", gdata.networks[si].need_voice, 0); /* NOTRANSLATE */
    dump_config_int3("need_level", gdata.networks[si].need_level, 10); /* NOTRANSLATE */
    dump_config_int3("getip_network", gdata.networks[si].getip_net, si); /* NOTRANSLATE */
    dump_config_int3("slow_privmsg", gdata.networks[si].slow_privmsg, 1); /* NOTRANSLATE */
    dump_config_int3("server_send_max", gdata.networks[si].server_send_max, EXCESS_BUCKET_MAX); /* NOTRANSLATE */
    dump_config_int3("server_send_rate", gdata.networks[si].server_send_rate, EXCESS_BUCKET_ADD); /* NOTRANSLATE */
    dump_config_int3("server_connect_timeout", gdata.networks[si].server_connect_timeout, CTIMEOUT); /* NOTRANSLATE */
    dump_config_bool3("noannounce", gdata.networks[si].noannounce, 0); /* NOTRANSLATE */
    dump_config_bool3("offline", gdata.networks[si].offline, 0); /* NOTRANSLATE */
    dump_config_bool3("plaintext", gdata.networks[si].plaintext, 0); /* NOTRANSLATE */
    dump_config_bool3("restrictsend", gdata.networks[si].restrictsend, 0); /* NOTRANSLATE */
    dump_config_bool3("restrictlist", gdata.networks[si].restrictlist, 0); /* NOTRANSLATE */
    dump_line("}"); /* NOTRANSLATE */
  } /* networks */
}

static void reset_config_func(void)
{
  autoqueue_t *aq;
  tupload_t *tu;
  qupload_t *qu;
  fetch_queue_t *fq;
  group_admin_t *ga;
  http_magic_t *mime;
  autoadd_group_t *ag;
  periodicmsg_t *pm;
  server_t *ss;
  unsigned int si;
  unsigned int ii;

  updatecontext();

  for (si=0; si<MAX_NETWORKS; ++si) {
    for (ss = irlist_get_head(&gdata.networks[si].servers);
         ss;
         ss = irlist_delete(&gdata.networks[si].servers, ss)) {
      mydelete(ss->hostname);
      mydelete(ss->password);
    }
    mydelete(gdata.networks[si].nickserv_pass);
    mydelete(gdata.networks[si].config_nick);
    mydelete(gdata.networks[si].user_modes);
    mydelete(gdata.networks[si].local_vhost);
    mydelete(gdata.networks[si].natip);
    mydelete(gdata.networks[si].auth_name);
    mydelete(gdata.networks[si].login_name);
    irlist_delete_all(&gdata.networks[si].channels);
    irlist_delete_all(&gdata.networks[si].server_join_raw);
    irlist_delete_all(&gdata.networks[si].server_connected_raw);
    irlist_delete_all(&gdata.networks[si].channel_join_raw);
    irlist_delete_all(&gdata.networks[si].proxyinfo);
    gdata.networks[si].connectionmethod.how = how_direct;
    gdata.networks[si].usenatip = 0;
    gdata.networks[si].slow_privmsg = 1;
    gdata.networks[si].restrictsend = 2;
    gdata.networks[si].restrictlist = 2;
    gdata.networks[si].respondtochannellist = 2;
    gdata.networks[si].respondtochannelxdcc = 2;
    gdata.networks[si].need_voice = 2;
    gdata.networks[si].need_level = 10;
    gdata.networks[si].getip_net = si;
    gdata.networks[si].server_connect_timeout = CTIMEOUT;
    gdata.networks[si].server_send_max = EXCESS_BUCKET_MAX;
    gdata.networks[si].server_send_rate = EXCESS_BUCKET_ADD;
    gdata.networks[si].noannounce = 0;
    gdata.networks[si].offline = 0;
    gdata.networks[si].plaintext = 0;
  } /* networks */
  gdata.networks_online = 0;
  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_delete(&gdata.autoqueue, aq)) {
    mydelete(aq->word);
    mydelete(aq->message);
  }
  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu;
       tu = irlist_delete(&gdata.tuploadhost, tu)) {
    mydelete(tu->u_host);
  }
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_delete(&gdata.quploadhost, qu)) {
    mydelete(qu->q_host);
    mydelete(qu->q_nick);
    mydelete(qu->q_pack);
  }
  for (fq = irlist_get_head(&gdata.fetch_queue);
       fq;
       fq = irlist_delete(&gdata.fetch_queue, fq)) {
    mydelete(fq->u.snick);
    mydelete(fq->name);
    mydelete(fq->url);
    mydelete(fq->uploaddir);
  }
  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_delete(&gdata.group_admin, ga)) {
    mydelete(ga->g_host);
    mydelete(ga->g_pass);
    mydelete(ga->g_groups);
    mydelete(ga->g_uploaddir);
  }
  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_delete(&gdata.mime_type, mime)) {
    mydelete(mime->m_ext);
    mydelete(mime->m_mime);
  }
  for (ag = irlist_get_head(&gdata.autoadd_group_match);
       ag;
       ag = irlist_delete(&gdata.autoadd_group_match, ag)) {
    mydelete(ag->a_group);
    mydelete(ag->a_pattern);
  }
  for (pm = irlist_get_head(&gdata.periodicmsg);
       pm;
       pm = irlist_delete(&gdata.periodicmsg, pm)) {
    mydelete(pm->pm_nick);
    mydelete(pm->pm_msg);
  }
  for (ii=0; ii<NUMBER_TRANSFERLIMITS; ++ii) {
    gdata.transferlimits[ii].limit = 0;
  }
  mydelete(gdata.statefile);
  /* int */
  gdata.overallmaxspeeddaydays = 0x7F; /* all days */
  gdata.logrotate = 0;
  gdata.send_listfile = 0;
  gdata.slotsmax = 0;
  gdata.overallmaxspeeddaytimestart = 0;
  gdata.overallmaxspeeddaytimeend = 0;
  /* 64bit */
  gdata.disk_quota = 0;
  gdata.uploadmaxsize = 0;
  gdata.uploadminspace = 0;
  /* float */
  gdata.transferminspeed = gdata.transfermaxspeed = 0.0;
}


static void parse_config_line(char **part)
{
  if (current_bracket == 0) {
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

/* process a line of the config */
void getconfig_set(const char *line)
{
  char *part[2];

  updatecontext();

  get_argv(part, line, 2);
  parse_config_line(part);
  mydelete(part[0]);
  mydelete(part[1]);
}

/* pint a global config entry */
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

  val = print_config_fprint(key);
  if (val != NULL)
    return val;

  return NULL;
}

/* dump the global config to console and logfile */
void config_dump(void)
{
  dump_line("CONFIG DUMP BEGIN");
  dump_config_bool();
  dump_config_int();
  dump_config_string();
  dump_config_list();
  dump_config_fdump();
  dump_line("CONFIG DUMP END");
}

/* reset config to default values */
void config_reset(void)
{
  updatecontext();

  reset_config_func();
  reset_config_bool();
  reset_config_int();
  reset_config_string();
  reset_config_list();
}

static size_t config_expand_search_typ(config_name_t config_name_f, const char *key, size_t len, const char **first)
{
  const char *name;
  size_t found = 0;
  unsigned int i;

  for (i = 0; ; ++i) {
    name = config_name_f(i);
    if (name == NULL)
      break;
    if (strncmp(name, key, len))
      continue;

    ++found;
    if (*first == NULL) *first = name;
  }
  return found;
}

static void config_expand_list_typ(config_name_t config_name_f, const char *key, size_t len, const char *first)
{
  const char *name;
  unsigned int i;

  for (i = 0; ; ++i) {
    name = config_name_f(i);
    if (name == NULL)
      break;
    if (strncmp(name, key, len))
      continue;

    if (first != name)
      tostdout(",");
    tostdout(" %s", name);
  }
}

/* expand config keyword to commandline */
size_t config_expand(char *buffer, size_t max, int print)
{
  const char *first = NULL;
  size_t found = 0;
  size_t j;

  updatecontext();

  j = strlen(buffer);
  found = config_expand_search_typ(config_name_bool, buffer, j, &first);
  found += config_expand_search_typ(config_name_int, buffer, j, &first);
  found += config_expand_search_typ(config_name_string, buffer, j, &first);
  found += config_expand_search_typ(print ? config_name_fprint : config_name_func, buffer, j, &first);
  if (found == 0)
    return 0;
  if (found == 1) {
    snprintf(buffer, max, "%s ", first);
    return strlen(first) - j + 1;
  }
  tostdout("** Args:");
  config_expand_list_typ(config_name_bool, buffer, j, first);
  config_expand_list_typ(config_name_int, buffer, j, first);
  config_expand_list_typ(config_name_string, buffer, j, first);
  config_expand_list_typ(print ? config_name_fprint : config_name_func, buffer, j, first);
  tostdout("\n");
  return 0;
}

/* check all tables are sorted for fast access */
void config_startup(void)
{
  config_bool_anzahl = config_sorted_check(config_name_bool);
  config_int_anzahl = config_sorted_check(config_name_int);
  config_string_anzahl = config_sorted_check(config_name_string);
  config_list_anzahl = config_sorted_check(config_name_list);
  config_func_anzahl = config_sorted_check(config_name_func);
  config_fprint_anzahl = config_sorted_check(config_name_fprint);
  config_fdump_anzahl = config_sorted_check(config_name_fdump);
  current_config = "";
  current_line = 0;
}

/* EOF */
