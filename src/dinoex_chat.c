/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2023 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from https://iroffer.net/
 *
 * SPDX-FileCopyrightText: 2004-2023 Dirk Meyer
 * SPDX-License-Identifier: GPL-2.0-only
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
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_misc.h"
#include "dinoex_kqueue.h"
#include "dinoex_badip.h"
#include "dinoex_ssl.h"
#include "dinoex_chat.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

/* shutdown the dcc chat */
void chat_shutdown(dccchat_t *chat, int flush)
{
  if (chat->status == DCCCHAT_UNUSED)
    return;

  if (flush) {
    flushdccchat(chat);
  }

  if (chat->status == DCCCHAT_LISTENING) {
    ir_listen_port_connected(chat->con.localport);
    event_close(chat->con.listensocket);
    chat->con.listensocket = FD_UNUSED;
  }

  if (chat->con.clientsocket != FD_UNUSED) {
    usleep(100*1000);
    shutdown_close(chat->con.clientsocket);
    chat->con.clientsocket = FD_UNUSED;
  }

  mydelete(chat->groups);
  mydelete(chat->hostmask);
  mydelete(chat->nick);
  mydelete(chat->con.localaddr);
  mydelete(chat->con.remoteaddr);
  ir_boutput_delete(&chat->boutput);

#ifdef USE_UPNP
    if (gdata.upnp_router && (chat->con.family == AF_INET))
      upnp_rem_redir(chat->con.localport);
#endif /* USE_UPNP */

  bzero(chat, sizeof(dccchat_t));
  chat->status = DCCCHAT_UNUSED;
}

/* create an outgoing dcc chat */
int chat_setup_out(const char *nick, const char *hostmask, const char *token,
                   int use_ssl)
{
  char *msg;
  char *token2 = NULL;
  unsigned int rc;
  dccchat_t *chat;
  const char *schat;

  updatecontext();

  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->name = gnetwork->name;
  chat->status = DCCCHAT_UNUSED;
  chat->con.family = gnetwork->myip.sa.sa_family;
  chat->use_ssl = use_ssl;
  schat = (use_ssl != 0) ? "S" : "";

  rc = irc_open_listen(&(chat->con));
  if (rc != 0)
    return 1;

  chat->status = DCCCHAT_LISTENING;
  chat->con.clientsocket = FD_UNUSED;
  chat->nick = mystrdup(nick);
  chat->net = gnetwork->net;
  chat->hostmask = mystrdup(hostmask);

  msg = setup_dcc_local(&(chat->con.local));
  if (token != NULL) {
    privmsg_fast(nick,
                 IRC_CTCP "DCC %sCHAT CHAT %s %s" IRC_CTCP, /* NOTRANSLATE */
                 schat, msg, token);
  } else {
    privmsg_fast(nick,
                 IRC_CTCP "DCC %sCHAT CHAT %s" IRC_CTCP, /* NOTRANSLATE */
                 schat, msg);
  }
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.local.sa));
  chat->con.localaddr = mystrdup(msg);
  mydelete(token2);
  mydelete(msg);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT sent to %s on %s, waiting for connection on %s",
          nick, chat->name, chat->con.localaddr);
  return 0;
}

/* create an incoming dcc chat */
int chat_setup(const char *nick, const char *hostmask, const char *line,
               int use_ssl)
{
  char *ip, *port;
  SIGNEDSOCK int addrlen;
  int retval;
  dccchat_t *chat;
  char *msg;

  updatecontext();

  ip = getpart(line, 7);
  port = getpart(line, 8);

  if ( ip == NULL || port == NULL ) {
    mydelete(ip);
    mydelete(port);
    return 1;
  }

  /* support passive dcc */
  if (strcmp(port, "0") == 0) {
    char *token;

    mydelete(ip);
    mydelete(port);
    if (gdata.passive_dcc_chat) {
      token = getpart(line, 9);
      chat_setup_out(nick, hostmask, token, use_ssl);
      mydelete(token);
    } else {
      notice(nick,
             "DCC passive Chat denied, use \"/MSG %s ADMIN password CHATME\" instead.",
             get_user_nick());
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC CHAT attempt denied from %s (%s on %s)",
              nick, hostmask, gnetwork->name);
    }
    return 1;
  }

  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->name = gnetwork->name;
  chat->use_ssl = use_ssl;
  bzero((char *) &(chat->con.remote), sizeof(chat->con.remote));

  chat->con.family = (strchr(ip, ':')) ? AF_INET6 : AF_INET;
  chat->con.clientsocket = socket(chat->con.family, SOCK_STREAM, 0);
  if (chat->con.clientsocket < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Socket Error: %s", strerror(errno));
    chat->con.clientsocket = FD_UNUSED;
    mydelete(ip);
    mydelete(port);
    return 1;
  }

  port[strlen(port)-1] = '\0';
  if (chat->con.family == AF_INET) {
    addrlen = sizeof(struct sockaddr_in);
    chat->con.remote.sin.sin_family = AF_INET;
    chat->con.remote.sin.sin_port = htons(atoi(port));
    chat->con.remote.sin.sin_addr.s_addr = htonl(atoul(ip));
  } else {
    addrlen = sizeof(struct sockaddr_in6);
    chat->con.remote.sin6.sin6_family = AF_INET6;
    chat->con.remote.sin6.sin6_port = htons(atoi(port));
    retval = inet_pton(AF_INET6, ip, &(chat->con.remote.sin6.sin6_addr));
    if (retval != 1)
      outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", ip);
  }

  mydelete(port);
  mydelete(ip);

  if (is_in_badip(&(chat->con.remote))) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "DCC CHAT attempt denied from %s (%s on %s)",
            nick, hostmask, gnetwork->name);
    chat_shutdown_ssl(chat, 0);
    return 1;
  }

  if (bind_irc_vhost(chat->con.family, chat->con.clientsocket) != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Couldn't Bind To Virtual Host: %s", strerror(errno));
    chat->con.clientsocket = FD_UNUSED;
    return 1;
  }

  if (set_socket_nonblocking(chat->con.clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  alarm(CTIMEOUT);
  retval = connect(chat->con.clientsocket, &(chat->con.remote.sa), addrlen);
  if ((retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN))) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Connection to DCC Chat Failed: %s", strerror(errno));
    chat->con.clientsocket = FD_UNUSED;
    return 1;
  }
  alarm(0);

  addrlen = sizeof(chat->con.local);
  if (getsockname(chat->con.clientsocket, &(chat->con.local.sa), &addrlen) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't get sock name: %s", strerror(errno));
    chat->con.clientsocket = FD_UNUSED;
    return 1;
  }

  if (gdata.debug > 0) {
    ioutput(OUT_S, COLOR_YELLOW, "dccchat socket = %d", chat->con.clientsocket);
  }

  chat->status = DCCCHAT_CONNECTING;
  chat->nick = mystrdup(nick);
  chat->hostmask = mystrdup(hostmask);
  chat->con.localport = 0;
  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;
  chat->net = gnetwork->net;

  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.local.sa));
  chat->con.localaddr = mystrdup(msg);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.remote.sa));
  chat->con.remoteaddr = mystrdup(msg);
  mydelete(msg);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT received from %s on %s, attempting connection to %s",
          nick, chat->name, chat->con.remoteaddr);
  return 0;
}

/* print the password prompt */
void chat_banner(dccchat_t *chat)
{
  char *tempstr;

  tempstr = mymalloc(maxtextlength);
  getuptime(tempstr, 0, gdata.startuptime, maxtextlength);

  writedccchat(chat, 0, "Welcome to %s\n",
               get_user_nick());
  writedccchat(chat, 0, "iroffer-dinoex" " " VERSIONLONG FEATURES "%s%s\n",
               gdata.hideos ? "" : " - ",
               gdata.hideos ? "" : gdata.osstring);
  writedccchat(chat, 0, "    running %s\n", tempstr);
  writedccchat(chat, 0, " \n");
  writedccchat(chat, 0, "Enter Your Password:\n");

  mydelete(tempstr);
}

/* check the given passwort matches hostmask and groups */
unsigned int dcc_host_password(dccchat_t *chat, char *passwd)
{
  group_admin_t *ga;

  updatecontext();

  if ( verifyshell(&gdata.adminhost, chat->hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      chat->level = gdata.adminlevel;
      return 1;
    }
    return 2;
  }
  if ( verifyshell(&gdata.hadminhost, chat->hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      chat->level = gdata.hadminlevel;
      return 1;
    }
    return 2;
  }
  if ((ga = verifypass_group(chat->hostmask, passwd))) {
    chat->level = ga->g_level;
    chat->groups = mystrdup(ga->g_groups);
    return 1;
  }
  return 0;
}

/* accept an incoming dcc chat */
static void chat_accept(dccchat_t *chat)
{
  SIGNEDSOCK int addrlen;
  char *msg;
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int flags = 0;
#endif /* USE_OPENSSL or USE_GNUTLS */

  updatecontext();

  addrlen = sizeof(struct sockaddr_in);
  if ((chat->con.clientsocket = accept(chat->con.listensocket, &(chat->con.remote.sa), &addrlen)) < 0)
    {
      outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
      event_close(chat->con.listensocket);
      chat->con.clientsocket = FD_UNUSED;
      chat->con.listensocket = FD_UNUSED;
      return;
    }

  ir_listen_port_connected(chat->con.localport);

  event_close(chat->con.listensocket);
  chat->con.listensocket = FD_UNUSED;

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT connection received, authenticating");

  if (set_socket_nonblocking(chat->con.clientsocket, 1) < 0 )
    {
      outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
    }

  if (is_in_badip(&(chat->con.remote))) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "DCC CHAT attempt denied from %s (%s on %s)",
            chat->name, chat->hostmask, gdata.networks[chat->net].name );
    chat_shutdown_ssl(chat, 0);
    return;
  }

  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  if (chat->use_ssl != 0)
    flags |= BOUTPUT_SSL;
  ir_boutput_init(&chat->boutput, chat->con.clientsocket, flags);
#ifdef USE_OPENSSL
  chat->boutput.sslp = &(chat->ssl);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  chat->boutput.tlsp = &(chat->tls);
#endif /* USE_GNUTLS */
#endif /* USE_OPENSSL or USE_GNUTLS */

  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.remote.sa));
  chat->con.remoteaddr = mystrdup(msg);
  mydelete(msg);

#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  if (chat->use_ssl) {
    chat->status = DCCCHAT_SSL_ACCEPT;
    chat_accept_ssl(chat);
    return;
  }
#endif /* USE_OPENSSL or USE_GNUTLS */

  chat->status = DCCCHAT_AUTHENTICATING;
  chat_banner(chat);
}

/* chat has connected */
static void chat_connected(dccchat_t *chat)
{
  int flags = 0;

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT connection succeeded, authenticating");

  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  if (chat->use_ssl != 0)
    flags |= BOUTPUT_SSL;
#endif /* USE_OPENSSL or USE_GNUTLS */
  ir_boutput_init(&chat->boutput, chat->con.clientsocket, flags);
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  if (chat->use_ssl) {
#ifdef USE_OPENSSL
    chat->boutput.sslp = &(chat->ssl);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
    chat->boutput.tlsp = &(chat->tls);
#endif /* USE_GNUTLS */
    chat->status = DCCCHAT_SSL_CONNECT;
    chat_handshake_ssl(chat);
    return;
  }
#endif /* USE_OPENSSL or USE_GNUTLS */

  chat->status = DCCCHAT_AUTHENTICATING;
  chat_banner(chat);
}

/* send dcc status line to all chats */
void chat_writestatus(void)
{
  dccchat_t *chat;

  if (gdata.no_status_chat)
    return;

  for (chat = irlist_get_head(&gdata.dccchats);
       chat;
       chat = irlist_get_next(chat)) {

    if (chat->status != DCCCHAT_CONNECTED)
      continue;

    gnetwork = &(gdata.networks[chat->net]);
    writestatus(chat);
  }
  gnetwork = NULL;
}

/* register active connections for select() */
int chat_select_fdset(int highests)
{
  dccchat_t *chat;

  for (chat = irlist_get_head(&gdata.dccchats);
       chat;
       chat = irlist_get_next(chat)) {
    if (chat->status == DCCCHAT_CONNECTING) {
      FD_SET(chat->con.clientsocket, &gdata.writeset);
      highests = max2(highests, chat->con.clientsocket);
      continue;
    }
    if (chat->status == DCCCHAT_LISTENING) {
      FD_SET(chat->con.listensocket, &gdata.readset);
      highests = max2(highests, chat->con.listensocket);
      continue;
    }
    if (chat->status != DCCCHAT_UNUSED) {
      FD_SET(chat->con.clientsocket, &gdata.readset);
      highests = max2(highests, chat->con.clientsocket);
      continue;
    }
  }
  return highests;
}

/* check for listen timeout */
static void chat_istimeout(dccchat_t *chat)
{
  updatecontext();

  if ((gdata.curtime - chat->con.lastcontact) > 180) {
    chat_shutdown_ssl(chat, 0);
  }
}

/* handle chat io events */
void chat_perform(int changesec)
{
  dccchat_t *chat;
  char tempbuffa[INPUT_BUFFER_LENGTH];
  ssize_t length;
  int callval_i;
  int connect_error;
  int errno2;
  SIGNEDSOCK int connect_error_len;
  unsigned int i;
  size_t j;

  updatecontext();
  /*----- see if dccchat is sending anything to us ----- */
  for (chat = irlist_get_head(&gdata.dccchats);
       chat;
       chat = irlist_get_next(chat)) {
    gnetwork = &(gdata.networks[chat->net]);
    if (chat->status == DCCCHAT_CONNECTING) {
      if (FD_ISSET(chat->con.clientsocket, &gdata.writeset)) {
        connect_error_len = sizeof(connect_error);
        callval_i = getsockopt(chat->con.clientsocket,
                               SOL_SOCKET, SO_ERROR,
                               &connect_error, &connect_error_len);
        if (callval_i < 0) {
          errno2 = errno;
          outerror(OUTERROR_TYPE_WARN,
                   "Couldn't determine dcc connection status on %s: %s",
                   chat->name, strerror(errno));
          notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(errno2));
          chat_shutdown_ssl(chat, 0);
          continue;
        }
        if (connect_error) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "DCC Chat Connect Attempt Failed on %s: %s",
                  chat->name, strerror(connect_error));
          notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(connect_error));
          chat_shutdown_ssl(chat, 0);
          continue;
        }
        chat_connected(chat);
      }
      continue;
    }
    if (chat->status == DCCCHAT_SSL_CONNECT) {
      chat_connect_retry_ssl(chat);
      if (changesec) {
        chat_istimeout(chat);
      }
      continue;
    }
    if (chat->status == DCCCHAT_LISTENING) {
      if (FD_ISSET(chat->con.listensocket, &gdata.readset)) {
        chat_accept(chat);
        continue;
      }
      if (changesec) {
        chat_istimeout(chat);
      }
      continue;
    }
    if (chat->status == DCCCHAT_SSL_ACCEPT) {
      chat_accept_retry_ssl(chat);
      if (changesec) {
        chat_istimeout(chat);
      }
      continue;
    }
    if ((chat->status == DCCCHAT_AUTHENTICATING) ||
        (chat->status == DCCCHAT_CONNECTED)) {
      if (FD_ISSET(chat->con.clientsocket, &gdata.readset)) {
        bzero(tempbuffa, INPUT_BUFFER_LENGTH);
        length = chat_read_ssl(chat, &tempbuffa, INPUT_BUFFER_LENGTH );
        if (length < 1) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "DCC Chat Lost on %s: %s",
                  chat->name,
                  (length<0) ? strerror(errno) : "Closed");
           notice(chat->nick, "DCC Chat Lost: %s", (length<0) ? strerror(errno) : "Closed");
           chat_shutdown_ssl(chat, 0);
           /* deleted later */
           continue;
        }
        j = strlen(chat->dcc_input_line);
        for (i=0; i<(unsigned int)length; i++) {
          if ((tempbuffa[i] == '\n') || (j == (INPUT_BUFFER_LENGTH-1))) {
            if (j && (chat->dcc_input_line[j-1] == 0x0D)) {
              j--;
            }
            chat->dcc_input_line[j] = '\0';
            parsedccchat(chat, chat->dcc_input_line);
            j = 0;
          } else {
            chat->dcc_input_line[j] = tempbuffa[i];
            j++;
          }
        }
        chat->dcc_input_line[j] = '\0';
      }
      continue;
    }
  }
  gnetwork = NULL;
}

/* close all dcc chats */
void chat_shutdown_all(void)
{
  dccchat_t *chat;

  updatecontext();
  for (chat = irlist_get_head(&gdata.dccchats);
       chat;
       chat = irlist_delete(&gdata.dccchats, chat)) {
     if (chat->status == DCCCHAT_CONNECTED) {
       writedccchat(chat, 0, "iroffer exited, Closing DCC Chat\n");
     }
     chat_shutdown_ssl(chat, 1);
  }
}

/* End of File */
