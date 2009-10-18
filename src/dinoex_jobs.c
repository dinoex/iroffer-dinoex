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
#include "dinoex_admin.h"
#include "dinoex_irc.h"
#include "dinoex_config.h"
#include "dinoex_misc.h"
#include "dinoex_jobs.h"
#include "crc32.h"

#include <ctype.h>

#ifndef WITHOUT_BLOWFISH

#include "blowfish.h"

static const unsigned char FISH64[] = "./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static unsigned char fish64decode[ 256 ];

void init_fish64decode( void )
{
  int i;

  memset(fish64decode, 0, sizeof(fish64decode));
  for ( i = 0; i < 64; i++) {
    fish64decode[ FISH64[ i ] ] = i;
  }
}

static unsigned long bytes_to_long( const char **str )
{
  unsigned long result;
  unsigned char ch;

  ch = *(*str);
  result = ch << 24;
  if (ch == 0) return result;

  ++(*str);
  ch = *(*str);
  result |= ch << 16;
  if (ch == 0) return result;

  ++(*str);
  ch = *(*str);
  result |= ch << 8;
  if (ch == 0) return result;

  ++(*str);
  ch = *(*str);
  result |= ch;
  if (ch != 0)
    ++(*str);
  return result;
}

static char *encrypt_fish( const char *str, int len, const char *key)
{
  BLOWFISH_CTX ctx;
  char *dest;
  char *msg;
  size_t max;
  unsigned long left, right;
  int i;

  if (key == NULL)
    return NULL;

  if (key[0] == 0)
    return NULL;

  updatecontext();

  max = ( len + 3 ) / 4; /* immer 32bit */
  max *= 12; /* base64 */
  msg = mycalloc(max+1);

  dest = msg;
  Blowfish_Init(&ctx, (const unsigned char*)key, strlen(key));
  while (*str) {
    left = bytes_to_long(&str);
    right = bytes_to_long(&str);
    Blowfish_Encrypt(&ctx, &left, &right);
    for (i = 0; i < 6; i++) {
      *(dest++) = FISH64[right & 0x3f];
      right = (right >> 6);
    }
    for (i = 0; i < 6; i++) {
      *(dest++) = FISH64[left & 0x3f];
      left = (left >> 6);
    }
  }
  *dest = 0;
  memset(&ctx, 0, sizeof(BLOWFISH_CTX));
  return msg;
}

static unsigned long base64_to_long( const char **str )
{
  unsigned long result;
  unsigned char ch;
  int i;

  result = 0L;
  for (i = 0; i < 6; i++) {
    ch = (unsigned char)(*(*str)++);
    result |= fish64decode[ ch ] << (i * 6);
  }
  return result;
}


static char *decrypt_fish( const char *str, int len, const char *key)
{
  BLOWFISH_CTX ctx;
  char *dest;
  char *work;
  char *msg;
  size_t max;
  unsigned long left, right;
  int i;

  if (key == NULL)
    return NULL;

  if (key[0] == 0)
    return NULL;

  updatecontext();

  max = ( len + 11 / 12 );
  max *= 4;
  msg = mycalloc(max+1);

  dest = msg;
  Blowfish_Init(&ctx, (const unsigned char*)key, strlen(key));
  while (*str) {
    right = base64_to_long(&str);
    left = base64_to_long(&str);
    Blowfish_Decrypt(&ctx, &left, &right);
    dest += 4;
    work = dest;
    for (i = 0; i < 4; i++) {
      *(--work) = left & 0xff;
      left >>= 8;
    }
    dest += 4;
    work = dest;
    for (i = 0; i < 4; i++) {
      *(--work) = right & 0xff;
      right >>= 8;
    }
  }
  *dest = 0;
  memset(&ctx, 0, sizeof(BLOWFISH_CTX));
  return msg;
}

static char *privmsg_decrypt(const char *line, const char *channel, const char *key, const char *data)
{
  char *newstr;
  char *newline;
  char *end;
  size_t len;

  updatecontext();

  newstr = decrypt_fish(data, strlen(data), key);
  if (newstr == NULL)
    return NULL;

  newline = mystrdup(line);
  len = strlen(newline);
  end = strchr(newline, ' ');
  if (end == NULL) {
    mydelete(newline);
    mydelete(newstr);
    return NULL;
  }

  end = strchr(++end, ' ');
  if (end == NULL) {
    mydelete(newline);
    mydelete(newstr);
    return NULL;
  }

  *end = 0;
  len -= strlen(newline) - 1;
  *(end++) = ' ';
  snprintf(end, len, "%s :%s", channel, newstr);
  mydelete(newstr);
  if (gdata.debug > 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, ">FISH>: %s", newline);
  }
  return newline;
}

static const char *find_fish_key(const char *channel)
{
  channel_t *ch;

  updatecontext();

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (ch->fish == NULL)
      continue;
    if ((ch->flags & CHAN_ONCHAN) == 0)
      continue;
    if (strcasecmp(ch->name, channel) != 0)
      continue;

    return ch->fish;
  }
  return gdata.privmsg_fish;
}

char *test_fish_message(const char *line, const char *channel, const char *str, const char *data)
{
  const char *key;

  updatecontext();

  if (!str)
    return NULL;

  if (!data)
    return NULL;

  if (strcmp(str, ":+OK") != 0)
    return NULL;

  key = find_fish_key(channel);
  if (key == NULL)
    return NULL;

  return privmsg_decrypt(line, channel, key, data);
}

#endif /* WITHOUT_BLOWFISH */

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vwriteserver_channel(int delay, const char *format, va_list ap)
{
  char *msg;
  channel_announce_t *item;
  int len;

  updatecontext();

  msg = mycalloc(maxtextlength);

  len = vsnprintf(msg, maxtextlength, format, ap);

  if ((len < 0) || (len >= maxtextlength)) {
    outerror(OUTERROR_TYPE_WARN, "WRITESERVER: Output too large, ignoring!");
    mydelete(msg);
    return;
  }

  if (gdata.exiting || (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)) {
    mydelete(msg);
    return;
  }

  if (gdata.debug > 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<QUES<: %s", msg);
  }

  if (len > EXCESS_BUCKET_MAX) {
    outerror(OUTERROR_TYPE_WARN, "Message Truncated!");
    msg[EXCESS_BUCKET_MAX] = '\0';
    len = EXCESS_BUCKET_MAX;
  }

  if (irlist_size(&(gnetwork->serverq_channel)) < MAXSENDQ) {
    item = irlist_add(&(gnetwork->serverq_channel), sizeof(channel_announce_t));
    item->delay = delay;
    item->msg = msg;
  } else {
    outerror(OUTERROR_TYPE_WARN, "Server queue is very large. Dropping additional output.");
    mydelete(msg);
  }

  return;
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
writeserver_channel(int delay, const char *format, ... )
{
  va_list args;
  va_start(args, format);
  vwriteserver_channel(delay, format, args);
  va_end(args);
}

void
#ifdef __GNUC__
__attribute__ ((format(printf, 4, 0)))
#endif
vprivmsg_chan(int delay, const char *name, const char *fish, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  int len;

  if (!name) return;

  len = vsnprintf(tempstr, maxtextlength, format, ap);

  if ((len < 0) || (len >= maxtextlength)) {
    outerror(OUTERROR_TYPE_WARN, "PRVMSG-CHAN: Output too large, ignoring!");
    return;
  }

#ifndef WITHOUT_BLOWFISH
  if (fish) {
    char *tempcrypt;

    if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<FISH<: %s", tempstr);
    }
    tempcrypt = encrypt_fish(tempstr, len, fish);
    if (tempcrypt) {
      writeserver_channel(delay, "PRIVMSG %s :+OK %s", name, tempcrypt);
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver_channel(delay, "PRIVMSG %s :%s", name, tempstr);
}

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_chan(const channel_t *ch, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg_chan(ch->delay, ch->name, ch->fish, format, args);
  va_end(args);
}

#ifndef WITHOUT_BLOWFISH

static const char *check_fish_exclude(const char *nick)
{
  if (!gdata.privmsg_encrypt)
    return NULL;

  if (verifyshell(&gdata.fish_exclude_nick, nick))
    return NULL;

  return gdata.privmsg_fish;
}

#endif /* WITHOUT_BLOWFISH */

void writeserver_privmsg(writeserver_type_e delay, const char *nick, const char *message, int len)
{
#ifndef WITHOUT_BLOWFISH
  const char *fish;

  updatecontext();

  fish = check_fish_exclude(nick);
  if (fish != NULL) {
    char *tempcrypt;

    if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<FISH<: %s", message);
    }
    tempcrypt = encrypt_fish(message, len, fish);
    if (tempcrypt) {
      writeserver(delay, "PRIVMSG %s :+OK %s", nick, tempcrypt);
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver(delay, "PRIVMSG %s :%s", nick, message);
}

void writeserver_notice(writeserver_type_e delay, const char *nick, const char *message, int len)
{
#ifndef WITHOUT_BLOWFISH
  const char *fish;

  updatecontext();

  fish = check_fish_exclude(nick);
  if (fish != NULL) {
    char *tempcrypt;

    if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<FISH<: %s", message);
    }
    tempcrypt = encrypt_fish(message, len, fish);
    if (tempcrypt) {
      writeserver(delay, "NOTICE %s :+OK %s", nick, tempcrypt);
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver(delay, "NOTICE %s :%s", nick, message);
}

void cleanannounce(void)
{
  channel_announce_t *item;

  for (item = irlist_get_head(&(gnetwork->serverq_channel));
       item;
       item = irlist_delete(&(gnetwork->serverq_channel), item)) {
     mydelete(item->msg);
  }
}

void sendannounce(void)
{
  channel_announce_t *item;

  item = irlist_get_head(&(gnetwork->serverq_channel));
  if (!item)
    return;

  if ( --(item->delay) > 0 )
    return;

  writeserver(WRITESERVER_SLOW, "%s", item->msg);
  mydelete(item->msg);
  irlist_delete(&(gnetwork->serverq_channel), item);
}

void a_fillwith_msg2(userinput * const u, const char *nick, const char *line)
{
  updatecontext();

  u->level = ADMIN_LEVEL_PUBLIC;
  if (gnetwork)
    u->net = gnetwork->net;
  u->chat = NULL;
  u->method = method_msg;
  u->snick = (nick != NULL) ? mystrdup(nick) : NULL;

  a_parse_inputline(u, line);
}

static void admin_line(int fd, const char *line)
{
  userinput *uxdl;

  if (line == NULL)
    return;

  uxdl = mycalloc(sizeof(userinput));

  a_fillwith_msg2(uxdl, NULL, line);
  uxdl->method = method_fd;
  uxdl->fd = fd;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_CONSOLE;
  u_parseit(uxdl);

  mydelete(uxdl);
}

static void admin_run(const char *cmd)
{
  int fd;
  const char *job;
  char *done;

  job = gdata.admin_job_file;
  if (job == NULL)
    return;

  done = mystrsuffix(job, ".done");
  fd = open(done,
            O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Create Admin Job Done File '%s': %s",
             done, strerror(errno));
  } else {
    admin_line(fd, cmd);
    close(fd);
  }
  mydelete(done)
}

void admin_jobs(void)
{
  FILE *fin;
  const char *job;
  char *line;
  char *l;
  char *r;

  job = gdata.admin_job_file;
  if (job == NULL)
    return;

  fin = fopen(job, "r" );
  if (fin == NULL)
    return;

  line = mycalloc(maxtextlength);
  while (!feof(fin)) {
    r = fgets(line, maxtextlength - 1, fin);
    if (r == NULL )
      break;
    l = line + strlen(line) - 1;
    while (( *l == '\r' ) || ( *l == '\n' ))
      *(l--) = 0;
    /* ignore empty lines and comments */
    if ((line[0] == 0) || (line[0] == '#'))
      continue;
    irlist_add_string(&gdata.jobs_delayed, line);
  }
  mydelete(line)
  fclose(fin);
  unlink(job);
}

static int check_for_file_remove(int n)
{
  xdcc *xd;
  userinput *pubplist;
  char *tempstr;

  updatecontext();

  xd = irlist_get_nth(&gdata.xdccs, n-1);
  if (look_for_file_changes(xd) == 0)
    return 0;

  pubplist = mycalloc(sizeof(userinput));
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr, maxtextlength, "REMOVE %d", n);
  u_fillwith_console(pubplist, tempstr);
  u_parseit(pubplist);
  mydelete(pubplist);
  mydelete(tempstr);
  return 1;
}

static int last_look_for_file_remove = -1;

void look_for_file_remove(void)
{
  int i;
  int p;
  int m;

  updatecontext();

  p = irlist_size(&gdata.xdccs);
  m = min2(gdata.monitor_files, p);
  for (i=0; i<m; i++) {
    last_look_for_file_remove ++;
    if (last_look_for_file_remove < 0 || last_look_for_file_remove >= p)
      last_look_for_file_remove = 0;

    if (check_for_file_remove(last_look_for_file_remove + 1)) {
      last_look_for_file_remove --;
      return;
    }
  }
  return;
}

void reset_download_limits(void)
{
  int num;
  int newlimit;
  xdcc *xd;

  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    num++;
    if (xd->dlimit_max == 0)
      continue;

    newlimit = xd->gets + xd->dlimit_max;
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "Resetting download limit of pack %d, used %d",
            num, newlimit - xd->dlimit_used);
    xd->dlimit_used = newlimit;
  }
}

static const char *badcrc = "badcrc";

const char *validate_crc32(xdcc *xd, int quiet)
{
  char *newcrc;
  char *line;
  const char *x;
  char *w;

  if (xd->has_crc32 == 0) {
    if (quiet && (gdata.verbose_crc32 == 0))
      return NULL;
    else
      return "no CRC32 calculated";
  }

  if (verifyshell(&gdata.autocrc_exclude, xd->file)) {
    if (quiet && (gdata.verbose_crc32 == 0))
      return NULL;
    else
      return "skipped CRC32";
  }

  newcrc = mycalloc(10);
  snprintf(newcrc, 10, "%.8lX", xd->crc32);
  line = mycalloc(strlen(xd->file)+1);

  /* ignore path */
  x = getfilename(xd->file);

  strcpy(line, x);
  /* ignore extension */
  w = strrchr(line, '.');
  if (w != NULL)
    *w = 0;

  caps(line);
  if (strstr(line, newcrc) != NULL) {
    if (quiet && (gdata.verbose_crc32 == 0))
      x = NULL;
    else
      x = "CRC32 verified OK";
    /* unlock pack */
    if ((quiet == 2) && (xd->lock != NULL)) {
      if (strcmp(xd->lock, badcrc) == 0) {
        ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "unlock Pack %d, File %s",
                number_of_pack(xd), line);
        mydelete(xd->lock);
        xd->lock = NULL;
      }
    }
  } else {
    x = "CRC32 not found";
    if (fnmatch("*[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]*", line, FNM_CASEFOLD) == 0) {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "crc expected %s, failed %s", newcrc, line);
      x = "CRC32 failed";
      if (quiet == 2) {
        ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "lock Pack %d, File %s",
                number_of_pack(xd), line);
        mydelete(xd->lock);
        xd->lock = mystrdup(badcrc);
      }
    }
  }
  mydelete(line)
  mydelete(newcrc)
  return x;
}


static void crc32_init(void)
{
  gdata.crc32build.crc = ~0;
  gdata.crc32build.crc_total = ~0;
}

void crc32_update(char *buf, unsigned long len)
{
  char *p;
  ir_uint32 crc = gdata.crc32build.crc;
  ir_uint32 crc_total = gdata.crc32build.crc_total;

  for (p = buf; len--; ++p) {
    crc = (crc >> 8) ^ crctable[(crc ^ *p) & 0xff];
    crc_total = (crc >> 8) ^ crctable[(crc_total ^ *p) & 0xff];
  }
  gdata.crc32build.crc = crc;
  gdata.crc32build.crc_total = crc_total;
}

void crc32_final(xdcc *xd)
{
  xd->crc32 = ~gdata.crc32build.crc;
  xd->has_crc32 = 1;
}

static void autoadd_scan(const char *dir, const char *group)
{
  userinput *uxdl;
  char *line;
  int net = 0;

  if (dir == NULL)
    return;

  updatecontext();

  gnetwork = &(gdata.networks[net]);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "autoadd scan %s", dir);
  line = mycalloc(maxtextlength);
  if (group != NULL)
    snprintf(line, maxtextlength, "ADDGROUP %s %s", group, dir);
  else
    snprintf(line, maxtextlength, "ADDNEW %s", dir);

  uxdl = mycalloc(sizeof(userinput));
  a_fillwith_msg2(uxdl, NULL, line);
  uxdl->method = method_out_all;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_AUTO;
  u_parseit(uxdl);
  mydelete(uxdl);
  mydelete(line);
}

void autoadd_all(void)
{
  char *dir;

  updatecontext();

  for (dir = irlist_get_head(&gdata.autoadd_dirs);
       dir;
       dir = irlist_get_next(dir)) {
    autoadd_scan(dir, gdata.autoadd_group);
  }
}

void run_delayed_jobs(void)
{
  userinput *u;
  char *job;

  for (u = irlist_get_head(&gdata.packs_delayed); u; ) {
    if (strcmp(u->cmd, "REMOVE") == 0) {
      a_remove_delayed(u);
      mydelete(u->cmd);
      mydelete(u->arg1);
      mydelete(u->arg2);
      u = irlist_delete(&gdata.packs_delayed, u);
      /* process only one file */
      return;
    }
    if (strcmp(u->cmd, "ADD") == 0) {
      a_add_delayed(u);
      mydelete(u->cmd);
      mydelete(u->arg1);
      mydelete(u->arg2);
      u = irlist_delete(&gdata.packs_delayed, u);
      /* process only one file */
      return;
    }
    /* ignore */
    outerror(OUTERROR_TYPE_WARN, "Unknown cmd %s in packs_delayed", u->cmd);
    u = irlist_delete(&gdata.packs_delayed, u);
  }

  for (job = irlist_get_head(&gdata.jobs_delayed); job; ) {
    admin_run(job);
    job = irlist_delete(&gdata.jobs_delayed, job);
    /* process only one job */
    return;
  }
}

static void admin_msg_line(const char *nick, char *line, int level)
{
  char *part5[6];
  userinput *u;

  updatecontext();
  get_argv(part5, line, 6);
  mydelete(part5[1]);
  mydelete(part5[2]);
  mydelete(part5[3]);
  mydelete(part5[4]);
  u = mycalloc(sizeof(userinput));
  a_fillwith_msg2(u, nick, part5[5]);
  u->hostmask = mystrdup(part5[0] + 1);
  mydelete(part5[0]);
  mydelete(part5[5]);
  u->level = level;
  u_parseit(u);
  mydelete(u);
}

static void reset_ignore(const char *hostmask)
{
  igninfo *ignore;

  /* admin commands shouldn't count against ignore */
  ignore = get_ignore(hostmask);
  ignore->bucket = 0;
}

static group_admin_t *verifypass_group(const char *hostmask, const char *passwd)
{
  group_admin_t *ga;

  if (!hostmask)
    return NULL;

  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_get_next(ga)) {
    if (fnmatch(ga->g_host, hostmask, FNM_CASEFOLD) != 0)
      continue;
    if ( !verifypass2(ga->g_pass, passwd) )
      continue;
    return ga;
  }
  return NULL;
}

static int msg_host_password(const char *nick, const char *hostmask, const char *passwd, char *line)
{
  group_admin_t *ga;

  if ( verifyshell(&gdata.adminhost, hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.adminlevel);
      return 1;
    }
    return -1;
  }
  if ( verifyshell(&gdata.hadminhost, hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.hadminlevel);
      return 1;
    }
    return -1;
  }
  if ((ga = verifypass_group(hostmask, passwd))) {
    reset_ignore(hostmask);
    admin_msg_line(nick, line, ga->g_level);
    return 1;
  }
  return 0;
}

int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line)
{
  int err = 0;

  err = msg_host_password(nick, hostmask, passwd, line);
  if (err == 0) {
    notice(nick, "ADMIN: %s is not allowed to issue admin commands", hostmask);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Hostname (%s on %s)",
            hostmask, gnetwork->name);
  }
  if (err < 0) {
    notice(nick, "ADMIN: Incorrect Password");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Password (%s on %s)",
            hostmask, gnetwork->name);
  }
  return err;
}

int dcc_host_password(dccchat_t *chat, char *passwd)
{
  group_admin_t *ga;

  updatecontext();

  if ( verifyshell(&gdata.adminhost, chat->hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      chat->level = gdata.adminlevel;
      return 1;
    }
    return -1;
  }
  if ( verifyshell(&gdata.hadminhost, chat->hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      chat->level = gdata.hadminlevel;
      return 1;
    }
    return -1;
  }
  if ((ga = verifypass_group(chat->hostmask, passwd))) {
    chat->level = ga->g_level;
    chat->groups = mystrdup(ga->g_groups);
    return 1;
  }
  return 0;
}

static const char *find_groupdesc(const char *group)
{
  xdcc *xd;

  if (group == NULL)
    return "";

  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        {
          if (strcasecmp(xd->group, group) == 0)
            {
              return xd->group_desc;
            }
        }
      xd = irlist_get_next(xd);
    }

  return "";
}

void write_removed_xdcc(xdcc *xd)
{
  char *line;
  int len;
  int fd;

  if (gdata.xdccremovefile == NULL)
    return;

  fd = open(gdata.xdccremovefile,
            O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);

  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Create XDCC Remove File '%s': %s",
             gdata.xdccremovefile, strerror(errno));
    return;
  }

  line = mycalloc(maxtextlength);
  len = snprintf(line, maxtextlength, "\n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_file %s\n", xd->file);
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_desc %s\n", xd->desc);
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_note %s\n", xd->note ? xd->note : "");
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_size %" LLPRINTFMT "d\n", xd->st_size);
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_gets %d\n", xd->gets);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength, "xx_mins \n");
  else
    len = snprintf(line, maxtextlength, "xx_mins %f\n", xd->minspeed);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength, "xx_maxs \n");
  else
    len = snprintf(line, maxtextlength, "xx_maxs %f\n", xd->maxspeed);
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_data %s\n", xd->group ? xd->group : "");
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_trig \n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength, "xx_trno %s\n", find_groupdesc(xd->group));
  write(fd, line, len);
  mydelete(line)

  close(fd);
}

static void u_fillwith_console2(userinput * const u)
{
  u->method = method_console;
  u->snick = NULL;
  u->chat = NULL;
  u->net = 0;
  u->level = ADMIN_LEVEL_CONSOLE;
  u->hostmask = NULL;
  u->cmd = NULL;
  u->arg1 = NULL;
  u->arg2 = NULL;
  u->arg3 = NULL;
  u->arg1e = NULL;
  u->arg2e = NULL;
  u->arg3e = NULL;
}

static void import_pack(const char *xx_file, const char *xx_desc, const char *xx_note,
                        int xx_gets, float xx_mins, float xx_maxs,
                        const char *xx_data, const char *xx_trno)
{
  char *file;
  const char *newfile;
  struct stat st;
  userinput u2;
  xdcc *xd;
  int xfiledescriptor;
  int rc;

  updatecontext();

  bzero((char *)&u2, sizeof(userinput));
  u_fillwith_console2(&u2);
  file = mystrdup(xx_file);
  convert_to_unix_slash(file);

  xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_fstat(&u2, xfiledescriptor, &file, &st))
    return;

  if (gdata.noduplicatefiles) {
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if ((xd->st_dev == st.st_dev) &&
          (xd->st_ino == st.st_ino)) {
        a_respond(&u2, "File '%s' is already added.", xx_file);
        mydelete(file);
        xd->gets += xx_gets;
        return;
      }
    }
  }

  if (gdata.no_duplicate_filenames) {
    newfile = getfilename(file);
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (strcasecmp(getfilename(xd->file), newfile) == 0) {
        a_respond(&u2, "File '%s' is already added.", xx_file);
        mydelete(file);
        xd->gets += xx_gets;
        return;
      }
    }
  }

  if (gdata.disk_quota) {
    off_t usedbytes = st.st_size;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      usedbytes += xd->st_size;
    }
    if (usedbytes >= gdata.disk_quota) {
      a_respond(&u2, "File '%s' not added, you reached your quoata", xx_file);
      mydelete(file);
      xd->gets += xx_gets;
      return;
    }
  }

  xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
  xd->file = file;

  if (xx_desc != NULL)
    xd->desc = mystrdup(xx_desc);

  if (xx_note != NULL)
    xd->desc = mystrdup(xx_note);

  xd->gets = xx_gets;
  xd->minspeed = gdata.transferminspeed;
  if (xx_mins > 0)
    xd->minspeed = xx_mins;

  xd->maxspeed = gdata.transfermaxspeed;
  if (xx_maxs > 0)
    xd->maxspeed = xx_maxs;

  xd->group = NULL;
  xd->group_desc = NULL;

  if (xx_data != NULL) {
    xd->group = mystrdup(xx_data);
    rc = add_default_groupdesc(xx_data);
    set_support_groups();
    if (rc == 1) {
      reorder_new_groupdesc(xx_data, xx_trno);
    }
  }

  xd->st_size  = st.st_size;
  xd->st_dev   = st.st_dev;
  xd->st_ino   = st.st_ino;
  xd->mtime    = st.st_mtime;
}

void import_xdccfile(void)
{
  char *templine;
  char *word;
  char *data;
  char *xx_file = NULL;
  char *xx_desc = NULL;
  char *xx_note = NULL;
  char *xx_data = NULL;
  char *xx_trno = NULL;
  float val;
  int filedescriptor;
  int part;
  int err;
  int xx_gets = 0;
  float xx_mins = 0.0;
  float xx_maxs = 0.0;

  updatecontext();

  if (gdata.import == NULL)
    return;

  filedescriptor = open(gdata.import, O_RDONLY | ADDED_OPEN_FLAGS );
  if (filedescriptor < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access XDCC File '%s': %s", gdata.import, strerror(errno));
    return;
  }

  templine = mycalloc(maxtextlength);
  if (getfline(templine, maxtextlength, filedescriptor, 1) == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Empty XDCC File");
    mydelete(templine);
    return;
  }

  part = 0;
  for (word = strtok(templine, " ");
       word;
       word = strtok(NULL, " ")) {
    part ++;
    switch (part) {
    case 6:
      val = atof(word);
      if (val > gdata.record)
         gdata.record = val;
      break;
    case 7:
      val = atof(word);
      if (val > gdata.sentrecord)
         gdata.sentrecord = val;
      break;
    case 8:
      gdata.totalsent += atoull(word);
      break;
    case 9:
      gdata.totaluptime += atol(word);
      break;
    default:
      break;
    }
  }
  err = 0;
  while (getfline(templine, maxtextlength, filedescriptor, 1) != NULL) {
    if (templine[0] == 0) {
      if (xx_file != NULL) {
        import_pack(xx_file, xx_desc, xx_note, xx_gets, xx_mins, xx_maxs, xx_data, xx_trno);
        mydelete(xx_file);
        mydelete(xx_desc);
        mydelete(xx_note);
        mydelete(xx_data);
        mydelete(xx_trno);
        xx_gets = 0;
        xx_mins = 0.0;
        xx_maxs = 0.0;
      }
      continue;
    }
    word = strtok(templine, " ");
    if (word == NULL) {
      err ++;
      break;
    }
    data = strtok(NULL, "\n");
    if (strcmp(word, "xx_file") == 0) {
      if (data == NULL) {
        err ++;
        break;
      }
      xx_file = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_desc") == 0) {
      if (data != NULL)
        xx_desc = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_note") == 0) {
      if (data != NULL)
        xx_note = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_size") == 0) {
      continue;
    }
    if (strcmp(word, "xx_gets") == 0) {
      if (data != NULL)
        xx_gets = atoi(data);
      continue;
    }
    if (strcmp(word, "xx_mins") == 0) {
      if (data != NULL)
        xx_mins = atof(data);
      continue;
    }
    if (strcmp(word, "xx_maxs") == 0) {
      if (data != NULL)
        xx_maxs = atof(data);
      continue;
    }
    if (strcmp(word, "xx_data") == 0) {
      if (data != NULL)
        xx_data = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_trig") == 0) {
      continue;
    }
    if (strcmp(word, "xx_trno") == 0) {
      if (data != NULL)
        xx_trno = mystrdup(data);
      continue;
    }
    err ++;
    break;
  }
  if (err > 0) {
    outerror(OUTERROR_TYPE_CRASH, "Error in XDCC File: %s", templine);
    mydelete(templine);
    return;
  }
  if (xx_file != NULL) {
    import_pack(xx_file, xx_desc, xx_note, xx_gets, xx_mins, xx_maxs, xx_data, xx_trno);
    mydelete(xx_file);
    mydelete(xx_desc);
    mydelete(xx_note);
    mydelete(xx_data);
    mydelete(xx_trno);
  }
  mydelete(templine);
}

void autotrigger_add(xdcc *xd)
{
  autotrigger_t *at;

  at = irlist_add(&gdata.autotrigger, sizeof(autotrigger_t));
  at->pack = xd;
  at->word = xd->trigger;
}

void autotrigger_rebuild(void)
{
  xdcc *xd;

  irlist_delete_all(&gdata.autotrigger);
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    if (xd->trigger != NULL)
      autotrigger_add(xd);
    xd = irlist_get_next(xd);
  }
}

void start_md5_hash(xdcc *xd, int packnum)
{
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "[MD5 Pack %d]: Calculating", packnum);

  gdata.md5build.file_fd = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (gdata.md5build.file_fd >= 0) {
    gdata.md5build.xpack = xd;
    if (!gdata.nocrc32)
      crc32_init();
    MD5Init(&gdata.md5build.md5sum);
    if (set_socket_nonblocking(gdata.md5build.file_fd, 1) < 0) {
      outerror(OUTERROR_TYPE_WARN, "[MD5 Pack %d]: Couldn't Set Non-Blocking", packnum);
    }
  } else {
    outerror(OUTERROR_TYPE_WARN,
             "[MD5 Pack %d]: Cant Access Offered File '%s': %s",
             packnum, xd->file, strerror(errno));
             gdata.md5build.file_fd = FD_UNUSED;
             check_for_file_remove(packnum);
  }
}

void cancel_md5_hash(xdcc *xd, const char *msg)
{
  if (gdata.md5build.xpack == xd) {
    outerror(OUTERROR_TYPE_WARN, "[MD5 Pack %d]: Canceled (%s)", number_of_pack(xd), msg);

    FD_CLR(gdata.md5build.file_fd, &gdata.readset);
    close(gdata.md5build.file_fd);
    gdata.md5build.file_fd = FD_UNUSED;
    gdata.md5build.xpack = NULL;
  }
  xd->has_md5sum = 0;
  xd->has_crc32 = 0;
  memset(xd->md5sum, 0, sizeof(MD5Digest));

  assert(xd->file_fd == FD_UNUSED);
  assert(xd->file_fd_count == 0);
#ifdef HAVE_MMAP
  assert(!irlist_size(&xd->mmaps));
#endif /* HAVE_MMAP */
}

void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch)
{
  char *line;
  userinput_method_e method;

  method = manplist->method;
  if (ch) {
    if (ch->pgroup) {
      line = mycalloc(maxtextlength);
      snprintf(line, maxtextlength, "XDLGROUP %s", ch->pgroup);
      a_fillwith_msg2(manplist, name, line);
      mydelete(line);
      manplist->method = method;
      return;
    }
  }
  if ((gdata.xdcclist_grouponly) || (method != method_xdl_channel)) {
    a_fillwith_msg2(manplist, name, "XDL");
  } else {
    a_fillwith_msg2(manplist, name, "XDLFULL");
  }
  manplist->method = method;
}

static int file_not_exits(const char *path)
{
  int fd;

  fd = open(path, O_WRONLY | ADDED_OPEN_FLAGS);
  if (fd < 0) {
    if (errno != ENOENT)
      return 1;
    return 0;
  }
  close(fd);
  return 1;
}

int save_unlink(const char *path)
{
  const char *file;
  char *dest;
  size_t len;
  int rc;
  int num;

  if (gdata.trashcan_dir) {
    file = getfilename(path);
    len = strlen(gdata.trashcan_dir) + strlen(file) + 15;
    dest = mycalloc(len);
    snprintf(dest, len, "%s/%s", gdata.trashcan_dir, file);
    num = 0;
    while (file_not_exits(dest)) {
      snprintf(dest, len, "%s/%s.%03d", gdata.trashcan_dir, file, ++num);
      if (num >= 200) {
         outerror(OUTERROR_TYPE_WARN_LOUD,
                  "Cant move file '%s' to '%s': %s",
                  path, dest, strerror(errno));
         mydelete(dest);
         return -1;
      }
    }
    rc = rename(path, dest);
    if (rc != 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Cant move file '%s' to '%s': %s",
               path, dest, strerror(errno));
      mydelete(dest);
      return -1;
    }
    mydelete(dest);
    return 0;
  }
  return unlink(path);
}

void rename_with_backup(const char *filename, const char *backup, const char *temp, const char *msg)
{
  int callval;

  /* remove old bkup */
  callval = unlink(backup);
  if ((callval < 0) && (errno != ENOENT)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Remove Old %s '%s': %s",
             msg, backup, strerror(errno));
    /* ignore, continue */
  }

  /* backup old -> bkup */
  callval = link(filename, backup);
  if ((callval < 0) && (errno != ENOENT)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Backup Old %s '%s' -> '%s': %s",
             msg, filename, backup, strerror(errno));
    /* ignore, continue */
  }

  /* rename new -> current */
  callval = rename(temp, filename);
  if (callval < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Save New %s '%s': %s",
             msg, filename, strerror(errno));
    /* ignore, continue */
  }
}

static size_t write_string(int fd, const char *line)
{
  size_t len;

  len = strlen(line);
  return write(fd, line, len);
}

static size_t write_asc_int(int fd, int spaces, const char *tag, int val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlengthshort);
  len = snprintf(tempstr, maxtextlengthshort, "%*s<%s>%d</%s>\n", spaces, "", tag, val, tag);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static size_t write_asc_int64(int fd, int spaces, const char *tag, ir_int64 val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength, "%*s<%s>%" LLPRINTFMT "d</%s>\n", spaces, "", tag, val, tag);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static size_t write_asc_hex(int fd, int spaces, const char *tag, unsigned long val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlengthshort);
  len = snprintf(tempstr, maxtextlengthshort, "%*s<%s>%.8lX</%s>\n", spaces, "", tag, val, tag);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static size_t write_asc_text(int fd, int spaces, const char *tag, const char *val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength, "%*s<%s><![CDATA[%s]]></%s>\n", spaces, "", tag, val, tag);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static size_t write_asc_plain(int fd, int spaces, const char *tag, const char *val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength, "%*s<%s>%s</%s>\n", spaces, "", tag, val, tag);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static void xdcc_save_xml(void)
{
  channel_t *ch;
  gnetwork_t *backup;
  char *filename_tmp;
  char *filename_bak;
  char *tempstr;
  xdcc *xd;
  off_t toffered;
  int fd;
  int num;
  int groups;
  int ss;

  updatecontext();

  if (!gdata.xdccxmlfile)
    return;

  filename_tmp = mystrsuffix(gdata.xdccxmlfile, ".tmp");
  filename_bak = mystrsuffix(gdata.xdccxmlfile, "~");

  fd = open(filename_tmp,
            O_WRONLY | O_CREAT | O_TRUNC | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Create XDCC List File '%s': %s",
             filename_tmp, strerror(errno));
    mydelete(filename_tmp);
    mydelete(filename_bak);
    return;
  }

  write_string(fd, "<?xml version=\"1.0\" encoding=\"");
  if (gdata.charset != NULL)
    write_string(fd, gdata.charset);
  else
    write_string(fd, "UTF-8");
  write_string(fd, "\"?>\n");
  write_string(fd, "<packlist>\n\n");

  groups = 0;
  toffered = 0;
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    num++;
    if (hide_pack(xd))
      continue;

    toffered += xd->st_size;
    write_string(fd, "<pack>\n");
    write_asc_int(fd, 2, "packnr", num);
    tempstr = mystrdup(xd->desc);
    removenonprintablectrl(tempstr);
    write_asc_text(fd, 2, "packname", tempstr);
    mydelete(tempstr);
    tempstr = sizestr(0, xd->st_size);
    write_asc_text(fd, 2, "packsize", tempstr);
    mydelete(tempstr);
    write_asc_int64(fd, 2, "packbytes", xd->st_size);
    write_asc_int(fd, 2, "packgets", xd->gets);
    write_asc_int(fd, 2, "adddate", xd->xtime ? xd->xtime : xd->mtime);
    if (xd->group != NULL) {
      groups ++;
      write_asc_text(fd, 2, "groupname", xd->group);
    }
    if (xd->has_md5sum) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
      write_asc_text(fd, 2, "md5sum", tempstr);
      mydelete(tempstr);
    }
    if (xd->has_crc32) {
      write_asc_hex(fd, 2, "crc32", xd->crc32);
    }
    write_string(fd, "</pack>\n\n");
  }

  if (groups > 0) {
    write_string(fd, "<grouplist>\n");
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (xd->group == NULL)
        continue;

      if (xd->group_desc == NULL)
        continue;

      write_string(fd, "  <group>\n");
      write_asc_text(fd, 4, "groupname", xd->group);
      tempstr = mystrdup(xd->group_desc);
      removenonprintablectrl(tempstr);
      write_asc_text(fd, 4, "groupdesc", xd->group_desc);
      mydelete(tempstr);
      write_string(fd, "  </group>\n");
    }
    write_string(fd, "</grouplist>\n\n");
  }

  write_string(fd, "<sysinfo>\n");
  write_string(fd, "  <slots>\n");
  write_asc_int(fd, 4, "slotsfree", gdata.slotsmax - irlist_size(&gdata.trans));
  write_asc_int(fd, 4, "slotsmax", gdata.slotsmax);
  write_string(fd, "  </slots>\n");

  write_string(fd, "  <bandwith>\n");
  tempstr = get_current_bandwidth();
  write_asc_plain(fd, 4, "banduse", tempstr);
  mydelete(tempstr);
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%u.0KB/s", gdata.maxb / 4);
  write_asc_plain(fd, 4, "bandmax", tempstr);
  mydelete(tempstr);
  write_string(fd, "  </bandwith>\n");

  write_string(fd, "  <quota>\n");
  write_asc_int(fd, 4, "packsum", num);
  tempstr = sizestr(0, toffered);
  write_asc_plain(fd, 4, "diskspace", tempstr);
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  write_asc_plain(fd, 4, "transfereddaily", tempstr);
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  write_asc_plain(fd, 4, "transferedweekly", tempstr);
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  write_asc_plain(fd, 4, "transferedmonthly", tempstr);
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.totalsent);
  write_asc_plain(fd, 4, "transferedtotal", tempstr);
  write_asc_int64(fd, 4, "transferedtotalbytes", gdata.totalsent);
  mydelete(tempstr);
  write_string(fd, "  </quota>\n");

  write_string(fd, "  <limits>\n");
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fKB/s", gdata.transferminspeed);
  write_asc_plain(fd, 4, "minspeed", tempstr);
  mydelete(tempstr);
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fKB/s", gdata.transfermaxspeed);
  write_asc_plain(fd, 4, "maxspeed", tempstr);
  mydelete(tempstr);
  write_string(fd, "  </limits>\n");

  write_string(fd, "  <networks>\n");
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    write_asc_plain(fd, 4, "networkname", gdata.networks[ss].name);
    gnetwork = &(gdata.networks[ss]);
    write_asc_plain(fd, 4, "confignick", get_config_nick());
    write_asc_plain(fd, 4, "currentnick", get_user_nick());
    write_asc_plain(fd, 4, "servername", gdata.networks[ss].curserver.hostname);
    if (gdata.networks[ss].curserveractualname != NULL)
      write_asc_plain(fd, 4, "currentservername", gdata.networks[ss].curserveractualname);
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_delete(&(gnetwork->channels), ch)) {
      if ((ch->flags & CHAN_ONCHAN) == 0)
        continue;
      write_asc_plain(fd, 4, "channel", ch->name);
    }
  }
  gnetwork = backup;
  write_string(fd, "  </networks>\n");

  write_string(fd, "  <stats>\n");
  write_asc_plain(fd, 4, "version", "iroffer-dinoex " VERSIONLONG );
  tempstr = mycalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 1, gdata.startuptime, maxtextlengthshort);
  write_asc_plain(fd, 4, "uptime", tempstr);
  mydelete(tempstr);
  tempstr = mycalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
  write_asc_plain(fd, 4, "totaluptime", tempstr);
  mydelete(tempstr);
  write_asc_int(fd, 4, "lastupdate", gdata.curtime);
  write_string(fd, "  </stats>\n");
  write_string(fd, "</sysinfo>\n\n");

  write_string(fd, "</packlist>\n");
  close(fd);

  rename_with_backup(gdata.xdccxmlfile, filename_bak, filename_tmp, "XDCC XML");

  mydelete(filename_tmp);
  mydelete(filename_bak);
  return;
}

void write_files(void)
{
  gdata.last_update = gdata.curtime;
  write_statefile();
  xdccsavetext();
  xdcc_save_xml();
}

void start_qupload(void)
{
  gnetwork_t *backup;
  qupload_t *qu;

  updatecontext();

  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (qu->q_state == 2)
      return; /* busy */
  }
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (qu->q_state != 1)
      continue;

    backup = gnetwork;
    gnetwork = &(gdata.networks[qu->q_net]);
    privmsg_fast(qu->q_nick, "XDCC GET %s", qu->q_pack);
    gnetwork = backup;
    qu->q_state = 2;
    return;
  }
}

int close_qupload(int net, const char *nick)
{
  qupload_t *qu;

  updatecontext();

  if (nick == NULL)
    return 0;

  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (qu->q_net != net)
      continue;

    if (qu->q_state != 2)
      continue;

    if (strcasecmp(qu->q_nick, nick) != 0)
      continue;

    mydelete(qu->q_host);
    mydelete(qu->q_nick);
    mydelete(qu->q_pack);
    irlist_delete(&gdata.quploadhost, qu);
    start_qupload();
    return 1;
  }
  return 0;
}

void lag_message(void)
{
  userinput *u;

  gnetwork->lag = gdata.curtimems - gnetwork->lastping;
  gnetwork->lastping = 0;
    u = mycalloc(sizeof(userinput));
    u->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
    u->net = gnetwork->net;
    a_respond(u, "LAG on %s is %ld ms", gnetwork->name, gnetwork->lag);
    mydelete(u);
}

static char *r_local_vhost;
static char *r_config_nick;

static void irlist_move(irlist_t *dest, irlist_t *src)
{
  *dest = *src;
  src->size = 0;
  src->head = NULL;
  src->tail = NULL;
}

void a_rehash_prepare(void)
{
  int ss;

  gdata.r_networks_online = gdata.networks_online;
  gdata.r_xdcclist_grouponly = gdata.xdcclist_grouponly;

  gdata.r_pidfile = NULL;
  if (gdata.pidfile)
    gdata.r_pidfile = mystrdup(gdata.pidfile);
  r_config_nick = NULL;
  if (gdata.config_nick)
    r_config_nick = mystrdup(gdata.config_nick);
  r_local_vhost = NULL;
  if (gdata.local_vhost)
    r_local_vhost = mystrdup(gdata.local_vhost);

  for (ss=0; ss<gdata.networks_online; ss++) {
    gdata.networks[ss].r_needtojump = 0;
    gdata.networks[ss].r_connectionmethod = gdata.networks[ss].connectionmethod.how;
    gdata.networks[ss].r_config_nick = NULL;
    gdata.networks[ss].r_ourip = gdata.getipfromserver || gdata.getipfromupnp ? gdata.networks[ss].ourip : 0;
    if (gdata.networks[ss].config_nick)
      gdata.networks[ss].r_config_nick = mystrdup(gdata.networks[ss].config_nick);
    gdata.networks[ss].r_local_vhost = NULL;
    if (gdata.networks[ss].local_vhost)
      gdata.networks[ss].r_local_vhost = mystrdup(gdata.networks[ss].local_vhost);
    irlist_move(&(gdata.networks[ss].r_channels), &(gdata.networks[ss].channels));
  }
}

void a_rehash_needtojump(const userinput *u)
{
  char *new_vhost;
  char *old_vhost;
  gnetwork_t *backup;
  channel_t *ch;
  int ss;

  updatecontext();


  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    /* keep dynamic IP */
    if (gdata.getipfromserver || gdata.getipfromupnp) {
      gnetwork->ourip = gnetwork->r_ourip;
      gnetwork->usenatip = 1;
    }
    gnetwork->r_ourip = 0;
    if (gnetwork->r_connectionmethod != gnetwork->connectionmethod.how) {
      a_respond(u, "connectionmethod changed, reconnecting");
      gnetwork->r_needtojump = 1;
    }
    gnetwork->r_connectionmethod = 0;
    new_vhost = get_local_vhost();
    old_vhost = (gnetwork->r_local_vhost) ? gnetwork->r_local_vhost : r_local_vhost;
    if (strcmp_null(new_vhost, old_vhost) != 0) {
      a_respond(u, "vhost changed, reconnecting");
      gnetwork->r_needtojump = 1;
    }
  }
  gnetwork = backup;

  /* dopped networks */
  if (gdata.networks_online < gdata.r_networks_online) {
    a_respond(u, "network dropped, reconnecting");
    backup = gnetwork;
    for (ss=gdata.networks_online; ss<gdata.r_networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      quit_server();
      for (ch = irlist_get_head(&(gnetwork->channels));
           ch;
           ch = irlist_delete(&(gnetwork->channels), ch)) {
        clearmemberlist(ch);
        free_channel_data(ch);
      }
      mydelete(gnetwork->user_nick);
      mydelete(gnetwork->caps_nick);
      mydelete(gnetwork->name);
    }
    gnetwork = backup;
  }
}

void a_rehash_channels(void)
{
  gnetwork_t *backup;
  channel_t *rch;
  channel_t *ch;
  int ss;

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);

    /* part deleted channels, add common channels */
    rch = irlist_get_head(&(gnetwork->r_channels));
    while(rch) {
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if (strcmp(rch->name, ch->name) == 0) {
          break;
        }
        ch = irlist_get_next(ch);
      }

      if (!ch) {
        if (!gnetwork->r_needtojump && (rch->flags & CHAN_ONCHAN)) {
          writeserver(WRITESERVER_NORMAL, "PART %s", rch->name);
        }
        if (gdata.debug > 2) {
           ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
                   "1 = %s parted", rch->name);
        }
        clearmemberlist(rch);
        free_channel_data(rch);
        rch = irlist_delete(&(gnetwork->r_channels), rch);
      } else {
        irlist_move(&(ch->members), &(rch->members));
        ch->flags |= rch->flags & CHAN_ONCHAN;
        ch->lastjoin = rch->lastjoin;
        ch->nextann = rch->nextann;
        if (gdata.debug > 2) {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
                  "2 = %s common", ch->name);
        }
        rch = irlist_get_next(rch);
      }
    }

    /* join/add new channels */
    ch = irlist_get_head(&(gnetwork->channels));
    while(ch) {
      rch = irlist_get_head(&(gnetwork->r_channels));
      while(rch) {
        if (strcmp(rch->name, ch->name) == 0) {
          break;
        }
        rch = irlist_get_next(rch);
      }

      if (!rch) {
        ch->flags &= ~CHAN_ONCHAN;
        if (!gnetwork->r_needtojump) {
          joinchannel(ch);
        }
        if (gdata.debug > 2) {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
                  "3 = %s new", ch->name);
        }
      }
      ch = irlist_get_next(ch);
    }

    for (rch = irlist_get_head(&(gnetwork->r_channels));
         rch;
         rch = irlist_delete(&(gnetwork->r_channels), rch)) {
       free_channel_data(rch);
    }
  } /* networks */
  gnetwork = backup;
}

void a_rehash_jump(void)
{
  gnetwork_t *backup;
  int ss;

  updatecontext();

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    if (gnetwork->r_needtojump == 0)
      continue;

    gnetwork->serverconnectbackoff = 0;
    switchserver(-1);
    /* switchserver takes care of joining channels */
  }
  gnetwork = backup;
}

void a_rehash_cleanup(const userinput *u)
{
  gnetwork_t *backup;
  char *new_nick;
  char *old_nick;
  int ss;

  updatecontext();

  mydelete(r_local_vhost);
  mydelete(gdata.r_pidfile);

  if (!gdata.config_nick) {
    a_respond(u, "user_nick missing! keeping old nick!");
    gdata.config_nick = r_config_nick;
    r_config_nick = NULL;
    for (ss=0; ss<gdata.networks_online; ss++) {
      mydelete(gnetwork->config_nick);
      gnetwork->config_nick = gnetwork->r_config_nick;
      mydelete(gnetwork->r_local_vhost);
    }
  } else {
    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      old_nick = (gnetwork->r_config_nick) ? gnetwork->r_config_nick : r_config_nick;
      new_nick = get_config_nick();
      if (strcmp(new_nick, old_nick)) {
        a_respond(u, "user_nick changed, renaming nick to %s", new_nick);
        writeserver(WRITESERVER_NOW, "NICK %s", new_nick);
      }
      mydelete(gnetwork->r_config_nick);
      mydelete(gnetwork->r_local_vhost);
    }
    gnetwork = backup;
    mydelete(r_config_nick);
  }
  mydelete(r_local_vhost);
  if (gdata.xdcclist_grouponly != gdata.r_xdcclist_grouponly)
    write_files();
}

void a_read_config_files(const userinput *u)
{
  struct stat st;
  char *templine;
  FILE *fin;
  char *r;
  char *l;
  int h;

  updatecontext();

  templine = mycalloc(maxtextlength);
  for (h=0; h<MAXCONFIG && gdata.configfile[h]; h++) {
    if (u == NULL)
      printf("** Loading %s ... \n", gdata.configfile[h]);
    else
      a_respond(u, "Reloading %s ...", gdata.configfile[h]);

    fin = fopen(gdata.configfile[h], "r");
    if (fin == NULL) {
      if (u == NULL) {
          outerror(OUTERROR_TYPE_CRASH, "Cant Access Config File '%s': %s",
                   gdata.configfile[h], strerror(errno));
      } else {
          a_respond(u, "Cant Access File '%s', Aborting rehash: %s",
                    gdata.configfile[h], strerror(errno));
      }
      continue;
    }

    if (stat(gdata.configfile[h], &st) < 0) {
      outerror(OUTERROR_TYPE_CRASH,
               "Unable to stat file '%s': %s",
               gdata.configfile[h], strerror(errno));
    }
    gdata.configtime[h] = st.st_mtime;
    current_config = gdata.configfile[h];
    current_line = 0;
    gdata.bracket = 0;
    while (!feof(fin)) {
      r = fgets(templine, maxtextlength, fin);
      if (r == NULL )
        break;
      l = templine + strlen(templine) - 1;
      while (( *l == '\r' ) || ( *l == '\n' ))
        *(l--) = 0;
      current_line ++;
      if ((templine[0] != '#') && templine[0]) {
        getconfig_set(templine);
      }
    }
    fclose(fin);
  }
  gdata.networks_online ++;
  mydelete(templine);
}

/* End of File */
