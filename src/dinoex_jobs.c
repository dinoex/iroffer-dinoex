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
#include "dinoex_kqueue.h"
#include "dinoex_admin.h"
#include "dinoex_irc.h"
#include "dinoex_config.h"
#include "dinoex_misc.h"
#include "dinoex_curl.h"
#include "dinoex_jobs.h"
#include "crc32.h"

#include <ctype.h>

typedef struct {
  const char *filename;
  char buffer[MAX_XML_CHUNK];
  size_t len;
  int fd;
  int dummy;
} xml_buffer_t;

typedef struct {
  size_t offset;
  size_t len;
  unsigned char magic[8];
} magic_crc_t;

#ifndef WITHOUT_BLOWFISH

#include "blowfish.h"

static const unsigned char FISH64[] = "./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"; /* NOTRANSLATE */

static unsigned char fish64decode[ 256 ];

void init_fish64decode( void )
{
  unsigned int i;

  memset(fish64decode, 0, sizeof(fish64decode));
  for ( i = 0; i < 64; ++i) {
    fish64decode[ FISH64[ i ] ] = (unsigned char)i;
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

static char *encrypt_fish( const char *str, size_t len, const char *key)
{
  BLOWFISH_CTX ctx;
  char *dest;
  char *msg;
  size_t max;
  unsigned long left, right;
  unsigned int i;

  if (key == NULL)
    return NULL;

  if (key[0] == 0)
    return NULL;

  updatecontext();

  max = ( len + 3 ) / 4; /* immer 32bit */
  max *= 12; /* base64 */
  msg = mymalloc(max+1);

  dest = msg;
  Blowfish_Init(&ctx, (const unsigned char*)key, strlen(key));
  while (*str) {
    left = bytes_to_long(&str);
    right = bytes_to_long(&str);
    Blowfish_Encrypt(&ctx, &left, &right);
    for (i = 0; i < 6; ++i) {
      *(dest++) = FISH64[right & 0x3f];
      right = (right >> 6);
    }
    for (i = 0; i < 6; ++i) {
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
  unsigned int i;

  result = 0L;
  for (i = 0; i < 6; ++i) {
    ch = (unsigned char)(*(*str)++);
    result |= fish64decode[ ch ] << (i * 6);
  }
  return result;
}


static char *decrypt_fish( const char *str, size_t len, const char *key)
{
  BLOWFISH_CTX ctx;
  char *dest;
  char *work;
  char *msg;
  size_t max;
  unsigned long left, right;
  unsigned int i;

  if (key == NULL)
    return NULL;

  if (key[0] == 0)
    return NULL;

  updatecontext();

  max = ( len + 11 / 12 );
  max *= 4;
  msg = mymalloc(max+1);

  dest = msg;
  Blowfish_Init(&ctx, (const unsigned char*)key, strlen(key));
  while (*str) {
    right = base64_to_long(&str);
    left = base64_to_long(&str);
    Blowfish_Decrypt(&ctx, &left, &right);
    dest += 4;
    work = dest;
    for (i = 0; i < 4; ++i) {
      *(--work) = left & 0xff;
      left >>= 8;
    }
    dest += 4;
    work = dest;
    for (i = 0; i < 4; ++i) {
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
  snprintf(end, len, "%s :%s", channel, newstr); /* NOTRANSLATE */
  mydelete(newstr);
  if (gdata.debug > 13) {
    ioutput(OUT_S|OUT_L, COLOR_MAGENTA, ">FISH>: %s", newline);
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

  if (strcmp(str, ":+OK") != 0) /* NOTRANSLATE */
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
vwriteserver_channel(channel_t * const ch, const char *format, va_list ap)
{
  char *msg;
  channel_announce_t *item;
  ssize_t len;

  updatecontext();

  msg = mymalloc(maxtextlength);

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

  if (gdata.debug > 13) {
    ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<QUES<: %s %s :%s",
            ch->name, "PRIVMSG", msg); /* NOTRANSLATE */
  }

  if (len > gnetwork->server_send_max) {
    outerror(OUTERROR_TYPE_WARN, "Message Truncated!");
    msg[gnetwork->server_send_max] = '\0';
    len = gnetwork->server_send_max;
  }

  if (irlist_size(&(gnetwork->serverq_channel)) < MAXSENDQ) {
    item = irlist_add(&(gnetwork->serverq_channel), sizeof(channel_announce_t));
    if (ch->nextmsg < gdata.curtime) {
      /* fast announce */
      ch->nextmsg = gdata.curtime;
    }
    item->c_time = ch->nextmsg;
    ch->nextmsg += ch->delay;
    item->channel = mystrdup(ch->name);
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
writeserver_channel(channel_t * const ch, const char *format, ... )
{
  va_list args;
  va_start(args, format);
  vwriteserver_channel(ch, format, args);
  va_end(args);
}

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vprivmsg_chan(channel_t * const ch, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  size_t ulen;

  if (ch == NULL) return;
  if (!ch->name) return;

  len = vsnprintf(tempstr, maxtextlength, format, ap);

  if ((len < 0) || (len >= maxtextlength)) {
    outerror(OUTERROR_TYPE_WARN, "PRVMSG-CHAN: Output too large, ignoring!");
    return;
  }
  ulen = (size_t)len;
  if (gnetwork->plaintext || ch->plaintext) {
    ulen = removenonprintable(tempstr);
  }

#ifndef WITHOUT_BLOWFISH
  if (ch->fish) {
    char *tempcrypt;

    if (gdata.debug > 13) {
      ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<FISH<: %s", tempstr);
    }
    tempcrypt = encrypt_fish(tempstr, ulen, ch->fish);
    if (tempcrypt) {
      writeserver_channel(ch, "+OK %s", tempcrypt); /* NOTRANSLATE */
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver_channel(ch, "%s", tempstr); /* NOTRANSLATE */
}

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_chan(channel_t * const ch, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg_chan(ch, format, args);
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

void writeserver_privmsg(writeserver_type_e delay, const char *nick, const char *message, size_t len)
{
#ifndef WITHOUT_BLOWFISH
  const char *fish;

  updatecontext();

  fish = check_fish_exclude(nick);
  if (fish != NULL) {
    char *tempcrypt;

    if (gdata.debug > 13) {
      ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<FISH<: %s", message);
    }
    tempcrypt = encrypt_fish(message, len, fish);
    if (tempcrypt) {
      writeserver(delay, "PRIVMSG %s :+OK %s", nick, tempcrypt); /* NOTRANSLATE */
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver(delay, "PRIVMSG %s :%s", nick, message); /* NOTRANSLATE */
}

void writeserver_notice(writeserver_type_e delay, const char *nick, const char *message, size_t len)
{
#ifndef WITHOUT_BLOWFISH
  const char *fish;

  updatecontext();

  fish = check_fish_exclude(nick);
  if (fish != NULL) {
    char *tempcrypt;

    if (gdata.debug > 13) {
      ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<FISH<: %s", message);
    }
    tempcrypt = encrypt_fish(message, len, fish);
    if (tempcrypt) {
      writeserver(delay, "NOTICE %s :+OK %s", nick, tempcrypt); /* NOTRANSLATE */
      mydelete(tempcrypt);
      return;
    }
  }
#endif /* WITHOUT_BLOWFISH */
  writeserver(delay, "NOTICE %s :%s", nick, message); /* NOTRANSLATE */
}

static void cleanannounce(void)
{
  channel_announce_t *item;

  for (item = irlist_get_head(&(gnetwork->serverq_channel));
       item;
       item = irlist_delete(&(gnetwork->serverq_channel), item)) {
     mydelete(item->channel);
     mydelete(item->msg);
  }
}

void clean_send_buffers(void)
{
  cleanannounce();
  irlist_delete_all(&(gnetwork->serverq_slow));
  irlist_delete_all(&(gnetwork->serverq_normal));
  irlist_delete_all(&(gnetwork->serverq_fast));
}

void sendannounce(void)
{
  channel_announce_t *item;

  for (item = irlist_get_head(&(gnetwork->serverq_channel)); item;) {
    if (gdata.curtime < item->c_time) {
      item = irlist_get_next(item);
      continue;
    }

    if (gdata.noannounce_start <= gdata.curtime)
      writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :%s", item->channel, item->msg); /* NOTRANSLATE */
    mydelete(item->channel);
    mydelete(item->msg);
    item = irlist_delete(&(gnetwork->serverq_channel), item);
  }
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

  if ( gdata.admin_job_done_file != NULL)
    done = mystrdup(gdata.admin_job_done_file);
  else
    done = mystrsuffix(job, ".done");
  fd = open_append(done, "Admin Job Done");
  if (fd >= 0) {
    admin_line(fd, cmd);
    close(fd);
  }
  mydelete(done)
}

/* ignore empty lines and comments and trailing spaces */
static int admin_clean_input(char * line)
{
  char *l;

  if (line[0] == 0)
    return 1;

  if (line[0] == '#')
    return 1;

  l = line + strlen(line) - 1;
  while (( l >= line ) && (( *l == '\r' ) || ( *l == '\n' ) || ( *l == '\t' ) || ( *l == ' ' )))
    *(l--) = 0;

  if (line[0] == 0)
    return 1;

  return 0;
}

void admin_jobs(void)
{
  FILE *fin;
  const char *job;
  char *line;
  char *r;

  job = gdata.admin_job_file;
  if (job == NULL)
    return;

  fin = fopen(job, "r" ); /* NOTRANSLATE */
  if (fin == NULL)
    return;

  line = mymalloc(maxtextlength);
  while (!feof(fin)) {
    r = fgets(line, maxtextlength - 1, fin);
    if (r == NULL )
      break;
    if (admin_clean_input(line))
      continue;
    irlist_add_string(&gdata.jobs_delayed, line);
  }
  mydelete(line)
  fclose(fin);
  unlink(job);
}

static int check_for_file_remove(unsigned int n)
{
  xdcc *xd;
  userinput *pubplist;
  char *tempstr;

  updatecontext();

  xd = irlist_get_nth(&gdata.xdccs, n - 1);
  if (look_for_file_changes(xd) == 0)
    return 0;

  if (gdata.removelostfiles == 0)
    return 0;

  pubplist = mycalloc(sizeof(userinput));
  tempstr = mymalloc(maxtextlength);
  snprintf(tempstr, maxtextlength, "REMOVE %u", n); /* NOTRANSLATE */
  u_fillwith_console(pubplist, tempstr);
  u_parseit(pubplist);
  mydelete(pubplist);
  mydelete(tempstr);
  return 1;
}

static unsigned int last_look_for_file_remove = 0;

void look_for_file_remove(void)
{
  unsigned int i;
  unsigned int p;
  unsigned int m;

  updatecontext();

  p = irlist_size(&gdata.xdccs);
  m = min2(gdata.monitor_files, p);
  for (i=0; i<m; ++i) {
    if (last_look_for_file_remove >= p)
      last_look_for_file_remove = 0;

    ++last_look_for_file_remove;
    if (check_for_file_remove(last_look_for_file_remove)) {
      --last_look_for_file_remove;
      return;
    }
  }
  return;
}

void reset_download_limits(void)
{
  unsigned int num;
  unsigned int newlimit;
  xdcc *xd;

  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (xd->dlimit_max == 0)
      continue;

    newlimit = xd->gets + xd->dlimit_max;
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "Resetting download limit of pack %u, used %u",
            num, newlimit - xd->dlimit_used);
    xd->dlimit_used = newlimit;
  }
}

static const char *badcrc = "badcrc"; /* NOTRANSLATE */

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

  newcrc = mymalloc(10);
  snprintf(newcrc, 10, CRC32_PRINT_FMT, xd->crc32);
  line = mymalloc(strlen(xd->file)+1);

  /* ignore path */
  x = getfilename(xd->file);

  strcpy(line, x);
  /* ignore extension */
  w = strrchr(line, '.');
  if (w != NULL)
    *w = 0;

  caps(line);
  if (strcasestr(line, newcrc) != NULL) {
    if (quiet && (gdata.verbose_crc32 == 0))
      x = NULL;
    else
      x = "CRC32 verified OK";
    /* unlock pack */
    if ((quiet == 2) && (xd->lock != NULL)) {
      if (strcmp(xd->lock, badcrc) == 0) {
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "unlock Pack %u, File %s",
                number_of_pack(xd), line);
        mydelete(xd->lock);
        xd->lock = NULL;
      }
    }
  } else {
    x = "CRC32 not found";
    if (fnmatch("*[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]*", line, FNM_CASEFOLD) == 0) {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "crc expected %s, failed %s", newcrc, line);
      x = "CRC32 failed";
      if (quiet == 2) {
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "lock Pack %u, File %s",
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
  gdata.crc32build.crc = ~0U;
}

void crc32_update(char *buf, size_t len)
{
  char *p;
  ir_uint32 crc = gdata.crc32build.crc;

  for (p = buf; len--; ++p) {
    crc = (crc >> 8) ^ crctable[(crc ^ *p) & 0xff];
  }
  gdata.crc32build.crc = crc;
}

static void crc32_final(xdcc *xd)
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
  if (gdata.debug > 0)
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "autoadd scan %s", dir);
  line = mymalloc(maxtextlength);
  if (group != NULL)
    snprintf(line, maxtextlength, "ADDGROUP %s \"%s\"", group, dir); /* NOTRANSLATE */
  else
    snprintf(line, maxtextlength, "ADDNEW \"%s\"", dir); /* NOTRANSLATE */

  uxdl = irlist_add(&gdata.packs_delayed, sizeof(userinput));
  a_fillwith_msg2(uxdl, NULL, line);
  uxdl->method = method_out_all;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_AUTO;
  uxdl->fd = FD_UNUSED;
  mydelete(line);
}

void autoadd_all(void)
{
  char *dir;

  updatecontext();

  if (gdata.noautoadd > gdata.curtime)
    return;

  /* stop here if we have already files to add/remove */
  if (irlist_size(&gdata.packs_delayed) > 0)
    return;

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
    if (strcmp(u->cmd, "REMOVE") == 0) { /* NOTRANSLATE */
      a_remove_delayed(u);
      mydelete(u->cmd);
      mydelete(u->arg1);
      mydelete(u->arg2);
      u = irlist_delete(&gdata.packs_delayed, u);
      /* process only one file */
      return;
    }
    if (strcmp(u->cmd, "ADD") == 0) { /* NOTRANSLATE */
      a_add_delayed(u);
      mydelete(u->cmd);
      mydelete(u->arg1);
      mydelete(u->arg2);
      u = irlist_delete(&gdata.packs_delayed, u);
      /* process only one file */
      return;
    }
    if (strcmp(u->cmd, "ADDGROUP") == 0) { /* NOTRANSLATE */
      a_addgroup(u);
      free_userinput(u);
      u = irlist_delete(&gdata.packs_delayed, u);
      /* process only one file */
      return;
    }
    if (strcmp(u->cmd, "ADDNEW") == 0) { /* NOTRANSLATE */
      a_addnew(u);
      free_userinput(u);
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

const char *find_groupdesc(const char *group)
{
  xdcc *xd;

  if (group == NULL)
    return "";

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, group) == 0)
      return xd->group_desc;

  }
  return "";
}

void write_removed_xdcc(xdcc *xd)
{
  char *line;
  size_t len;
  int fd;

  if (gdata.xdccremovefile == NULL)
    return;

  fd = open_append(gdata.xdccremovefile, "XDCC Remove");
  if (fd < 0)
    return;

  line = mymalloc(maxtextlength);
  len = add_snprintf(line, maxtextlength, "\n"); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_file %s\n", xd->file); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_desc %s\n", xd->desc); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_note %s\n", xd->note ? xd->note : ""); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_size %" LLPRINTFMT "d\n", xd->st_size); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_gets %u\n", xd->gets); /* NOTRANSLATE */
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = add_snprintf(line, maxtextlength, "xx_mins \n"); /* NOTRANSLATE */
  else
    len = add_snprintf(line, maxtextlength, "xx_mins %f\n", xd->minspeed); /* NOTRANSLATE */
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = add_snprintf(line, maxtextlength, "xx_maxs \n"); /* NOTRANSLATE */
  else
    len = add_snprintf(line, maxtextlength, "xx_maxs %f\n", xd->maxspeed); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_data %s\n", xd->group ? xd->group : ""); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_trig \n"); /* NOTRANSLATE */
  write(fd, line, len);
  len = add_snprintf(line, maxtextlength, "xx_trno %s\n", find_groupdesc(xd->group)); /* NOTRANSLATE */
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
                        unsigned int xx_gets, float xx_mins, float xx_maxs,
                        const char *xx_data, const char *xx_trno)
{
  char *file;
  const char *newfile;
  struct stat st;
  userinput u2;
  xdcc *xd;
  int xfiledescriptor;
  unsigned int rc;

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
  xd->file_fd = FD_UNUSED;

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

unsigned int import_xdccfile(void)
{
  char *templine;
  char *word;
  char *data;
  char *r;
  char *xx_file = NULL;
  char *xx_desc = NULL;
  char *xx_note = NULL;
  char *xx_data = NULL;
  char *xx_trno = NULL;
  FILE *fin;
  float val;
  unsigned int found = 0;
  unsigned int part;
  unsigned int err = 0;
  unsigned int step = 0;
  unsigned int xx_gets = 0;
  float xx_mins = 0.0;
  float xx_maxs = 0.0;

  updatecontext();

  if (gdata.import == NULL)
    return found;

  fin = fopen(gdata.import, "r" ); /* NOTRANSLATE */
  if (fin == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access XDCC File '%s': %s", gdata.import, strerror(errno));
    return found;
  }

  templine = mymalloc(maxtextlength);
  while (!feof(fin)) {
    r = fgets(templine, maxtextlength - 1, fin);
    if (r == NULL )
      break;
    ++step;
    admin_clean_input(templine);
    if (step == 1) {
      part = 0;
      for (word = strtok(templine, " "); /* NOTRANSLATE */
           word;
           word = strtok(NULL, " ")) { /* NOTRANSLATE */
        ++part;
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
      continue;
    }
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
        ++found;
      }
      continue;
    }
    word = strtok(templine, " "); /* NOTRANSLATE */
    if (word == NULL) {
      ++err;
      break;
    }
    data = strtok(NULL, "\n"); /* NOTRANSLATE */
    if (strcmp(word, "xx_file") == 0) { /* NOTRANSLATE */
      if (data == NULL) {
        ++err;
        break;
      }
      xx_file = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_desc") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_desc = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_note") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_note = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_size") == 0) { /* NOTRANSLATE */
      continue;
    }
    if (strcmp(word, "xx_gets") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_gets = atoi(data);
      continue;
    }
    if (strcmp(word, "xx_mins") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_mins = atof(data);
      continue;
    }
    if (strcmp(word, "xx_maxs") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_maxs = atof(data);
      continue;
    }
    if (strcmp(word, "xx_data") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_data = mystrdup(data);
      continue;
    }
    if (strcmp(word, "xx_trig") == 0) { /* NOTRANSLATE */
      continue;
    }
    if (strcmp(word, "xx_trno") == 0) { /* NOTRANSLATE */
      if (data != NULL)
        xx_trno = mystrdup(data);
      continue;
    }
    ++err;
    break;
  }
  fclose(fin);

  if (err > 0) {
    outerror(OUTERROR_TYPE_CRASH, "Error in XDCC File: %s", templine);
    mydelete(templine);
    return found;
  }
  mydelete(templine)

  if (step == 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Empty XDCC File");
    return found;
  }
  if (xx_file != NULL) {
    import_pack(xx_file, xx_desc, xx_note, xx_gets, xx_mins, xx_maxs, xx_data, xx_trno);
    mydelete(xx_file);
    mydelete(xx_desc);
    mydelete(xx_note);
    mydelete(xx_data);
    mydelete(xx_trno);
    ++found;
  }
  return found;
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
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->trigger != NULL)
      autotrigger_add(xd);
  }
}

void start_md5_hash(xdcc *xd, unsigned int packnum)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "MD5: [Pack %u] Calculating", packnum);

  gdata.md5build.bytes = 0;
  gdata.md5build.file_fd = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (gdata.md5build.file_fd >= 0) {
    gdata.md5build.xpack = xd;
    if (!gdata.nocrc32)
      crc32_init();
    MD5Init(&gdata.md5build.md5sum);
    if (set_socket_nonblocking(gdata.md5build.file_fd, 1) < 0) {
      outerror(OUTERROR_TYPE_WARN, "MD5: [Pack %u] Couldn't Set Non-Blocking", packnum);
    }
  } else {
    outerror(OUTERROR_TYPE_WARN,
             "MD5: [Pack %u] Cant Access Offered File '%s': %s",
             packnum, xd->file, strerror(errno));
             gdata.md5build.file_fd = FD_UNUSED;
             check_for_file_remove(packnum);
  }
}

void cancel_md5_hash(xdcc *xd, const char *msg)
{
  if (gdata.md5build.xpack == xd) {
    outerror(OUTERROR_TYPE_WARN, "MD5: [Pack %u] Canceled (%s)", number_of_pack(xd), msg);

    event_close(gdata.md5build.file_fd);
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

void complete_md5_hash(void)
{
  MD5Final(gdata.md5build.xpack->md5sum, &gdata.md5build.md5sum);
  if (!gdata.nocrc32)
    crc32_final(gdata.md5build.xpack);
  gdata.md5build.xpack->has_md5sum = 1;

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "MD5: [Pack %u] is " MD5_PRINT_FMT,
          number_of_pack(gdata.md5build.xpack),
          MD5_PRINT_DATA(gdata.md5build.xpack->md5sum));
  if (!gdata.nocrc32)
     ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
             "CRC32: [Pack %u] is " CRC32_PRINT_FMT,
             number_of_pack(gdata.md5build.xpack), gdata.md5build.xpack->crc32);

  event_close(gdata.md5build.file_fd);
  gdata.md5build.file_fd = FD_UNUSED;
  if (!gdata.nocrc32 && gdata.auto_crc_check) {
    const char *crcmsg = validate_crc32(gdata.md5build.xpack, 2);
    if (crcmsg != NULL) {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
              "CRC32: [Pack %u] File '%s' %s.",
              number_of_pack(gdata.md5build.xpack),
              gdata.md5build.xpack->file, crcmsg);
    }
  }
  gdata.md5build.xpack = NULL;
}

void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch)
{
  char *line;
  userinput_method_e method;

  method = manplist->method;
  if (ch) {
    if (ch->pgroup) {
      line = mymalloc(maxtextlength);
      snprintf(line, maxtextlength, "XDLGROUP %s", ch->pgroup); /* NOTRANSLATE */
      a_fillwith_msg2(manplist, name, line);
      mydelete(line);
      manplist->method = method;
      return;
    }
  }
  if ((gdata.xdcclist_grouponly) || (method != method_xdl_channel)) {
    a_fillwith_msg2(manplist, name, "XDL"); /* NOTRANSLATE */
  } else {
    a_fillwith_msg2(manplist, name, "XDLFULL"); /* NOTRANSLATE */
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

static void save_unlink_failed(const char *path, const char *dest)
{
   outerror(OUTERROR_TYPE_WARN_LOUD,
            "Cant move file '%s' to '%s': %s",
            path, dest, strerror(errno));
}

int save_unlink(const char *path)
{
  const char *file;
  char *dest;
  size_t len;
  int rc;
  unsigned int num;

  if (gdata.trashcan_dir) {
    file = getfilename(path);
    len = strlen(gdata.trashcan_dir) + strlen(file) + 15;
    dest = mymalloc(len);
    snprintf(dest, len, "%s/%s", gdata.trashcan_dir, file); /* NOTRANSLATE */
    num = 0;
    while (file_not_exits(dest)) {
      snprintf(dest, len, "%s/%s.%03u", gdata.trashcan_dir, file, ++num); /* NOTRANSLATE */
      if (num >= 200) {
	save_unlink_failed(path, dest);
        mydelete(dest);
        return -1;
      }
    }
    rc = rename(path, dest);
    if (rc != 0) {
      save_unlink_failed(path, dest);
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

static void xml_buffer_init(xml_buffer_t *xmlbuf)
{
  xmlbuf->len = 0;
  xmlbuf->buffer[0] = 0;
}

static void xml_buffer_flush(xml_buffer_t *xmlbuf)
{
  ssize_t len;

  if (xmlbuf->len == 0)
    return;

  if (xmlbuf->buffer[0] == 0)
    return;

  len = write(xmlbuf->fd, xmlbuf->buffer, xmlbuf->len);
  if (len != (ssize_t)(xmlbuf->len))
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Write XDCC List File '%s': %s",
             xmlbuf->filename, strerror(errno));
  xml_buffer_init(xmlbuf);
  return;
}

static void xml_buffer_check(xml_buffer_t *xmlbuf, size_t len)
{
  if (xmlbuf->len == 0)
    return;

  if (xmlbuf->buffer[0] == 0)
    return;

  if (xmlbuf->len + len < MAX_XML_CHUNK)
    return;

  xml_buffer_flush(xmlbuf);
}

static void write_string(xml_buffer_t *xmlbuf, const char *line)
{
  size_t len;

  len = strlen(line);
  xml_buffer_check(xmlbuf, len);
  strncpy(xmlbuf->buffer+ xmlbuf->len, line, len);
  xmlbuf->len += len;
}

static void write_asc_int(xml_buffer_t *xmlbuf, int spaces, const char *tag, unsigned int val)
{
  size_t len;

  xml_buffer_check(xmlbuf, maxtextlengthshort);
  len = add_snprintf(xmlbuf->buffer + xmlbuf->len, MAX_XML_CHUNK - xmlbuf->len,
                     "%*s<%s>%u</%s>\n", spaces, "", tag, val, tag); /* NOTRANSLATE */
  xmlbuf->len += len;
}

static void write_asc_int64(xml_buffer_t *xmlbuf, int spaces, const char *tag, ir_int64 val)
{
  size_t len;

  xml_buffer_check(xmlbuf, maxtextlength);
  len = add_snprintf(xmlbuf->buffer + xmlbuf->len, MAX_XML_CHUNK - xmlbuf->len,
                     "%*s<%s>%" LLPRINTFMT "d</%s>\n", spaces, "", tag, val, tag); /* NOTRANSLATE */
  xmlbuf->len += len;
}

static void write_asc_hex(xml_buffer_t *xmlbuf, int spaces, const char *tag, unsigned long val)
{
  size_t len;

  xml_buffer_check(xmlbuf, maxtextlengthshort);
  len = add_snprintf(xmlbuf->buffer + xmlbuf->len, MAX_XML_CHUNK - xmlbuf->len,
                     "%*s<%s>%.8lX</%s>\n", spaces, "", tag, val, tag); /* NOTRANSLATE */
  xmlbuf->len += len;
}

static void write_asc_text(xml_buffer_t *xmlbuf, int spaces, const char *tag, const char *val)
{
  size_t len;

  xml_buffer_check(xmlbuf, maxtextlength);
  len = add_snprintf(xmlbuf->buffer + xmlbuf->len, MAX_XML_CHUNK - xmlbuf->len,
                     "%*s<%s><![CDATA[%s]]></%s>\n", spaces, "", tag, val, tag); /* NOTRANSLATE */
  xmlbuf->len += len;
}

static void write_asc_plain(xml_buffer_t *xmlbuf, int spaces, const char *tag, const char *val)
{
  size_t len;

  xml_buffer_check(xmlbuf, maxtextlength);
  len = add_snprintf(xmlbuf->buffer + xmlbuf->len, MAX_XML_CHUNK - xmlbuf->len,
                     "%*s<%s>%s</%s>\n", spaces, "", tag, val, tag); /* NOTRANSLATE */
  xmlbuf->len += len;
}

static void xdcc_save_xml(void)
{
  channel_t *ch;
  gnetwork_t *backup;
  xml_buffer_t *xmlbuf;
  char *filename_tmp;
  char *filename_bak;
  char *tempstr;
  xdcc *xd;
  off_t toffered;
  int fd;
  unsigned int num;
  unsigned int groups;
  unsigned int ss;

  updatecontext();

  if (!gdata.xdccxmlfile)
    return;

  filename_tmp = mystrsuffix(gdata.xdccxmlfile, ".tmp"); /* NOTRANSLATE */
  filename_bak = mystrsuffix(gdata.xdccxmlfile, "~"); /* NOTRANSLATE */

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

  xmlbuf = mymalloc(sizeof(xml_buffer_t));
  xml_buffer_init(xmlbuf);
  xmlbuf->filename = filename_tmp;
  xmlbuf->fd = fd;

  write_string(xmlbuf, "<?xml version=\"1.0\" encoding=\""); /* NOTRANSLATE */
  if (gdata.charset != NULL)
    write_string(xmlbuf, gdata.charset);
  else
    write_string(xmlbuf, "UTF-8"); /* NOTRANSLATE */
  write_string(xmlbuf, "\"?>\n"
               "<!DOCTYPE " "iroffer" " PUBLIC \"-//iroffer.dinoex.net//DTD " "iroffer" " 1.0//EN\" \"" /* NOTRANSLATE */
               "http://iroffer.dinoex.net/" "dtd/iroffer-10.dtd\">\n" "<iroffer>\n"); /* NOTRANSLATE */
  if (irlist_size(&gdata.xdccs) > 0)
    write_string(xmlbuf, "<packlist>\n\n"); /* NOTRANSLATE */

  groups = 0;
  toffered = 0;
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (hide_pack(xd))
      continue;

    toffered += xd->st_size;
    write_string(xmlbuf, "<pack>\n"); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 2, "packnr", num); /* NOTRANSLATE */
    tempstr = mystrdup(xd->desc);
    removenonprintable(tempstr);
    write_asc_text(xmlbuf, 2, "packname", tempstr); /* NOTRANSLATE */
    mydelete(tempstr);
    tempstr = sizestr(0, xd->st_size);
    write_asc_text(xmlbuf, 2, "packsize", tempstr); /* NOTRANSLATE */
    mydelete(tempstr);
    write_asc_int64(xmlbuf, 2, "packbytes", xd->st_size); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 2, "packgets", xd->gets); /* NOTRANSLATE */
    if (xd->color > 0) {
      write_asc_int(xmlbuf, 2, "packcolor", xd->color); /* NOTRANSLATE */
    }
    write_asc_int(xmlbuf, 2, "adddate", xd->xtime ? xd->xtime : xd->mtime); /* NOTRANSLATE */
    if (xd->group != NULL) {
      ++groups;
      write_asc_text(xmlbuf, 2, "groupname", xd->group); /* NOTRANSLATE */
    }
    if (xd->has_md5sum) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
      write_asc_text(xmlbuf, 2, "md5sum", tempstr); /* NOTRANSLATE */
      mydelete(tempstr);
    }
    if (xd->has_crc32) {
      write_asc_hex(xmlbuf, 2, "crc32", xd->crc32); /* NOTRANSLATE */
    }
    write_string(xmlbuf, "</pack>\n\n"); /* NOTRANSLATE */
  }
  if (irlist_size(&gdata.xdccs) > 0)
    write_string(xmlbuf, "</packlist>\n\n"); /* NOTRANSLATE */

  if (groups > 0) {
    write_string(xmlbuf, "<grouplist>\n"); /* NOTRANSLATE */
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (xd->group == NULL)
        continue;

      if (xd->group_desc == NULL)
        continue;

      write_string(xmlbuf, "  <group>\n"); /* NOTRANSLATE */
      write_asc_text(xmlbuf, 4, "groupname", xd->group); /* NOTRANSLATE */
      tempstr = mystrdup(xd->group_desc);
      removenonprintable(tempstr);
      write_asc_text(xmlbuf, 4, "groupdesc", xd->group_desc); /* NOTRANSLATE */
      mydelete(tempstr);
      write_string(xmlbuf, "  </group>\n"); /* NOTRANSLATE */
    }
    write_string(xmlbuf, "</grouplist>\n\n"); /* NOTRANSLATE */
  }

  write_string(xmlbuf, "<sysinfo>\n" /* NOTRANSLATE */
                   "  <slots>\n"); /* NOTRANSLATE */
  write_asc_int(xmlbuf, 4, "slotsfree", slotsfree()); /* NOTRANSLATE */
  write_asc_int(xmlbuf, 4, "slotsmax", gdata.slotsmax); /* NOTRANSLATE */
  write_string(xmlbuf, "  </slots>\n"); /* NOTRANSLATE */

  if (gdata.queuesize > 0) {
    write_string(xmlbuf, "  <mainqueue>\n"); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 4, "queueuse", irlist_size(&gdata.mainqueue)); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 4, "queuemax", gdata.queuesize); /* NOTRANSLATE */
    write_string(xmlbuf, "  </mainqueue>\n"); /* NOTRANSLATE */
  }

  if (gdata.idlequeuesize > 0) {
    write_string(xmlbuf, "  <idlequeue>\n"); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 4, "queueuse", irlist_size(&gdata.idlequeue)); /* NOTRANSLATE */
    write_asc_int(xmlbuf, 4, "queuemax", gdata.idlequeuesize); /* NOTRANSLATE */
    write_string(xmlbuf, "  </idlequeue>\n"); /* NOTRANSLATE */
  }

  write_string(xmlbuf, "  <bandwidth>\n"); /* NOTRANSLATE */
  tempstr = get_current_bandwidth();
  write_asc_plain(xmlbuf, 4, "banduse", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = mymalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%u.0kB/s", gdata.maxb / 4); /* NOTRANSLATE */
  write_asc_plain(xmlbuf, 4, "bandmax", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  write_string(xmlbuf, "  </bandwidth>\n"); /* NOTRANSLATE */

  write_string(xmlbuf, "  <quota>\n"); /* NOTRANSLATE */
  write_asc_int(xmlbuf, 4, "packsum", num); /* NOTRANSLATE */
  tempstr = sizestr(0, toffered);
  write_asc_text(xmlbuf, 4, "diskspace", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  write_asc_text(xmlbuf, 4, "transfereddaily", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  write_asc_text(xmlbuf, 4, "transferedweekly", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  write_asc_text(xmlbuf, 4, "transferedmonthly", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.totalsent);
  write_asc_text(xmlbuf, 4, "transferedtotal", tempstr); /* NOTRANSLATE */
  write_asc_int64(xmlbuf, 4, "transferedtotalbytes", gdata.totalsent); /* NOTRANSLATE */
  mydelete(tempstr);
  write_string(xmlbuf, "  </quota>\n"); /* NOTRANSLATE */

  write_string(xmlbuf, "  <limits>\n"); /* NOTRANSLATE */
  tempstr = mymalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.transferminspeed);
  write_asc_plain(xmlbuf, 4, "minspeed", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = mymalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.transfermaxspeed);
  write_asc_plain(xmlbuf, 4, "maxspeed", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  write_string(xmlbuf, "  </limits>\n"); /* NOTRANSLATE */

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    write_string(xmlbuf, "  <network>\n"); /* NOTRANSLATE */
    write_asc_plain(xmlbuf, 4, "networkname", gdata.networks[ss].name); /* NOTRANSLATE */
    gnetwork = &(gdata.networks[ss]);
    write_asc_plain(xmlbuf, 4, "confignick", get_config_nick()); /* NOTRANSLATE */
    write_asc_plain(xmlbuf, 4, "currentnick", get_user_nick()); /* NOTRANSLATE */
    write_asc_plain(xmlbuf, 4, "servername", gdata.networks[ss].curserver.hostname); /* NOTRANSLATE */
    if (gdata.networks[ss].curserveractualname != NULL)
      write_asc_plain(xmlbuf, 4, "currentservername", gdata.networks[ss].curserveractualname); /* NOTRANSLATE */
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if ((ch->flags & CHAN_ONCHAN) == 0)
        continue;
      write_asc_plain(xmlbuf, 4, "channel", ch->name); /* NOTRANSLATE */
    }
    write_string(xmlbuf, "  </network>\n"); /* NOTRANSLATE */
  }
  gnetwork = backup;

  write_string(xmlbuf, "  <stats>\n"); /* NOTRANSLATE */
  write_asc_plain(xmlbuf, 4, "version", "iroffer-dinoex " VERSIONLONG ); /* NOTRANSLATE */
  tempstr = mymalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 1, gdata.startuptime, maxtextlengthshort);
  write_asc_plain(xmlbuf, 4, "uptime", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  tempstr = mymalloc(maxtextlengthshort);
  tempstr = getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
  write_asc_plain(xmlbuf, 4, "totaluptime", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  write_asc_int(xmlbuf, 4, "lastupdate", gdata.curtime); /* NOTRANSLATE */
  write_string(xmlbuf, "  </stats>\n" /* NOTRANSLATE */
                   "</sysinfo>\n\n" /* NOTRANSLATE */
                   "</iroffer>\n"); /* NOTRANSLATE */

  xml_buffer_flush(xmlbuf);
  mydelete(xmlbuf);
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
  /* stop here if more to add/remove */
  if (irlist_size(&gdata.packs_delayed) > 1)
    return;

  xdccsavetext();
  xdcc_save_xml();
}

/* check if max_uploads is reached */
unsigned int max_uploads_reached( void )
{
  qupload_t *qu;
  unsigned int uploads;

  if ( gdata.max_uploads == 0 )
    return 0; /* not limited */

  uploads = irlist_size(&gdata.uploads);

  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (qu->q_state == QUPLOAD_RUNNING)
      uploads += 1;
  }

#ifdef USE_CURL
  uploads += fetch_running();
#endif /* USE_CURL */

  if ( uploads < gdata.max_uploads )
    return 0; /* not limited */

  return 1; /* limited */
}

void start_qupload(void)
{
  gnetwork_t *backup;
  qupload_t *qu;

  updatecontext();

  if (max_uploads_reached() != 0)
    return; /* busy */

  /* start next XDCC GET */
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {

    if (qu->q_state == QUPLOAD_TRYING)
      break;

    if (qu->q_state != QUPLOAD_WAITING)
      continue;

    backup = gnetwork;
    gnetwork = &(gdata.networks[qu->q_net]);
    privmsg_fast(qu->q_nick, "XDCC GET %s", qu->q_pack); /* NOTRANSLATE */
    gnetwork = backup;
    qu->q_state = QUPLOAD_TRYING;
    return;
  }

  /* start next fetch */
#ifdef USE_CURL
  fetch_next();
#endif /* USE_CURL */
}

unsigned int close_qupload(unsigned int net, const char *nick)
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

void send_periodicmsg(void)
{
  periodicmsg_t *pm;

  updatecontext();

  for (pm = irlist_get_head(&gdata.periodicmsg);
       pm;
       pm = irlist_get_next(pm)) {
    gnetwork = &(gdata.networks[pm->pm_net]);
    if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
      continue;

    if ((unsigned)gdata.curtime <= pm->pm_next_time)
      continue;

    pm->pm_next_time = gdata.curtime + (pm->pm_time * 60);
    privmsg(pm->pm_nick, "%s", pm->pm_msg);
  }
  gnetwork = NULL;
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
  unsigned int ss;

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

  for (ss=0; ss<gdata.networks_online; ++ss) {
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

void a_quit_network(void)
{
  channel_t *ch;

  quit_server();
  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_delete(&(gnetwork->channels), ch)) {
    clearmemberlist(ch);
    free_channel_data(ch);
  }
  mydelete(gnetwork->user_nick);
  mydelete(gnetwork->caps_nick);
}

void a_rehash_needtojump(const userinput *u)
{
  char *new_vhost;
  char *old_vhost;
  gnetwork_t *backup;
  unsigned int ss;

  updatecontext();


  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
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
    for (ss=gdata.networks_online; ss<gdata.r_networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      a_quit_network();
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
  unsigned int ss;

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
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
          writeserver(WRITESERVER_NORMAL, "PART %s", rch->name); /* NOTRANSLATE */
        }
        if (gdata.debug > 0) {
           ioutput(OUT_S, COLOR_NO_COLOR,
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
        if (gdata.debug > 0) {
          ioutput(OUT_S, COLOR_NO_COLOR,
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
        if (gdata.debug > 0) {
          ioutput(OUT_S, COLOR_NO_COLOR,
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
  unsigned int ss;

  updatecontext();

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
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
  unsigned int ss;

  updatecontext();

  mydelete(r_local_vhost);
  mydelete(gdata.r_pidfile);

  if (!gdata.config_nick) {
    a_respond(u, "user_nick missing! keeping old nick!");
    gdata.config_nick = mystrdup(r_config_nick);
  }
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);
    old_nick = (gnetwork->r_config_nick) ? gnetwork->r_config_nick : r_config_nick;
    new_nick = get_config_nick();
    if (strcmp(new_nick, old_nick)) {
      a_respond(u, "user_nick changed, renaming nick to %s", new_nick);
      writeserver(WRITESERVER_NOW, "NICK %s", new_nick); /* NOTRANSLATE */
    }
    mydelete(gnetwork->r_config_nick);
    mydelete(gnetwork->r_local_vhost);
  }
  gnetwork = backup;
  mydelete(r_config_nick);
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
  unsigned int h;

  updatecontext();

  current_network = 0;
  templine = mymalloc(maxtextlength);
  for (h=0; h<MAXCONFIG && gdata.configfile[h]; ++h) {
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
    current_bracket = 0;
    while (!feof(fin)) {
      r = fgets(templine, maxtextlength, fin);
      if (r == NULL )
        break;
      ++current_line;
      if (admin_clean_input(templine))
        continue;
      getconfig_set(templine);
    }
    fclose(fin);
  }
  ++(gdata.networks_online);
  current_line = 0;
  mydelete(templine);
}

/* compare time for logrotate */
unsigned int compare_logrotate_time(void)
{
  struct tm lt_last;
  struct tm lt_now;
  int hrs;

  if (gdata.logrotate == 0) /* never */
    return 0;

  lt_last = *localtime(&gdata.last_logrotate);
  lt_now = *localtime(&gdata.curtime);

  if (lt_now.tm_year != lt_last.tm_year )
    return 1;

  if (gdata.logrotate == 7*24*60*60) { /* weekly */
    if (lt_now.tm_yday >= (lt_last.tm_yday + 7))
      return 1; /* time reached */

    if (lt_now.tm_wday != 1) /* not monday */
      return 0;

    if (lt_now.tm_hour != 0) /* not 00:00 */
      return 0;

    if (lt_now.tm_mon != lt_last.tm_mon )
      return 1; /* new month */

    if (lt_now.tm_mday == lt_last.tm_mday )
      return 0; /* same day */

    return 1; /* new week */
  }

  if (lt_now.tm_mon != lt_last.tm_mon )
    return 1;

  if (gdata.logrotate == 30*24*60*60) /* monthly */
    return 0; /* same month */

  if (lt_now.tm_mday != lt_last.tm_mday )
    return 1;

  if (gdata.logrotate == 24*60*60) /* daily */
    return 0; /* same day */

  if (lt_now.tm_hour == lt_last.tm_hour )
    return 0; /* same hour */

  hrs = (int)( gdata.logrotate / ( 60*60 ) );
  if (lt_now.tm_hour < (lt_last.tm_hour + hrs))
    return 0; /* time not reached */

  return 1;
}

/* get the new filename for a logfile */
char *new_logfilename(const char *logfile)
{
  struct tm *lthen;
  char *newname;
  size_t max;

  updatecontext();

  if (logfile == NULL)
    return NULL;

  lthen = localtime(&gdata.curtime);
  max = strlen(logfile) + 12;
  if (gdata.logrotate < 24*60*60) {
    max += 4;
    newname = mymalloc(max);
    snprintf(newname, max, "%s.%04i-%02i-%02i-%02i", /* NOTRANSLATE */
            logfile,
            lthen->tm_year+1900,
            lthen->tm_mon+1,
            lthen->tm_mday,
            lthen->tm_hour);
  } else {
    newname = mymalloc(max);
    snprintf(newname, max, "%s.%04i-%02i-%02i", /* NOTRANSLATE */
            logfile,
            lthen->tm_year+1900,
            lthen->tm_mon+1,
            lthen->tm_mday);
  }
  return newname;
}

static int readdir_sub(const char *thedir, DIR *dirp, struct dirent *entry, struct dirent **result)
{
  int rc = 0;
  unsigned int max = 3;

  for (max = 3; max > 0; --max) {
    rc = readdir_r(dirp, entry, result);
    if (rc == 0)
      break;

    outerror(OUTERROR_TYPE_WARN_LOUD, "Error Reading Directory %s: %s", thedir, strerror(errno));
    if (rc != EAGAIN)
      break;
  }
  return rc;
}

/* expire old logfiles */
void expire_logfiles(const char *logfile)
{
  struct stat st;
  struct dirent f2;
  struct dirent *f;
  DIR *d;
  const char *base;
  char *thedir;
  char *tempstr;
  size_t len;
  long expire_seconds;

  if (logfile == NULL)
    return;

  if (gdata.expire_logfiles == 0)
    return;

  updatecontext();

  if (logfile[0] == 0)
    return;

  thedir = mystrdup(logfile);
  tempstr = strrchr(thedir, '/');
  if (tempstr != NULL) {
    tempstr[0] = 0;
    base = tempstr + 1;
    if (base[0] == 0)
      return;
  } else {
    base = logfile;
    mydelete(thedir);
    thedir = mystrdup("."); /* NOTRANSLATE */
  }
  len = strlen(base);
  d = opendir(thedir);
  if (d == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Can't Access Directory: %s %s", thedir, strerror(errno));
    mydelete(thedir);
    return;
  }
  expire_seconds = gdata.expire_logfiles;
  expire_seconds *= 24*60*60;
  for (;;) {
    if (readdir_sub(thedir, d, &f2, &f) != 0)
      break;

    if (f == NULL)
      break;

    tempstr = mystrjoin(thedir, f->d_name, '/');
    if (stat(tempstr, &st) < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "cannot access '%s', ignoring: %s", tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      mydelete(tempstr);
      continue;
    }
    if (!S_ISREG(st.st_mode)) {
      mydelete(tempstr);
      continue;
    }

    if (strncmp(f->d_name, base, len) != 0) {
      mydelete(tempstr);
      continue;
    }

    if (strcmp(f->d_name, base) == 0) {
      mydelete(tempstr);
      continue;
    }

    if (gdata.curtime - st.st_mtime < expire_seconds ) {
      mydelete(tempstr);
      continue;
    }
    /* remove old log */
    if (unlink(tempstr) < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "File %s could not be deleted: %s", tempstr, strerror(errno));
    }

    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,  "File %s was deleted.", tempstr);
    mydelete(tempstr);
  }

  closedir(d);
  mydelete(thedir);
  return;
}

/* rotate a logfile */
int rotatelog(const char *logfile)
{
  char *newname;

  updatecontext();

  newname = new_logfilename(logfile);
  if (newname == NULL)
    return 0;

  if (rename(logfile, newname) < 0) {
    mylog("File %s could not be moved to %s: %s", logfile, newname, strerror(errno));
    mydelete(newname);
    return 0;
  }
  mylog("File %s was moved to %s.", logfile, newname);
  mydelete(newname);
  expire_logfiles(logfile);
  return 1;
}

/* delayed_announce */
void delayed_announce(void)
{
  xdcc *xd;
  unsigned int packnum;

  /* stop here if more to add/remove prending */
  if (irlist_size(&gdata.packs_delayed) > 0)
    return;

  if (gdata.nomd5sum == 0) {
    if (gdata.md5build.xpack) {
      /* checksum in progress */
      return;
    }
  }

  packnum = 1;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd), ++packnum) {
    if (xd->announce == 0)
      continue;

    if (gdata.nomd5sum == 0) {
      if (xd->has_md5sum == 0)
        continue;

      if (gdata.nocrc32 == 0) {
        if (xd->has_crc32 == 0)
          continue;
      }
    }

    /* wait for pack to be unlocked */
    if (xd->lock != NULL)
      continue;

    xd->announce = 0;
    a_autoaddann(xd, packnum);
  }
}

void backup_statefile(void)
{
  char *statefile_tmp;
  char *statefile_date;
  int callval;

  statefile_date = mycalloc(maxtextlengthshort);
  getdatestr(statefile_date, 0, maxtextlengthshort);
  statefile_tmp = mystrjoin(gdata.statefile, statefile_date, '.');
  mydelete(statefile_date);
  callval = rename(gdata.statefile, statefile_tmp);
  if (callval < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Save New State File '%s': %s",
             gdata.statefile, strerror(errno));
  }
  mydelete(statefile_tmp);
}

/* get rid of unused options */
void expire_options(void)
{
  dcc_options_t *dcc_options;
  time_t too_old;
  unsigned int ss;

  too_old = gdata.curtime;
  too_old -= 24*60*60; /* 24 hrs */
  for (ss = gdata.networks_online; ss--; ) {
    dcc_options = irlist_get_head(&gdata.networks[ss].dcc_options);
    while(dcc_options) {
      if (dcc_options->last_seen >= too_old) {
        dcc_options = irlist_get_next(dcc_options);
        continue;
      }
      mydelete(dcc_options->nick);
      dcc_options = irlist_delete(&gdata.networks[ss].dcc_options, dcc_options);
    }
  }
}

static magic_crc_t magic_crc_table[] = {
  { 0x24, 7, { 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00, 0} },
  { 0x0E, 4, { 0x50, 0x4b, 0x03, 0x04, 0, 0, 0, 0 } },
  { 0x00, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

typedef union {
  ir_uint32 i;
  unsigned char b[4];
} u_uint32_bytes_t;

static ir_uint32 byte4_swap(ir_uint32 in)
{
  u_uint32_bytes_t u;
  unsigned char c;

  u.i = in;

  c = u.b[3];
  u.b[3] = u.b[0];
  u.b[0] = c;

  c = u.b[2];
  u.b[2] = u.b[1];
  u.b[1] = c;

  return u.i;
}

/* check for CRC32 in compressed buffer */
static ir_uint32 get_zip_crc32_buffer(char *buffer)
{
  ir_uint32 zipcrc32;
  unsigned int i;

  for (i=0; magic_crc_table[i].len > 0; ++i) {
    if (memcmp(buffer, magic_crc_table[i].magic, magic_crc_table[i].len) != 0)
      continue;
    zipcrc32 = 0;
    memcpy(&zipcrc32, buffer + magic_crc_table[i].offset, 4);
    zipcrc32 = ntohl(zipcrc32);
    zipcrc32 = byte4_swap(zipcrc32);
    return zipcrc32;
  }
  return 0;
}

/* check for CRC32 in compressed pack */
ir_uint32 get_zip_crc32_pack(xdcc *xd)
{
  ir_uint32 zipcrc32;
  char *file;
  char *buffer;
  int xfiledescriptor;
  ssize_t howmuch;

  updatecontext();

  zipcrc32 = 0;
  /* verify file is ok first */
  file = mystrdup(xd->file);
  xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
  mydelete(file);
  if (xfiledescriptor < 0)
    return zipcrc32;
  buffer = mymalloc(maxtextlength);
  howmuch = read(xfiledescriptor, buffer, maxtextlength);
  if (howmuch == maxtextlength)
    zipcrc32 = get_zip_crc32_buffer(buffer);
  mydelete(buffer);
  close(xfiledescriptor);
  return zipcrc32;
}

/* End of File */
