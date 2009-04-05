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
#include "dinoex_upload.h"
#include "dinoex_transfer.h"
#include "dinoex_jobs.h"
#include "dinoex_queue.h"
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
} privmsginput;

static const char *type_list[2] = { "NOTICE", "PRIVMSG" };

static void strip_trailing_action(char *str)
{
  size_t len;

  if (str == NULL)
    return;

  len = strlen(str);
  if (len == 0)
    return;

  if (str[--len] != '\1')
    return;

  str[len] = '\0';
}

static int test_ctcp(const char *msg1, const char *key)
{
  size_t len;

  len = strlen(key);
  if (strncmp(msg1, key, len) != 0)
    return 0;

  if ((key[len] == 0) || (key[len] == '\1'))
    return 1;

  return 0;
}

static const char *sendxdccinfo2(const char* nick, const char* hostmask, int pack)
{
  userinput *pubinfo;
  xdcc *xd;
  char tempstr[maxtextlengthshort];

  updatecontext();

  if (gdata.disablexdccinfo) {
    notice(nick, "** XDCC INFO denied, disabled by configuration");
    return " ignored: ";
  }

  xd = get_download_pack(nick, hostmask, pack, 0, "INFO", gdata.restrictlist);
  if (xd == NULL)
    return NULL;

  if (hide_pack(xd) != 0) {
    notice(nick, "** Invalid Pack Number, Try Again");
    return " Denied (pack locked): ";
  }

  pubinfo = mycalloc(sizeof(userinput));
  snprintf(tempstr, sizeof(tempstr), "INFO %d", pack);
  a_fillwith_msg2(pubinfo, nick, tempstr);
  pubinfo->method = method_xdl_user_notice;
  u_parseit(pubinfo);
  mydelete(pubinfo);
  return " requested: ";
}

static void sendxdccinfo(const char* nick, const char* hostmask, int pack)
{
  const char *msg;

  updatecontext();

  msg = sendxdccinfo2(nick, hostmask, pack);
  if (msg) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "%s", msg);
  }
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "%s (%s on %s)",
          nick, hostmask, gnetwork->name);
}

static void send_clientinfo(const char *nick, char *msg2)
{
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "[CTCP] %s in %s: CLIENTINFO",
          nick, gnetwork->name);

  if (!msg2) {
    notice(nick, "\1CLIENTINFO DCC PING VERSION XDCC UPTIME "
    ":Use CTCP CLIENTINFO <COMMAND> to get more specific information\1");
    return;
  }
  caps(msg2);
  if (strncmp(msg2, "PING", 4) == 0) {
    notice(nick, "\1CLIENTINFO PING returns the arguments it receives\1");
    return;
  }
  if (strncmp(msg2, "DCC", 3) == 0) {
    notice(nick, "\1CLIENTINFO DCC requests a DCC for chatting or file transfer\1");
    return;
  }
  if (strncmp(msg2, "VERSION", 7) == 0) {
    notice(nick, "\1CLIENTINFO VERSION shows information about this client's version\1");
    return;
  }
  if (strncmp(msg2, "XDCC", 4) == 0) {
    notice(nick, "\1CLIENTINFO XDCC LIST|SEND list and DCC file(s) to you\1");
    return;
  }
  if (strncmp(msg2, "UPTIME", 6) == 0) {
    notice(nick, "\1CLIENTINFO UPTIME shows how long this client has been running\1");
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

static void command_dcc(privmsginput *pi)
{
  if (strcmp(pi->msg2, "RESUME") == 0) {
    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL))
      return;

    strip_trailing_action(pi->msg5);
    strip_trailing_action(pi->msg6);
    t_find_resume(pi->nick, pi->msg3, pi->msg4, pi->msg5, pi->msg6);
    return;
  }

  if (strcmp(pi->msg2, "CHAT") == 0) {
    if (verifyshell(&gdata.adminhost, pi->hostmask)) {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC CHAT attempt authorized from %s on %s",
              pi->hostmask, gnetwork->name);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    if (verifyshell(&gdata.hadminhost, pi->hostmask)) {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC CHAT attempt authorized from %s on %s",
              pi->hostmask, gnetwork->name);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    if (verifyhost_group(pi->hostmask)) {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC CHAT attempt authorized from %s on %s",
              pi->hostmask, gnetwork->name);
      setupdccchat(pi->nick, pi->hostmask, pi->line);
      return;
    }
    notice(pi->nick, "DCC Chat denied from %s", pi->hostmask);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC CHAT attempt denied from %s on %s",
            pi->hostmask, gnetwork->name);
    return;
  }

  if (strcmp(pi->msg2, "SEND") == 0) {
    char *msg7;
    int down = 0;

    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL) || (pi->msg6 == NULL))
      return;

    msg7 = getpart(pi->line, 10);
    strip_trailing_action(pi->msg6);
    if (msg7) {
      strip_trailing_action(msg7);
      down = t_find_transfer(pi->nick, pi->msg3, pi->msg4, pi->msg5, msg7);
    }
    if (!down) {
      removenonprintablefile(pi->msg3);
      upload_start(pi->nick, pi->hostname, pi->hostmask, pi->msg3, pi->msg4, pi->msg5, pi->msg6, msg7);
    }
    mydelete(msg7);
    return;
  }

  if (strcmp(pi->msg2, "ACCEPT") == 0) {
    upload *ul;
    char *tempstr;
    off_t len;
    int port;

    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL))
      return;

    strip_trailing_action(pi->msg5);
    len = atoull(pi->msg5);
    if (verify_uploadhost(pi->hostmask)) {
      notice(pi->nick, "DCC Send Denied, I don't accept transfers from %s", pi->hostmask);
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC Send Denied from %s on %s",
              pi->hostmask, gnetwork->name);
      return;
    }
    if (gdata.uploadmaxsize && (len > gdata.uploadmaxsize)) {
      notice(pi->nick, "DCC Send Denied, I don't accept transfers that big");
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC Send denied from %s on %s (too big)",
              pi->hostmask, gnetwork->name);
      return;
    }
    if (len > gdata.max_file_size) {
      notice(pi->nick, "DCC Send Denied, I can't accept transfers that large");
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC Send denied from %s on %s (too large)",
              pi->hostmask, gnetwork->name);
      return;
    }

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
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "DCC Send Resumed from %s on %s: %s (%" LLPRINTFMT "d of %" LLPRINTFMT "dKB left)",
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

static int send_batch_group(const char* nick, const char* hostname, const char* hostmask, const char* what, const char* pwd)
{
  xdcc *xd;
  int num;
  int found;

  found = 0;
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    num ++;
    if (xd->group == NULL)
      continue;
    if (strcasecmp(what, xd->group) != 0)
      continue;

    found ++;
    if (sendxdccfile(nick, hostname, hostmask, num, NULL, pwd))
      return found;
  }
  return found;
}

static int send_batch_search(const char* nick, const char* hostname, const char* hostmask, const char* what, const char* pwd)
{
  char *end;
  int num;
  int found;
  int first;
  int last;

  found = send_batch_group(nick, hostname, hostmask, what, pwd);
  if (found != 0)
    return found;

  /* range */
  if (*what == '#') what ++;
  end = strchr(what, '-');
  if (end == NULL)
    return found;
  first = atoi(what);
  last = atoi(++end);
  for (num = first; num <= last; num ++) {
    found ++;
    if (sendxdccfile(nick, hostname, hostmask, num, NULL, pwd))
      return found;
  }
  return found;
}

static void send_batch(const char* nick, const char* hostname, const char* hostmask, const char* what, const char* pwd)
{
  int found;

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC BATCH %s (%s on %s)",
          what, hostmask, gnetwork->name);

  found = send_batch_search(nick, hostname, hostmask, what, pwd);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC BATCH %s, %d packs (%s on %s)",
          what, found, hostmask, gnetwork->name);
  if (found != 0)
    return;

  notice(nick, "** Invalid Pack Number, Try Again");
}

static int packnumtonum(const char *a)
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

static void restrictprivlistmsg(const char *nick)
{
  if (gdata.restrictprivlistmsg) {
    notice(nick, "** XDCC LIST Denied. %s", gdata.restrictprivlistmsg);
  } else {
    notice(nick, "** XDCC LIST Denied. Wait for the public list in the channel.");
  }
}

static int parse_xdcc_list(const char *nick, char*msg3)
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
  }
  user = irlist_add(&(gnetwork->xlistqueue), sizeof(xlistqueue_t));
  user->nick = mystrdup(nick);
  user->msg = mystrdup(msg3);
  return 3; /* queued */
}

static void log_xdcc_request(const char *msg, privmsginput *pi)
{
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC %s (%s %s on %s)",
          msg, pi->nick, pi->hostmask, gnetwork->name);
}

static void log_xdcc_request2(const char *msg, const char *arg, privmsginput *pi)
{
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC %s %s (%s %s on %s)",
          msg, pi->nick, arg, pi->hostmask, gnetwork->name);
}

static void send_cancel(const char *nick)
{
  transfer *tr;
  int k = 0;

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

static void send_remove(const char *nick)
{
  int k = 0;

  k += queue_xdcc_remove(&gdata.mainqueue, gnetwork->net, nick);
  k += queue_xdcc_remove(&gdata.idlequeue, gnetwork->net, nick);
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
  if (gdata.send_batch)
    notice_slow(nick, "\2**\2 Download batch:    \"/MSG %s XDCC BATCH group\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Search listing:    \"/MSG %s XDCC SEARCH pattern\" \2**\2", mynick);
  if ((gdata.hide_list_info == 0) && (gdata.disablexdccinfo == 0))
    notice_slow(nick, "\2**\2 Request details:   \"/MSG %s XDCC INFO pack\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Request download:  \"/MSG %s XDCC SEND pack\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Show time to wait: \"/MSG %s XDCC QUEUE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Remove from queue: \"/MSG %s XDCC REMOVE\" \2**\2", mynick);
  notice_slow(nick, "\2**\2 Cancel download:   \"/MSG %s XDCC CANCEL\" \2**\2", mynick);
}

static void command_xdcc(privmsginput *pi)
{
  strip_trailing_action(pi->msg2);
  strip_trailing_action(pi->msg3);
  caps(pi->msg2);

  if (strcmp(pi->msg2, "LIST") == 0) {
    int j;
    /* detect xdcc list group xxx */
    if ((pi->msg3) && (pi->msg4) && (strcmp(caps(pi->msg3), "GROUP") == 0))
      j = parse_xdcc_list(pi->nick, pi->msg4);
    else
      j = parse_xdcc_list(pi->nick, pi->msg3);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC LIST %s: (%s on %s)",
            (j==1 ? "ignored" : (j==2 ? "denied" : "queued")),
            pi->hostmask, gnetwork->name);
    return;
  }

  if (((strcmp(pi->msg2, "SEND") == 0) ||
       (strcmp(pi->msg2, "GET") == 0)) && pi->msg3) {
    strip_trailing_action(pi->msg3);
    strip_trailing_action(pi->msg4);
    ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC SEND %s", pi->msg3);
    sendxdccfile(pi->nick, pi->hostname, pi->hostmask, packnumtonum(pi->msg3), NULL, pi->msg4);
    return;
  }

  if ((strcmp(pi->msg2, "INFO") == 0) && pi->msg3) {
    ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC INFO %s", pi->msg3);
    sendxdccinfo(pi->nick, pi->hostmask, packnumtonum(pi->msg3));
    return;
  }
  if (strcmp(pi->msg2, "QUEUE") == 0) {
    log_xdcc_request("QUEUE", pi);
    notifyqueued_nick(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "STOP") == 0) {
    stoplist(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "CANCEL") == 0) {
    log_xdcc_request("CANCEL", pi);
    send_cancel(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "REMOVE") == 0) {
    log_xdcc_request("REMOVE", pi);
    send_remove(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "OWNER") == 0) {
    log_xdcc_request("OWNER", pi);
    send_owner(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "HELP") == 0) {
    log_xdcc_request("HELP", pi);
    send_help(pi->nick);
    return;
  }

  if (gdata.send_batch) {
    if ((strcmp(pi->msg2, "BATCH") == 0) && pi->msg3) {
      strip_trailing_action(pi->msg4);
      send_batch(pi->nick, pi->hostname, pi->hostmask, pi->msg3, pi->msg4);
      return;
    }
  }

  if ((strcmp(pi->msg2, "SEARCH") == 0) && pi->msg3) {
    xdcc *xd;
    char *match;
    char *msg3e;
    char *grouplist;
    int i;
    int k;

    /* if restrictlist is enabled, visibility rules apply */
    grouplist = gdata.restrictlist ? get_grouplist_access(pi->nick) : NULL;
    msg3e = getpart_eol(pi->line, 6);
    log_xdcc_request2("SEARCH", msg3e, pi);
    notice_slow(pi->nick, "Searching for \"%s\"...", msg3e);
    convert_spaces_to_match(msg3e);
    match = grep_to_fnmatch(msg3e);

    i = 0;
    k = 0;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      i++;
      if (hide_pack(xd))
        continue;
      if (verify_group_in_grouplist(xd->group, grouplist) == 0)
        continue;
      if (fnmatch_xdcc(match, xd)) {
        notice_slow(pi->nick, " - Pack #%i matches, \"%s\"", i, xd->desc);
        k++;
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
  log_xdcc_request2("unsupported", pi->msg2, pi);
  notice(pi->nick, "Sorry, this command is unsupported" );
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

  /* this must be for the bot */
  if (not_for_me(pi->dest))
    return 1;

  /*----- CLIENTINFO ----- */
  if (test_ctcp(pi->msg1, "\1CLIENTINFO")) {
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    send_clientinfo(pi->nick, pi->msg2);
    return 0;
  }
  if (type == 0) {
    /*----- PING ----- */
    if (test_ctcp(pi->msg1, "\1PING")) {
      if (check_ignore(pi->nick, pi->hostmask))
        return 0;
      strip_trailing_action(pi->msg2);
      strip_trailing_action(pi->msg3);
      notice(pi->nick, "\1PING%s%s%s%s\1",
             pi->msg2 ? " " : "",
             pi->msg2 ? pi->msg2 : "",
             pi->msg3 ? " " : "",
             pi->msg3 ? pi->msg3 : "");
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "[CTCP] %s on %s: PING",
              pi->nick, gnetwork->name);
      return 0;
    }
  }
 /*----- VERSION ----- */
  if (test_ctcp(pi->msg1, "\1VERSION")) {
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    notice(pi->nick, "\1VERSION iroffer-dinoex " VERSIONLONG ", " "http://iroffer.dinoex.net/" "%s%s\1",
           gdata.hideos ? "" : " - ",
           gdata.hideos ? "" : gdata.osstring);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "[CTCP] %s on %s: VERSION",
            pi->nick, gnetwork->name);
    return 0;
  }
 /*----- UPTIME ----- */
  if (test_ctcp(pi->msg1, "\1UPTIME")) {
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    tempstr2 = mycalloc(maxtextlength);
    tempstr2 = getuptime(tempstr2, 0, gdata.startuptime, maxtextlength);
    notice(pi->nick, "\1UPTIME %s\1", tempstr2);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "[CTCP] %s on %s: UPTIME",
             pi->nick, gnetwork->name);
    mydelete(tempstr2);
    return 0;
  }
  /*----- STATUS ----- */
  if (test_ctcp(pi->msg1, "\1STATUS")) {
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    tempstr2 = mycalloc(maxtextlength);
    tempstr2 = getstatuslinenums(tempstr2, maxtextlength);
    notice(pi->nick, "\1%s\1", tempstr2);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
               "[CTCP] %s on %s: STATUS",
               pi->nick, gnetwork->name);
    mydelete(tempstr2);
    return 0;
  }
  /*----- DCC SEND/CHAT/RESUME ----- */
  if (test_ctcp(pi->msg1, "\1DCC")) {
    if (pi->msg2 == NULL)
      return 0; /* ignore */
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    caps(pi->msg2);
    command_dcc(pi);
    return 0;
  }

  /*----- ADMIN ----- */
  if (strcmp(pi->msg1, "ADMIN") == 0) {
    if (check_ignore(pi->nick, pi->hostmask))
      return 0;
    admin_message(pi->nick, pi->hostmask, pi->msg2, pi->line);
    return 0;
  }

  return 1;
}

static void autoqueuef(int pack, const char *message, privmsginput *pi)
{
  const char *format = "** Sending You %s by DCC";
  char *tempstr = NULL;

  updatecontext();

  ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "AutoSend ");
  gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
  if (message) {
    tempstr = mycalloc(strlen(message) + strlen(format) - 1);
    snprintf(tempstr, strlen(message) + strlen(format) - 1,
             format, message);
  }
  sendxdccfile(pi->nick, pi->hostname, pi->hostmask, pack, tempstr, NULL);
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
static int noticeresults(const char *nick, const char *pattern, const char *dest)
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
        snprintf(tempstr, maxtextlength, "XDCC SERVER - %s:[%i/%i]", j != 1 ? "slots" : "slot", j - irlist_size(&gdata.trans), j);
        len = strlen(tempstr);
        if (gdata.slotsmax <= irlist_size(&gdata.trans)) {
          snprintf(tempstr + len, maxtextlength - len, ", Queue:[%i/%i]", irlist_size(&gdata.mainqueue), gdata.queuesize);
          len = strlen(tempstr);
        }
        if (gdata.transferminspeed > 0) {
          snprintf(tempstr + len, maxtextlength - len, ", Min:%1.1fKB/s", gdata.transferminspeed);
          len = strlen(tempstr);
        }
        if (gdata.transfermaxspeed > 0) {
          snprintf(tempstr + len, maxtextlength - len, ", Max:%1.1fKB/s", gdata.transfermaxspeed);
          len = strlen(tempstr);
        }
        if (gdata.maxb) {
          snprintf(tempstr + len, maxtextlength - len, ", Cap:%u.0KB/s", gdata.maxb / 4);
          len = strlen(tempstr);
        }
        snprintf(tempstr + len, maxtextlength - len, " - /MSG %s XDCC SEND x -",
                 get_user_nick());
        len = strlen(tempstr);
        if (!strcmp(match, "*"))
          snprintf(tempstr + len, maxtextlength - len, " Packs:");
        else
          snprintf(tempstr + len, maxtextlength - len, " Found:");
      len = strlen(tempstr);
    }
    sizestrstr = sizestr(0, xd->st_size);
    snprintf(tempstr + len, maxtextlength - len, " #%i:%s,%s", i, xd->desc, sizestrstr);
    if (strlen(tempstr) > get_channel_limit(dest)) {
      snprintf(tempstr + len, maxtextlength - len, " [...]");
      notice_slow(nick, "%s", tempstr);
      snprintf(tempstr, maxtextlength, "[...] #%i:%s,%s", i, xd->desc, sizestrstr);
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

static int run_new_trigger(const char *nick, const char *grouplist)
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

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_GREEN,
          "%s from %s on %s logged, use MSGREAD to display it.",
          type_list[type], pi->nick, gnetwork->name);

  ml = irlist_add(&gdata.msglog, sizeof(msglog_t));

  ml->when = gdata.curtime;
  ml->hostmask = mystrdup(pi->hostmask);
  ml->message = mystrdup(begin);

  write_statefile();
}

static void privmsgparse2(int type, int decoded, privmsginput *pi)
{
  char *begin;
  int exclude;

  /*----- CTCP ----- */
  if (botonly_parse(type, pi) == 0) {
    gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
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
  if ((strcmp(pi->msg1, "XDCC") == 0) ||
      (strcmp(pi->msg1, "\1XDCC") == 0) ||
      (strcmp(pi->msg1, "CDCC") == 0) ||
      (strcmp(pi->msg1, "\1CDCC") == 0)) {
    if (!gdata.respondtochannelxdcc) {
      if (not_for_me(pi->dest))
        return; /* ignore */
    }
    if (check_ignore(pi->nick, pi->hostmask))
      return;
    if (pi->msg2 != NULL) {
      command_xdcc(pi);
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      return;
    }
  }

  /*----- !LIST ----- */
  if ((test_ctcp(pi->msg1, "\1!LIST")) ||
      (strcmp(pi->msg1, "!LIST") == 0)) {
    const char *rtclmsg;
    char *tempstr2;

    if (!gdata.respondtochannellist) {
      if (not_for_me(pi->dest))
        return; /* ignore */
    }
    if (pi->msg2 != NULL) {
      caps(pi->msg2);
      strip_trailing_action(pi->msg2);
      if (not_for_me(pi->msg2))
        return; /* ignore */
    }
    if (check_ignore(pi->nick, pi->hostmask))
      return;

    tempstr2 = mycalloc(maxtextlength);
    /* generate !list styled message */
    /* search for custom listmsg */
    rtclmsg = get_listmsg_channel(pi->dest);
    if (gdata.restrictprivlist)
      strcpy(tempstr2, "");
    else
      snprintf(tempstr2, maxtextlength,
               "Trigger:\2(\2/MSG %s XDCC LIST\2)\2 ",
               save_nick(gnetwork->user_nick));
    notice_slow(pi->nick,
           "\2(\2XDCC\2)\2 Packs:\2(\2%d\2)\2 "
           "%s%s"
           "%s"
           "Sends:\2(\2%i/%i\2)\2 "
           "Queues:\2(\2%i/%i\2)\2 "
           "Record:\2(\2%1.1fKB/s\2)\2 "
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
    gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
    return;
  }

  /* iroffer-lamm: @find */
  if (gdata.atfind && pi->msg2) {
    if ((strcmp(pi->msg1, "@FIND") == 0) ||
        (strcmp(pi->msg1, "!FIND") == 0)) {
      char *msg2e;
      int k;

      if (check_ignore(pi->nick, pi->hostmask))
        return;

      msg2e = getpart_eol(pi->line, 5);
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      k = convert_spaces_to_match(msg2e);
      if (k >= gdata.atfind) {
        k = noticeresults(pi->nick, msg2e, pi->dest);
        if (k) {
          ioutput(CALLTYPE_NORMAL, OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
                  "@FIND %s (%s on %s) - %i %s found.",
                  msg2e, pi->hostmask, gnetwork->name, k, k != 1 ? "packs" : "pack");
        }
      }
      mydelete(msg2e);
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      return;
    }
  }

  if (gdata.new_trigger) {
    if (strcmp(pi->msg1, "!NEW") == 0) {
      const char *grouplist;
      int k;

      if (check_ignore(pi->nick, pi->hostmask))
        return;

      /* apply per-channel visibility rules */
      grouplist = get_grouplist_channel(pi->dest);
      k = run_new_trigger(pi->nick, grouplist);
      if (k) {
        ioutput(CALLTYPE_NORMAL, OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
                "!NEW (%s on %s) - %i packs.",
                pi->hostmask, gnetwork->name, k);
      }
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
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
    exclude ++;
  if (verifyshell(&gdata.log_exclude_text, begin))
    exclude ++;

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
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_GREEN,
          "%s on %s: %s",
          type_list[type], gnetwork->name, pi->line);
}

static int get_nick_hostname(char *nick, char *hostname, const char* line)
{
  if (line && *line == ':')
    line++;
  for (; *line && *line != '!'; line++)
    *(nick++) = *line;
  *nick = 0;
  *hostname = 0;

  if (*line == 0)
    return 1;

  for (; *line && *line != '@'; line++)
    ;

  if (*line == 0)
    return 1;

  for (line++; *line && *line != ' '; line++)
    *(hostname++) = *line;
  *hostname = 0;
  return 0;
}

#define MAX_PRIVMSG_PARTS 9

void privmsgparse(int type, int decoded, char* line)
{
  privmsginput pi;
  char *part[MAX_PRIVMSG_PARTS];
  int m;
  int line_len;

  updatecontext();

  floodchk();
  if (gdata.ignore)
    return;

  bzero((char *)&pi, sizeof(pi));
  m = get_argv(part, line, MAX_PRIVMSG_PARTS);
  pi.line = line;
  pi.hostmask = part[0] + 1;
  pi.dest = part[2];
  pi.msg1 = part[3];
  pi.msg2 = part[4];
  pi.msg3 = part[5];
  pi.msg4 = part[6];
  pi.msg5 = part[7];
  pi.msg6 = part[8];

  caps(pi.dest);
  if (pi.msg1) {
    caps(++pi.msg1); /* point past the ":" */
  }

  line_len = sstrlen(pi.hostmask);
  pi.nick = mycalloc(line_len+1);
  pi.hostname = mycalloc(line_len+1);
  /* see if it came from a user or server, ignore if from server */
  if (get_nick_hostname(pi.nick, pi.hostname, pi.hostmask) == 0)
    privmsgparse2(type, decoded, &pi);
  mydelete(pi.nick);
  mydelete(pi.hostname);
  for (m = 0; m < MAX_PRIVMSG_PARTS; m++)
    mydelete(part[m]);
}

/* End of File */
