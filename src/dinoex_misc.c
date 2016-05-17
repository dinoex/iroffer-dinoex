/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2013 Dirk Meyer
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

#if defined(WITH_RESINIT)
#include <resolv.h>
#endif /* WITH_RESINIT */

xdcc xdcc_statefile;
xdcc xdcc_listfile;

/* returns true if pack should not be listed */
unsigned int hide_pack(const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;

  if (xd->lock == NULL)
    return 0;

  return 1;
}

/* returns the current number of the pack */
unsigned int number_of_pack(xdcc *pack)
{
  xdcc *xd;
  unsigned int n;

  updatecontext();

  if (pack == &xdcc_listfile)
    return XDCC_SEND_LIST;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++n;
    if (xd == pack)
      return n;
  }

  return 0;
}

static unsigned int get_level(void)
{
  if (gnetwork->need_level != 10)
    return gnetwork->need_level;

  return gdata.need_level;
}

static unsigned int get_voice(void)
{
  if (gnetwork->need_voice != 2)
    return gnetwork->need_voice;

  return gdata.need_voice;
}

/* returns true if prefix of user matches need_level */
unsigned int check_level(int prefix)
{
  unsigned int ii;
  unsigned int need_level;
  unsigned int level;

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
  for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_symbol); ++ii) {
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

/* get coorent free slots */
unsigned int slotsfree(void)
{
  unsigned int slots;
  unsigned int trans;

  slots = 0;
  trans = irlist_size(&gdata.trans);
  if (trans < gdata.slotsmax)
    slots = gdata.slotsmax - trans;
  return slots;
}

/* ckeck if we have any pack with a group */
void set_support_groups(void)
{
  xdcc *xd;

  gdata.support_groups = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL) {
      ++(gdata.support_groups);
      return;
    }
  }
}

typedef struct {
  transfer *tr;
  int left;
  int dummy;
} remaining_transfer_time;

static irlist_t end_trans;
static remaining_transfer_time *end_remain;
static unsigned long end_lastrtime;

/* calculate next transfer time */
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

/* add up the time for a queued pack */
void add_new_transfer_time(xdcc *xd)
{
  float speed = 0.0;
  unsigned int nolimit = 1;
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

/* calculate the time for the current running traansfers to end */
void guess_end_transfers(void)
{
  transfer *tr;
  irlist_t list2;
  remaining_transfer_time *remain;
  remaining_transfer_time *rm;
  remaining_transfer_time *rmlast;
  int left;
  unsigned int i;

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
  for (i=irlist_size(&gdata.trans); i > gdata.slotsmax; --i) {
    get_next_transfer_time();
  }
}

/* reset calculated times */
void guess_end_cleanup(void)
{
  irlist_delete_all(&end_trans);
}

static unsigned int notifyqueued_queue(irlist_t *list, const char *nick, const char *ntime, unsigned int idle)
{
  ir_pqueue *pq;
  unsigned long rtime;
  unsigned int i = 0;
  unsigned int found = 0;

  updatecontext();
  i = 0;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    ++i;
    rtime = get_next_transfer_time();
    add_new_transfer_time(pq->xpack);

    if (pq->net != gnetwork->net)
      continue;

    if (nick != NULL) {
      if (strcasecmp(pq->nick, nick) != 0)
        continue;
    }

    ++found;
    if (idle) {
      /* report only the first pack found */
      if (found > 1)
        continue;
    }
    ioutput(OUT_S|OUT_D, COLOR_YELLOW,
            "Notifying Queued status to %s on %s",
            pq->nick, gnetwork->name);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %u of %u. %lih%lim or %s remaining. (at %s)",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(list),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999U) ? "more" : "less",
                ntime);
  }
  return found;
}

/* infom the user of each pack in main queue and the first pack in idle queue */
unsigned int notifyqueued_nick(const char *nick)
{
  struct tm *localt;
  char ntime[ 16 ];
  unsigned int found = 0;

  updatecontext();

  localt = localtime(&gdata.curtime);
  strftime(ntime, sizeof(ntime) - 1, "%H:%M", localt); /* NOTRANSLATE */

  guess_end_transfers();
  found += notifyqueued_queue(&gdata.mainqueue, nick, ntime, 0);
  found += notifyqueued_queue(&gdata.idlequeue, nick, ntime, 1);
  guess_end_cleanup();
  return found;
}

/* notify all queued users */
void notifyqueued(void)
{
  unsigned int found;

  updatecontext();

  if (gdata.exiting)
    return;

  if (irlist_size(&gdata.mainqueue) == 0)
    return;

  found = notifyqueued_nick(NULL);
  if (found == 0)
    return;

  ioutput(OUT_S|OUT_D, COLOR_YELLOW,
          "Notifying %u Queued Packs on %s",
          found, gnetwork->name);
}

static long dinoex_nexthour;

/* clear all data of a pack */
static void init_xdcc(xdcc *xd)
{
  bzero((char *)xd, sizeof(xdcc));
  xd->file_fd = FD_UNUSED;
}

#if defined(WITH_RESINIT)
/* res_init with checks */
static void res_init_dinoex(void)
{
  int rc;

  rc = res_init();
  if (rc == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD,
           "res_init() failed with %d: %s",
           rc, strerror(errno));
}
#endif /* WITH_RESINIT */


/* initializes sub systems prior config */
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
  gdata.md5build.file_fd = FD_UNUSED;
  config_startup();
  init_xdcc(&xdcc_statefile);
  init_xdcc(&xdcc_listfile);
  dinoex_nexthour = 0;
#ifdef USE_CURL
  curl_startup();
#endif /* USE_CURL */
  ssl_startup();
#if defined(WITH_RESINIT)
  res_init_dinoex();
#endif /* WITH_RESINIT */
}

/* copy global settings into each network */
static void global_defaults(void)
{
  gnetwork_t *backup;
  unsigned int ss;

  if (!gdata.group_seperator)
    gdata.group_seperator = mystrdup(" "); /* NOTRANSLATE */
  if (!gdata.announce_seperator)
    gdata.announce_seperator = mystrdup(" "); /* NOTRANSLATE */

  if (gdata.usenatip) {
    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      if (gnetwork->usenatip != 0)
        continue;
      update_natip(gnetwork->natip ? gnetwork->natip : gdata.usenatip);
    }
    gnetwork = backup;
  }
}

/* initializes sub systems after config */
void config_dinoex(void)
{
#ifdef USE_RUBY
  startup_myruby();
#endif /* USE_RUBY */
#ifdef USE_UPNP
  if (gdata.upnp_router || gdata.getipfromupnp)
    init_upnp();
#endif /* USE_UPNP */
#ifndef WITHOUT_TELNET
  telnet_setup_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_setup_listen();
#endif /* WITHOUT_HTTP */
  global_defaults();
}

/* shutdown sub systems */
void shutdown_dinoex(void)
{
#ifndef WITHOUT_TELNET
  telnet_close_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_close_listen();
#endif /* WITHOUT_HTTP */
#ifdef USE_GEOIP
  geoip_shutdown();
#endif /* USE_GEOIP */
#ifdef USE_CURL
  curl_shutdown();
#endif /* USE_CURL */
#ifdef USE_RUBY
  shutdown_myruby();
#endif /* USE_RUBY */
}

/* do a reahsh for all sub systems */
void rehash_dinoex(void)
{
#ifndef WITHOUT_TELNET
  telnet_reash_listen();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
  h_reash_listen();
#endif /* WITHOUT_HTTP */
  global_defaults();
#ifdef USE_GEOIP
  geoip_shutdown();
#endif /* USE_GEOIP */
#ifdef USE_RUBY
  rehash_myruby(0);
#endif /* USE_RUBY */
#if defined(WITH_RESINIT)
  res_init_dinoex();
#endif /* WITH_RESINIT */
}

/* init a pack with the information from disk */
unsigned int init_xdcc_file(xdcc *xd, char *file)
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
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Send: %s to %s bytes=%" LLPRINTFMT "d",
          file, nick, xd->st_size);
  tr = create_transfer(xd, nick, hostname);
  t_setup_dcc(tr);
  return 0;
}

/* send statefile once a hour */
void update_hour_dinoex(unsigned int minute)
{
  xdcc *xd;
  transfer *tr;
  unsigned int lastminute;
  unsigned int net = 0;

  if (!gdata.send_statefile)
    return;

  updatecontext();

  if (gdata.curtime < dinoex_nexthour)
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

  if (gnetwork->botstatus != BOTSTATUS_JOINED)
     return;

 /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gnetwork->lastservercontact >= 150)
     return;

  xd = &xdcc_statefile;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (xd == tr->xpack)
      return;
  }
  if (send_xdcc_file(&xdcc_statefile, gdata.statefile, gdata.send_statefile, "man") == 0) /* NOTRANSLATE */
    dinoex_nexthour = gdata.curtime + (60 * 30);
}

/* check if a pack matches a fnmatch search */
unsigned int fnmatch_xdcc(const char *match, xdcc *xd)
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

/* check for disk full on uploads */
unsigned int disk_full(const char *path)
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
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Unable to determine device sizes: %s",
            strerror(errno));
  } else {
    freebytes = (off_t)stf.f_bavail * (off_t)stf.f_frsize;
  }
#else /* NO_STATVFS */
#ifndef NO_STATFS
  if (statfs(path, &stf) < 0) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Unable to determine device sizes: %s",
            strerror(errno));
  } else {
    freebytes = (off_t)stf.f_bavail * (off_t)stf.f_bsize;
  }
#endif /* NO_STATFS */
#endif /* NO_STATVFS */

  if (gdata.debug > 0)
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
           "disk_free= %" LLPRINTFMT "d, required= %" LLPRINTFMT "d",
           freebytes, gdata.uploadminspace);

  if (freebytes >= gdata.uploadminspace)
    return 0;

  return 1;
}

/* get user_modes for current network */
char *get_user_modes(void)
{
  return (gnetwork->user_modes) ? gnetwork->user_modes : gdata.user_modes;
}

/* get get_nickserv_pass for current network */
char *get_nickserv_pass(void)
{
  return (gnetwork->nickserv_pass) ? gnetwork->nickserv_pass : gdata.nickserv_pass;
}

/* get restrictsend for current network */
unsigned int get_restrictsend(void)
{
  return (gnetwork->restrictsend != 2) ? gnetwork->restrictsend : gdata.restrictsend;
}

static unsigned int get_xdcc_oldpack = 0;
static xdcc *get_xdcc_oldxdcc = NULL;

/* get pack by number, pack -1 is the xdcc_listfile */
xdcc *get_xdcc_pack(unsigned int pack)
{
  xdcc *xd;
  unsigned int oldpack;
  unsigned int minus;

  oldpack = get_xdcc_oldpack;
  if (pack == oldpack)
    return get_xdcc_oldxdcc;

  if (pack == 0) {
    get_xdcc_oldpack = 0;
    return NULL;
  }

  if (pack == XDCC_SEND_LIST)
    return &xdcc_listfile;

  get_xdcc_oldpack = pack;
  minus = pack - 1;
  if ((minus == oldpack) && (oldpack > 0)) {
    xd = irlist_get_next(get_xdcc_oldxdcc);
  } else {
    xd = irlist_get_nth(&gdata.xdccs, minus);
  }
  get_xdcc_oldxdcc = xd;
  return xd;
}

static const char *access_need_text(void)
{
  unsigned int need_level;

  need_level = get_level();
  if (need_level == 0)
    need_level = get_voice();

  switch (need_level) {
  case 0:
    return NULL;
  case 1:
    return "voice"; /* NOTRANSLATE */
  case 2:
    return "halfop"; /* NOTRANSLATE */
  case 3:
    return "op"; /* NOTRANSLATE */
  default:
    return NULL;
  }
}

/* check level and inform the user which level he needs */
unsigned int access_need_level(const char *nick, const char *text)
{
  const char *level;

  if (!isinmemberlist(nick)) {
    level = access_need_text();
    if (level != NULL)
      notice(nick, "** XDCC %s denied, you must have %s on a known channel to request a pack", text, level);
    else
      notice(nick, "** XDCC %s denied, you must be on a known channel to request a pack", text);
    return 1;
  }
  return 0;
}

/* write a line with timestamp to a logfile */
void logfile_add(const char *logfile, const char *line)
{
  char tempstr[maxtextlengthshort];
  char logline[maxtextlength];
  size_t len;
  int logfd;

  if (logfile == NULL)
    return;

  logfd = open_append_log(logfile, "Log");
  if (logfd < 0)
    return;

  getdatestr(tempstr, 0, maxtextlengthshort);
  len = add_snprintf(logline, maxtextlength, "** %s: %s\n", tempstr, line);
  mylog_write(logfd, logfile, logline, len);
  mylog_close(logfd, logfile);
}

/* clculate current bandwidth as text */
char *get_current_bandwidth(void)
{
  char *tempstr;
  ir_uint64 xdccsent;
  unsigned int i;

  for (i=0, xdccsent=0; i<XDCC_SENT_SIZE; ++i) {
    xdccsent += (ir_uint64)gdata.xdccsent[i];
  }
  tempstr = mymalloc(maxtextlengthshort);
  snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s",
           ((float)xdccsent) / XDCC_SENT_SIZE / 1024.0);
  return tempstr;
}

/* inform the user a trasferlimit has reahed */
char *transfer_limit_exceeded_msg(unsigned int ii)
{
   char *tempstr = mymalloc(maxtextlength);
   char *tempstr2 = mymalloc(maxtextlengthshort);

   user_getdatestr(tempstr2, gdata.transferlimits[ii].ends, maxtextlengthshort);
   snprintf(tempstr, maxtextlength,
            "Sorry, I have exceeded my %s transfer limit of %" LLPRINTFMT "uMB.  Try again after %s.",
            transferlimit_type_to_string(ii),
            gdata.transferlimits[ii].limit / 1024U / 1024U,
            tempstr2);
   mydelete(tempstr2);
   return tempstr;
}

/* returns list of allowed groups for nick on current network, or NULL for unrestricted access. calling function must take care of freeing result */
char *get_grouplist_access(const char *nick)
{
  char *tempstr = mymalloc(maxtextlength);
  size_t len = 0;
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

      len += add_snprintf(tempstr + len, maxtextlength - len, " %s", ch->rgroup); /* NOTRANSLATE */
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

static unsigned int verify_bits(unsigned int bits, const unsigned char *data1, const unsigned char *data2)
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

/* chek if socket if it matches a list of networks */
unsigned int verify_cidr(irlist_t *list, const ir_sockaddr_union_t *remote)
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

/* chek if socket if it matches a list of networks */
void add_newest_xdcc(irlist_t *list, const char *grouplist)
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

static const char *style_on[ 16 ] = {
  /* 0 =*/ "", /* NOTRANSLATE */
  /* 1 =*/ IRC_BOLD,
  /* 2 =*/           IRC_UNDERLINE,
  /* 3 =*/ IRC_BOLD  IRC_UNDERLINE,
  /* 4 =*/                          IRC_ITALIC,
  /* 5 =*/ IRC_BOLD                 IRC_ITALIC,
  /* 6 =*/           IRC_UNDERLINE  IRC_ITALIC,
  /* 7 =*/ IRC_BOLD  IRC_UNDERLINE  IRC_ITALIC,
  /* 8 =*/                                      IRC_INVERSE,
  /* 9 =*/ IRC_BOLD                             IRC_INVERSE,
  /* 10 */           IRC_UNDERLINE              IRC_INVERSE,
  /* 11 */ IRC_BOLD  IRC_UNDERLINE              IRC_INVERSE,
  /* 12 */                          IRC_ITALIC  IRC_INVERSE,
  /* 13 */ IRC_BOLD                 IRC_ITALIC  IRC_INVERSE,
  /* 14 */           IRC_UNDERLINE  IRC_ITALIC  IRC_INVERSE,
  /* 15 */ IRC_BOLD  IRC_UNDERLINE  IRC_ITALIC  IRC_INVERSE
};

/* colored text */
char *color_text(char *desc, unsigned int color)
{
   char *colordesc;
   unsigned int foreground;
   unsigned int background;
   unsigned int style;

   if (color == 0)
     return desc;

   if (gnetwork->plaintext)
     return desc;

   colordesc = mymalloc(maxtextlength);
   foreground = color & 0xFF;
   background = (color >> 8) & 0xFF;
   style = (color >> 16 ) & 0x0F;
   if (background != 0) {
     snprintf(colordesc, maxtextlength, "%s" IRC_COLOR "%02u,%02u%s" IRC_COLOR IRC_NORMAL, style_on[ style ], foreground, background, desc); /* NOTRANSLATE */
     return colordesc;
   }
   if (foreground != 0) {
     snprintf(colordesc, maxtextlength, "%s" IRC_COLOR "%02u%s" IRC_COLOR IRC_NORMAL, style_on[ style ], foreground, desc); /* NOTRANSLATE */
     return colordesc;
   }
   snprintf(colordesc, maxtextlength, "%s%s" IRC_NORMAL, style_on[ style ], desc); /* NOTRANSLATE */
   return colordesc;
}

/* colored description of a pack */
char *xd_color_description(const xdcc *xd)
{
   return color_text(xd->desc, xd->color);
}


/* verify password for group admins */
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

/* show connectionmethod in text */
const char *text_connectionmethod(how_e how)
{
  switch (how) {
  case how_direct:
    return "direct"; /* NOTRANSLATE */
  case how_ssl:
    return "ssl"; /* NOTRANSLATE */
  case how_bnc:
    return "bnc"; /* NOTRANSLATE */
  case how_wingate:
    return "wingate"; /* NOTRANSLATE */
  case how_custom:
    return "custom"; /* NOTRANSLATE */
  }
  return "unknown";
}

/* show style of plist in text */
const char *text_pformat(unsigned int val)
{
  if ((val & CHAN_MINIMAL) != 0)
    return "minimal"; /* NOTRANSLATE */
  if ((val & CHAN_SUMMARY) != 0)
    return "summary"; /* NOTRANSLATE */
  return "full"; /* NOTRANSLATE */
}

/* free all strings from userinput*/
void free_userinput(userinput * const u)
{
  mydelete(u->snick);
  mydelete(u->hostmask);
  mydelete(u->cmd);
  mydelete(u->arg1e);
  mydelete(u->arg2e);
  mydelete(u->arg3e);
  mydelete(u->arg1);
  mydelete(u->arg2);
  mydelete(u->arg3);
}

/* drop all delayed actions */
void free_delayed(void)
{
  userinput *u;

  for (u = irlist_get_head(&gdata.packs_delayed);
       u;
       u = irlist_delete(&gdata.packs_delayed, u)) {
     free_userinput(u);
  }
}

/* drop all strings from a channel */
void free_channel_data(channel_t *ch)
{
  mydelete(ch->name);
  mydelete(ch->key);
  mydelete(ch->fish);
  mydelete(ch->pgroup);
  mydelete(ch->joinmsg);
  mydelete(ch->listmsg);
  mydelete(ch->rgroup);
  irlist_delete_all(&(ch->headline));
}

/* check if config files have changed and start a rehash */
void auto_rehash(void)
{
  struct stat st;
  int filedescriptor;
  unsigned int h;

  updatecontext();

  if (gdata.no_auto_rehash)
    return;

  for (h=0; h<MAXCONFIG && gdata.configfile[h]; ++h) {
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
    ++(gdata.needsrehash);
  }
}

/* convert to ASCII char */
static unsigned int onlyprintable(unsigned int a)
{
  if ( (unsigned char)a < 0x20U )
     return '.';
  if ( (unsigned char)a == 0x7FU )
     return '.';
  return a;
}

/* generate an hexdump from a buffer with given length */
void hexdump(int dest, unsigned int color_flags, const char *prefix, void *t, size_t max)
{
  char buffer[maxtextlength];
  unsigned char *ut = (unsigned char *)t;
  size_t len;
  unsigned int j;

  if (t == NULL) return;
  if (prefix == NULL) prefix = "";

  for (;;) {
    len = 0;
    for (j=0; j<16; ++j) {
      if (j >= max) {
        len += add_snprintf(buffer + len, maxtextlength - len, "   "); /* NOTRANSLATE */
      } else {
        len += add_snprintf(buffer + len, maxtextlength - len, " %2.2X", ut[j]); /* NOTRANSLATE */
      }
    }
    len += add_snprintf(buffer + len, maxtextlength - len, " \""); /* NOTRANSLATE */
    for (j=0; j<16; ++j) {
      if (j >= max)
        break;
      len += add_snprintf(buffer + len, maxtextlength - len, "%c", onlyprintable(ut[j])); /* NOTRANSLATE */
    }
    len += add_snprintf(buffer + len, maxtextlength - len, "\""); /* NOTRANSLATE */
    ioutput(dest, color_flags, "%s%s", prefix, buffer); /* NOTRANSLATE */
    if (max <= 16)
      break;
    j += 16;
    ut += 16;
    max -= 16;
  }
}

/* find a pack by its CRC32 */
static unsigned int find_pack_crc(const char *crc)
{
  xdcc *xd;
  char crctext[32];
  unsigned int num;
  size_t max;

  max = strlen(crc);
  max = min2(max, 8);
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (xd->has_crc32 == 0)
      continue;

    snprintf(crctext, sizeof(crctext), CRC32_PRINT_FMT, xd->crc32);
    if (strncmp(crctext, crc, max) == 0)
      return num;
  }
  return 0;
}

/* find a pack by a number, #number or trigger */
unsigned int packnumtonum(const char *a)
{
  autoqueue_t *aq;
  autotrigger_t *at;

  if (!a) return 0;

  if (a[0] == '[') {
    ++a;
    return find_pack_crc(a);
  }
  if (a[0] == '#') {
    ++a;
    return (unsigned)atoi(a);
  }
  if (gdata.send_listfile) {
    if (strcasecmp(a, "LIST") == 0) /* NOTRANSLATE */
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
  return (unsigned)atoi(a);
}

/* dump slow functions */
void dump_slow_context(void)
{
  char *lastline;
  context_t *c;
  context_t *last;
  ir_uint64 offset;
  unsigned int i;
  unsigned int show;

  last = NULL;
  lastline = mymalloc(maxtextlength);
  lastline[0] = 0;
  for (i=0; i<MAXCONTEXTS; ++i) {
    c = &gdata.context_log[(gdata.context_cur_ptr + 1 + i) % MAXCONTEXTS];
    if (c->file == NULL)
      continue;
    show = 0;
    if (last != NULL) {
      offset = c->tv.tv_sec - last->tv.tv_sec;
      offset *= 1000000;
      offset += c->tv.tv_usec - last->tv.tv_usec;
      if (offset > 171000) {
        ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "%s", lastline);
        show = 1;
      }
    }
    snprintf(lastline, maxtextlength,
             "Trace %3u  %-20s %-16s:%5u  %lu.%06lu",
             i-MAXCONTEXTS+1,
             c->func ? c->func : "UNKNOWN",
             c->file ? c->file : "UNKNOWN",
             c->line,
             (unsigned long)c->tv.tv_sec,
             (unsigned long)c->tv.tv_usec);
    last = c;
    if (show)
      ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "%s", lastline);
  }
  mydelete(lastline);
}

/* get options field by user */
dcc_options_t *get_options(const char *nick)
{
  dcc_options_t *dcc_options;

  updatecontext();
  if (gdata.debug > 10) {
    ioutput(OUT_S, COLOR_NO_COLOR, "checking for %s", nick);
  }

  for (dcc_options = irlist_get_head(&(gnetwork->dcc_options));
       dcc_options;
       dcc_options = irlist_get_next(dcc_options)) {
    if (strcasecmp(dcc_options->nick, nick) == 0) {
      dcc_options->last_seen = gdata.curtime; /* note in use */
      return dcc_options;
    }
  }
  return NULL;
}

/* End of File */
