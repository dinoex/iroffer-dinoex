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
#include "dinoex_http.h"
#include "dinoex_config.h"
#include "dinoex_transfer.h"
#include "dinoex_geoip.h"
#include "dinoex_ssl.h"
#include "dinoex_curl.h"
#include "dinoex_irc.h"
#include "dinoex_telnet.h"
#include "dinoex_queue.h"
#include "dinoex_jobs.h"
#include "dinoex_ruby.h"
#include "dinoex_misc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

#if !defined(_OS_CYGWIN)
#include <resolv.h>
#endif /* _OS_CYGWIN */

xdcc xdcc_statefile;
xdcc xdcc_listfile;

int hide_pack(const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;

  if (xd->lock == NULL)
    return 0;

  return 1;
}

int check_lock(const char* lockstr, const char* pwd)
{
  if (lockstr == NULL)
    return 0; /* no lock */
  if (pwd == NULL)
    return 1; /* locked */
  return strcmp(lockstr, pwd);
}

int number_of_pack(xdcc *pack)
{
  xdcc *xd;
  int n;

  updatecontext();

  if (pack == &xdcc_listfile)
    return -1;

  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    n++;
    if (xd == pack)
      return n;

    xd = irlist_get_next(xd);
  }

  return 0;
}

static int get_level(void)
{
  if (gnetwork->need_level >= 0)
    return gnetwork->need_level;

  return gdata.need_level;
}

static int get_voice(void)
{
  if (gnetwork->need_voice >= 0)
    return gnetwork->need_voice;

  return gdata.need_voice;
}

int check_level(char prefix)
{
  int ii;
  int need_level;
  int level;

  need_level = get_level();
  if (need_level == 0) {
    if (get_voice() == 0)
      return 1;
    /* any prefix is counted as voice */
    if ( prefix != 0 )
      return 1;
    /* no good prefix found try next chan */
    return 0;
  }

  level = 0;
  for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_symbol); ii++) {
    if (prefix == gnetwork->prefixes[ii].p_symbol) {
      /* found a nick mode */
      switch (gnetwork->prefixes[ii].p_mode) {
      case 'q':
      case 'a':
      case 'o':
        level = 3;
        break;
      case 'h':
        level = 2;
        break;
      case 'v':
        level = 1;
        break;
      default:
        level = 0;
      }
    }
  }

  if (level >= need_level)
    return 1;

  return 0;
}

void set_support_groups(void)
{
  xdcc *xd;

  gdata.support_groups = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL) {
      gdata.support_groups = 1;
      return;
    }
  }
}

static int stoplist_queue(const char *nick, irlist_t *list)
{
  char *item;
  char *copy;
  char *end;
  char *inick;
  int stopped = 0;

  for (item = irlist_get_head(list); item; ) {
    inick = NULL;
    copy = mystrdup(item);
    inick = strchr(copy, ' ');
    if (inick != NULL) {
      *(inick++) = 0;
      end = strchr(inick, ' ');
      if (end != NULL) {
        *(end++) = 0;
        if (strcasecmp(inick, nick) == 0) {
          if ( (strcmp(copy, "PRIVMSG") == 0) || (strcmp(copy, "NOTICE") == 0) ) {
            stopped ++;
            mydelete(copy);
            item = irlist_delete(list, item);
            continue;
          }
        }
      }
    }
    mydelete(copy);
    item = irlist_get_next(item);
  }
  return stopped;
}

static int stoplist_announce(const char *nick)
{
  channel_announce_t *item;
  char *copy;
  char *end;
  char *inick;
  int stopped = 0;

  item = irlist_get_head(&(gnetwork->serverq_channel));
  while (item) {
    inick = NULL;
    copy = mystrdup(item->msg);
    inick = strchr(copy, ' ');
    if (inick != NULL) {
      *(inick++) = 0;
      end = strchr(inick, ' ');
      if (end != NULL) {
        *(end++) = 0;
        if (strcasecmp(inick, nick) == 0) {
          if ( (strcmp(copy, "PRIVMSG") == 0) || (strcmp(copy, "NOTICE") == 0) ) {
            stopped ++;
            mydelete(copy);
            mydelete(item->msg);
            item = irlist_delete(&(gnetwork->serverq_channel), item);
            continue;
          }
        }
      }
    }
    mydelete(copy);
    item = irlist_get_next(item);
  }
  return stopped;
}

void stoplist(const char *nick)
{
  char *item;
  int stopped = 0;

  ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC STOP from (%s on %s)",
          nick, gnetwork->name);
  for (item = irlist_get_head(&(gnetwork->xlistqueue)); item; ) {
    if (strcasecmp(item, nick) == 0) {
      stopped ++;
      item = irlist_delete(&(gnetwork->xlistqueue), item);
      continue;
    }
    item = irlist_get_next(item);
  }

  stopped += stoplist_queue(nick, &(gnetwork->serverq_slow));
  stopped += stoplist_queue(nick, &(gnetwork->serverq_normal));
  stopped += stoplist_announce(nick);

  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " (stopped %d)", stopped);
  notice(nick, "LIST stopped (%d lines deleted)", stopped);
}

typedef struct {
  transfer *tr;
  int left;
} remaining_transfer_time;

static irlist_t end_trans;
remaining_transfer_time *end_remain;
unsigned long end_lastrtime;

unsigned long get_next_transfer_time(void)
{
  unsigned long old;

  if (end_remain == NULL)
    return end_lastrtime;

  end_remain = irlist_get_next(end_remain);
  if (end_remain == NULL)
    return end_lastrtime;

  old = end_lastrtime;
  end_lastrtime = end_remain->left;
  return old;
}

void add_new_transfer_time(xdcc *xd)
{
  float speed = 0.0;
  int nolimit = 1;
  int left;

  if (gdata.overallmaxspeed > 0) {
    speed = gdata.overallmaxspeed;
    nolimit = 0;
  }
  if (gdata.transfermaxspeed > 0) {
    if (nolimit != 0) {
      speed = gdata.transfermaxspeed;
      nolimit = 0;
    } else {
      speed = min2(speed, gdata.transfermaxspeed);
    }
  }
  if (xd->maxspeed > 0)
    speed = xd->maxspeed;
  if (speed <= 0.0)
    speed = 1000.0;

  left = min2(359999, (xd->st_size)/((int)(max2(speed, 0.001)*1024)));
  end_lastrtime += left;
}

void guess_end_transfers(void)
{
  transfer *tr;
  irlist_t list2;
  remaining_transfer_time *remain;
  remaining_transfer_time *rm;
  remaining_transfer_time *rmlast;
  int left;
  int i;

  updatecontext();

  /* make sortd list of all transfers */
  memset(&end_trans, 0, sizeof(irlist_t));
  memset(&list2, 0, sizeof(irlist_t));
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
    left = min2(359999, (tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed, 0.001)*1024)));
    remain = irlist_add(&list2, sizeof(remaining_transfer_time));
    remain->tr = tr;
    remain->left = left;
    irlist_remove(&list2, remain);

    rmlast = NULL;
    rm = irlist_get_head(&end_trans);
    while(rm) {
      if (remain->left < rm->left)
        break;
      rmlast = rm;
      rm = irlist_get_next(rm);
    }
    if (rmlast != NULL) {
      irlist_insert_after(&end_trans, remain, rmlast);
    } else {
      irlist_insert_head(&end_trans, remain);
    }
  }

  end_lastrtime = 0;
  end_remain = irlist_get_head(&end_trans);
  if (end_remain != NULL)
    end_lastrtime = end_remain->left;

  /* if we are sending more than allowed, we need to skip the difference */
  for (i=0; i<irlist_size(&gdata.trans) - gdata.slotsmax; i++) {
    get_next_transfer_time();
  }
}

void guess_end_cleanup(void)
{
  irlist_delete_all(&end_trans);
}

static int notifyqueued_queue(irlist_t *list, const char *nick, const char *ntime, int idle)
{
  ir_pqueue *pq;
  unsigned long rtime;
  int i = 0;
  int found = 0;

  updatecontext();
  i = 0;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    i ++;
    rtime = get_next_transfer_time();
    add_new_transfer_time(pq->xpack);

    if (pq->net != gnetwork->net)
      continue;

    if (nick != NULL) {
      if (strcasecmp(pq->nick, nick) != 0)
        continue;
    }

    found ++;
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_YELLOW,
            "Notifying Queued status to %s on %s",
            pq->nick, gnetwork->name);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining. (at %s)",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(list),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999U) ? "more" : "less",
                ntime);
    if (idle)
      break;
  }
  return found;
}

int notifyqueued_nick(const char *nick)
{
  struct tm *localt;
  char ntime[ 16 ];
  int found = 0;

  updatecontext();

  localt = localtime(&gdata.curtime);
  strftime(ntime, sizeof(ntime) - 1, "%H:%M", localt);

  guess_end_transfers();
  found += notifyqueued_queue(&gdata.mainqueue, nick, ntime, 0);
  found += notifyqueued_queue(&gdata.idlequeue, nick, ntime, 1);
  guess_end_cleanup();
  return found;
}

void notifyqueued(void)
{
  int found;

  updatecontext();

  if (gdata.exiting)
    return;

  if (irlist_size(&gdata.mainqueue) == 0)
    return;

  found = notifyqueued_nick(NULL);
  if (found == 0)
    return;

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_YELLOW,
          "Notifying %d Queued People on %s",
          found, gnetwork->name);
}

void check_new_connection(transfer *const tr)
{
  updatecontext();
  geoip_new_connection(tr);
  updatecontext();
  t_check_duplicateip(tr);
}

static int dinoex_lasthour;

static void init_xdcc(xdcc *xd)
{
  bzero((char *)xd, sizeof(xdcc));
  xd->file_fd = FD_UNUSED;
}

void startup_dinoex(void)
{
#ifndef WITHOUT_HTTP
#ifndef WITHOUT_HTTP_ADMIN
  init_base64decode();
#endif /* #ifndef WITHOUT_HTTP_ADMIN */
#endif /* #ifndef WITHOUT_HTTP */
#ifndef WITHOUT_BLOWFISH
  init_fish64decode();
#endif /* WITHOUT_BLOWFISH */
  gdata.support_groups = 0;
  config_startup();
  init_xdcc(&xdcc_statefile);
  init_xdcc(&xdcc_listfile);
  dinoex_lasthour = -1;
#ifdef USE_CURL
  curl_startup();
#endif /* USE_CURL */
  ssl_startup();
#if !defined(_OS_CYGWIN)
  res_init();
#endif /* _OS_CYGWIN */
  gdata.startingup = 1;
}

static void global_defaults(void)
{
  gnetwork_t *backup;
  int ss;

  if (!gdata.group_seperator)
    gdata.group_seperator = mystrdup(" ");
  if (!gdata.announce_seperator)
    gdata.announce_seperator = mystrdup(" ");

  if (gdata.usenatip) {
    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      if (gnetwork->usenatip != 0)
        continue;
      update_natip(gnetwork->natip ? gnetwork->natip : gdata.usenatip);
    }
    gnetwork = backup;
  }
}

void config_dinoex(void)
{
#ifdef USE_UPNP
  if (gdata.upnp_router)
    init_upnp();
#endif /* USE_UPNP */
#ifndef WITHOUT_TELNET
  telnet_setup_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_setup_listen();
#endif /* WITHOUT_HTTP */
#ifdef USE_RUBY
  startup_myruby();
#endif /* USE_RUBY */
  global_defaults();
}

void shutdown_dinoex(void)
{
#ifndef WITHOUT_TELNET
  telnet_close_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_close_listen();
#endif /* WITHOUT_HTTP */
  geoip_shutdown();
#ifdef USE_CURL
  curl_shutdown();
#endif /* USE_CURL */
#ifdef USE_RUBY
  shutdown_myruby();
#endif /* USE_RUBY */
}

void rehash_dinoex(void)
{
#ifndef WITHOUT_TELNET
  telnet_reash_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_reash_listen();
#endif /* WITHOUT_HTTP */
  global_defaults();
  geoip_shutdown();
#ifdef USE_RUBY
  rehash_myruby(0);
#endif /* USE_RUBY */
#if !defined(_OS_CYGWIN)
  res_init();
#endif /* _OS_CYGWIN */
}

static int init_xdcc_file(xdcc *xd, char *file)
{
  struct stat st;
  int xfiledescriptor;

  updatecontext();

  xd->file = file;
  mydelete(xd->desc);
  xd->desc = mystrdup( getfilename(xd->file) );
  xd->minspeed = gdata.transferminspeed;
  xd->maxspeed = gdata.transfermaxspeed;

  xfiledescriptor = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (xfiledescriptor < 0)
    return 1;

  if (fstat(xfiledescriptor, &st) < 0) {
    close(xfiledescriptor);
    return 1;
  }

  xd->st_size  = st.st_size;
  xd->st_dev   = st.st_dev;
  xd->st_ino   = st.st_ino;
  xd->mtime    = st.st_mtime;
  close(xfiledescriptor);
  return 0;
}

static int send_xdcc_file(xdcc *xd, char *file, const char *nick, const char *hostname)
{
  transfer *tr;

  updatecontext();

  if (init_xdcc_file(xd, file))
    return 1;
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Send: %s to %s bytes=%" LLPRINTFMT "d",
          file, nick, xd->st_size);
  tr = irlist_add(&gdata.trans, sizeof(transfer));
  t_initvalues(tr);
  tr->id = get_next_tr_id();
  tr->nick = mystrdup(nick);
  tr->caps_nick = mystrdup(nick);
  caps(tr->caps_nick);
  tr->hostname = mystrdup(hostname);
  tr->xpack = xd;
  tr->maxspeed = xd->maxspeed;
  tr->net = gnetwork->net;

  t_setup_dcc(tr, nick);
  return 0;
}

void update_hour_dinoex(int hour, int minute)
{
  xdcc *xd;
  transfer *tr;
  int lastminute;
  int net = 0;

  if (!gdata.send_statefile)
    return;

  updatecontext();

  if (dinoex_lasthour == hour)
    return;

  minute %= 60;
  lastminute = gdata.send_statefile_minute + 10;
  if (lastminute < 60) {
    if (minute < gdata.send_statefile_minute)
      return;
    if (minute >= lastminute)
      return;
  } else {
    lastminute = lastminute % 60;
    if ((minute < gdata.send_statefile_minute) && (minute > lastminute))
      return;
  }

  gnetwork = &(gdata.networks[net]);
  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
     return;

 /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gnetwork->lastservercontact >= 150)
     return;

  if (has_joined_channels(0) < 1)
     return;

  xd = &xdcc_statefile;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (xd == tr->xpack)
      return;
  }
  if (send_xdcc_file(&xdcc_statefile, gdata.statefile, gdata.send_statefile, "man") == 0)
    dinoex_lasthour = hour;
}

char *grep_to_fnmatch(const char *grep)
{
  char *raw;
  char *match;
  size_t len;

  len = strlen(grep) + 3;
  raw = mycalloc(len);
  snprintf(raw, len, "*%s*", grep);
  match = hostmask_to_fnmatch(raw);
  mydelete(raw);
  return match;
}

int fnmatch_xdcc(const char *match, xdcc *xd)
{
  const char *file;
  char datestr[maxtextlengthshort];

  file = getfilename(xd->file);
  if (fnmatch(match, file, FNM_CASEFOLD) == 0)
    return 1;
  if (fnmatch(match, xd->desc, FNM_CASEFOLD) == 0)
    return 1;
  if (xd->note != NULL) {
    if (fnmatch(match, xd->note, FNM_CASEFOLD) == 0)
      return 1;
  }
  if (gdata.show_date_added) {
    user_getdatestr(datestr, xd->xtime ? xd->xtime : xd->mtime, maxtextlengthshort - 1);
    if (fnmatch(match, datestr, FNM_CASEFOLD) == 0)
      return 1;
  }
  return 0;
}

int disk_full(const char *path)
{
#ifndef NO_STATVFS
  struct statvfs stf;
#else
#ifndef NO_STATFS
  struct statfs stf;
#endif /* NO_STATFS */
#endif /* NO_STATVFS */
  off_t freebytes;

  freebytes = 0L;
#ifndef NO_STATVFS
  if (statvfs(path, &stf) < 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Unable to determine device sizes: %s",
            strerror(errno));
  } else {
    freebytes = (off_t)stf.f_bavail * (off_t)stf.f_frsize;
  }
#else /* NO_STATVFS */
#ifndef NO_STATFS
  if (statfs(path, &stf) < 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Unable to determine device sizes: %s",
            strerror(errno));
  } else {
    freebytes = (off_t)stf.f_bavail * (off_t)stf.f_bsize;
  }
#endif /* NO_STATFS */
#endif /* NO_STATVFS */

  if (gdata.debug > 0)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
           "disk_free= %" LLPRINTFMT "d, required= %" LLPRINTFMT "d",
           freebytes, gdata.uploadminspace);

  if (freebytes >= gdata.uploadminspace)
    return 0;

  return 1;
}

char *get_user_modes(void)
{
  return (gnetwork->user_modes) ? gnetwork->user_modes : gdata.user_modes;
}

char *get_nickserv_pass(void)
{
  return (gnetwork->nickserv_pass) ? gnetwork->nickserv_pass : gdata.nickserv_pass;
}

xdcc *get_xdcc_pack(int pack)
{
  if (pack == -1)
    return &xdcc_listfile;

  return irlist_get_nth(&gdata.xdccs, pack - 1);
}

int access_need_level(const char *nick, const char *text)
{
  if (!isinmemberlist(nick)) {
    if ((get_voice() != 0) || (get_level() != 0))
      notice(nick, "** XDCC %s denied, you must have voice or more on a known channel to request a pack", text);
    else
      notice(nick, "** XDCC %s denied, you must be on a known channel to request a pack", text);
    return 1;
  }
  return 0;
}

static int check_manual_send(const char* hostname, int *man)
{
  if (man == NULL)
    return 0;

  if (!strcmp(hostname, "man")) {
    *man = 1;
  } else {
    *man = 0;
  }
  return *man;
}

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text, int restr)
{
  char *grouplist;
  xdcc *xd;
  int isman;

  updatecontext();

  isman = check_manual_send(hostname, man);
  if (isman == 0) {
    if (!verifyshell(&gdata.downloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (verifyshell(&gdata.nodownloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (restr && access_need_level(nick, text)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (restricted): ");
      return NULL;
    }
    if (gdata.enable_nick && !isinmemberlist(gdata.enable_nick)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (offline): ");
      notice(nick, "** XDCC %s denied, owner of this bot is not online", text);
      return NULL;
    }
  }

  if (pack == -1) {
    if (init_xdcc_file(&xdcc_listfile, gdata.xdcclistfile))
      return NULL;
    return &xdcc_listfile;
  }

  if ((pack > irlist_size(&gdata.xdccs)) || (pack < 1)) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " (Bad Pack Number): ");
    notice(nick, "** Invalid Pack Number, Try Again");
    return NULL;
  }

  xd = get_xdcc_pack(pack);
  if (xd == NULL)
    return NULL;

  if (isman != 0)
    return xd;

  /* apply group visibility rules */
  if (restr) {
    grouplist = get_grouplist_access(nick);
    if (!verify_group_in_grouplist(xd->group, grouplist)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (group access restricted): ");
      notice(nick, "** XDCC %s denied, you must be on the correct channel to request this pack", text);
      mydelete(grouplist);
      return NULL;
    }
    mydelete(grouplist);
  }
  return xd;
}

void lost_nick(const char *nick)
{
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Nickname %s on %s left",
          nick,
          gnetwork->name);
  stoplist(nick);
  queue_xdcc_remove(&gdata.mainqueue, gnetwork->net, nick);
  queue_xdcc_remove(&gdata.idlequeue, gnetwork->net, nick);
}

int is_unsave_directory(const char *dir)
{
  char *line;
  char *word;
  int bad = 0;

  /* no device letters */
  if (strchr(dir, ':'))
    return 1;

  line = mystrdup(dir);
  for (word = strtok(line, "/");
       word;
       word = strtok(NULL, "/")) {
    if (strcmp(word, "..") == 0) {
      bad ++;
      break;
    }
  }
  mydelete(line);
  return bad;
}

static void mylog_write(int logfd, const char *logfile, const char *msg, ssize_t len)
{
  ssize_t len2;

  len2 = write(logfd, msg, len);
  if (len2 == len)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD,
           "Cant Write Log File '%s': %s",
           logfile, strerror(errno));
}

void logfile_add(const char *logfile, const char *line)
{
  char tempstr[maxtextlength];
  int rc;
  int logfd;

  logfd = open(logfile,
               O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
               CREAT_PERMISSIONS);
  if (logfd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Access Log File '%s': %s",
             logfile, strerror(errno));
    return;
  }

  getdatestr(tempstr, 0, maxtextlength);
  mylog_write(logfd, logfile, "** ", 3);
  mylog_write(logfd, logfile, tempstr, strlen(tempstr));
  mylog_write(logfd, logfile, ": ", 2);
  mylog_write(logfd, logfile, line, strlen(line));
  mylog_write(logfd, logfile, "\n", 1);
  rc = close(logfd);
  if (rc != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Write Log File '%s': %s",
             logfile, strerror(errno));
  }
}

char *get_current_bandwidth(void)
{
  char *tempstr;
  ir_uint64 xdccsent;
  int i;

  for (i=0, xdccsent=0; i<XDCC_SENT_SIZE; i++) {
    xdccsent += (ir_uint64)gdata.xdccsent[i];
  }
  tempstr = mycalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fKB/s",
           ((float)xdccsent) / XDCC_SENT_SIZE / 1024.0);
  return tempstr;
}

char *transfer_limit_exceeded_msg(int ii)
{
   char *tempstr = mycalloc(maxtextlength);
   char *tempstr2 = mycalloc(maxtextlengthshort);

   getdatestr(tempstr2, gdata.transferlimits[ii].ends, maxtextlengthshort);
   snprintf(tempstr, maxtextlength,
            "Sorry, I have exceeded my %s transfer limit of %" LLPRINTFMT "uMB.  Try again after %s.",
            transferlimit_type_to_string(ii),
            gdata.transferlimits[ii].limit / 1024 / 1024,
            tempstr2);
   mydelete(tempstr2);
   return tempstr;
}

int verify_uploadhost(const char *hostmask)
{
  tupload_t *tu;
  qupload_t *qu;

  updatecontext();

  if ( verifyshell(&gdata.uploadhost, hostmask) != 0 )
    return 0;

  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu;
       tu = irlist_get_next(tu)) {
    if (tu->u_time <= gdata.curtime)
      continue;

    if (fnmatch(tu->u_host, hostmask, FNM_CASEFOLD) == 0)
      return 0;
  }
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (gnetwork->net != qu->q_net)
      continue;

    if (fnmatch(qu->q_host, hostmask, FNM_CASEFOLD) == 0)
      return 0;
  }
  return 1;
}

void clean_uploadhost(void)
{
  tupload_t *tu;

  updatecontext();

  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu; ) {
    if (tu->u_time >= gdata.curtime) {
      tu = irlist_get_next(tu);
      continue;
    }
    mydelete(tu->u_host);
    tu = irlist_delete(&gdata.tuploadhost, tu);
  }
}

char *user_getdatestr(char* str, time_t Tp, int len)
{
  const char *format;
  struct tm *localt = NULL;
  ssize_t llen;

  localt = localtime(&Tp);
  format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";
  llen = strftime(str, len, format, localt);
  if ((llen == 0) || (llen == len))
    str[0] = '\0';
  return str;
}

/* returns list of allowed groups for nick on current network, or NULL for unrestricted access. calling function must take care of freeing result */
char *get_grouplist_access(const char *nick)
{
  char *tempstr = mycalloc(maxtextlength);
  int len = 0;
  channel_t *ch;
  member_t *member;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    for (member = irlist_get_head(&ch->members);
         member;
	 member = irlist_get_next(member)) {
      if (strcasecmp(member->nick, nick) != 0)
        continue;

      if (check_level( member->prefixes[0] ) == 0)
        continue;

      if (!ch->rgroup) {
        /* no restrictions in this channel */
        mydelete(tempstr);
        return NULL;
      }

      len += snprintf(tempstr + len, maxtextlength - len, " %s", ch->rgroup);
    }
  }
  return tempstr;
}

/* apply per-channel visibility rules */
const char *get_grouplist_channel(const char *dest)
{
  channel_t *ch;

  if (dest && (*dest=='#')) {
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if (strcasecmp(ch->name, dest) == 0) {
        return ch->rgroup;
      }
    }
  }
  return NULL;
}

static int verify_bits(int bits, const unsigned char *data1, const unsigned char *data2)
{
  unsigned char ch1;
  unsigned char ch2;
  unsigned char mask;

  if (bits <= 0)
    return 1; /* 0 bits matches all */

  while (bits >= 8) {
    if (*data1 != *data2)
      return 0; /* not found */

    /* next byte */
    ++data1;
    ++data2;
    bits -= 8;
  }

  mask = 0xFF << (8 - bits);
  ch1 = *data1 & mask;
  ch2 = *data2 & mask;
  if (ch1 != ch2)
    return 0; /* not found */

  return 1; /* bits matched */
}

int verify_cidr(irlist_t *list, const ir_sockaddr_union_t *remote)
{
  ir_cidr_t *cidr;
  const unsigned char *data1;
  const unsigned char *data2;

  updatecontext();

  if (remote->sa.sa_family == AF_INET) {
    data1 = (const unsigned char *)&(remote->sin.sin_addr);
  } else {
    data1 = (const unsigned char *)&(remote->sin6.sin6_addr);
  }

  for (cidr = irlist_get_head(list);
       cidr;
       cidr = irlist_get_next(cidr)) {
    if (cidr->family != remote->sa.sa_family)
      continue;
    if (cidr->family == AF_INET) {
      data2 = (const unsigned char *)&(cidr->remote.sin.sin_addr);
    } else {
      data2 = (const unsigned char *)&(cidr->remote.sin6.sin6_addr);
    }
    if (verify_bits(cidr->netmask, data1, data2))
      return 1; /* bits matched */
  }
  return 0;
}

void free_channel_data(channel_t *ch)
{
  mydelete(ch->name);
  mydelete(ch->key);
  mydelete(ch->fish);
  mydelete(ch->headline);
  mydelete(ch->pgroup);
  mydelete(ch->joinmsg);
  mydelete(ch->listmsg);
  mydelete(ch->rgroup);
}

void auto_rehash(void)
{
  struct stat st;
  int filedescriptor;
  int h;

  updatecontext();

  if (gdata.no_auto_rehash)
    return;

  for (h=0; h<MAXCONFIG && gdata.configfile[h]; h++) {
    filedescriptor = open(gdata.configfile[h], O_RDONLY | ADDED_OPEN_FLAGS);
    if (filedescriptor < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
              "Cant Access Config File '%s': %s",
              gdata.configfile[h], strerror(errno));
      continue;
    }
    if (fstat(filedescriptor, &st) < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
              "Unable to stat file '%s': %s",
              gdata.configfile[h], strerror(errno));
      close(filedescriptor);
      continue;
    }
    close(filedescriptor);
    if (gdata.configtime[h] == st.st_mtime)
      continue;

    outerror(OUTERROR_TYPE_WARN_LOUD,
             "File '%s' has changed",
             gdata.configfile[h]);
    gdata.needsrehash = 1;
  }
}

/* End of File */
