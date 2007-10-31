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
#include "dinoex_irc.h"
#include "dinoex_telnet.h"

static int telnet_listen[MAX_VHOSTS];
static int telnet_family[MAX_VHOSTS];

void telnet_close_listen(void)
{
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (telnet_listen[i] != FD_UNUSED) {
      FD_CLR(telnet_listen[i], &gdata.readset);
      close(telnet_listen[i]);
      telnet_listen[i] = FD_UNUSED;
    }
  }
}

static int telnet_open_listen(int i)
{
  char *vhost = NULL;
  char *msg;
  int rc;
  ir_sockaddr_union_t listenaddr;

  updatecontext();

  vhost = irlist_get_nth(&gdata.telnet_vhost, i);
  if (vhost == NULL)
    return 1;

  rc = open_listen(0, &listenaddr, &(telnet_listen[i]), gdata.telnet_port, 1, 0, vhost);
  if (rc != 0)
    return 1;

  telnet_family[i] = listenaddr.sa.sa_family;
  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &listenaddr.sa, 0);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Telnet SERVER waiting for connection on %s",  msg);
  mydelete(msg);
  return 0;
}

int telnet_setup_listen(void)
{
  int i;
  int rc = 0;

  updatecontext();

  for (i=0; i<MAX_VHOSTS; i++) {
    telnet_listen[i] = FD_UNUSED;
    telnet_family[i] = 0;
  }

  if (gdata.telnet_port == 0)
    return 1;

  for (i=0; i<MAX_VHOSTS; i++) {
    rc += telnet_open_listen(i);
  }
  return rc;
}

void telnet_reash_listen(void)
{
  telnet_close_listen();
  if (gdata.telnet_port == 0)
    return;
  telnet_setup_listen();
}

int telnet_select_listen(int highests)
{
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (telnet_listen[i] != FD_UNUSED) {
      FD_SET(telnet_listen[i], &gdata.readset);
      highests = max2(highests, telnet_listen[i]);
    }
  }
  return highests;
}

/* connections */

static void telnet_accept(int i)
{
  gnetwork_t *backup;
  char *msg;
  dccchat_t *chat;
  ir_sockaddr_union_t remoteaddr;
  ir_sockaddr_union_t localaddr;
  SIGNEDSOCK int addrlen;
  int clientsocket;

  updatecontext();

  if (telnet_family[i] != AF_INET) {
    addrlen = sizeof (struct sockaddr_in6);
    clientsocket = accept(telnet_listen[i], &remoteaddr.sa, &addrlen);
  } else {
    addrlen = sizeof (struct sockaddr_in);
    clientsocket = accept(telnet_listen[i], &remoteaddr.sa, &addrlen);
  }
  if (clientsocket < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    return;
  }

  if (set_socket_nonblocking(clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  addrlen = sizeof(localaddr);
  if (getsockname(clientsocket, &localaddr.sa, &addrlen) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't get sock name: %s", strerror(errno));
    return;
  }
 
  gdata.num_dccchats++;
  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->family = telnet_family[i];
  chat->fd = clientsocket;
  chat->status = DCCCHAT_AUTHENTICATING;
  chat->net = 0;
  chat->nick = mystrdup(".telnet");
  chat->localport = gdata.telnet_port;
  chat->connecttime = gdata.curtime;
  chat->lastcontact = gdata.curtime;

  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &remoteaddr.sa, addrlen);
  chat->localaddr = mystrdup(msg);
  my_getnameinfo(msg, maxtextlength -1, &localaddr.sa, 0);
  chat->remoteaddr = mystrdup(msg);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Telnet connection received from %s",  msg);
  mydelete(msg);

  ir_boutput_init(&chat->boutput, chat->fd, 0);

  backup = gnetwork;
  gnetwork = &(gdata.networks[chat->net]);
  setup_chat_banner(chat);
  gnetwork = backup;
}

void telnet_done_select(int changesec)
{
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (telnet_listen[i] != FD_UNUSED) {
      if (FD_ISSET(telnet_listen[i], &gdata.readset)) {
        telnet_accept(i);
      }
    }
  }
}

/* End of File */
