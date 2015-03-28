/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the LICENSE file.
 * 
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.org/
 * 
 * @(#) iroffer_dccchat.c 1.77@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_dccchat.c|20050313183434|02319
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_kqueue.h"
#include "dinoex_irc.h"
#include "dinoex_badip.h"
#include "dinoex_jobs.h"
#include "dinoex_chat.h"
#include "dinoex_misc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

int setupdccchatout(const char *nick, const char *hostmask, const char *token)
{
  char *msg;
  char *token2 = NULL;
  unsigned int rc;
  dccchat_t *chat;
  
  updatecontext();
  
  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->name = gnetwork->name;
  chat->status = DCCCHAT_UNUSED;
  chat->con.family = gnetwork->myip.sa.sa_family;

  rc = irc_open_listen(&(chat->con));
  if (rc != 0)
    return 1;
  
  gdata.num_dccchats++;
  chat->status = DCCCHAT_LISTENING;
  chat->con.clientsocket = FD_UNUSED;
  chat->nick = mystrdup(nick);
  chat->net = gnetwork->net;
  chat->hostmask = mystrdup(hostmask);
  
  msg = setup_dcc_local(&(chat->con.local));
  if (token != NULL) {
    privmsg_fast(nick, IRC_CTCP "DCC CHAT CHAT %s %s" IRC_CTCP, msg, token);
  } else {
    privmsg_fast(nick, IRC_CTCP "DCC CHAT CHAT %s" IRC_CTCP, msg);
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

void setup_chat_banner(dccchat_t *chat)
{
  char *tempstr;

  tempstr = mymalloc(maxtextlength);
  getuptime(tempstr, 0, gdata.startuptime, maxtextlength);
  
  writedccchat(chat, 0, "Welcome to %s\n",
               get_user_nick());
  writedccchat(chat, 0, "iroffer-dinoex " VERSIONLONG FEATURES "%s%s\n",
               gdata.hideos ? "" : " - ",
               gdata.hideos ? "" : gdata.osstring);
  writedccchat(chat, 0, "    running %s\n", tempstr);
  writedccchat(chat, 0, " \n");
  writedccchat(chat, 0, "Enter Your Password:\n");
  
  mydelete(tempstr);
}

void setupdccchataccept(dccchat_t *chat)
{
  SIGNEDSOCK int addrlen;
  char *msg;
  
  updatecontext();
  
  addrlen = sizeof(struct sockaddr_in);
  if ((chat->con.clientsocket = accept(chat->con.listensocket, &(chat->con.remote.sa), &addrlen)) < 0)
    {
      outerror(OUTERROR_TYPE_WARN,"Accept Error, Aborting: %s",strerror(errno));
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
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
    }
  
  if (is_in_badip(&(chat->con.remote))) {
    shutdowndccchat(chat, 0);
    return;
  }
  
  chat->status     = DCCCHAT_AUTHENTICATING;
  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;
  ir_boutput_init(&chat->boutput, chat->con.clientsocket, 0);
  
  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(chat->con.remote.sa));
  chat->con.remoteaddr = mystrdup(msg);
  mydelete(msg);

  setup_chat_banner(chat);
}

int setupdccchat(const char *nick,
                 const char *hostmask,
                 const char *line)
{
  char *ip, *port;
  SIGNEDSOCK int addrlen;
  int retval;
  dccchat_t *chat;
  char *msg;
  
  updatecontext();
  
  ip = getpart(line,7);
  port = getpart(line,8);
  
  if ( !ip || !port )
    {
      mydelete(ip);
      mydelete(port);
      return 1;
    }
  
  /* support passive dcc */
  if (strcmp(port, "0") == 0)
    {
      char *token;

      mydelete(ip);
      mydelete(port);
      if (gdata.passive_dcc_chat)
        {
          token = getpart(line, 9);
          setupdccchatout(nick, hostmask, token);
          mydelete(token);
        }
      else
        {
          notice(nick, "DCC passive Chat denied, use \"/MSG %s ADMIN password CHATME\" instead.", get_user_nick());
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT attempt denied from %s (%s on %s)",
                  nick, hostmask, gnetwork->name);
        }
      return 1;
    }
  
  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  chat->name = gnetwork->name;
  
  bzero((char *) &(chat->con.remote), sizeof(chat->con.remote));
  
  chat->con.family = (strchr(ip, ':')) ? AF_INET6 : AF_INET;
  chat->con.clientsocket = socket(chat->con.family, SOCK_STREAM, 0);
  if (chat->con.clientsocket < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Socket Error: %s", strerror(errno));
      chat->con.clientsocket = FD_UNUSED;
      mydelete(ip);
      mydelete(port);
      return 1;
    }
  
  port[strlen(port)-1] = '\0';
  if (chat->con.family == AF_INET)
    {
      addrlen = sizeof(struct sockaddr_in);
      chat->con.remote.sin.sin_family = AF_INET;
      chat->con.remote.sin.sin_port = htons(atoi(port));
      chat->con.remote.sin.sin_addr.s_addr = htonl(atoul(ip));
    }
  else
    {
      addrlen = sizeof(struct sockaddr_in6);
      chat->con.remote.sin6.sin6_family = AF_INET6;
      chat->con.remote.sin6.sin6_port = htons(atoi(port));
      retval = inet_pton(AF_INET6, ip, &(chat->con.remote.sin6.sin6_addr));
      if (retval != 0)
        outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", ip);
    }
  
  mydelete(port);
  mydelete(ip);
  
  if (is_in_badip(&(chat->con.remote))) {
    shutdowndccchat(chat, 0);
    return 1;
  }
  
  if (bind_irc_vhost(chat->con.family, chat->con.clientsocket) != 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't Bind To Virtual Host: %s", strerror(errno));
      chat->con.clientsocket = FD_UNUSED;
      return 1;
    }
  
  if (set_socket_nonblocking(chat->con.clientsocket, 1) < 0 )
    {
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
    }
  
  alarm(CTIMEOUT);
  retval = connect(chat->con.clientsocket, &(chat->con.remote.sa), addrlen);
  if ((retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Connection to DCC Chat Failed: %s", strerror(errno));
      chat->con.clientsocket = FD_UNUSED;
      return 1;
    }
  alarm(0);
  
  addrlen = sizeof(chat->con.local);
  if (getsockname(chat->con.clientsocket, &(chat->con.local.sa), &addrlen) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't get sock name: %s", strerror(errno));
      chat->con.clientsocket = FD_UNUSED;
      return 1;
    }
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S, COLOR_YELLOW, "dccchat socket = %d", chat->con.clientsocket);
    }
  
  gdata.num_dccchats++;
  chat->status = DCCCHAT_CONNECTING;
  chat->nick = mystrdup(nick);
  chat->hostmask = mystrdup(hostmask);
  chat->con.localport  = 0;
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

void setupdccchatconnected(dccchat_t *chat)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT connection succeeded, authenticating");
  
  chat->status = DCCCHAT_AUTHENTICATING;
  chat->con.connecttime = gdata.curtime;
  chat->con.lastcontact = gdata.curtime;
  ir_boutput_init(&chat->boutput, chat->con.clientsocket, 0);
  
  setup_chat_banner(chat);
}


void parsedccchat(dccchat_t *chat,
                  char* line)
{
  char *linec;
  userinput ui;
  unsigned int count;
  unsigned int found;
  
  updatecontext();
  
  linec = mystrdup(line);
  caps(linec);
  
  chat->con.lastcontact = gdata.curtime;
  
  switch (chat->status)
    {
    case DCCCHAT_AUTHENTICATING:
      found = dcc_host_password(chat, line);
      if (found == 1)
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT Correct password");
          
          chat->status = DCCCHAT_CONNECTED;
          
          writedccchat(chat,0," \n");
          writedccchat(chat,0,"Entering DCC Chat Admin Interface\n");
          writedccchat(chat,0,"For Help type \"help\"\n");
          
          if (chat->level >= ADMIN_LEVEL_FULL)
            {
          count = irlist_size(&gdata.msglog);
          writedccchat(chat, 0, "** You have %u %s in the message log%s\n",
                       count,
                       count != 1 ? "messages" : "message",
                       count ? ", use MSGREAD to read them" : "");
          writedccchat(chat,0," \n");
            }
        }
      else
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT: Incorrect password");
          
          writedccchat(chat,0,"Incorrect Password\n");
          count_badip(&(chat->con.remote));
          shutdowndccchat(chat,1);
          /* caller deletes */
        }
      break;
      
    case DCCCHAT_CONNECTED:
      if (gdata.debug > 11)
        {
          ioutput(OUT_S|OUT_L, COLOR_CYAN, ">DCC>: %s", line);
        }
      u_fillwith_dcc(&ui,chat,line);
      u_parseit(&ui);
      break;
      
    case DCCCHAT_UNUSED:
    case DCCCHAT_LISTENING:
    case DCCCHAT_CONNECTING:
    default:
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Unexpected dccchat state %u", chat->status);
      break;
    }
  
  mydelete(linec);
}

void writedccchat(dccchat_t *chat, int add_return, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vwritedccchat(chat, add_return, format, args);
   va_end(args);
}

void vwritedccchat(dccchat_t *chat, int add_return, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  int len;
  
  updatecontext();
  
  if ((chat->status != DCCCHAT_AUTHENTICATING) &&
      (chat->status != DCCCHAT_CONNECTED))
    {
      return;
    }
  
  len = vsnprintf(tempstr, maxtextlength, format, ap);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD | OUTERROR_TYPE_NOLOG,
               "DCCCHAT: Output too large, ignoring!");
      return;
    }
  
  ir_boutput_write(&chat->boutput, tempstr, len);
  
  if (add_return)
    {
      ir_boutput_write(&chat->boutput, "\n", 1);
    }
  
  if (gdata.debug > 11)
    {
      if (tempstr[len-1] == '\n')
        {
          tempstr[len-1] = '\0';
        }
      ioutput(OUT_S|OUT_L, COLOR_CYAN, "<DCC<: %s", tempstr);
    }
}

void flushdccchat(dccchat_t *chat)
{
  if ((chat->status == DCCCHAT_AUTHENTICATING) ||
      (chat->status == DCCCHAT_CONNECTED))
    {
      ir_boutput_attempt_flush(&chat->boutput);
    }
  return;
}

void writestatus(dccchat_t *chat) {
   char *tempstr;
   
   updatecontext();
   
   tempstr = mymalloc(maxtextlength);
   
   getstatusline(tempstr,maxtextlength);
   writedccchat(chat,0,"%s\n",tempstr);
   
   mydelete(tempstr);
   }

void shutdowndccchat(dccchat_t *chat, int flush)
{
  if (chat->status != DCCCHAT_UNUSED)
    {
      if (flush)
        {
          flushdccchat(chat);
        }
      
      usleep(100*1000);
      shutdown_close(chat->con.clientsocket);
      mydelete(chat->groups);
      mydelete(chat->hostmask);
      mydelete(chat->nick);
      mydelete(chat->con.localaddr);
      mydelete(chat->con.remoteaddr);
      ir_boutput_delete(&chat->boutput);

      if (chat->status == DCCCHAT_LISTENING)
        ir_listen_port_connected(chat->con.localport);

#ifdef USE_UPNP
      if (gdata.upnp_router && (chat->con.family == AF_INET))
        upnp_rem_redir(chat->con.localport);
#endif /* USE_UPNP */

      memset(chat, 0, sizeof(dccchat_t));
      chat->con.clientsocket = FD_UNUSED;
      chat->status = DCCCHAT_UNUSED;
      
      gdata.num_dccchats--;
    }
  return;
}


/* End of File */
