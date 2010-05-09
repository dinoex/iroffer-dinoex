/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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
#include "dinoex_chat.h"

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

/* End of File */
