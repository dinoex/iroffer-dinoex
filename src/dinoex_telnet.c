/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2011 Dirk Meyer
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
#include "dinoex_badip.h"
#include "dinoex_telnet.h"

#ifndef WITHOUT_TELNET

static int telnet_listen[MAX_VHOSTS];
static int telnet_family[MAX_VHOSTS];

/* close all telnet interfaces */
void telnet_close_listen(void)
{
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (telnet_listen[i] != FD_UNUSED) {
      FD_CLR(telnet_listen[i], &gdata.readset);
      close(telnet_listen[i]);
      telnet_listen[i] = FD_UNUSED;
    }
  }
}

static unsigned int telnet_open_listen(unsigned int i)
{
  char *vhost = NULL;
  char *msg;
  unsigned int rc;
  ir_sockaddr_union_t listenaddr;

  updatecontext();

  vhost = irlist_get_nth(&gdata.telnet_vhost, i);
  if (vhost == NULL)
    return 1;

  rc = open_listen(0, &listenaddr, &(telnet_listen[i]), gdata.telnet_port, 1, 0, vhost);
  if (rc != 0)
    return 1;

  telnet_family[i] = listenaddr.sa.sa_family;
  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &listenaddr.sa);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Telnet SERVER waiting for connection on %s",  msg);
  mydelete(msg);
  return 0;
}

/* setup all telnet interfaces */
unsigned int telnet_setup_listen(void)
{
  unsigned int i;
  unsigned int rc = 0;

  updatecontext();

  for (i=0; i<MAX_VHOSTS; ++i) {
    telnet_listen[i] = FD_UNUSED;
    telnet_family[i] = 0;
  }

  if (gdata.telnet_port == 0)
    return 1;

  for (i=0; i<MAX_VHOSTS; ++i) {
    rc += telnet_open_listen(i);
  }
  return rc;
}

/* close and setup all telnet interfaces */
void telnet_reash_listen(void)
{
  telnet_close_listen();
  if (gdata.telnet_port == 0)
    return;
  telnet_setup_listen();
}

/* register active connections for select() */
int telnet_select_fdset(int highests)
{
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (telnet_listen[i] != FD_UNUSED) {
      FD_SET(telnet_listen[i], &gdata.readset);
      highests = max2(highests, telnet_listen[i]);
    }
  }
  return highests;
}

/* accept incoming connection */
static void telnet_accept(unsigned int i)
{
  gnetwork_t *backup;
  char *msg;
  dccchat_t *chat;
  SIGNEDSOCK int addrlen;

  updatecontext();

  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->status = DCCCHAT_UNUSED;
  chat->con.family = telnet_family[i];
  if (chat->con.family != AF_INET) {
    addrlen = sizeof (struct sockaddr_in6);
    chat->con.clientsocket = accept(telnet_listen[i], &(chat->con.remote.sa), &addrlen);
  } else {
    addrlen = sizeof (struct sockaddr_in);
    chat->con.clientsocket = accept(telnet_listen[i], &(chat->con.remote.sa), &addrlen);
  }
  if (chat->con.clientsocket < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    return;
  }

  if (set_socket_nonblocking(chat->con.clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  addrlen = sizeof(chat->con.local);
  if (getsockname(chat->con.clientsocket, &(chat->con.local.sa), &addrlen) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't get sock name: %s", strerror(errno));
    close(chat->con.clientsocket);
    chat->con.clientsocket = FD_UNUSED;
    return;
  }

  ++(gdata.num_dccchats);
  chat->status = DCCCHAT_AUTHENTICATING;
  chat->net = 0;
  chat->nick = mystrdup("telnet"); /* NOTRANSLATE */
  chat->hostmask = to_hostmask(chat->nick, "telnet"); /* NOTRANSLATE */
  chat->con.localport = gdata.telnet_port;
  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;

  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.remote.sa));
  chat->con.localaddr = mystrdup(msg);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.local.sa));
  chat->con.remoteaddr = mystrdup(msg);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "Telnet connection received from %s",  msg);
  mydelete(msg);

  if (is_in_badip(&(chat->con.remote))) {
    shutdowndccchat(chat, 0);
    return;
  }

  ir_boutput_init(&chat->boutput, chat->con.clientsocket, 0);

  backup = gnetwork;
  gnetwork = &(gdata.networks[chat->net]);
  setup_chat_banner(chat);
  gnetwork = backup;
}

/* process all telnet connections */
void telnet_perform(void)
{
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (telnet_listen[i] != FD_UNUSED) {
      if (FD_ISSET(telnet_listen[i], &gdata.readset)) {
        telnet_accept(i);
      }
    }
  }
}

#endif /* WITHOUT_TELNET */

/* End of File */
