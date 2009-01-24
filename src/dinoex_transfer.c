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
#include "dinoex_transfer.h"
#include "dinoex_irc.h"
#include "dinoex_misc.h"

void t_setup_dcc(transfer *tr, const char *nick)
{
  char *dccdata;
  char *sendnamestr;
  char *vhost;
  int e = 1;

  updatecontext();

  vhost = get_local_vhost();
  if (gdata.passive_dcc) {
    bzero((char *) &(tr->con.local), sizeof(tr->con.local));
    if (tr->con.family == AF_INET) {
      tr->con.local.sin.sin_family = AF_INET;
      tr->con.local.sin.sin_port = htons(0);
      if (vhost) {
        e = inet_pton(tr->con.family, vhost, &(tr->con.local.sin.sin_addr));
      } else {
        tr->con.local.sin.sin_addr.s_addr = gnetwork->myip.sin.sin_addr.s_addr;
      }
    } else {
      tr->con.local.sin6.sin6_family = AF_INET6;
      tr->con.local.sin6.sin6_port = htons(0);
      if (vhost) {
        e = inet_pton(tr->con.family, vhost, &(tr->con.local.sin6.sin6_addr));
      }
    }
    if (e != 1) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", vhost);
    }
    tr->tr_status = TRANSFER_STATUS_RESUME;
    tr->con.localport = 0;
  } else {
    t_setuplisten(tr);

    if (tr->tr_status != TRANSFER_STATUS_LISTENING)
      return;
  }

  sendnamestr = getsendname(tr->xpack->file);
  dccdata = setup_dcc_local(&tr->con.local);
  if (gdata.passive_dcc) {
    bzero((char *) &(tr->con.local), sizeof(tr->con.local));
    privmsg_fast(nick, "\1DCC SEND %s %s %" LLPRINTFMT "d %d\1",
                 sendnamestr, dccdata,
                 tr->xpack->st_size,
                 tr->id);
  } else {
    privmsg_fast(nick, "\1DCC SEND %s %s %" LLPRINTFMT "d\1",
                 sendnamestr, dccdata,
                 tr->xpack->st_size);

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "listen on port %d for %s (%s on %s)",
            tr->con.localport, nick, tr->hostname, gnetwork->name);
  }
  mydelete(dccdata);
  mydelete(sendnamestr);
}

int t_check_ip_access(transfer *const tr)
{
  const char *msg;
  if (irlist_size(&gdata.xdcc_allow) > 0) {
    if (!verify_cidr(&gdata.xdcc_allow, &(tr->con.remote))) {
      msg = "Sorry, downloads to your IP not allowed, ask owner.";
      t_closeconn(tr, msg, 0);
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "%s", msg);
      return 1;
    }
  }

  if (verify_cidr(&gdata.xdcc_deny, &(tr->con.remote))) {
    msg = "Sorry, downloads to your IP denied, ask owner.";
    t_closeconn(tr, msg, 0);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "%s", msg);
    return 1;
  }
  return 0;
}

static void t_passive(transfer * const tr, unsigned short remoteport)
{
  char *msg;
  SIGNEDSOCK int addrlen;
  int retval;

  updatecontext();

  bzero ((char *) &(tr->con.remote), sizeof(tr->con.remote));
  tr->con.clientsocket = socket(tr->con.family, SOCK_STREAM, 0);
  if (tr->con.clientsocket < 0) {
      t_closeconn(tr, "Socket Error", errno);
      return;
  }

  if (tr->con.family == AF_INET ) {
    addrlen = sizeof(struct sockaddr_in);
    tr->con.remote.sin.sin_family = AF_INET;
    tr->con.remote.sin.sin_port = htons(remoteport);
    tr->con.remote.sin.sin_addr.s_addr = htonl(atoul(tr->con.remoteaddr));
  } else {
    addrlen = sizeof(struct sockaddr_in6);
    tr->con.remote.sin6.sin6_family = AF_INET6;
    tr->con.remote.sin6.sin6_port = htons(remoteport);
    retval = inet_pton(AF_INET6, tr->con.remoteaddr, &(tr->con.remote.sin6.sin6_addr));
    if (retval < 0)
      outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", tr->con.remoteaddr);
  }

  if (t_check_ip_access(tr))
    return;

  if (bind_irc_vhost(tr->con.family, tr->con.clientsocket) != 0) {
    t_closeconn(tr, "Couldn't Bind Virtual Host, Sorry", errno);
    return;
  }

  if (set_socket_nonblocking(tr->con.clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  if (gdata.debug > 0) {
    msg = mycalloc(maxtextlength);
    my_getnameinfo(msg, maxtextlength -1, &(tr->con.remote.sa), addrlen);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC SEND passive sent to %s on %s, connecting to %s",
            tr->nick, gnetwork->name, msg);
    mydelete(msg);
  }

  alarm(CTIMEOUT);
  retval = connect(tr->con.clientsocket, &(tr->con.remote.sa), addrlen);
  if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) ) {
    t_closeconn(tr, "Couldn't Connect", errno);
    alarm(0);
    return;
  }
  alarm(0);

  tr->tr_status = TRANSFER_STATUS_CONNECTING;
}

static void t_find_debug(const char *nick, const char *filename, const char *remoteport)
{
  transfer *tr;

  outerror(OUTERROR_TYPE_WARN,
           "Couldn't find transfer that %s on %s tried to resume!",
           nick, gnetwork->name);
  outerror(OUTERROR_TYPE_WARN,
          "resume trying %s, %s, %i",
          nick, filename, atoi(remoteport));
  if (gdata.debug == 0)
    return;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
            "transfer %i: %s on %s, %s, %i\n",
            tr->tr_status,
            tr->caps_nick,
            gdata.networks[tr->net].name,
            tr->xpack->file,
            tr->con.localport);
  }
}

int t_find_transfer(const char *nick, const char *filename, const char *remoteip, const char *remoteport, const char *token)
{
  transfer *tr;
  int myid;

  if (atoi(remoteport) == 0)
    return 0;

  myid = atoi(token);
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_RESUME)
      continue;
    if (tr->id != myid)
      continue;
    if (strcasecmp(tr->caps_nick, nick))
      continue;

    tr->con.remoteaddr = mystrdup(remoteip);
    t_passive(tr, atoi(remoteport));
    return 1;
  }
  t_find_debug(nick, filename, remoteport);
  return 1;
}

int t_find_resume(const char *nick, const char *filename, const char *localport, const char *bytes, char *token)
{
  char *sendnamestr;
  transfer *guess;
  transfer *tr;
  off_t len;

  guess = NULL;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if ((tr->tr_status != TRANSFER_STATUS_LISTENING) && (tr->tr_status != TRANSFER_STATUS_RESUME))
      continue;
    if (strcasecmp(tr->caps_nick, nick))
      continue;

    if (guess == NULL)
      guess = tr;
    if (strcasestr(tr->xpack->file, filename))
      break;
    if (tr->con.localport == atoi(localport))
      break;
  }
  if (tr == NULL) {
    if (guess != NULL) {
      outerror(OUTERROR_TYPE_WARN,
               "Guessed transfer that %s on %s tried to resume!",
               nick, gnetwork->name);
      outerror(OUTERROR_TYPE_WARN,
               "resume trying %s, %s, %i",
               nick, filename, atoi(localport));
      tr = guess;
    } else {
      t_find_debug(nick, filename, localport);
      return 1;
    }
  }
  len = atoull(bytes);
  if (len >= tr->xpack->st_size) {
    notice(nick, "You can't resume the transfer at a point greater than the size of the file");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC [%02i:%s on %s]: Resume attempted beyond end of file ( %" LLPRINTFMT "d >= %" LLPRINTFMT "d )",
            tr->id, tr->nick, gnetwork->name, len,
            tr->xpack->st_size);
    return 1;
  }
  t_setresume(tr, bytes);
  sendnamestr = getsendname(filename);
  if ((tr->tr_status == TRANSFER_STATUS_RESUME) && (token != NULL)) {
    if (token[strlen(token)-1] == '\1') token[strlen(token)-1] = '\0';
    privmsg_fast(nick, "\1DCC ACCEPT %s %s %s %s\1", sendnamestr, localport, bytes, token);
  } else {
    privmsg_fast(nick, "\1DCC ACCEPT %s %s %s\1", sendnamestr, localport, bytes);
  }
  mydelete(sendnamestr);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC [%02i:%s on %s]: Resumed at %" LLPRINTFMT "dK", tr->id,
          tr->nick, gnetwork->name, tr->startresume/1024);
  return 0;
}

void t_connected(transfer *tr)
{
  int callval_i;
  int connect_error;
  unsigned int connect_error_len = sizeof(connect_error);

  tr->con.connecttime = gdata.curtime;
  FD_CLR(tr->con.clientsocket, &gdata.writeset);
  callval_i = getsockopt(tr->con.clientsocket,
                         SOL_SOCKET, SO_ERROR,
                         &connect_error, &connect_error_len);

  if (callval_i < 0) {
    outerror(OUTERROR_TYPE_WARN,
             "Couldn't determine upload connection status on %s: %s",
             gnetwork->name, strerror(errno));
    t_closeconn(tr, "Download Connection Failed status:", errno);
    return;
  }
  if (connect_error) {
    t_closeconn(tr, "Download Connection Failed", connect_error);
    return;
  }

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Download Connection Established on %s", gnetwork->name);

  t_setup_send(tr);
  return;
}

void t_check_duplicateip(transfer *const newtr)
{
  igninfo *ignore;
  char *bhostmask;
  transfer *tr;
  int found;
  int num;

  if (!gdata.ignoreduplicateip)
    return;

  if (gdata.maxtransfersperperson == 0)
    return;

  updatecontext();

  if (newtr->con.family != AF_INET)
    return;

  num = 24 * 60; /* 1 day */
  found = 0;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;
    if (tr->remoteip != newtr->remoteip)
      continue;
    if (!strcmp(tr->hostname, "man"))
      continue;

    found ++;
  }

  if (found <= gdata.maxtransfersperperson)
    return;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;
    if (tr->remoteip != newtr->remoteip)
      continue;
    if (!strcmp(tr->hostname, "man"))
      continue;

    t_closeconn(tr, "You are being punished for parallel downloads", 0);
    bhostmask = to_hostmask( "*", tr->hostname);
    ignore = get_ignore(bhostmask);
    ignore->flags |= IGN_IGNORING;
    ignore->flags |= IGN_MANUAL;
    ignore->bucket = (num*60)/gdata.autoignore_threshold;

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "same IP detected, Ignore activated for %s which will last %i min",
            bhostmask, num);
    mydelete(bhostmask);
  }

  write_statefile();
}

int verify_acknowlede(transfer *tr)
{
  int show = 0;
  off_t halfack;

  if (tr->mirc_dcc64 != 0)
    return show;

  if (tr->firstack == 0) {
    tr->halfack = tr->curack;
    tr->firstack = tr->curack;
    show ++;
    if (tr->firstack <= tr->startresume) {
      if (tr->xpack->st_size > 0xFFFFFFFFLL) {
         tr->mirc_dcc64 = 1;
         tr->curack = tr->firstack << 32;
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                 "XDCC [%02i:%s on %s]: Acknowleged %" LLPRINTFMT "d Bytes, forcing 64bit",
                 tr->id, tr->nick, gdata.networks[ tr->net ].name,
                 tr->firstack );
      }
    }
    if (tr->firstack == 0)
      tr->firstack = 64;
  }

  if (tr->mirc_dcc64 == 0) {
    if (tr->xpack->st_size > 0x0FFFFFFFFLL) {
      halfack = tr->halfack;
      tr->halfack = tr->curack;
      while (tr->curack < tr->lastack) {
        if (tr->curack < halfack)
          outerror(OUTERROR_TYPE_WARN,
                   "XDCC [%02i:%s on %s]: Acknowleged %" LLPRINTFMT "d Bytes after %" LLPRINTFMT "d Bytes",
                   tr->id, tr->nick, gdata.networks[ tr->net ].name,
                   tr->curack, tr->lastack);
        tr->curack += 0x100000000LL;
      }
    }
  }
  return show;
}

int t_select_fdset(int highests, int changequartersec)
{
  transfer *tr;
  unsigned long sum;
  int overlimit;

  sum = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime-1)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime-2)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime-3)%XDCC_SENT_SIZE];
  overlimit = (gdata.maxb && (sum >= gdata.maxb*1024));
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status == TRANSFER_STATUS_LISTENING) {
      FD_SET(tr->con.listensocket, &gdata.readset);
      highests = max2(highests, tr->con.listensocket);
      continue;
    }
    if (tr->tr_status == TRANSFER_STATUS_CONNECTING) {
      FD_SET(tr->con.clientsocket, &gdata.writeset);
      highests = max2(highests, tr->con.clientsocket);
      continue;
    }
    if (tr->tr_status == TRANSFER_STATUS_SENDING) {
      if (!overlimit && !tr->overlimit) {
        FD_SET(tr->con.clientsocket, &gdata.writeset);
        highests = max2(highests, tr->con.clientsocket);
      }
      if (changequartersec || ((tr->bytessent - tr->lastack) > 512*1024)) {
        FD_SET(tr->con.clientsocket, &gdata.readset);
        highests = max2(highests, tr->con.clientsocket);
      }
      continue;
    }
    if (tr->tr_status == TRANSFER_STATUS_WAITING) {
      FD_SET(tr->con.clientsocket, &gdata.readset);
      highests = max2(highests, tr->con.clientsocket);
      continue;
    }
  }
  return highests;
}

/* End of File */
