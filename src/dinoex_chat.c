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
#include "dinoex_utilities.h"
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_chat.h"


static group_admin_t *verifypass_group(const char *hostmask, const char *passwd)
{
  group_admin_t *ga;

  if (!hostmask)
    return NULL;

  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_get_next(ga)) {
    if (fnmatch(ga->g_host, hostmask, FNM_CASEFOLD) != 0)
      continue;
    if ( !verifypass2(ga->g_pass, passwd) )
      continue;
    return ga;
  }
  return NULL;
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

static void admin_msg_line(const char *nick, char *line, unsigned int level)
{
  char *part5[6];
  userinput *u;

  updatecontext();
  get_argv(part5, line, 6);
  mydelete(part5[1]);
  mydelete(part5[2]);
  mydelete(part5[3]);
  mydelete(part5[4]);
  u = mycalloc(sizeof(userinput));
  a_fillwith_msg2(u, nick, part5[5]);
  u->hostmask = mystrdup(part5[0] + 1);
  mydelete(part5[0]);
  mydelete(part5[5]);
  u->level = level;
  u_parseit(u);
  mydelete(u);
}

static void reset_ignore(const char *hostmask)
{
  igninfo *ignore;

  /* admin commands shouldn't count against ignore */
  ignore = get_ignore(hostmask);
  ignore->bucket = 0;
}

static unsigned int msg_host_password(const char *nick, const char *hostmask, const char *passwd, char *line)
{
  group_admin_t *ga;

  if ( verifyshell(&gdata.adminhost, hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.adminlevel);
      return 1;
    }
    return 2;
  }
  if ( verifyshell(&gdata.hadminhost, hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      reset_ignore(hostmask);
      admin_msg_line(nick, line, gdata.hadminlevel);
      return 1;
    }
    return 2;
  }
  if ((ga = verifypass_group(hostmask, passwd))) {
    reset_ignore(hostmask);
    admin_msg_line(nick, line, ga->g_level);
    return 1;
  }
  return 0;
}

unsigned int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line)
{
  unsigned int err;

  err = msg_host_password(nick, hostmask, passwd, line);
  if (err == 0) {
    notice(nick, "ADMIN: %s is not allowed to issue admin commands", hostmask);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Hostname (%s on %s)",
            hostmask, gnetwork->name);
  }
  if (err == 2) {
    notice(nick, "ADMIN: Incorrect Password");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Password (%s on %s)",
            hostmask, gnetwork->name);
  }
  return err;
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

/* End of File */
