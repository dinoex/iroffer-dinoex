/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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
#include "dinoex_misc.h"
#include "dinoex_jobs.h"

#include <ctype.h>

extern const ir_uint32 crctable[256];

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

void vprivmsg_chan(int delay, const char *name, const char *fish, const char *format, va_list ap)
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
writeserver_channel (int delay, const char *format, ... )
{
  va_list args;
  va_start(args, format);
  vwriteserver_channel(delay, format, args);
  va_end(args);
}

void vwriteserver_channel(int delay, const char *format, va_list ap)
{
  char *msg;
  channel_announce_t *item;
  int len;

  updatecontext();

  msg = mycalloc(maxtextlength+1);

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

void writeserver_privmsg(int delay, const char *nick, const char *message, int len)
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

void writeserver_notice(int delay, const char *nick, const char *message, int len)
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

static void admin_line(int fd, const char *line) {
  userinput *uxdl;
  char *full;

  if (line == NULL)
    return;

  uxdl = mycalloc(sizeof(userinput));
  full = mycalloc(maxtextlength);

  snprintf(full, maxtextlength -1, "A A A A A %s", line);
  u_fillwith_msg(uxdl, NULL, full);
  uxdl->method = method_fd;
  uxdl->fd = fd;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_CONSOLE;
  u_parseit(uxdl);

  mydelete(uxdl);
  mydelete(full);
}

static void admin_run(const char *cmd) {
  int fd;
  const char *job;
  char *done;

  job = gdata.admin_job_file;
  if (job == NULL)
    return;

  done = mycalloc(strlen(job)+6);
  strcpy(done, job);
  strcat(done, ".done");
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

void admin_jobs(void) {
  FILE *fin;
  const char *job;
  char *line;
  char *l;
  char *r;
  char *new;

  job = gdata.admin_job_file;
  if (job == NULL)
    return;

  fin = fopen(job, "ra" );
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
    new = irlist_add(&gdata.jobs_delayed, strlen(line) + 1);
    strcpy(new, line);
  }
  mydelete(line)
  fclose(fin);
  unlink(job);
}

int check_for_file_remove(int n)
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
  snprintf(tempstr, maxtextlength-1, "remove %d", n);
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

    if (check_for_file_remove(last_look_for_file_remove + 1))
      return;
  }
  return;
}

void reset_download_limits(void)
{
  int num;
  int new;
  xdcc *xd;

  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    num++;
    if (xd->dlimit_max == 0)
      continue;

    new = xd->gets + xd->dlimit_max;
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "Resetting download limit of pack %d, used %d",
            num, new - xd->dlimit_used);
    xd->dlimit_used = new;
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
  x = get_basename(xd->file);

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
        xd->lock = mystrdup(badcrc);
      }
    }
  }
  mydelete(line)
  mydelete(newcrc)
  return x;
}


void crc32_init(void)
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

void autoadd_scan(const char *dir, const char *group)
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
    snprintf(line, maxtextlength -1, "A A A A A ADDGROUP %s %s", group, dir);
  else
    snprintf(line, maxtextlength -1, "A A A A A ADDNEW %s", dir);

  uxdl = mycalloc(sizeof(userinput));
  u_fillwith_msg(uxdl, NULL, line);
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

static void admin_msg_line(const char *nick, char *line, int line_len, int level)
{
  userinput ui;

  if (line[line_len-1] == '\n') {
    line[line_len-1] = '\0';
    line_len--;
  }
  u_fillwith_msg(&ui, nick, line);
  ui.net = gnetwork->net;
  ui.level = level;
  u_parseit(&ui);
}

static int msg_host_password(const char *nick, const char *hostmask, const char *passwd, char *line, int line_len)
{
  group_admin_t *ga;
  char *cmd;

  if ( verifyshell(&gdata.adminhost, hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      admin_msg_line(nick, line, line_len, gdata.adminlevel);
      return 1;
    }
    return -1;
  }
  if ( verifyshell(&gdata.hadminhost, hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      admin_msg_line(nick, line, line_len, gdata.hadminlevel);
      return 1;
    }
    return -1;
  }
  cmd = getpart(line, 6);
  if (strcasecmp(cmd, "CHATME") == 0) {
    if ((ga = verifypass_group(hostmask, passwd))) {
       admin_msg_line(nick, line, line_len, ga->g_level);
       mydelete(cmd);
       return 1;
    }
  }
  return 0;
}

int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line, int line_len)
{
  int err = 0;

  err = msg_host_password(nick, hostmask, passwd, line, line_len);
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

static const char *find_groupdesc(const char *group)
{
  xdcc *xd;
  int k;

  if (group == NULL)
    return "";

  k = 0;
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
  len = snprintf(line, maxtextlength -1, "\n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_file %s\n", xd->file);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_desc %s\n", xd->desc);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_note %s\n", xd->note ? xd->note : "");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_size %" LLPRINTFMT "u\n", xd->st_size);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_gets %d\n", xd->gets);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength -1, "xx_mins \n");
  else
    len = snprintf(line, maxtextlength -1, "xx_mins %f\n", xd->minspeed);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength -1, "xx_maxs \n");
  else
    len = snprintf(line, maxtextlength -1, "xx_maxs %f\n", xd->maxspeed);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_data %s\n", xd->group ? xd->group : "");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_trig \n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_trno %s\n", find_groupdesc(xd->group));
  write(fd, line, len);
  mydelete(line)

  close(fd);
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
      snprintf(line, maxtextlength -1, "A A A A A xdlgroup %s", ch->pgroup);
      u_fillwith_msg(manplist, name, line);
      mydelete(line);
      manplist->method = method;
      return;
    }
  }
  if ((gdata.xdcclist_grouponly) || (method != method_xdl_channel)) {
    u_fillwith_msg(manplist, name, "A A A A A xdl");
  } else {
    u_fillwith_msg(manplist, name, "A A A A A xdlfull");
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
    snprintf(dest, len - 1, "%s/%s", gdata.trashcan_dir, file);
    num = 0;
    while (file_not_exits(dest)) {
      snprintf(dest, len - 1, "%s/%s.%03d", gdata.trashcan_dir, file, ++num);
      if (num >= 200) {
         outerror(OUTERROR_TYPE_WARN_LOUD,
                  "Cant move file '%s' to '%s': %s",
                  path, dest, strerror(errno));
         mydelete(dest);
         return unlink(path);
      }
    }
    rc = rename(path, dest);
    if (rc != 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Cant move file '%s' to '%s': %s",
               path, dest, strerror(errno));
    }
    mydelete(dest);
    if (rc == 0)
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

static size_t write_asc_int(int fd, int val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlengthshort + 1);
  len = snprintf(tempstr, maxtextlengthshort, "%d", val);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static size_t write_asc_hex(int fd, long val)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlengthshort + 1);
  len = snprintf(tempstr, maxtextlengthshort, "%.8lX", val);
  len = write(fd, tempstr, len);
  mydelete(tempstr);
  return len;
}

static void xdcc_save_xml(void)
{
  char *filename_tmp;
  char *filename_bak;
  char *tempstr;
  xdcc *xd;
  off_t toffered;
  int fd;
  int num;
  int groups;

  updatecontext();

  if (!gdata.xdccxmlfile)
    return;

  filename_tmp = mycalloc(strlen(gdata.xdccxmlfile)+5);
  sprintf(filename_tmp, "%s.tmp", gdata.xdccxmlfile);
  filename_bak = mycalloc(strlen(gdata.xdccxmlfile)+2);
  sprintf(filename_bak, "%s~", gdata.xdccxmlfile);

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

  write_string(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
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
    write_string(fd, "  <packnr>");
    write_asc_int(fd, num);
    write_string(fd, "</packnr>\n");
    write_string(fd, "  <packname><![CDATA[");
    tempstr = mystrdup(xd->desc);
    removenonprintablectrl(tempstr);
    write_string(fd, tempstr);
    mydelete(tempstr);
    write_string(fd, "]]></packname>\n");
    write_string(fd, "  <packsize><![CDATA[");
    tempstr = sizestr(0, xd->st_size);
    write_string(fd, tempstr);
    mydelete(tempstr);
    write_string(fd, "]]></packsize>\n");
    write_string(fd, "  <packgets>");
    write_asc_int(fd, xd->gets);
    write_string(fd, "</packgets>\n");
    if (xd->group != NULL) {
      groups ++;
      write_string(fd, "  <groupname><![CDATA[");
      write_string(fd, xd->group);
      write_string(fd, "]]></groupname>\n");
    }
    if (xd->has_md5sum) {
      size_t len;

      write_string(fd, "  <md5sum>");
      tempstr = mycalloc(maxtextlengthshort + 1);
      len = snprintf(tempstr, maxtextlengthshort, MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
      write(fd, tempstr, len);
      mydelete(tempstr);
      write_string(fd, "</md5sum>\n");
    }
    if (xd->has_crc32) {
      write_string(fd, "  <crc32>");
      write_asc_hex(fd, xd->crc32);
      write_string(fd, "</crc32>\n");
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
      write_string(fd, "    <groupname><![CDATA[");
      write_string(fd, xd->group);
      write_string(fd, "]]></groupname>\n");
      write_string(fd, "    <groupdesc><![CDATA[");
      tempstr = mystrdup(xd->group_desc);
      removenonprintablectrl(tempstr);
      write_string(fd, tempstr);
      mydelete(tempstr);
      write_string(fd, "]]></groupdesc>\n");
      write_string(fd, "  </group>\n");
    }
    write_string(fd, "</grouplist>\n\n");
  }

  write_string(fd, "<sysinfo>\n");
  write_string(fd, "  <slots>\n");
  write_string(fd, "    <slotsfree>");
  write_asc_int(fd, gdata.slotsmax - irlist_size(&gdata.trans));
  write_string(fd, "</slotsfree>\n");
  write_string(fd, "    <slotsmax>");
  write_asc_int(fd, gdata.slotsmax);
  write_string(fd, "</slotsmax>\n");
  write_string(fd, "  </slots>\n");

  write_string(fd, "  <bandwith>\n");
  write_string(fd, "    <banduse>");
  tempstr = get_current_bandwidth();
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</banduse>\n");
  write_string(fd, "    <bandmax>");
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort - 1, "%i.0KB/s", gdata.maxb / 4);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</bandmax>\n");
  write_string(fd, "  </bandwith>\n");

  write_string(fd, "  <quota>\n");
  write_string(fd, "    <packsum>");
  write_asc_int(fd, num);
  write_string(fd, "</packsum>\n");
  write_string(fd, "    <diskspace>");
  tempstr = sizestr(0, toffered);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</diskspace>\n");
  write_string(fd, "    <transfereddaily>");
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</transfereddaily>\n");
  write_string(fd, "    <transferedweekly>");
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</transferedweekly>\n");
  write_string(fd, "    <transferedmonthly>");
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</transferedmonthly>\n");
  write_string(fd, "    <transferedtotal>");
  tempstr = sizestr(0, gdata.totalsent);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</transferedtotal>\n");
  write_string(fd, "  </quota>\n");

  write_string(fd, "  <limits>\n");
  write_string(fd, "    <minspeed>");
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.transferminspeed);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</minspeed>\n");
  write_string(fd, "    <maxspeed>");
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.transfermaxspeed);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</maxspeed>\n");
  write_string(fd, "  </limits>\n");

  write_string(fd, "  <stats>\n");
  write_string(fd, "    <version>");
  write_string(fd, "iroffer-dinoex " VERSIONLONG );
  write_string(fd, "</version>\n");
  write_string(fd, "    <uptime>");
  tempstr = mycalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 1, gdata.startuptime, maxtextlengthshort);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</uptime>\n");
  write_string(fd, "    <totaluptime>");
  tempstr = mycalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
  write_string(fd, tempstr);
  mydelete(tempstr);
  write_string(fd, "</totaluptime>\n");
  write_string(fd, "    <lastupdate>");
  write_asc_int(fd, gdata.curtime);
  write_string(fd, "</lastupdate>\n");
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

static char *r_local_vhost;
static char *r_config_nick;

void a_rehash_prepare(void)
{
  int ss;

  gdata.r_networks_online = gdata.networks_online;

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
    gdata.networks[ss].r_ourip = gdata.getipfromserver ? gdata.networks[ss].ourip : 0;
    if (gdata.networks[ss].config_nick)
      gdata.networks[ss].r_config_nick = mystrdup(gdata.networks[ss].config_nick);
    gdata.networks[ss].r_local_vhost = NULL;
    if (gdata.networks[ss].local_vhost)
      gdata.networks[ss].r_local_vhost = mystrdup(gdata.networks[ss].local_vhost);
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
    if (gdata.getipfromserver) {
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
}

/* End of File */
