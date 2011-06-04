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
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_misc.h"
#include "dinoex_chat.h"


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

    writestatus(chat);
  }
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

/* handle chat io events */
void chat_perform(void)
{
  dccchat_t *chat;
  char tempbuffa[INPUT_BUFFER_LENGTH];
  int length;
  int callval_i;
  int connect_error;
  int errno2;
  SIGNEDSOCK int connect_error_len;
  unsigned int i, j;

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
                   gnetwork->name, strerror(errno));
          notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(errno2));
          shutdowndccchat(chat, 0);
          continue;
        }
        if (connect_error) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "DCC Chat Connect Attempt Failed on %s: %s",
                  gnetwork->name, strerror(connect_error));
          notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(connect_error));
          shutdowndccchat(chat, 0);
          continue;
        }
        setupdccchatconnected(chat);
      }
      continue;
    }
    if (chat->status == DCCCHAT_LISTENING) {
      if (FD_ISSET(chat->con.listensocket, &gdata.readset)) {
        setupdccchataccept(chat);
      }
      continue;
    }
    if ((chat->status == DCCCHAT_AUTHENTICATING) ||
        (chat->status == DCCCHAT_CONNECTED)) {
      if (FD_ISSET(chat->con.clientsocket, &gdata.readset))  {
        memset(tempbuffa, 0, INPUT_BUFFER_LENGTH);
        length = recv(chat->con.clientsocket, &tempbuffa, INPUT_BUFFER_LENGTH, MSG_DONTWAIT);
        if (length < 1) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "DCC Chat Lost on %s: %s",
                  gnetwork->name,
                  (length<0) ? strerror(errno) : "Closed");
           notice(chat->nick, "DCC Chat Lost: %s", (length<0) ? strerror(errno) : "Closed");
           shutdowndccchat(chat, 0);
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

/* End of File */
