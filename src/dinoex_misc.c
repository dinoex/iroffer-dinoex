/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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
#include "dinoex_misc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

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

  mynick = save_nick(gnetwork->user_nick);
  if (!gdata.restrictprivlist) {
    notice_slow(nick, "\2**\2 Request listing:   \"/MSG %s XDCC LIST\" \2**\2", mynick);
    if (gdata.support_groups)
      notice_slow(nick, "\2**\2 Request listing:   \"/MSG %s XDCC LIST group\" \2**\2", mynick);
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

void stoplist(const char *nick)
{
  char *item;
  char *copy;
  char *end;
  char *inick;
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

  for (item = irlist_get_head(&(gnetwork->serverq_slow)); item; ) {
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
            item = irlist_delete(&(gnetwork->serverq_slow), item);
            continue;
          }
        }
      }
    }
    mydelete(copy);
    item = irlist_get_next(item);
  }
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " (stopped %d)", stopped);
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

void notifyqueued_nick(const char *nick)
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

  updatecontext();

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

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_YELLOW,
            "Notifying Queued status to %s",
            pq->nick);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(&gdata.mainqueue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999) ? "more" : "less");
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

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_YELLOW,
            "Notifying Queued status to %s",
            pq->nick);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(&gdata.idlequeue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999) ? "more" : "less");
  }

  irlist_delete_all(&list);
}

void check_new_connection(transfer *const tr)
{
  geoip_new_connection(tr);
  t_check_duplicateip(tr);
}

static xdcc xdcc_statefile;
static xdcc xdcc_listfile;
static int dinoex_lasthour;

static void init_xdcc(xdcc *xd)
{
  bzero((char *)xd, sizeof(xdcc));
  xd->note = mystrdup("");
  xd->file_fd = FD_UNUSED;
  xd->has_md5sum = 0;
  xd->has_crc32 = 0;
}

void startup_dinoex(void)
{
  gdata.support_groups = 0;
  config_startup();
  init_xdcc(&xdcc_statefile);
  init_xdcc(&xdcc_listfile);
  dinoex_lasthour = -1;
  curl_startup();
  ssl_startup();
}

static void global_defaults(void)
{
  gnetwork_t *backup;
  int ss;

  if (!gdata.group_seperator)
    gdata.group_seperator = mystrdup(" ");

  if (gdata.usenatip) {
    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[gdata.networks_online]);
      if (gnetwork->usenatip == 0)
        update_natip(gdata.usenatip);
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
  telnet_setup_listen();
  h_setup_listen();
  global_defaults();
}

void shutdown_dinoex(void)
{
  telnet_close_listen();
  h_close_listen();
  geoip_shutdown();
  curl_shutdown();
}

void rehash_dinoex(void)
{
  telnet_reash_listen();
  h_reash_listen();
  global_defaults();
  geoip_shutdown();
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

/* iroffer-lamm: @find and long !list */
int noticeresults(const char *nick, const char *match)
{
  int i, j, k, len;
  char *tempstr = mycalloc(maxtextlength);
  char *sizestrstr;
  char *tempr;
  regex_t *regexp = mycalloc(sizeof(regex_t));
  xdcc *xd;

  len = k = 0;

  tempr = hostmasktoregex(match);

  if (!regcomp(regexp, tempr, REG_ICASE | REG_NOSUB)) {
    i = 0;
    for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd)) {
      i++;
      if (hide_pack(xd))
        continue;
      if (!regexec(regexp, xd->file, 0, NULL, 0) ||
          !regexec(regexp, xd->desc, 0, NULL, 0) ||
          !regexec(regexp, xd->note, 0, NULL, 0)) {
        if (!k) {
          if (gdata.slotsmax - irlist_size(&gdata.trans) < 0)
            j = irlist_size(&gdata.trans);
          else
            j = gdata.slotsmax;
          snprintf(tempstr, maxtextlength - 1, "XDCC SERVER - Slot%s:[%i/%i]", j != 1 ? "s" : "", j - irlist_size(&gdata.trans), j);
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
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Cap:%i.0KB/s", gdata.maxb / 4);
            len = strlen(tempstr);
          }
          snprintf(tempstr + len, maxtextlength - 1 - len, " - /MSG %s XDCC SEND x -",
                   save_nick(gnetwork->user_nick));
          len = strlen(tempstr);
          if (!strcmp(match, "*"))
            snprintf(tempstr + len, maxtextlength - 1 - len, " Packs:");
          else
            snprintf(tempstr + len, maxtextlength - 1 - len, " Found:");
        len = strlen(tempstr);
      }
      sizestrstr = sizestr(0, xd->st_size);
      snprintf(tempstr + len, maxtextlength - 1 - len, " #%i:%s,%s", i, xd->desc, sizestrstr);
      if (strlen(tempstr) > 400) {
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
  }

  if (k)
    notice_slow(nick, "%s", tempstr);
  mydelete(tempr);
  mydelete(regexp);
  mydelete(tempstr);
  return k;
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
#endif
#endif
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
#else
#ifndef NO_STATFS
  if (statfs(dpath, &stf) < 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Unable to determine device sizes: %s",
            strerror(errno));
  } else {
    freebytes = (off_t)stf.f_bavail * (off_t)stf.f_bsize;
  }
#endif
#endif

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
  privmsg("nickserv", "IDENTIFY %s", pwd);
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
  if (strstr(line, "please choose a different nick") != NULL) {
      identify_needed(0);
  }
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

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text)
{
  updatecontext();

  if (check_manual_send(hostname, man) == 0) {
    if (!verifyhost(&gdata.downloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (verifyhost(&gdata.nodownloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (gdata.restrictsend && !isinmemberlist(nick)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (restricted): ");
      if ((gdata.need_voice != 0) || (gdata.need_level != 0))
        notice(nick, "** XDCC %s denied, you must have voice on a known channel to request a pack", text);
      else
        notice(nick, "** XDCC %s denied, you must be on a known channel to request a pack", text);
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
  return irlist_get_nth(&gdata.xdccs, pack-1);
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

void lost_nick(char *nick)
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

#ifdef DEBUG

static void free_state(void)
{
  xdcc *xd;
  channel_t *ch;
  transfer *tr;
  upload *up;
  ir_pqueue *pq;
  userinput *u;
  igninfo *i;
  msglog_t *ml;
  gnetwork_t *backup;
  xlistqueue_t *user;
  http *h;
  http_magic_t *mime;
  int ss;

  updatecontext();

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_delete(&gdata.xdccs, xd)) {
     mydelete(xd->file);
     mydelete(xd->desc);
     mydelete(xd->note);
     mydelete(xd->group);
     mydelete(xd->group_desc);
     mydelete(xd->lock);
     mydelete(xd->dlimit_desc);
     mydelete(xd->trigger);
  }

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    mydelete(gnetwork->curserveractualname);
    mydelete(gnetwork->user_nick);
    mydelete(gnetwork->caps_nick);
    mydelete(gnetwork->name);
    mydelete(gnetwork->curserver.hostname);
    mydelete(gnetwork->curserver.password);
    mydelete(gnetwork->connectionmethod.host);
    mydelete(gnetwork->connectionmethod.password);
    mydelete(gnetwork->connectionmethod.vhost);
    irlist_delete_all(&(gnetwork->xlistqueue));

    for (user = irlist_get_head(&(gnetwork->xlistqueue));
         user;
         user = irlist_delete(&(gnetwork->xlistqueue), user)) {
       mydelete(user->nick);
       mydelete(user->msg);
    }

    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_delete(&(gnetwork->channels), ch)) {
       clearmemberlist(ch);
       mydelete(ch->name);
       mydelete(ch->key);
       mydelete(ch->headline);
       mydelete(ch->pgroup);
    }
  }

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_delete(&gdata.trans, tr)) {
     mydelete(tr->nick);
     mydelete(tr->caps_nick);
     mydelete(tr->hostname);
     mydelete(tr->con.localaddr);
     mydelete(tr->con.remoteaddr);
  }

  for (up = irlist_get_head(&gdata.uploads);
       up;
       up = irlist_delete(&gdata.uploads, up)) {
     mydelete(up->nick);
     mydelete(up->hostname);
     mydelete(up->file);
     mydelete(tr->con.remoteaddr);
  }

  for (pq = irlist_get_head(&gdata.mainqueue);
       pq;
       pq = irlist_delete(&gdata.mainqueue, pq)) {
     mydelete(pq->nick);
     mydelete(pq->hostname);
  }
  for (pq = irlist_get_head(&gdata.idlequeue);
       pq;
       pq = irlist_delete(&gdata.idlequeue, pq)) {
     mydelete(pq->nick);
     mydelete(pq->hostname);
  }
  for (u = irlist_get_head(&gdata.packs_delayed);
       u;
       u = irlist_delete(&gdata.packs_delayed, u)) {
     mydelete(u->cmd);
     mydelete(u->arg1);
     mydelete(u->arg2);
  }
  for (i = irlist_get_head(&gdata.ignorelist);
       i;
       i = irlist_delete(&gdata.ignorelist, i)) {
     mydelete(i->regexp);
     mydelete(i->hostmask);
  }
  for (ml = irlist_get_head(&gdata.msglog);
       ml;
       ml = irlist_delete(&gdata.msglog, ml)) {
     mydelete(ml->hostmask);
     mydelete(ml->message);
  }
  for (h = irlist_get_head(&gdata.https);
       h;
       h = irlist_delete(&gdata.https, h)) {
     mydelete(h->file);
     mydelete(h->url);
     mydelete(h->authorization);
     mydelete(h->group);
     mydelete(h->order);
     mydelete(h->search);
     mydelete(h->pattern);
     mydelete(h->modified);
     mydelete(h->buffer_out);
     mydelete(h->con.remoteaddr);
  }
  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_delete(&gdata.mime_type, mime)) {
     mydelete(mime->m_ext);
  }
  irlist_delete_all(&gdata.autotrigger);
  irlist_delete_all(&gdata.console_history);
  irlist_delete_all(&gdata.jobs_delayed);
  irlist_delete_all(&gdata.http_bad_ip4);
  irlist_delete_all(&gdata.http_bad_ip6);
  mydelete(gdata.sendbuff);
  mydelete(gdata.console_input_line);
  mydelete(gdata.osstring);


  mydelete(xdcc_statefile.note);
  mydelete(xdcc_statefile.desc);
  mydelete(xdcc_listfile.note);
  mydelete(xdcc_listfile.desc);
}

static void free_config(void)
{
  autoqueue_t *aq;
  regex_t *rh;
  int si;

  updatecontext();
  /* clear old config items */
  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_delete(&gdata.autoqueue, aq))
    {
       mydelete(aq->word);
       mydelete(aq->message);
    }
  for (rh = irlist_get_head(&gdata.autoignore_exclude);
       rh;
       rh = irlist_delete(&gdata.autoignore_exclude, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.adminhost);
       rh;
       rh = irlist_delete(&gdata.adminhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.hadminhost);
       rh;
       rh = irlist_delete(&gdata.hadminhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.uploadhost);
       rh;
       rh = irlist_delete(&gdata.uploadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.downloadhost);
       rh;
       rh = irlist_delete(&gdata.downloadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.nodownloadhost);
       rh;
       rh = irlist_delete(&gdata.nodownloadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.unlimitedhost);
       rh;
       rh = irlist_delete(&gdata.unlimitedhost, rh))
    {
      regfree(rh);
    }
  mydelete(gdata.r_pidfile);
  mydelete(gdata.pidfile);
  mydelete(gdata.config_nick);
  for (si=0; si<MAX_NETWORKS; si++)
  {
    server_t *ss;
    for (ss = irlist_get_head(&gdata.networks[si].servers);
         ss;
         ss = irlist_delete(&gdata.networks[si].servers, ss))
      {
        mydelete(ss->hostname);
        mydelete(ss->password);
      }
    mydelete(gdata.networks[si].nickserv_pass);
    mydelete(gdata.networks[si].config_nick);
    mydelete(gdata.networks[si].user_modes);
    mydelete(gdata.networks[si].local_vhost);
    irlist_delete_all(&gdata.networks[si].r_channels);
    irlist_delete_all(&gdata.networks[si].server_join_raw);
    irlist_delete_all(&gdata.networks[si].server_connected_raw);
    irlist_delete_all(&gdata.networks[si].channel_join_raw);
    irlist_delete_all(&gdata.networks[si].proxyinfo);
  } /* networks */
  mydelete(gdata.logfile);
  mydelete(gdata.user_realname);
  mydelete(gdata.user_modes);
  irlist_delete_all(&gdata.adddir_exclude);
  irlist_delete_all(&gdata.geoipcountry);
  irlist_delete_all(&gdata.nogeoipcountry);
  irlist_delete_all(&gdata.geoipexcludenick);
  irlist_delete_all(&gdata.autoadd_dirs);
  irlist_delete_all(&gdata.autocrc_exclude);
  irlist_delete_all(&gdata.filedir);
  irlist_delete_all(&gdata.http_vhost);
  irlist_delete_all(&gdata.telnet_vhost);
  irlist_delete_all(&gdata.weblist_info);
  mydelete(gdata.enable_nick);
  mydelete(gdata.owner_nick);
  mydelete(gdata.geoipdatabase);
  mydelete(gdata.respondtochannellistmsg);
  mydelete(gdata.admin_job_file);
  mydelete(gdata.autoaddann);
  mydelete(gdata.autoadd_group);
  mydelete(gdata.send_statefile);
  mydelete(gdata.xdccremovefile);
  mydelete(gdata.autoadd_sort);
  mydelete(gdata.creditline);
  mydelete(gdata.headline);
  mydelete(gdata.nickserv_pass);
  mydelete(gdata.periodicmsg_nick);
  mydelete(gdata.periodicmsg_msg);
  mydelete(gdata.uploaddir);
  mydelete(gdata.restrictprivlistmsg);
  mydelete(gdata.loginname);
  mydelete(gdata.statefile);
  mydelete(gdata.xdcclistfile);
  mydelete(gdata.adminpass);
  mydelete(gdata.hadminpass);
  mydelete(gdata.http_admin);
  mydelete(gdata.http_dir);
  mydelete(gdata.group_seperator);
  mydelete(gdata.local_vhost);
  mydelete(gdata.usenatip);
  mydelete(gdata.logfile_notices);
  mydelete(gdata.logfile_messages);
  mydelete(gdata.trashcan_dir);
  mydelete(gdata.xdccxmlfile);
}

#endif

void exit_iroffer(void)
{
#ifdef DEBUG
  meminfo_t *mi;
  unsigned char *ut;
  int j;
  int i;
  int leak = 0;

  signal(SIGSEGV, SIG_DFL);
  free_config();
  free_state();
  updatecontext();

  for (j=1; j<(MEMINFOHASHSIZE * gdata.meminfo_depth); j++) {
    mi = &(gdata.meminfo[j]);
    ut = mi->ptr;
    if (ut != NULL) {
      leak ++;
      outerror(OUTERROR_TYPE_WARN_LOUD, "Pointer 0x%8.8lX not free", (long)ut);
      outerror(OUTERROR_TYPE_WARN_LOUD, "alloctime = %ld", (long)(mi->alloctime));
      outerror(OUTERROR_TYPE_WARN_LOUD, "size      = %ld", (long)(mi->size));
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_func  = %s", mi->src_func);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_file  = %s", mi->src_file);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_line  = %d", mi->src_line);
      for(i=0; i<(12*12); i+=12) {
        outerror(OUTERROR_TYPE_WARN_LOUD, " : %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X = \"%c%c%c%c%c%c%c%c%c%c%c%c\"",
               ut[i+0], ut[i+1], ut[i+2], ut[i+3], ut[i+4], ut[i+5], ut[i+6], ut[i+7], ut[i+8], ut[i+9], ut[i+10], ut[i+11],
               onlyprintable(ut[i+0]), onlyprintable(ut[i+1]),
               onlyprintable(ut[i+2]), onlyprintable(ut[i+3]),
               onlyprintable(ut[i+4]), onlyprintable(ut[i+5]),
               onlyprintable(ut[i+6]), onlyprintable(ut[i+7]),
               onlyprintable(ut[i+8]), onlyprintable(ut[i+9]),
               onlyprintable(ut[i+10]), onlyprintable(ut[i+11]));
      }
    }
  }
  if (leak == 0)
    exit(0);

  *((int*)(0)) = 0;
#else
  exit(0);
#endif
}

/* End of File */
