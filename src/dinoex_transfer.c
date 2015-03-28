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
#include "dinoex_transfer.h"
#include "dinoex_irc.h"
#include "dinoex_geoip.h"
#include "dinoex_queue.h"
#include "dinoex_misc.h"

/* send DCC command to start download */
void t_start_dcc_send(transfer *tr)
{
  char *dccdata;
  char *sendnamestr;

  updatecontext();

  sendnamestr = getsendname(tr->xpack->file);
  dccdata = setup_dcc_local(&tr->con.local);
  if (tr->passive_dcc) {
    bzero((char *) &(tr->con.local), sizeof(tr->con.local));
    privmsg_fast(tr->nick, IRC_CTCP "DCC SEND %s %s %" LLPRINTFMT "d %u" IRC_CTCP, /* NOTRANSLATE */
                 sendnamestr, dccdata,
                 tr->xpack->st_size,
                 tr->id);
  } else {
    privmsg_fast(tr->nick, IRC_CTCP "DCC SEND %s %s %" LLPRINTFMT "d" IRC_CTCP, /* NOTRANSLATE */
                 sendnamestr, dccdata,
                 tr->xpack->st_size);

    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "listen on port %d for %s (%s on %s)",
            tr->con.localport, tr->nick, tr->hostname, gnetwork->name);
  }
  mydelete(dccdata);
  mydelete(sendnamestr);
}

/* start normal or passive DCC download */
void t_setup_dcc(transfer *tr)
{
  char *vhost;
  int e = 1;

  updatecontext();

  vhost = get_local_vhost();
  if (tr->passive_dcc) {
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

  t_start_dcc_send(tr);
}

/* create a new transfer */
transfer *create_transfer(xdcc *xd, const char *nick, const char *hostname)
{
  transfer *tr;
  dcc_options_t *dcc_options;

  ++(xd->file_fd_count);
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
  tr->quietmode = gdata.quietmode;
  tr->passive_dcc = gdata.passive_dcc;
  tr->con.family = gnetwork->myip.sa.sa_family;
  tr->con.localport = 0;
  dcc_options = get_options(tr->nick);
  if (dcc_options != NULL) {
    if ((dcc_options->options & DCC_OPTION_IPV4) != 0)
      tr->con.family = AF_INET;
    if ((dcc_options->options & DCC_OPTION_IPV6) != 0)
      tr->con.family = AF_INET6;
    if ((dcc_options->options & DCC_OPTION_ACTIVE) != 0)
      tr->passive_dcc = 0;
    if ((dcc_options->options & DCC_OPTION_PASSIVE) != 0)
      tr->passive_dcc = 1;
    if ((dcc_options->options & DCC_OPTION_QUIET) != 0)
      tr->quietmode = 1;
  }
  return tr;
}

static void t_unlmited2(transfer * const tr, const char *hostmask)
{
  tr->unlimited = verifyshell(&gdata.unlimitedhost, hostmask);
  if (!tr->unlimited)
    return;

  tr->nomax = tr->unlimited;
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "unlimitedhost found: %s (%s on %s)",
           tr->nick, hostmask, gnetwork->name);
}

/* check for a match with unlimitedhost */
void t_unlmited(transfer * const tr, const char *hostmask)
{
  char *qhostmask;

  if (hostmask != NULL) {
    t_unlmited2(tr, hostmask);
    return;
  }

  qhostmask = to_hostmask(tr->nick, tr->hostname);
  t_unlmited2(tr, qhostmask);
  mydelete(qhostmask);
}

static void notice_transfer(const char *nick, xdcc *xd, const char *msg)
{
  char *sizestrstr;

  sizestrstr = sizestr(0, xd->st_size);
  notice_fast(nick, "%s, which is %sB. (resume supported)", msg, sizestrstr);
  mydelete(sizestrstr);
}

/* inform the user that the bot is starting a transfer */
void t_notice_transfer(transfer * const tr, const char *msg, unsigned int pack, unsigned int queue)
{
  char *tempstr;

  if (tr->quietmode)
     return;

  if (pack == XDCC_SEND_LIST)
     return;

  if (msg != NULL) {
    notice_transfer(tr->nick, tr->xpack, msg);
    return;
  }

  tempstr = mymalloc(maxtextlength);
  if (queue) {
    snprintf(tempstr, maxtextlength, "** Sending you queued pack #%u (\"%s\")", pack, tr->xpack->desc);
  } else {
    snprintf(tempstr, maxtextlength, "** Sending you pack #%u (\"%s\")", pack, tr->xpack->desc);
  }
  notice_transfer(tr->nick, tr->xpack, tempstr);
  mydelete(tempstr);
}

/* check ip for matching blacklist and whitelist */
unsigned int t_check_ip_access(transfer *const tr)
{
  const char *msg;
  if (irlist_size(&gdata.xdcc_allow) > 0) {
    if (!verify_cidr(&gdata.xdcc_allow, &(tr->con.remote))) {
      msg = "Sorry, downloads to your IP not allowed, ask owner.";
      t_closeconn(tr, msg, 0);
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "%s", msg);
      return 1;
    }
  }

  if (verify_cidr(&gdata.xdcc_deny, &(tr->con.remote))) {
    msg = "Sorry, downloads to your IP denied, ask owner.";
    t_closeconn(tr, msg, 0);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "%s", msg);
    return 1;
  }
  return 0;
}

static void t_passive(transfer * const tr, ir_uint16 remoteport)
{
  char *msg;
  ir_uint32 netval;
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
    netval = atoul(tr->con.remoteaddr);
    tr->con.remote.sin.sin_addr.s_addr = htonl(netval);
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
    msg = mymalloc(maxtextlength);
    my_getnameinfo(msg, maxtextlength -1, &(tr->con.remote.sa));
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
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
          "resume trying %s, %s, %d",
          nick, filename, atoi(remoteport));
  if (gdata.debug == 0)
    return;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    ioutput(OUT_S, COLOR_NO_COLOR,
            "transfer %u: %s on %s, %s, %d\n",
            tr->tr_status,
            tr->caps_nick,
            gdata.networks[tr->net].name,
            tr->xpack->file,
            tr->con.localport);
  }
}

/* search the DDC transfer a user wants to accept */
unsigned int t_find_transfer(const char *nick, const char *filename, const char *remoteip, const char *remoteport, const char *token)
{
  transfer *tr;
  unsigned int myid;

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
    t_passive(tr, (unsigned)atoi(remoteport));
    return 1;
  }
  t_find_debug(nick, filename, remoteport);
  return 1;
}

/* search the DDC transfer a user wants to resume */
unsigned int t_find_resume(const char *nick, const char *filename, const char *localport, const char *bytes, char *token)
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

    /* the filename can be converted */
    if (guess == NULL)
      guess = tr;
    if (strcasestr(tr->xpack->file, filename))
      break;
    if (tr->con.localport == (unsigned)atoi(localport))
      break;
  }
  if (tr == NULL) {
    if (guess != NULL) {
      outerror(OUTERROR_TYPE_WARN,
               "Guessed transfer that %s on %s tried to resume!",
               nick, gnetwork->name);
      outerror(OUTERROR_TYPE_WARN,
               "resume trying %s, %s, %d",
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
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC [%02i:%s on %s]: Resume attempted beyond end of file ( %" LLPRINTFMT "d >= %" LLPRINTFMT "d )",
            tr->id, tr->nick, gnetwork->name, len,
            tr->xpack->st_size);
    return 1;
  }
  t_setresume(tr, bytes);
  sendnamestr = getsendname(filename);
  if ((tr->tr_status == TRANSFER_STATUS_RESUME) && (token != NULL)) {
    privmsg_fast(nick, IRC_CTCP "DCC ACCEPT %s %s %s %s" IRC_CTCP, sendnamestr, localport, bytes, token); /* NOTRANSLATE */
  } else {
    privmsg_fast(nick, IRC_CTCP "DCC ACCEPT %s %s %s" IRC_CTCP, sendnamestr, localport, bytes); /* NOTRANSLATE */
  }
  mydelete(sendnamestr);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC [%02i:%s on %s]: Resumed at %" LLPRINTFMT "d, %" LLPRINTFMT "dK", tr->id,
          tr->nick, gnetwork->name, tr->startresume, tr->startresume/1024);
  return 0;
}

/* process successful connection */
static void t_connected(transfer *tr)
{
  int callval_i;
  int connect_error;
  SIGNEDSOCK int connect_error_len = sizeof(connect_error);

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

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Download Connection Established on %s", gnetwork->name);

  t_setup_send(tr);
  return;
}

/* check if a transfer will use more  connections per user than allowed */
static void t_check_duplicateip(transfer *const newtr)
{
  igninfo *ignore;
  char *bhostmask;
  transfer *tr;
  unsigned int found;
  unsigned int num;

  if (gdata.ignore_duplicate_ip == 0)
    return;

  if (gdata.maxtransfersperperson == 0)
    return;

  updatecontext();

  if (newtr->con.family != AF_INET)
    return;

  found = 0;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;
    if (tr->remoteip != newtr->remoteip)
      continue;
    if (!strcmp(tr->hostname, "man")) /* NOTRANSLATE */
      continue;

    ++found;
  }

  if (found <= gdata.maxtransfersperperson)
    return;

  num = gdata.ignore_duplicate_ip * 60; /* n hours */
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;
    if (tr->remoteip != newtr->remoteip)
      continue;
    if (!strcmp(tr->hostname, "man")) /* NOTRANSLATE */
      continue;

    t_closeconn(tr, "You are being punished for parallel downloads", 0);
    queue_punish_abuse( "You are being punished for parallel downloads", tr->net, tr->nick);

    bhostmask = to_hostmask( "*", tr->hostname); /* NOTRANSLATE */
    ignore = get_ignore(bhostmask);
    ignore->flags |= IGN_IGNORING;
    ignore->flags |= IGN_MANUAL;
    ignore->bucket = (num*60)/gdata.autoignore_threshold;

    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "same IP detected, Ignore activated for %s which will last %u min",
            bhostmask, num);
    mydelete(bhostmask);
  }

  write_statefile();
}

/* check a new transfer */
static void t_check_new_connection(transfer *const tr)
{
  t_establishcon(tr);
#ifdef USE_GEOIP
  updatecontext();
  geoip_new_connection(tr);
#endif /* USE_GEOIP */
  updatecontext();
  t_check_duplicateip(tr);
}

/* find out how many bytes the user has received */
unsigned int verify_acknowlede(transfer *tr)
{
  unsigned int show = 0;
  off_t halfack;

  if (tr->mirc_dcc64 != 0)
    return show;

  if (tr->firstack == 0) {
    tr->halfack = tr->curack;
    tr->firstack = tr->curack;
    ++show;
    if (tr->firstack <= tr->startresume) {
      if (tr->xpack->st_size > 0xFFFFFFFFLL) {
         tr->mirc_dcc64 = 1;
         tr->curack = tr->firstack << 32;
         ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
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

/* returns a text for the state of the download */
const char *t_print_state(transfer *const tr)
{
  switch (tr->tr_status) {
  case TRANSFER_STATUS_LISTENING:
    return "Listening";
  case TRANSFER_STATUS_SENDING:
    return "Sending";
  case TRANSFER_STATUS_WAITING:
    return "Finishing";
  case TRANSFER_STATUS_DONE:
    return "Closing";
  case TRANSFER_STATUS_RESUME:
    return "Resume";
  case TRANSFER_STATUS_CONNECTING:
    return "Connecting";
  case TRANSFER_STATUS_UNUSED:
  default:
    return "Unknown!";
  }
}

/* register active connections for select() */
int t_select_fdset(int highests, int changequartersec)
{
  transfer *tr;
  unsigned long sum;
  unsigned int overlimit;

  sum = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 1)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 2)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 3)%XDCC_SENT_SIZE];
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
        continue;
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

/* select a transfer to start with */
static unsigned int select_starting_transfer(void)
{
  unsigned int t;

  t = gdata.cursendptr;
  if (++t >= irlist_size(&gdata.trans))
    t = 0;
  gdata.cursendptr = t;
  return ++t;
}

/* handle transfer ip events */
void t_perform(int changesec, int changequartersec)
{
  transfer *tr;
  unsigned int i, j;

  updatecontext();

  if (changequartersec) {
    for (tr = irlist_get_head(&gdata.trans);
         tr;
         tr = irlist_get_next(tr)) {
      if (tr->nomax)
        continue;

      if (tr->maxspeed <= 0)
        continue;

      tr->tx_bucket += tr->maxspeed * (1024 / 4);
      tr->tx_bucket = min2(tr->tx_bucket, MAX_TRANSFER_TX_BURST_SIZE * tr->maxspeed * 1024);
    }
  }

  i = j = select_starting_transfer();

  /* first: do from cur to end */
  for (tr = irlist_get_nth(&gdata.trans, i);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;

    /*----- look for transfer some -----  send at least once a second, or more if necessary */
    if (changequartersec || FD_ISSET(tr->con.clientsocket, &gdata.writeset)) {
      gnetwork = &(gdata.networks[tr->net]);
      t_transfersome(tr);
    }
  }

  /* second: do from begin to cur-1 */
  for (tr = irlist_get_head(&gdata.trans);
       tr && j;
       tr = irlist_get_next(tr), --j) {
    if (tr->tr_status != TRANSFER_STATUS_SENDING)
      continue;

    /*----- look for transfer some -----  send at least once a second, or more if necessary */
    if (changequartersec || FD_ISSET(tr->con.clientsocket, &gdata.writeset)) {
      gnetwork = &(gdata.networks[tr->net]);
      t_transfersome(tr);
    }
  }

  tr = irlist_get_head(&gdata.trans);
  while(tr) {
    gnetwork = &(gdata.networks[tr->net]);

    /*----- look for listen->connected ----- */
    if (tr->tr_status == TRANSFER_STATUS_LISTENING) {
      if (FD_ISSET(tr->con.listensocket, &gdata.readset)) {
        t_check_new_connection(tr);
        tr = irlist_get_next(tr);
        continue;
      }
      /*----- look for listen reminders ----- */
      if (changesec) {
        if (tr->reminded == 0) {
          if ((gdata.curtime - tr->con.lastcontact) >= 30)
            t_remind(tr);
        }
        if (tr->reminded == 1) {
          if ((gdata.curtime - tr->con.lastcontact) >= 90)
            t_remind(tr);
        }
        if (tr->reminded == 2) {
          if ((gdata.curtime - tr->con.lastcontact) >= 150)
            t_remind(tr);
        }
      }
    }

    if (tr->tr_status == TRANSFER_STATUS_CONNECTING) {
      if (FD_ISSET(tr->con.clientsocket, &gdata.writeset))
        t_connected(tr);
    }

    /*----- look for junk to read ----- */
    if ((tr->tr_status == TRANSFER_STATUS_SENDING) ||
        (tr->tr_status == TRANSFER_STATUS_WAITING)) {
      if (FD_ISSET(tr->con.clientsocket, &gdata.readset))
        t_readjunk(tr);
    }

    /*----- look for done flushed status ----- */
    if (tr->tr_status == TRANSFER_STATUS_WAITING) {
      t_flushed(tr);
    }

    if (changesec) {
      /*----- look for lost transfers ----- */
      if (tr->tr_status != TRANSFER_STATUS_DONE) {
        t_istimeout(tr);
      }

      /*----- look for finished transfers ----- */
      if (tr->tr_status == TRANSFER_STATUS_DONE) {
        char *trnick;

        trnick = tr->nick;
        mydelete(tr->caps_nick);
        mydelete(tr->hostname);
        mydelete(tr->con.localaddr);
        mydelete(tr->con.remoteaddr);
        mydelete(tr->country);
        tr = irlist_delete(&gdata.trans, tr);

        if ( check_main_queue( gdata.slotsmax ) ) {
          send_from_queue(0, 0, trnick);
        }
        mydelete(trnick);
        continue;
      }
    }
    tr = irlist_get_next(tr);
  }
  gnetwork = NULL;
}

/* End of File */
