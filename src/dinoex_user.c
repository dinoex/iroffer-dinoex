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

  if (str[len] != '\1')
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

static const char *sendxdccinfo2(const char* nick, const char* hostname, const char* hostmask, int pack)
{
  userinput *pubinfo;
  xdcc *xd;
  char tempstr[maxtextlengthshort];

  updatecontext();

  if (gdata.disablexdccinfo) {
    notice(nick, "** XDCC INFO denied, disabled by configuration");
    return " ignored: ";
  }

  xd = get_download_pack(nick, hostname, hostmask, pack, NULL, "INFO", gdata.restrictlist);
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

static void sendxdccinfo(const char* nick,
                  const char* hostname,
                  const char* hostmask,
                  int pack)
{
  const char *msg;

  updatecontext();

  msg = sendxdccinfo2(nick, hostname, hostmask, pack);
  if (msg) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "%s", msg);
  }
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "%s (%s on %s)",
          nick, hostname, gnetwork->name);
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

static void command_dcc(privmsginput *pi)
{
  if (strcmp(pi->msg2, "RESUME") == 0) {
    if ((pi->msg3 == NULL) || (pi->msg4 == NULL) || (pi->msg5 == NULL))
      return;

    strip_trailing_action(pi->msg5);
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
    ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC SEND %s",
            pi->msg3);
    sendxdccfile(pi->nick, pi->hostname, pi->hostmask, packnumtonum(pi->msg3), NULL, pi->msg4);
    return;
  }

  if ((strcmp(pi->msg2, "INFO") == 0) && pi->msg3) {
    ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC INFO %s",
            pi->msg3);
    sendxdccinfo(pi->nick, pi->hostname, pi->hostmask, packnumtonum(pi->msg3));
    return;
  }
  if (strcmp(pi->msg2, "QUEUE") == 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC QUEUE (%s on %s)",
            pi->hostmask, gnetwork->name);
    notifyqueued_nick(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "STOP") == 0) {
    stoplist(pi->nick);
    return;
  }

  if (strcmp(pi->msg2, "CANCEL") == 0) {
    send_cancel(pi->nick, pi->hostmask);
    return;
  }

  if (strcmp(pi->msg2, "REMOVE") == 0) {
    send_remove(pi->nick, pi->hostmask);
    return;
  }

  if (strcmp(pi->msg2, "OWNER") == 0) {
    send_owner(pi->nick, pi->hostmask);
    return;
  }

  if (strcmp(pi->msg2, "HELP") == 0) {
    send_help(pi->nick, pi->hostmask);
    return;
  }

  if (gdata.send_batch) {
    if ((strcmp(pi->msg2, "BATCH") == 0) && pi->msg3 && pi->msg4) {
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
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
             "XDCC SEARCH %s (%s on %s)",
             msg3e, pi->hostmask, gnetwork->name);
    mydelete(msg3e);
    return;
  }
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC unsupported (%s on %s) ",
          pi->hostmask, gnetwork->name);
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

static void autoqueuef(const char* line, int pack, const char *message)
{
  char *nick, *hostname, *hostmask;
  int i;
  int line_len;

  updatecontext();

  floodchk();

  line_len = sstrlen(line);
  nick = mycalloc(line_len+1);
  hostname = mycalloc(line_len+1);

  hostmask = caps(getpart(line, 1));
  for (i=1; i<=sstrlen(hostmask); i++)
     hostmask[i-1] = hostmask[i];

  get_nick_hostname(nick, hostname, line);

  if ( !gdata.ignore ) {
    char *tempstr = NULL;
    const char *format = "** Sending You %s by DCC";

    gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;

    ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "AutoSend ");

    if (message) {
      tempstr = mycalloc(strlen(message) + strlen(format) - 1);
      snprintf(tempstr, strlen(message) + strlen(format) - 1,
               format, message);
    }

    sendxdccfile(nick, hostname, hostmask, pack, tempstr, NULL);
    mydelete(tempstr);
  }

  mydelete(hostmask);
  mydelete(nick);
  mydelete(hostname);
}

static int check_trigger(const char *line, int type, const char *nick, const char *hostmask, const char *msg)
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

  if (check_trigger(pi->line, type, pi->nick, pi->hostmask, pi->msg1)) {
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

  caps(pi.hostmask);
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
