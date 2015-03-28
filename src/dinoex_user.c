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
#include "dinoex_upload.h"
#include "dinoex_transfer.h"
#include "dinoex_jobs.h"
#include "dinoex_queue.h"
#include "dinoex_chat.h"
#include "dinoex_misc.h"
#include "dinoex_user.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

typedef struct
{
  char *line;
  char *hostmask;
  char *nick;
  char *hostname;
  char *dest;
  char *msg1;
  char *msg2;
  char *msg3;
  char *msg4;
  char *msg5;
  char *msg6;
  char *msg7;
} privmsginput;

typedef struct {
  const char *name;
  unsigned int setval;
  unsigned int resetval;
} option_list_t;

static const char *type_list[2] = { "NOTICE", "PRIVMSG" }; /* NOTRANSLATE */

const static const option_list_t xdcc_option_list[] = {
  { "IPV4",    DCC_OPTION_IPV4,    ~DCC_OPTION_IPV4 }, /* NOTRANSLATE */
  { "IPV6",    DCC_OPTION_IPV6,    ~DCC_OPTION_IPV6 }, /* NOTRANSLATE */
  { "ACTIVE",  DCC_OPTION_ACTIVE , ~DCC_OPTION_ACTIVE }, /* NOTRANSLATE */
  { "PASSIVE", DCC_OPTION_PASSIVE, ~DCC_OPTION_PASSIVE }, /* NOTRANSLATE */
  { "QUIET",   DCC_OPTION_QUIET,   ~DCC_OPTION_QUIET }, /* NOTRANSLATE */
  { NULL, 0, 0 },
};

static void strip_trailing_action(char *str)
{
  size_t len;

  if (str == NULL)
    return;

  if (str[0] == 0)
    return;

  len = strlen(str);
  if (str[--len] != IRCCTCP)
    return;

  str[len] = '\0';
}

static int test_ctcp(const char *msg1, const char *key)
{
  size_t len;

  len = strlen(key);
  if (strncmp(msg1, key, len) != 0)
    return 0;

  if ((key[len] == 0) || (key[len] == IRCCTCP))
    return 1;

  return 0;
}

static void send_clientinfo(const char *nick, char *msg2)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "[CTCP] %s in %s: CLIENTINFO",
          nick, gnetwork->name);

  if (!msg2) {
    notice(nick, IRC_CTCP "CLIENTINFO DCC PING VERSION XDCC UPTIME " /* NOTRANSLATE */
    ":Use CTCP CLIENTINFO <COMMAND> to get more specific information" IRC_CTCP);
    return;
  }
  caps(msg2);
  if (strncmp(msg2, "PING", 4) == 0) {
    notice(nick, IRC_CTCP "CLIENTINFO PING returns the arguments it receives" IRC_CTCP);
    return;
  }
  if (strncmp(msg2, "DCC", 3) == 0) {
    notice(nick, IRC_CTCP "CLIENTINFO DCC requests a DCC for chatting or file transfer" IRC_CTCP);
    return;
  }
  if (strncmp(msg2, "VERSION", 7) == 0) {
    notice(nick, IRC_CTCP "CLIENTINFO VERSION shows information about this client's version" IRC_CTCP);
    return;
  }
  if (strncmp(msg2, "XDCC", 4) == 0) {
    notice(nick, IRC_CTCP "CLIENTINFO XDCC LIST|SEND list and DCC file(s) to you" IRC_CTCP);
    return;
  }
  if (strncmp(msg2, "UPTIME", 6) == 0) {
    notice(nick, IRC_CTCP "CLIENTINFO UPTIME shows how long this client has been running" IRC_CTCP);
    return;
  }
}

static int verifyhost_group(const char *hostmask)
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

static void log_chat_attempt(privmsginput *pi)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
         "DCC CHAT attempt authorized from %s (%s on %s)",
          pi->nick, pi->hostmask, gnetwork->name);
}

static void command_dcc(privmsginput *pi)
{
  if (strcmp(pi->msg2, "RESUME") == 0) { /* NOTRANSLATE */
    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL))
      return;

    t_find_resume(pi->nick, pi->msg3, pi->msg4, pi->msg5, pi->msg6);
    return;
  }

  if (strcmp(pi->msg2, "CHAT") == 0) { /* NOTRANSLATE */
    if (verifyshell(&gdata.adminhost, pi->hostmask)) {
      log_chat_attempt(pi);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    if (verifyshell(&gdata.hadminhost, pi->hostmask)) {
      log_chat_attempt(pi);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    if (verifyhost_group(pi->hostmask)) {
      log_chat_attempt(pi);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    notice(pi->nick, "DCC Chat denied from %s", pi->hostmask);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC CHAT attempt denied from %s (%s on %s)",
            pi->nick, pi->hostmask, gnetwork->name);
    return;
  }

  if (strcmp(pi->msg2, "SEND") == 0) { /* NOTRANSLATE */
    unsigned int down = 0;

    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL) || (pi->msg6 == NULL))
      return;

    if (pi->msg7) {
      down = t_find_transfer(pi->nick, pi->msg3, pi->msg4, pi->msg5, pi->msg7);
    }
    if (!down) {
      removenonprintablefile(pi->msg3);
      upload_start(pi->nick, pi->hostname, pi->hostmask, pi->msg3, pi->msg4, pi->msg5, pi->msg6, pi->msg7);
    }
    return;
  }

  if (strcmp(pi->msg2, "ACCEPT") == 0) { /* NOTRANSLATE */
    upload *ul;
    char *tempstr;
    off_t len;
    unsigned int port;

    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL))
      return;

    len = atoull(pi->msg5);
    if (invalid_upload(pi->nick, pi->hostmask, len))
      return;

    port = atoi(pi->msg4);
    for (ul = irlist_get_head(&gdata.uploads);
         ul;
         ul = irlist_get_next(ul)) {

      if (ul->con.remoteport != port)
        continue;

      if (strcmp(ul->nick, pi->nick) != 0)
        continue;

      ul->resumed = 1;
      tempstr = getsendname(ul->file);
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "DCC Send Resumed from %s on %s: %s (%" LLPRINTFMT "d of %" LLPRINTFMT "dkB left)",
              pi->nick, gnetwork->name, tempstr,
              ((ul->totalsize - ul->resumesize) / 1024),
              (ul->totalsize / 1024));
      mydelete(tempstr);
      if (ul->con.remoteport > 0U) {
        l_establishcon(ul);
      } else {
        /* Passive DCC */
        l_setup_passive(ul, pi->msg6 ? pi->msg6 : pi->msg4);
      }
      break;
    }

    if (!ul) {
      notice(pi->nick, "DCC Resume Denied, unable to find transfer");
      outerror(OUTERROR_TYPE_WARN,
               "Couldn't find upload that %s on %s tried to resume!",
               pi->nick, gnetwork->name);
    }
    return;
  }
  return;
}

/* get pack by number and returns an error string if it failks */
static xdcc *get_download_pack(const char **bad, const char* nick, const char* hostmask, unsigned int pack, const char* text, unsigned int restr)
{
  char *grouplist;
  xdcc *xd;

  updatecontext();

  if (!verifyshell(&gdata.downloadhost, hostmask)) {
    notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
    *bad = "Denied (host denied)";
    return NULL;
  }
  if (verifyshell(&gdata.nodownloadhost, hostmask)) {
    notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
    *bad = "Denied (host denied)";
    return NULL;
  }
  if (restr && access_need_level(nick, text)) {
    *bad = "Denied (restricted)";
    return NULL;
  }
  if (gdata.enable_nick && !isinmemberlist(gdata.enable_nick)) {
    notice(nick, "** XDCC %s denied, owner of this bot is not online", text);
    *bad = "Denied (offline)";
    return NULL;
  }

  if (pack == XDCC_SEND_LIST) {
    if ( (gdata.xdcclistfile) && (gdata.send_listfile != 0) ) {
      if (init_xdcc_file(&xdcc_listfile, gdata.xdcclistfile)) {
        *bad = "(Bad Pack Number)";
        return NULL;
      }
      *bad = NULL;
      return &xdcc_listfile;
    }
  }

  if ((pack > irlist_size(&gdata.xdccs)) || (pack < 1)) {
    notice(nick, "** Invalid Pack Number, Try Again");
    *bad = "(Bad Pack Number)";
    return NULL;
  }

  xd = get_xdcc_pack(pack);
  if (xd == NULL) {
    *bad = "(Bad Pack Number)";
    return NULL;
  }

  /* apply group visibility rules */
  if (restr) {
    grouplist = get_grouplist_access(nick);
    if (!verify_group_in_grouplist(xd->group, grouplist)) {
      notice(nick, "** XDCC %s denied, you must be on the correct channel to request this pack", text);
      mydelete(grouplist);
      *bad = "Denied (group access restricted)";
      return NULL;
    }
    mydelete(grouplist);
  }
  *bad = NULL;
  return xd;
}

/* returns true if password does not match the pack */
static int check_lock(const char* lockstr, const char* pwd)
{
  if (lockstr == NULL)
    return 0; /* no lock */
  if (pwd == NULL)
    return 1; /* locked */
  return strcmp(lockstr, pwd);
}

static unsigned int send_xdcc_file2(const char **bad, privmsginput *pi, unsigned int pack, const char *msg, const char *pwd, const char *filter)
{
  xdcc *xd;
  transfer *tr;
  char *tempstr;
  unsigned int usertrans;
  unsigned int fatal;

  updatecontext();

  xd = get_download_pack(bad, pi->nick, pi->hostmask, pack, "SEND", get_restrictsend()); /* NOTRANSLATE */
  if (xd == NULL)
    return 1;

  if (filter != NULL) {
    if (!fnmatch_xdcc(filter, xd)) {
      *bad = NULL; /* skipped */
      return 0;
    }
  }

  if (check_lock(xd->lock, pwd) != 0) {
    notice(pi->nick, "** XDCC SEND denied, this pack is locked");
    *bad = "Denied (pack locked)";
    return 1;
  }

  usertrans = 0;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (strcmp(tr->hostname, pi->hostname) != 0)
      continue;
    ++usertrans;
    if (xd == tr->xpack) {
      notice(pi->nick, "** You already requested that pack");
      *bad = "Denied (dup)";
      return 1;
    }
  }

  if (gdata.nonewcons > gdata.curtime) {
    notice(pi->nick, "** The Owner Has Requested That No New Connections Are Made In The Next %li Minutes%s%s",
           (long)((gdata.nonewcons - gdata.curtime + 1)/60),
           gdata.nosendmsg ? ", " : "",
           gdata.nosendmsg ? gdata.nosendmsg : "");
    *bad = "(No New Cons)";
    return 1;
  }
  if (gdata.transferlimits_over) {
    tempstr = transfer_limit_exceeded_msg(gdata.transferlimits_over - 1);
    notice(pi->nick, "** %s", tempstr);
    mydelete(tempstr);
    *bad = "(Over Transfer Limit)";
    return 1;
  }
  if ((xd->dlimit_max != 0) && (xd->gets >= xd->dlimit_used)) {
    notice(pi->nick, "** Sorry, This Pack is over download limit for today.  Try again tomorrow.");
    if (xd->dlimit_desc != NULL)
      notice(pi->nick, "%s", xd->dlimit_desc);
    *bad = "(Over Pack Transfer Limit)";
    return 1;
  }

  /* if maxtransfersperperson is reached, queue the file, unless queues are used up, which is checked in addtoqueue() */
  if (usertrans >= gdata.maxtransfersperperson) {
    tempstr = mymalloc(maxtextlength);
    fatal = addtomainqueue(bad, tempstr, pi->nick, pi->hostname, pack);
    notice(pi->nick, "** You can only have %u %s at a time, %s",
            gdata.maxtransfersperperson,
            gdata.maxtransfersperperson != 1 ? "transfers" : "transfer",
            tempstr);
    mydelete(tempstr);
    return fatal;
  }
  if ((irlist_size(&gdata.trans) >= gdata.maxtrans) || (gdata.holdqueue) || (gnetwork->botstatus != BOTSTATUS_JOINED) ||
      ((xd->st_size >= gdata.smallfilebypass) && (irlist_size(&gdata.trans) >= gdata.slotsmax))) {
    tempstr = mymalloc(maxtextlength);
    fatal = addtomainqueue(bad, tempstr, pi->nick, pi->hostname, pack);
    notice(pi->nick, "** All Slots Full, %s", tempstr);
    mydelete(tempstr);
    return fatal;
  }
  if (pack == XDCC_SEND_LIST)
    init_xdcc_file(xd, gdata.xdcclistfile);
  look_for_file_changes(xd);

  tr = create_transfer(xd, pi->nick, pi->hostname);
  t_notice_transfer(tr, msg, pack, 0);
  t_unlmited(tr, pi->hostmask);
  t_setup_dcc(tr);
  *bad = "requested";
  return 0;
}

static unsigned int send_batch_group(privmsginput *pi, const char *what, const char *pwd, const char *match)
{
  xdcc *xd;
  const char *bad;
  unsigned int num;
  unsigned int found;

  updatecontext();

  found = 0;
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (xd->group == NULL)
      continue;
    if (strcasecmp(what, xd->group) != 0)
      continue;

    if (send_xdcc_file2(&bad, pi, num, NULL, pwd, match))
      break;

    if (bad != NULL)
      ++found;
  }
  return found;
}

static unsigned int send_batch_search(privmsginput *pi, const char *what, const char *pwd)
{
  const char *bad;
  char *end;
  char *pattern;
  char *match = NULL;
  unsigned int found = 0;
  unsigned int first;
  unsigned int last;

  if (*what == 0)
    return found;

  updatecontext();

  pattern = strchr(what, '*');
  if (pattern != NULL) {
    *(pattern++) = 0; /* cut pattern */
    match = grep_to_fnmatch(pattern);
  }

  found = send_batch_group(pi, what, pwd, match);
  if (found != 0) {
    mydelete(match);
    return found;
  }

  updatecontext();

  /* search range */
  end = strchr(what, '-');
  if (end == NULL) {
    mydelete(match);
    first = packnumtonum(what);
    if (send_xdcc_file2(&bad, pi, first, NULL, pwd, NULL))
      return found;
    return ++found;
  }
  *(end++) = 0; /* cut range */
  first = packnumtonum(what);
  last = packnumtonum(end);
  if (last < first) {
    /* count backwards */
    for (; first >= last; --first) {
      if (send_xdcc_file2(&bad, pi, first, NULL, pwd, match))
        break;
      if (bad != NULL)
        ++found;
    }
  } else {
    /* count forwards */
    for (; first <= last; ++first) {
      if (send_xdcc_file2(&bad, pi, first, NULL, pwd, match))
        break;
      if (bad != NULL)
        ++found;
    }
  }
  mydelete(match);
  return found;
}

static void send_batch(privmsginput *pi, const char *what, const char *pwd)
{
  char *copy;
  char *data;
  unsigned int found;

  copy = mystrdup(what);
  found = 0;
  /* parse list of packs */
  for (data = strtok(copy, ","); /* NOTRANSLATE */
       data;
       data = strtok(NULL, ",")) { /* NOTRANSLATE */
    if (data[0] == 0)
      break;
    found += send_batch_search(pi, data, pwd);
  }
  mydelete(copy);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC BATCH %s: %u packs %s (%s on %s)",
          what, found, pi->nick, pi->hostmask, gnetwork->name);
  if (found != 0)
    return;

  notice(pi->nick, "** Invalid Pack Number, Try Again");
}

static void log_xdcc_request1(privmsginput *pi)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC %s %s (%s on %s)",
          pi->msg2, pi->nick, pi->hostmask, gnetwork->name);
}

static void log_xdcc_request2(const char *msg, const char *arg, privmsginput *pi)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC %s %s %s (%s on %s)",
          msg, arg, pi->nick, pi->hostmask, gnetwork->name);
}

static void log_xdcc_request3(privmsginput *pi, const char *msg)
{
  const char *sep;
  const char *part3;

  if (msg == NULL) {
    msg = "";
    sep = "";
  } else {
    sep = ": ";
  }
  part3 = pi->msg3;
  if (part3 == NULL)
    part3 = "";
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC %s %s%s%s %s (%s on %s)",
          pi->msg2, part3, sep, msg, pi->nick, pi->hostmask, gnetwork->name);
}

/* get restrictlist for current network */
static unsigned int get_restrictlist(void)
{
  return (gnetwork->restrictlist != 2) ? gnetwork->restrictlist : gdata.restrictlist;
}

static void send_xdcc_info(const char **bad, const char *nick, const char *hostmask, const char *arg)
{
  userinput *pubinfo;
  xdcc *xd;
  char tempstr[maxtextlengthshort];
  unsigned int pack;

  updatecontext();

  if (gdata.disablexdccinfo) {
    notice(nick, "** XDCC INFO denied, disabled by configuration");
    *bad = "ignored";
    return;
  }

  pack = packnumtonum(arg);
  xd = get_download_pack(bad, nick, hostmask, pack, "INFO", get_restrictlist()); /* NOTRANSLATE */
  if (xd == NULL)
    return;

  if (hide_pack(xd) != 0) {
    notice(nick, "** Invalid Pack Number, Try Again");
    *bad = "Denied (pack locked)";
    return;
  }

  pubinfo = mycalloc(sizeof(userinput));
  snprintf(tempstr, sizeof(tempstr), "INFO %u", pack);
  a_fillwith_msg2(pubinfo, nick, tempstr);
  pubinfo->method = method_xdl_user_notice;
  u_parseit(pubinfo);
  mydelete(pubinfo);
  *bad = "requested";
}

static void restrictprivlistmsg2(const char *nick, const char *msg)
{
  notice(nick, "** XDCC LIST Denied. %s", msg);
}

static void restrictprivlistmsg(const char *nick)
{
  const char *msg;

  msg = gdata.restrictprivlistmsg;
  if (msg == NULL)
    msg = "Wait for the public list in the channel.";
  restrictprivlistmsg2(nick, msg);
}

static unsigned int parse_xdcc_list(const char *nick, char *msg3)
{
  xlistqueue_t *user;

  if (gdata.restrictprivlist) {
    restrictprivlistmsg(nick);
    return 2; /* deny */
  }
  if (get_restrictlist() && access_need_level(nick, "LIST")) { /* NOTRANSLATE */
    return 2; /* deny */
  }
  for (user = irlist_get_head(&(gnetwork->xlistqueue));
       user;
       user = irlist_get_next(user)) {
    if (!strcmp(user->nick, nick))
      return 1; /* ignore */
  }
  if (irlist_size(&(gnetwork->xlistqueue)) >= MAXXLQUE) {
    restrictprivlistmsg2(nick, "I'm rather busy at the moment, try again later");
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
  if (strcmp(caps(msg3), "ALL") == 0) { /* NOTRANSLATE */
    if (gdata.restrictprivlistfull) {
      restrictprivlistmsg(nick);
      return 2; /* deny */
    }
  }
  user = irlist_add(&(gnetwork->xlistqueue), sizeof(xlistqueue_t));
  user->nick = mystrdup(nick);
  user->msg = mystrdup(msg3);
  return 3; /* queued */
}

static void send_cancel(const char *nick)
{
  transfer *tr;
  unsigned int k = 0;

  /* stop transfers */
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if ((tr->net == gnetwork->net) && (!strcasecmp(tr->nick, nick))) {
      if (tr->tr_status != TRANSFER_STATUS_DONE) {
        k += 1;
        t_closeconn(tr, "Transfer canceled by user", 0);
      }
    }
  }
  if (!k) notice(nick, "You don't have a transfer running");
}

static void send_remove(const char *nick, unsigned int number)
{
  unsigned int k = 0;

  k += queue_xdcc_remove(&gdata.mainqueue, gnetwork->net, nick, number);
  k += queue_xdcc_remove(&gdata.idlequeue, gnetwork->net, nick, number);
  if (!k) notice(nick, "You Don't Appear To Be In A Queue");
}

static void send_owner(const char *nick)
{
   notice(nick, "Owner for this bot is: %s",
          gdata.owner_nick ? gdata.owner_nick : "(unknown)");
}

static void send_help(const char *nick)
{
  const char *mynick;

  updatecontext();

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
  if (gdata.send_batch)
    notice_slow(nick, "\2**\2 Download batch:    \"/MSG %s XDCC BATCH group\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Show time to wait: \"/MSG %s XDCC QUEUE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Remove from queue: \"/MSG %s XDCC REMOVE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Cancel download:   \"/MSG %s XDCC CANCEL\" \2**\2", mynick);
}

static unsigned int stoplist_queue(const char *nick, irlist_t *list)
{
  char *item;
  char *copy;
  char *end;
  char *inick;
  unsigned int stopped = 0;

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
          if ( (strcmp(copy, "PRIVMSG") == 0) || (strcmp(copy, "NOTICE") == 0) ) { /* NOTRANSLATE */
            ++stopped;
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

static unsigned int stoplist_announce(const char *nick)
{
  channel_announce_t *item;
  unsigned int stopped = 0;

  item = irlist_get_head(&(gnetwork->serverq_channel));
  while (item) {
    if (strcasecmp(item->channel, nick) == 0) {
      mydelete(item->channel);
      mydelete(item->msg);
      item = irlist_delete(&(gnetwork->serverq_channel), item);
      continue;
    }
    item = irlist_get_next(item);
  }
  return stopped;
}

static unsigned int stoplist_xlistqueue(const char *nick)
{
  xlistqueue_t *user;
  unsigned int k;

  k = 0;
  for (user = irlist_get_head(&(gnetwork->xlistqueue));
       user; ) {
    if (strcmp(user->nick, nick) != 0) {
      user = irlist_get_next(user);
      continue;
    }
    ++k;
    mydelete(user->nick);
    mydelete(user->msg);
    user = irlist_delete(&(gnetwork->xlistqueue), user);
  }
  return k;
}

/* remove all queued lines for this user */
static unsigned int stoplist(const char *nick)
{
  char *item;
  unsigned int stopped = 0;

  for (item = irlist_get_head(&(gnetwork->xlistqueue)); item; ) {
    if (strcasecmp(item, nick) == 0) {
      ++stopped;
      item = irlist_delete(&(gnetwork->xlistqueue), item);
      continue;
    }
    item = irlist_get_next(item);
  }

  stopped += stoplist_queue(nick, &(gnetwork->serverq_fast));
  stopped += stoplist_queue(nick, &(gnetwork->serverq_slow));
  stopped += stoplist_queue(nick, &(gnetwork->serverq_normal));
  stopped += stoplist_announce(nick);
  stopped += stoplist_xlistqueue(nick);
  return stopped;
}

/* remove all queued lines for this user */
static void xdcc_stop(privmsginput *pi)
{
  unsigned int stopped;

  stopped = stoplist(pi->nick);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC STOP from %s (%s on %s) stopped %u",
           pi->nick, pi->hostmask, gnetwork->name, stopped);
  notice(pi->nick, "LIST stopped (%u lines deleted)", stopped);
}

static const char *send_xdcc_file(privmsginput *pi, const char *arg, const char *pwd)
{
  const char *bad;
  unsigned int pack;

  updatecontext();

  pack = packnumtonum(arg);
  send_xdcc_file2(&bad, pi, pack, NULL, pwd, NULL);
  return bad;
}

static void xdcc_send(privmsginput *pi)
{
  const char *msg;

  msg = send_xdcc_file(pi, pi->msg3, pi->msg4);
  log_xdcc_request3(pi, msg);
}

static void xdcc_option(privmsginput *pi)
{
  dcc_options_t *dcc_options;
  const char *option;
  unsigned int ss;
  char action;

  dcc_options = get_options(pi->nick);
  if (dcc_options == NULL) {
    dcc_options = irlist_add(&(gnetwork->dcc_options), sizeof(dcc_options_t));
    dcc_options->nick = mystrdup(pi->nick);
    dcc_options->last_seen = gdata.curtime;
    dcc_options->options = 0;
  }

  action = pi->msg3[0];
  option = pi->msg3 + 1;
  for (ss=0; xdcc_option_list[ss].name != NULL; ++ss) {
    if (strcasecmp(option, xdcc_option_list[ss].name) == 0) {
      if (action == '+') {
        dcc_options->options |= xdcc_option_list[ss].setval;
        return;
      }
      if (action == '-') {
        dcc_options->options &= xdcc_option_list[ss].resetval;
        return;
      }
      return;
    }
  }
}

static void command_xdcc(privmsginput *pi)
{
  const char *msg;
  char *tmp;

  caps(pi->msg2);

  if (strcmp(pi->msg2, "LIST") == 0) { /* NOTRANSLATE */
    unsigned int j;
    /* detect xdcc list group xxx */
    if ((pi->msg3) && (pi->msg4) && (strcmp(caps(pi->msg3), "GROUP") == 0)) { /* NOTRANSLATE */
      tmp = pi->msg3;
      pi->msg4 = pi->msg3;
      pi->msg3 = tmp;
    }
    j = parse_xdcc_list(pi->nick, pi->msg3);
    log_xdcc_request3(pi, (j==1 ? "ignored" : (j==2 ? "denied" : "queued")));
    return;
  }

  if ((strcmp(pi->msg2, "GET") == 0) && pi->msg3) { /* NOTRANSLATE */
    xdcc_send(pi);
    return;
  }
  if ((strcmp(pi->msg2, "SEND") == 0) && pi->msg3) { /* NOTRANSLATE */
    xdcc_send(pi);
    return;
  }

  if ((strcmp(pi->msg2, "OPTION") == 0) && pi->msg3) { /* NOTRANSLATE */
    log_xdcc_request1(pi);
    xdcc_option(pi);
    return;
  }

  if ((strcmp(pi->msg2, "INFO") == 0) && pi->msg3) { /* NOTRANSLATE */
    send_xdcc_info(&msg, pi->nick, pi->hostmask, pi->msg3);
    log_xdcc_request3(pi, msg);
    return;
  }
  if (strcmp(pi->msg2, "QUEUE") == 0) { /* NOTRANSLATE */
    log_xdcc_request1(pi);
    notifyqueued_nick(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "STOP") == 0) { /* NOTRANSLATE */
    xdcc_stop(pi);
    return;
  }

  if (strcmp(pi->msg2, "CANCEL") == 0) { /* NOTRANSLATE */
    log_xdcc_request1(pi);
    send_cancel(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "REMOVE") == 0) { /* NOTRANSLATE */
    unsigned int number = 0;
    if (pi->msg3) {
      number = atoi(pi->msg3);
      log_xdcc_request2(pi->msg2, pi->msg3, pi);
    } else {
      log_xdcc_request1(pi);
    }
    send_remove(pi->nick, number);
    return;
  }

  if (strcmp(pi->msg2, "OWNER") == 0) { /* NOTRANSLATE */
    log_xdcc_request1(pi);
    send_owner(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "HELP") == 0) { /* NOTRANSLATE */
    log_xdcc_request1(pi);
    send_help(pi->nick);
    return;
  }

  if (gdata.send_batch) {
    if ((strcmp(pi->msg2, "BATCH") == 0) && pi->msg3) { /* NOTRANSLATE */
      send_batch(pi, pi->msg3, pi->msg4);
      return;
    }
  }

  if ((strcmp(pi->msg2, "SEARCH") == 0) && pi->msg3) { /* NOTRANSLATE */
    xdcc *xd;
    char *match;
    char *msg3e;
    char *grouplist;
    char *colordesc;
    unsigned int i;
    unsigned int k;

    /* if restrictlist is enabled, visibility rules apply */
    grouplist = get_restrictlist() ? get_grouplist_access(pi->nick) : NULL;
    msg3e = getpart_eol(pi->line, 6);
    strip_trailing_action(msg3e);
    log_xdcc_request2(pi->msg2, msg3e, pi);
    notice_slow(pi->nick, "Searching for \"%s\"...", msg3e);
    convert_spaces_to_match(msg3e);
    match = grep_to_fnmatch(msg3e);

    i = 0;
    k = 0;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      ++i;
      if (hide_pack(xd))
        continue;
      if (verify_group_in_grouplist(xd->group, grouplist) == 0)
        continue;
      if (fnmatch_xdcc(match, xd)) {
        colordesc = xd_color_description(xd);
        notice_slow(pi->nick, " - Pack #%u matches, \"%s\"", i, colordesc);
        if (colordesc != xd->desc)
          mydelete(colordesc);
        ++k;
        /* limit matches */
        if ((gdata.max_find != 0) && (k >= gdata.max_find)) {
          notice_slow(pi->nick, "Search limit exceeded, please check the packlist for more results.");
          break;
        }
      }
    }
    mydelete(grouplist);
    mydelete(match);
    if (!k) {
       notice_slow(pi->nick, "Sorry, nothing was found, try a XDCC LIST");
    }
    mydelete(msg3e);
    return;
  }

  /* don't respond to unsupported comamnds in channels */
  if (pi->nick[0] == '#')
    return;

  log_xdcc_request2("unsupported", pi->msg2, pi);
  notice(pi->nick, "Sorry, this command is unsupported" );
}

static void admin_msg_line(const char *nick, char *line, unsigned int level)
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

static unsigned int msg_host_password(const char *nick, const char *hostmask, const char *passwd, char *line)
{
  group_admin_t *ga;

  if ( verifyshell(&gdata.adminhost, hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.adminlevel);
      return 1;
    }
    return 2;
  }
  if ( verifyshell(&gdata.hadminhost, hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.hadminlevel);
      return 1;
    }
    return 2;
  }
  if ((ga = verifypass_group(hostmask, passwd))) {
    reset_ignore(hostmask);
    admin_msg_line(nick, line, ga->g_level);
    return 1;
  }
  return 0;
}

static void admin_message(privmsginput *pi)
{
  unsigned int err;

  err = msg_host_password(pi->nick, pi->hostmask, pi->msg2, pi->line);
  if (err == 0) {
    notice(pi->nick, "ADMIN: %s is not allowed to issue admin commands", pi->hostmask);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Hostname (%s on %s)",
            pi->hostmask, gnetwork->name);
    return;
  }
  if (err == 2) {
    notice(pi->nick, "ADMIN: Incorrect Password");
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Password (%s on %s)",
            pi->hostmask, gnetwork->name);
  }
}

static int not_for_me(const char *dest)
{
  if (dest == NULL)
    return 1;
  if (!gnetwork->caps_nick)
    return 1;
  if (strcmp(gnetwork->caps_nick, dest) != 0)
    return 1;
  /* this must be for the bot */
  return 0;
}

static int botonly_parse(int type, privmsginput *pi)
{
  char *tempstr2;

  /*----- CLIENTINFO ----- */
  if (test_ctcp(pi->msg1, IRC_CTCP "CLIENTINFO")) { /* NOTRANSLATE */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    send_clientinfo(pi->nick, pi->msg2);
    return 0;
  }
  if (type != 0) {
    /*----- PING ----- */
    if (test_ctcp(pi->msg1, IRC_CTCP "PING")) { /* NOTRANSLATE */
      if (check_ignore(pi->nick, pi->hostmask))
        return 0;
      notice(pi->nick, IRC_CTCP "PING%s%s%s%s" IRC_CTCP, /* NOTRANSLATE */
             pi->msg2 ? " " : "", /* NOTRANSLATE */
             pi->msg2 ? pi->msg2 : "",
             pi->msg3 ? " " : "", /* NOTRANSLATE */
             pi->msg3 ? pi->msg3 : "");
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "[CTCP] %s on %s: PING",
              pi->nick, gnetwork->name);
      return 0;
    }
  }
 /*----- VERSION ----- */
  if (test_ctcp(pi->msg1, IRC_CTCP "VERSION")) { /* NOTRANSLATE */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    notice(pi->nick, IRC_CTCP "VERSION iroffer-dinoex " VERSIONLONG FEATURES ", " "http://iroffer.dinoex.net/" "%s%s" IRC_CTCP,
           gdata.hideos ? "" : " - ", /* NOTRANSLATE */
           gdata.hideos ? "" : gdata.osstring);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "[CTCP] %s on %s: VERSION",
            pi->nick, gnetwork->name);
    return 0;
  }
 /*----- UPTIME ----- */
  if (test_ctcp(pi->msg1, IRC_CTCP "UPTIME")) { /* NOTRANSLATE */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    tempstr2 = mymalloc(maxtextlength);
    tempstr2 = getuptime(tempstr2, 0, gdata.startuptime, maxtextlength);
    notice(pi->nick, IRC_CTCP "UPTIME %s" IRC_CTCP, tempstr2); /* NOTRANSLATE */
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "[CTCP] %s on %s: UPTIME",
             pi->nick, gnetwork->name);
    mydelete(tempstr2);
    return 0;
  }
  /*----- STATUS ----- */
  if (test_ctcp(pi->msg1, IRC_CTCP "STATUS")) { /* NOTRANSLATE */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    tempstr2 = mymalloc(maxtextlength);
    tempstr2 = getstatuslinenums(tempstr2, maxtextlength);
    notice(pi->nick, IRC_CTCP "%s" IRC_CTCP, tempstr2); /* NOTRANSLATE */
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
               "[CTCP] %s on %s: STATUS",
               pi->nick, gnetwork->name);
    mydelete(tempstr2);
    return 0;
  }

  if (not_for_me(pi->dest))
    return 1;

  /*----- DCC SEND/CHAT/RESUME ----- */
  if (test_ctcp(pi->msg1, IRC_CTCP "DCC")) { /* NOTRANSLATE */
    if (pi->msg2 == NULL)
      return 0; /* ignore */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    caps(pi->msg2);
    command_dcc(pi);
    return 0;
  }

  /*----- ADMIN ----- */
  if (strcmp(pi->msg1, "ADMIN") == 0) { /* NOTRANSLATE */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    admin_message(pi);
    return 0;
  }

  return 1;
}

static void autoqueuef(unsigned int pack, const char *message, privmsginput *pi)
{
  const char *format = "** Sending You %s by DCC";
  char *tempstr = NULL;
  const char *msg;

  updatecontext();

  ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
  if (message) {
    tempstr = mymalloc(strlen(message) + strlen(format) - 1);
    snprintf(tempstr, strlen(message) + strlen(format) - 1,
             format, message);
  }
  send_xdcc_file2(&msg, pi, pack, tempstr, NULL, NULL);
  pi->msg2 = mystrdup("AutoSend");
  log_xdcc_request3(pi, msg);
  mydelete(pi->msg2);
  mydelete(tempstr);
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
static unsigned int noticeresults(const char *nick, const char *pattern, const char *dest)
{
  unsigned int i, j, k;
  size_t len;
  const char *grouplist;
  char *tempstr = mymalloc(maxtextlength);
  char *match;
  char *sizestrstr;
  char *colordesc;
  xdcc *xd;

  /* apply per-channel visibility rules */
  grouplist = get_grouplist_channel(dest);
  match = grep_to_fnmatch(pattern);
  len = k = 0;

  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd)) {
    ++i;
    if (hide_pack(xd))
      continue;

    /* check group visibility rules */
    if (!verify_group_in_grouplist(xd->group, grouplist))
      continue;

    if (fnmatch_xdcc(match, xd)) {
      if (!k) {
        if (gdata.slotsmax < irlist_size(&gdata.trans))
          j = irlist_size(&gdata.trans);
        else
          j = gdata.slotsmax;
        snprintf(tempstr, maxtextlength, "XDCC SERVER - %s:[%u/%u]", j != 1 ? "slots" : "slot", j - irlist_size(&gdata.trans), j);
        len = strlen(tempstr);
        if ((irlist_size(&gdata.mainqueue) > 0) || (irlist_size(&gdata.trans)) > 0) {
          snprintf(tempstr + len, maxtextlength - len, ", Queue:[%u/%u]", irlist_size(&gdata.mainqueue), gdata.queuesize);
          len = strlen(tempstr);
        }
        if (gdata.transferminspeed > 0) {
          snprintf(tempstr + len, maxtextlength - len, ", Min:%1.1fkB/s", gdata.transferminspeed);
          len = strlen(tempstr);
        }
        if (gdata.transfermaxspeed > 0) {
          snprintf(tempstr + len, maxtextlength - len, ", Max:%1.1fkB/s", gdata.transfermaxspeed);
          len = strlen(tempstr);
        }
        if (gdata.maxb) {
          snprintf(tempstr + len, maxtextlength - len, ", Cap:%u.0kB/s", gdata.maxb / 4);
          len = strlen(tempstr);
        }
        snprintf(tempstr + len, maxtextlength - len, " - /MSG %s XDCC SEND x -",
                 get_user_nick());
        len = strlen(tempstr);
        if (!strcmp(match, "*")) /* NOTRANSLATE */
          snprintf(tempstr + len, maxtextlength - len, " Packs:");
        else
          snprintf(tempstr + len, maxtextlength - len, " Found:");
      len = strlen(tempstr);
    }
    sizestrstr = sizestr(0, xd->st_size);
    colordesc = xd_color_description(xd);
    snprintf(tempstr + len, maxtextlength - len, " #%u:%s,%s", i, colordesc, sizestrstr);
    if (strlen(tempstr) > get_channel_limit(dest)) {
      snprintf(tempstr + len, maxtextlength - len, " [...]");
      notice_slow(nick, "%s", tempstr); /* NOTRANSLATE */
      snprintf(tempstr, maxtextlength, "[...] #%u:%s,%s", i, colordesc, sizestrstr);
    }
    if (colordesc != xd->desc)
      mydelete(colordesc);
    len = strlen(tempstr);
    mydelete(sizestrstr);
    ++k;
    /* limit matches */
    if ((gdata.max_find != 0) && (k >= gdata.max_find))
      break;
    }
  }

  mydelete(match);

  if (k)
    notice_slow(nick, "%s", tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  return k;
}

/* iroffer-lamm: @find */
static void do_atfind(unsigned int min, privmsginput *pi)
{
  char *msg2e;
  unsigned int k;

  if (check_ignore(pi->nick, pi->hostmask))
    return;

  msg2e = getpart_eol(pi->line, 5);
  ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
  k = convert_spaces_to_match(msg2e);
  if (k >= min) {
    k = noticeresults(pi->nick, msg2e, pi->dest);
    if (k) {
      ioutput(OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
              "@FIND %s (%s on %s) - %u %s found.",
              msg2e, pi->hostmask, gnetwork->name, k, k != 1 ? "packs" : "pack");
    }
  }
  mydelete(msg2e);
  ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
}

static unsigned int run_new_trigger(const char *nick, const char *grouplist)
{
  irlist_t list;
  xdcc **best;
  xdcc *xd;
  char *tempstr;
  char *colordesc;
  time_t now;
  unsigned int i;

  memset(&list, 0, sizeof(irlist_t));
  for (i=0; i<gdata.new_trigger; ++i)
    add_newest_xdcc(&list, grouplist);

  i = 0;
  for (best = irlist_get_head(&list);
       best;
       best = irlist_delete(&list, best)) {
    xd = *best;
    now = xd->xtime;
    tempstr = mymalloc(maxtextlengthshort);
    user_getdatestr(tempstr, now, maxtextlengthshort);
    colordesc = xd_color_description(xd);
    notice_slow(nick, "Added: %s \2%u\2%s%s",
                tempstr, number_of_pack(xd), gdata.announce_seperator, colordesc);
    if (colordesc != xd->desc)
      mydelete(colordesc);
    mydelete(tempstr);
    ++i;
  }
  return i;
}

static int check_trigger(int type, privmsginput *pi)
{
  autoqueue_t *aq;
  autotrigger_t *at;

  if (type == 0)
    return 0;

  if (pi->msg1 == NULL)
    return 0;

  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_get_next(aq)) {
    if (!strcasecmp(pi->msg1, aq->word)) {
      /* add/increment ignore list */
      if (check_ignore(pi->nick, pi->hostmask))
        return 0;

      autoqueuef(aq->pack, aq->message, pi);
      /* only first match is activated */
      return 1;
    }
  }
  for (at = irlist_get_head(&gdata.autotrigger);
       at;
       at = irlist_get_next(at)) {
    if (!strcasecmp(pi->msg1, at->word)) {
      /* add/increment ignore list */
      if (check_ignore(pi->nick, pi->hostmask))
        return 0;

      autoqueuef(number_of_pack(at->pack), NULL, pi);
      /* only first match is activated */
      return 1;
    }
  }
  /* nothing found */
  return 0;
}

/* search for custom listmsg */
static const char *get_listmsg_channel(const char *dest)
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

static int ignore_trigger_dest(const char *dest)
{
  channel_t *ch;

  if (!dest)
    return 1;

  if (*dest != '#')
    return 0;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, dest) != 0)
      continue;

    if (ch->notrigger)
      return 1;

    return 0;
  }
  return 0;
}

static void add_msg_statefile(const  char *begin, int type, privmsginput *pi)
{
  msglog_t *ml;

  ioutput(OUT_S|OUT_D, COLOR_GREEN,
          "%s from %s on %s logged, use MSGREAD to display it.",
          type_list[type], pi->nick, gnetwork->name);

  ml = irlist_add(&gdata.msglog, sizeof(msglog_t));

  ml->when = gdata.curtime;
  ml->hostmask = mystrdup(pi->hostmask);
  ml->message = mystrdup(begin);

  write_statefile();
}

/* get respondtochannelxdccn for current network */
static unsigned int get_respondtochannelxdcc(void)
{
  return (gnetwork->respondtochannelxdcc != 2) ? gnetwork->respondtochannelxdcc : gdata.respondtochannelxdcc;
}

/* get respondtochannellist for current network */
static unsigned int get_respondtochannellist(void)
{
  return (gnetwork->respondtochannellist != 2) ? gnetwork->respondtochannellist : gdata.respondtochannellist;
}

static void privmsgparse2(int type, int decoded, privmsginput *pi)
{
  char *begin;
  unsigned int exclude;

  /*----- CTCP ----- */
  if (botonly_parse(type, pi) == 0) {
    ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
    return;
  }

  /* skip channels marked notrigger */
  if (ignore_trigger_dest(pi->dest))
    return;

  if ((type != 0) && (decoded == 0) && gdata.fish_only) {
    return; /* ignore */
  }

  caps(pi->hostmask);
  /*----- XDCC ----- */
  if ((strcmp(pi->msg1, "XDCC") == 0) || /* NOTRANSLATE */
      (strcmp(pi->msg1, IRC_CTCP "XDCC") == 0) || /* NOTRANSLATE */
      (strcmp(pi->msg1, "CDCC") == 0) || /* NOTRANSLATE */
      (strcmp(pi->msg1, IRC_CTCP "CDCC") == 0)) { /* NOTRANSLATE */
    if (get_respondtochannelxdcc() == 0) {
      if (not_for_me(pi->dest))
        return; /* ignore */
    }
    if (check_ignore(pi->nick, pi->hostmask))
      return;
    if (pi->msg2 != NULL) {
      command_xdcc(pi);
      ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
      return;
    }
  }

  /*----- !LIST ----- */
  if ((test_ctcp(pi->msg1, IRC_CTCP "!LIST")) || /* NOTRANSLATE */
      (strcmp(pi->msg1, "!LIST") == 0)) { /* NOTRANSLATE */
    const char *rtclmsg;
    char *tempstr2;

    if (get_respondtochannellist() == 0) {
      if (not_for_me(pi->dest))
        return; /* ignore */
    }
    if (pi->msg2 != NULL) {
      caps(pi->msg2);
      if (not_for_me(pi->msg2))
        return; /* ignore */
    }
    if (check_ignore(pi->nick, pi->hostmask))
      return;

    tempstr2 = mymalloc(maxtextlength);
    /* generate !list styled message */
    /* search for custom listmsg */
    rtclmsg = get_listmsg_channel(pi->dest);
    if (gdata.restrictprivlist)
      tempstr2[0] = 0;
    else
      snprintf(tempstr2, maxtextlength,
               "Trigger:\2(\2/MSG %s XDCC LIST\2)\2 ",
               save_nick(gnetwork->user_nick));
    notice_slow(pi->nick,
           "\2(\2XDCC\2)\2 Packs:\2(\2%u\2)\2 "
           "%s%s"
           "%s"
           "Sends:\2(\2%u/%u\2)\2 "
           "Queues:\2(\2%u/%u\2)\2 "
           "Record:\2(\2%1.1fkB/s\2)\2 "
           "%s%s%s\2=\2iroffer\2=\2",
           irlist_size(&gdata.xdccs),
           (rtclmsg ? rtclmsg : ""),
           (rtclmsg ? " " : ""),
           tempstr2,
           irlist_size(&gdata.trans), gdata.slotsmax,
           irlist_size(&gdata.mainqueue), gdata.queuesize,
           gdata.record,
           gdata.creditline ? "Note:\2(\2" : "",
           gdata.creditline ? gdata.creditline : "",
           gdata.creditline ? "\2)\2 " : "");

    mydelete(tempstr2);
    ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
    return;
  }

  /* iroffer-lamm: @find */
  if (gdata.atfind && pi->msg2) {
    if (strcmp(pi->msg1, "@FIND") == 0) { /* NOTRANSLATE */
      do_atfind( gdata.atfind, pi);
      return;
    }
    if (!gdata.no_find_trigger) {
      if (strcmp(pi->msg1, "!FIND") == 0) { /* NOTRANSLATE */
        do_atfind( gdata.atfind, pi);
        return;
      }
    }
  }

  if (gdata.new_trigger) {
    if (strcmp(pi->msg1, "!NEW") == 0) { /* NOTRANSLATE */
      const char *grouplist;
      unsigned int k;

      if (check_ignore(pi->nick, pi->hostmask))
        return;

      /* apply per-channel visibility rules */
      grouplist = get_grouplist_channel(pi->dest);
      k = run_new_trigger(pi->nick, grouplist);
      if (k) {
        ioutput(OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
                "!NEW (%s on %s) - %u packs.",
                pi->hostmask, gnetwork->name, k);
      }
      ++(gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]);
      return;
    }
  }

  if (check_trigger(type, pi)) {
    /* matched lines are skipped */
    return;
  }

  if (not_for_me(pi->dest))
    return; /* ignore */

  exclude = 0;
  begin = pi->line + 5 + strlen(pi->hostmask) + strlen(type_list[type]) + strlen(pi->dest);
  if (verifyshell(&gdata.log_exclude_host, pi->hostmask))
    ++exclude;
  if (verifyshell(&gdata.log_exclude_text, begin))
    ++exclude;

  if (exclude == 0) {
    if (type == 0) {
      if (gdata.lognotices) {
        add_msg_statefile(begin, type, pi);
        return;
      }
      if (gdata.logfile_notices) {
        logfile_add(gdata.logfile_notices, pi->line);
        return;
      }
    } else {
      if (gdata.logmessages) {
        add_msg_statefile(begin, type, pi);
        return;
      }
      if (gdata.logfile_messages) {
        logfile_add(gdata.logfile_messages, pi->line);
        return;
      }
    }
  }
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_GREEN,
          "%s on %s: %s",
          type_list[type], gnetwork->name, pi->line);
}

static int get_nick_hostname(char *nick, char *hostname, const char *line)
{
  if (line && *line == ':')
    ++line;
  for (; *line && *line != '!'; ++line)
    *(nick++) = *line;
  *nick = 0;
  *hostname = 0;

  if (*line == 0)
    return 1;

  for (; *line && *line != '@'; ++line)
    ;

  if (*line == 0)
    return 1;

  for (++line; *line && *line != ' '; ++line)
    *(hostname++) = *line;
  *hostname = 0;
  return 0;
}

#define MAX_PRIVMSG_PARTS 10

/* parse a privmsg from server for user actions */
void privmsgparse(int type, int decoded, char *line)
{
  privmsginput pi;
  char *part[MAX_PRIVMSG_PARTS];
  unsigned int m;
  unsigned int i;
  size_t line_len;

  updatecontext();

  floodchk();
  if (gdata.ignore)
    return;

  bzero((char *)&pi, sizeof(pi));
  pi.line = line;
  m = get_argv(part, line, MAX_PRIVMSG_PARTS);
  if ((part[3] != NULL) && (m >= 3)) {
    if (part[3][1] == IRCCTCP) {
      /* remove action only on the last arg */
      strip_trailing_action(part[m - 1]);
    }
  }
  pi.hostmask = part[0] + 1;
  pi.dest = part[2];
  pi.msg1 = part[3];
  pi.msg2 = part[4];
  pi.msg3 = part[5];
  pi.msg4 = part[6];
  pi.msg5 = part[7];
  pi.msg6 = part[8];
  pi.msg7 = part[9];

  caps(pi.dest);
  if (pi.msg1) {
    caps(++(pi.msg1)); /* point past the ':' */
  }

  line_len = sstrlen(pi.hostmask);
  pi.nick = mymalloc(line_len+1);
  pi.hostname = mymalloc(line_len+1);
  /* see if it came from a user or server, ignore if from server */
  if (get_nick_hostname(pi.nick, pi.hostname, pi.hostmask) == 0)
    privmsgparse2(type, decoded, &pi);
  mydelete(pi.nick);
  mydelete(pi.hostname);
  for (i = 0; i < m; ++i)
    mydelete(part[i]);
}

/* remove user from queue after he left chammel or network */
void lost_nick(const char *nick)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Nickname %s on %s left",
          nick,
          gnetwork->name);
  stoplist(nick);
  queue_xdcc_remove(&gdata.mainqueue, gnetwork->net, nick, 0);
  queue_xdcc_remove(&gdata.idlequeue, gnetwork->net, nick, 0);
}

/* End of File */
