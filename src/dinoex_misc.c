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

#include <resolv.h>

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

int get_level(void)
{
  if (gnetwork->need_level >= 0)
    return gnetwork->need_level;

  return gdata.need_level;
}

int get_voice(void)
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

void send_help(const char *nick)
{
  const char *mynick;

  updatecontext();

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC HELP from (%s on %s)",
          nick, gnetwork->name);

  mynick = get_user_nick();
  if (!gdata.restrictprivlist) {
    notice_slow(nick, "\2**\2 Request listing:   \"/MSG %s XDCC LIST\" \2**\2", mynick);
    if (gdata.support_groups) {
      notice_slow(nick, "\2**\2 Request listing:   \"/MSG %s XDCC LIST group\" \2**\2", mynick);
      if ((gdata.show_list_all != 0) && (gdata.restrictprivlistfull == 0))
        notice_slow(nick, "\2**\2 Request listing:   \"/MSG %s XDCC LIST ALL\" \2**\2", mynick);
    }
  }
  notice_slow(nick, "\2**\2 Stop listing:      \"/MSG %s XDCC STOP\" \2**\2", mynick);
  if (gdata.send_listfile)
    notice_slow(nick, "\2**\2 Download listing:  \"/MSG %s XDCC SEND LIST\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Search listing:    \"/MSG %s XDCC SEARCH pattern\" \2**\2", mynick);
  if ((gdata.hide_list_info == 0) && (gdata.disablexdccinfo == 0))
    notice_slow(nick, "\2**\2 Request details:   \"/MSG %s XDCC INFO pack\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Request download:  \"/MSG %s XDCC SEND pack\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Show time to wait: \"/MSG %s XDCC QUEUE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Remove from queue: \"/MSG %s XDCC REMOVE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Cancel download:   \"/MSG %s XDCC CANCEL\" \2**\2", mynick);
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

static float guess_maxspeed(xdcc *xd)
{
  int nolimit = 1;
  float speed = 0.0;

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
  return speed;
}

typedef struct {
  transfer *tr;
  int left;
} remaining_transfer_time;

static remaining_transfer_time *next_remaining_transfer(remaining_transfer_time *rm)
{
  if (rm == NULL)
    return rm;
  return irlist_get_next(rm);
}

int notifyqueued_nick(const char *nick)
{
  ir_pqueue *pq;
  transfer *tr;
  irlist_t list;
  irlist_t list2;
  remaining_transfer_time *remain;
  remaining_transfer_time *rm;
  unsigned long rtime;
  unsigned long lastrtime;
  float speed;
  int left;
  int i;
  int found = 0;
  struct tm *localt;
  char ntime[ 16 ];

  updatecontext();

  localt = localtime(&gdata.curtime);
  strftime( ntime, sizeof(ntime) - 1, "%H:%M", localt);

  /* make sortd list of all transfers */
  memset(&list, 0, sizeof(irlist_t));
  memset(&list2, 0, sizeof(irlist_t));
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
    left = min2(359999, (tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed, 0.001)*1024)));
    remain = irlist_add(&list2, sizeof(remaining_transfer_time));
    remain->tr = tr;
    remain->left = left;
    irlist_remove(&list2, remain);

    rm = irlist_get_head(&list);
    while(rm) {
      if (remain->left < rm->left)
        break;
      rm = irlist_get_next(rm);
    }
    if (rm != NULL) {
      irlist_insert_before(&list, remain, rm);
    } else {
      irlist_insert_tail(&list, remain);
    }
  }

  lastrtime = 0;
  remain = irlist_get_head(&list);
  if (remain != NULL)
    lastrtime = remain->left;

  /* if we are sending more than allowed, we need to skip the difference */
  for (i=0; i<irlist_size(&gdata.trans) - gdata.slotsmax; i++) {
    remain = next_remaining_transfer(remain);
    if (remain == NULL)
      break;
    lastrtime = remain->left;
  }

  updatecontext();
  i = 0;
  for (pq = irlist_get_head(&gdata.mainqueue);
       pq;
       pq = irlist_get_next(pq)) {
    i ++;
    rtime = lastrtime;
    remain = next_remaining_transfer(remain);
    if (remain != NULL)
      lastrtime = remain->left;
    speed = guess_maxspeed(pq->xpack);
    left = min2(359999, (pq->xpack->st_size)/((int)(max2(speed, 0.001)*1024)));
    lastrtime += left;

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
                irlist_size(&gdata.mainqueue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999U) ? "more" : "less",
                ntime);
  }

  i = 0;
  for (pq = irlist_get_head(&gdata.idlequeue);
       pq;
       pq = irlist_get_next(pq)) {
    i ++;
    rtime = lastrtime;
    remain = next_remaining_transfer(remain);
    if (remain != NULL)
      lastrtime = remain->left;
    speed = guess_maxspeed(pq->xpack);
    left = min2(359999, (pq->xpack->st_size)/((int)(max2(speed, 0.001)*1024)));
    lastrtime += left;

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
                irlist_size(&gdata.idlequeue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999U) ? "more" : "less",
                ntime);
    break;
  }

  irlist_delete_all(&list);
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
  res_init();
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
  rehash_myruby();
#endif /* USE_RUBY */
  res_init();
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
          "Send: %s to %s bytes=%" LLPRINTFMT "u",
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

  file = get_basename(xd->file);
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

static size_t get_channel_limit(const char *dest)
{
  channel_t *ch;
  size_t max = 400;

  if (dest && (*dest=='#')) {
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if (strcasecmp(ch->name, dest) != 0)
        continue;

      if (ch->fish)
        max /= 3;
      break;
    }
  }
  return max;
}

/* iroffer-lamm: @find and long !list */
int noticeresults(const char *nick, const char *pattern, const char *dest)
{
  int i, j, k, len;
  const char *grouplist;
  char *tempstr = mycalloc(maxtextlength);
  char *match;
  char *sizestrstr;
  xdcc *xd;

  /* apply per-channel visibility rules */
  grouplist = get_grouplist_channel(dest);
  match = grep_to_fnmatch(pattern);
  len = k = 0;

    i = 0;
    for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd)) {
      i++;
      if (hide_pack(xd))
        continue;

      /* check group visibility rules */
      if (!verify_group_in_grouplist(xd->group, grouplist))
        continue;

      if (fnmatch_xdcc(match, xd)) {
        if (!k) {
          if (gdata.slotsmax - irlist_size(&gdata.trans) < 0)
            j = irlist_size(&gdata.trans);
          else
            j = gdata.slotsmax;
          snprintf(tempstr, maxtextlength - 1, "XDCC SERVER - %s:[%i/%i]", j != 1 ? "Slots" : "Slot", j - irlist_size(&gdata.trans), j);
          len = strlen(tempstr);
          if (gdata.slotsmax <= irlist_size(&gdata.trans)) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Queue:[%i/%i]", irlist_size(&gdata.mainqueue), gdata.queuesize);
            len = strlen(tempstr);
          }
          if (gdata.transferminspeed > 0) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Min:%1.1fKB/s", gdata.transferminspeed);
            len = strlen(tempstr);
          }
          if (gdata.transfermaxspeed > 0) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Max:%1.1fKB/s", gdata.transfermaxspeed);
            len = strlen(tempstr);
          }
          if (gdata.maxb) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Cap:%u.0KB/s", gdata.maxb / 4);
            len = strlen(tempstr);
          }
          snprintf(tempstr + len, maxtextlength - 1 - len, " - /MSG %s XDCC SEND x -",
                   get_user_nick());
          len = strlen(tempstr);
          if (!strcmp(match, "*"))
            snprintf(tempstr + len, maxtextlength - 1 - len, " Packs:");
          else
            snprintf(tempstr + len, maxtextlength - 1 - len, " Found:");
        len = strlen(tempstr);
      }
      sizestrstr = sizestr(0, xd->st_size);
      snprintf(tempstr + len, maxtextlength - 1 - len, " #%i:%s,%s", i, xd->desc, sizestrstr);
      if (strlen(tempstr) > get_channel_limit(dest)) {
        snprintf(tempstr + len, maxtextlength - 1 - len, " [...]");
        notice_slow(nick, "%s", tempstr);
        snprintf(tempstr, maxtextlength - 2, "[...] #%i:%s,%s", i, xd->desc, sizestrstr);
      }
      len = strlen(tempstr);
      mydelete(sizestrstr);
      k++;
      /* limit matches */
      if ((gdata.max_find != 0) && (k >= gdata.max_find))
        break;
      }
    }

  mydelete(match);

  if (k)
    notice_slow(nick, "%s", tempstr);
  mydelete(tempstr);
  return k;
}

static void add_newest_xdcc(irlist_t *list, const char *grouplist)
{
  xdcc **best;
  xdcc *xd;
  xdcc *old;

  old = NULL;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (hide_pack(xd) != 0)
      continue;

    /* check group visibility rules */
    if (!verify_group_in_grouplist(xd->group, grouplist))
      continue;

    for (best = irlist_get_head(list);
         best;
         best = irlist_get_next(best)) {
      if (*best == xd)
        break;
    }
    if (best != NULL)
      continue;

    if (old != NULL) {
      if (old->xtime > xd->xtime)
        continue;
    }
    old = xd;
  }
  if (old == NULL)
    return;

  best = irlist_add(list, sizeof(void *));
  *best = old;
}

int run_new_trigger(const char *nick, const char *grouplist)
{
  struct tm *localt = NULL;
  irlist_t list;
  xdcc **best;
  xdcc *xd;
  const char *format;
  char *tempstr;
  time_t now;
  ssize_t llen;
  int i;

  format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";

  memset(&list, 0, sizeof(irlist_t));
  for (i=0; i<gdata.new_trigger; i++)
    add_newest_xdcc(&list, grouplist);

  i = 0;
  for (best = irlist_get_head(&list);
       best;
       best = irlist_delete(&list, best)) {
    xd = *best;
    now = xd->xtime;
    localt = localtime(&now);
    tempstr = mycalloc(maxtextlengthshort);
    llen = strftime(tempstr, maxtextlengthshort - 1, format, localt);
    if (llen == 0)
      tempstr[0] = '\0';
    notice_slow(nick, "Added: %s \2%i\2%s%s",
                tempstr, number_of_pack(xd), gdata.announce_seperator, xd->desc);
    mydelete(tempstr);
    i++;
  }
  return i;
}

char* getpart_eol(const char *line, int howmany)
{
  char *part;
  int li;
  size_t plen;
  int hi;

  li=0;

  for (hi = 1; hi < howmany; hi++) {
    while (line[li] != ' ') {
      if (line[li] == '\0')
        return NULL;
      li++;
    }
    li++;
  }

  if (line[li] == '\0')
      return NULL;

  plen = strlen(line+li);
  part = mycalloc(plen+1);
  memcpy(part, line+li, plen);
  part[plen] = '\0';

  return part;
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
  if (statfs(dpath, &stf) < 0) {
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
           "disk_free= %" LLPRINTFMT "u, required= %" LLPRINTFMT "u",
           (long long)freebytes, (long long)gdata.uploadminspace);

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

void identify_needed(int force)
{
  char *pwd;

  pwd = get_nickserv_pass();
  if (pwd == NULL)
    return;

  if (force == 0) {
    if ((gnetwork->next_identify > 0) && (gnetwork->next_identify >= gdata.curtime))
      return;
  }
  /* wait 1 sec before idetify again */
  gnetwork->next_identify = gdata.curtime + 1;
  writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :IDENTIFY %s", "nickserv", pwd);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "IDENTIFY send to nickserv on %s.", gnetwork->name);
}

void identify_check(const char *line)
{
  char *pwd;

  pwd = get_nickserv_pass();
  if (pwd == NULL)
    return;

  if (strstr(line, "Nickname is registered to someone else") != NULL) {
      identify_needed(0);
  }
  if (strstr(line, "This nickname has been registered") != NULL) {
      identify_needed(0);
  }
  if (strstr(line, "This nickname is registered and protected") != NULL) {
      identify_needed(0);
  }
  if (strcasestr(line, "please choose a different nick") != NULL) {
      identify_needed(0);
  }
}

static void restrictprivlistmsg(const char *nick)
{
  if (gdata.restrictprivlistmsg) {
    notice(nick, "** XDCC LIST Denied. %s", gdata.restrictprivlistmsg);
  } else {
    notice(nick, "** XDCC LIST Denied. Wait for the public list in the channel.");
  }
}

static int access_need_level(const char *nick, const char *text)
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

int parse_xdcc_list(const char *nick, char*msg3)
{
  xlistqueue_t *user;

  if (gdata.restrictprivlist) {
    restrictprivlistmsg(nick);
    return 2; /* deny */
  }
  if (gdata.restrictlist && access_need_level(nick, "LIST")) {
    return 2; /* deny */
  }
  for (user = irlist_get_head(&(gnetwork->xlistqueue));
       user;
       user = irlist_get_next(user)) {
    if (!strcmp(user->nick, nick))
      return 1; /* ignore */
  }
  if (irlist_size(&(gnetwork->xlistqueue)) >= MAXXLQUE) {
    notice(nick, "** XDCC LIST Denied. I'm rather busy at the moment, try again later");
    return 1; /* ignore */
  }
  if (!msg3) {
    if (gdata.restrictprivlistmain) {
      restrictprivlistmsg(nick);
      return 2; /* deny */
    }
    user = irlist_add(&(gnetwork->xlistqueue), sizeof(xlistqueue_t));
    user->nick = mystrdup(nick);
    return 3; /* queued */
  }
  if (strcmp(caps(msg3), "ALL") == 0) {
    if (gdata.restrictprivlistfull) {
      restrictprivlistmsg(nick);
      return 2; /* deny */
    }
    user = irlist_add(&(gnetwork->xlistqueue), sizeof(xlistqueue_t));
    user->nick = mystrdup(nick);
    user->msg = mystrdup(msg3);
    return 3; /* queued */
  }
  user = irlist_add(&(gnetwork->xlistqueue), sizeof(xlistqueue_t));
  user->nick = mystrdup(nick);
  user->msg = mystrdup(msg3);
  return 3; /* queued */
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

xdcc *get_xdcc_pack(int pack)
{
  if (pack == -1)
    return &xdcc_listfile;

  return irlist_get_nth(&gdata.xdccs, pack - 1);
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

int packnumtonum(const char *a)
{
  autoqueue_t *aq;
  autotrigger_t *at;

  if (!a) return 0;

  if (a[0] == '#') {
    a++;
    return atoi(a);
  }
  if (gdata.send_listfile) {
    if (strcasecmp(a, "LIST") == 0)
      return gdata.send_listfile;
  }

  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_get_next(aq)) {
    if (!strcasecmp(a, aq->word))
      return aq->pack;
  }
  for (at = irlist_get_head(&gdata.autotrigger);
       at;
       at = irlist_get_next(at)) {
    if (!strcasecmp(a, at->word))
      return number_of_pack(at->pack);
  }
  return atoi(a);
}

int check_trigger(const char *line, int type, const char *nick, const char *hostmask, const char *msg)
{
  autoqueue_t *aq;
  autotrigger_t *at;

  if (type == 0)
    return 0;

  if (!msg)
    return 0;

  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_get_next(aq)) {
    if (!strcasecmp(msg, aq->word)) {
      /* add/increment ignore list */
      if (check_ignore(nick, hostmask))
        return 0;

      autoqueuef(line, aq->pack, aq->message);
      /* only first match is activated */
      return 1;
    }
  }
  for (at = irlist_get_head(&gdata.autotrigger);
       at;
       at = irlist_get_next(at)) {
    if (!strcasecmp(msg, at->word)) {
      /* add/increment ignore list */
      if (check_ignore(nick, hostmask))
        return 0;

      autoqueuef(line, number_of_pack(at->pack), NULL);
      /* only first match is activated */
      return 1;
    }
  }
  /* nothing found */
  return 0;
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
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr, maxtextlength - 1, "%1.1fKB/s",
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

      len += snprintf(tempstr + len, maxtextlength - 1 - len, " %s", ch->rgroup);
    }
  }
  return tempstr;
}

/* search for custom listmsg */
const char *get_listmsg_channel(const char *dest)
{
  channel_t *ch;
  char *rtclmsg = gdata.respondtochannellistmsg;

  if (dest && (*dest=='#')) {
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if (strcasecmp(ch->name, dest) != 0)
        continue;

      if (ch->listmsg)
        rtclmsg = ch->listmsg;
      break;
    }
  }
  return rtclmsg;
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

int verifyhost_group(const char *hostmask)
{
  group_admin_t *ga;

  updatecontext();

  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_get_next(ga)) {

    if (fnmatch(ga->g_host, hostmask, FNM_CASEFOLD) == 0)
      return 1;
  }
  return 0;
}

group_admin_t *verifypass_group(const char *hostmask, const char *passwd)
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

/* End of File */
