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
#include "dinoex_transfer.h"
#include "dinoex_irc.h"

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
    privmsg_fast(nick, "\1DCC SEND %s %s %" LLPRINTFMT "u %d\1",
                 sendnamestr, dccdata,
                 (unsigned long long)tr->xpack->st_size,
                 tr->id);
  } else {
    privmsg_fast(nick, "\1DCC SEND %s %s %" LLPRINTFMT "u\1",
                 sendnamestr, dccdata,
                 (unsigned long long)tr->xpack->st_size);

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "listen on port %d for %s (%s on %s)",
            tr->con.localport, nick, tr->hostname, gnetwork->name);
  }
  mydelete(dccdata);
  mydelete(sendnamestr);
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

int t_find_transfer(char *nick, char *filename, char *remoteip, char *remoteport, char *token)
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
  outerror(OUTERROR_TYPE_WARN,
           "Couldn't find transfer that %s on %s tried to resume!",
           nick, gnetwork->name);
  if (gdata.debug == 0)
    return 1;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
            "resume trying %i: %s == %s, %s == %s, %i == %i\n",
            tr->tr_status,
            tr->caps_nick, nick,
            tr->xpack->file,
            filename,
            tr->con.localport,
            atoi(remoteport));
  }
  return 1;
}

void t_connected(transfer *tr)
{
  int callval_i;
  int connect_error;
  unsigned int connect_error_len = sizeof(connect_error);

  callval_i = getsockopt(tr->con.clientsocket,
                         SOL_SOCKET, SO_ERROR,
                         &connect_error, &connect_error_len);

  if (callval_i < 0) {
    outerror(OUTERROR_TYPE_WARN,
             "Couldn't determine upload connection status on %s: %s",
             gnetwork->name, strerror(errno));
    t_closeconn(tr, "Download Connection Failed status:", errno);
  } else if (connect_error) {
    t_closeconn(tr, "Download Connection Failed", connect_error);
  }

  if ((callval_i < 0) || connect_error) {
    FD_CLR(tr->con.clientsocket, &gdata.writeset);
  } else {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Download Connection Established on %s", gnetwork->name);
    FD_CLR(tr->con.clientsocket, &gdata.writeset);
    tr->con.connecttime = gdata.curtime;
    if (set_socket_nonblocking(tr->con.clientsocket, 0) < 0 ) {
      outerror(OUTERROR_TYPE_WARN, "Couldn't Set Blocking");
    }
  }

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

    t_closeconn(tr, "You are being punished for pararell downloads", 0);
    bhostmask = to_hostmask( "*", tr->hostname);
    for (ignore = irlist_get_head(&gdata.ignorelist);
         ignore;
         ignore = irlist_get_next(ignore)) {
      
      if (fnmatch(ignore->hostmask, bhostmask, FNM_CASEFOLD) == 0)
        break;
    }

    if (!ignore) {
      ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));

      ignore->hostmask = mystrdup(bhostmask);

      ignore->flags |= IGN_IGNORING;
      ignore->lastcontact = gdata.curtime;
    }

    ignore->flags |= IGN_MANUAL;
    ignore->bucket = (num*60)/gdata.autoignore_threshold;

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "same IP detected, Ignore activated for %s which will last %i min",
            bhostmask, num);
    mydelete(bhostmask);
  }

  write_statefile();
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
