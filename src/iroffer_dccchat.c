/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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
#include "dinoex_misc.h"


int setupdccchatout(const char *nick)
{
  char *msg;
  int rc;
  int listenport;
  ir_sockaddr_union_t listenaddr;
  dccchat_t *chat;
  
  updatecontext();
  
  while (gdata.num_dccchats >= MAXCHATS)
    {
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_get_next(chat))
        {
          if ((chat->status != DCCCHAT_UNUSED) &&
              (chat->status != DCCCHAT_CONNECTED))
            {
              writedccchat(chat,0,"Another DCC Chat Request Received\n");
              shutdowndccchat(chat,1);
              break;
            }
        }
      if (!chat)
        {
          for (chat = irlist_get_head(&gdata.dccchats);
               chat;
               chat = irlist_get_next(chat))
            {
              if (chat->status != DCCCHAT_UNUSED)
                {
                  writedccchat(chat,0,"Another DCC Chat Request Received\n");
                  shutdowndccchat(chat,1);
                  break;
                }
            }
        }
      if (!chat)
        {
          chat = irlist_get_head(&gdata.dccchats);
          writedccchat(chat,0,"Another DCC Chat Request Received\n");
          shutdowndccchat(chat,1);
        }
    }
  
  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));

  rc = open_listen(gnetwork->myip.sa.sa_family, &listenaddr, &(chat->fd), 0, gdata.tcprangestart, 1);
  if (rc != 0)
    return 1;
  
  listenport = get_port(&listenaddr);
  
  gdata.num_dccchats++;
  chat->status = DCCCHAT_LISTENING;
  chat->nick = mystrdup(nick);
  chat->connecttime = gdata.curtime;
  chat->lastcontact = gdata.curtime;
  chat->family = listenaddr.sa.sa_family;
  chat->localport = listenport;
  chat->net = gnetwork->net;
  
  if (chat->family == AF_INET)
    listenaddr.sin.sin_addr = gnetwork->myip.sin.sin_addr;
  else
    listenaddr.sin6.sin6_addr = gnetwork->myip.sin6.sin6_addr;
  msg = mycalloc(maxtextlength);
  my_dcc_ip_port(msg, maxtextlength -1, &listenaddr, listenaddr.sa.sa_len);
  privmsg_fast(nick,"\1DCC CHAT CHAT %s\1", msg);
  my_getnameinfo(msg, maxtextlength -1, &listenaddr.sa, listenaddr.sa.sa_len);
  chat->localaddr = mystrdup(msg);
  mydelete(msg);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT sent to %s on %s, waiting for connection on %s",
          nick, gnetwork->name, chat->localaddr);
  return 0;
}

void setupdccchataccept(dccchat_t *chat)
{
  SIGNEDSOCK int addrlen;
  ir_sockaddr_union_t remoteaddr;
  char *tempstr;
  char *msg;
  int listen_fd;
  
  updatecontext();
  
  listen_fd = chat->fd;
  addrlen = sizeof(struct sockaddr_in);
  if ((chat->fd = accept(listen_fd, &remoteaddr.sa, &addrlen)) < 0)
    {
      outerror(OUTERROR_TYPE_WARN,"Accept Error, Aborting: %s",strerror(errno));
      FD_CLR(listen_fd, &gdata.readset);
      close(listen_fd);
      chat->fd = FD_UNUSED;
      return;
    }

  ir_listen_port_connected(chat->localport);

  FD_CLR(listen_fd, &gdata.readset);
  close(listen_fd);
  
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT connection received, authenticating");
  
  if (set_socket_nonblocking(chat->fd,1) < 0 )
    {
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
    }
  
  chat->status     = DCCCHAT_AUTHENTICATING;
  chat->connecttime = gdata.curtime;
  chat->lastcontact = gdata.curtime;
  ir_boutput_init(&chat->boutput, chat->fd, 0);
  
  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &remoteaddr.sa, remoteaddr.sa.sa_len);
  chat->remoteaddr = mystrdup(msg);
  mydelete(msg);

  tempstr = mycalloc(maxtextlength);
  getuptime(tempstr, 0, gdata.startuptime, maxtextlength);
  
  writedccchat(chat,0,"Welcome to %s\n",
               save_nick(gnetwork->user_nick));
  writedccchat(chat,0,"iroffer v" VERSIONLONG "%s%s\n",
               gdata.hideos ? "" : " - ",
               gdata.hideos ? "" : gdata.osstring);
  writedccchat(chat,0,"    running %s\n", tempstr);
  writedccchat(chat,0," \n");
  writedccchat(chat,0,"Enter Your Password:\n");
  
  mydelete(tempstr);
  
}

int setupdccchat(const char *nick,
                 const char *line)
{
  ir_sockaddr_union_t remoteip;
  ir_sockaddr_union_t localaddr;
  char *ip, *port;
  SIGNEDSOCK int addrlen;
  int retval;
  dccchat_t *chat;
  char *msg;
  
  updatecontext();
  
  while (gdata.num_dccchats >= MAXCHATS)
    {
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_get_next(chat))
        {
          if ((chat->status != DCCCHAT_UNUSED) &&
              (chat->status != DCCCHAT_CONNECTED))
            {
              writedccchat(chat,0,"Another DCC Chat Request Received\n");
              shutdowndccchat(chat,1);
              break;
            }
        }
      if (!chat)
        {
          for (chat = irlist_get_head(&gdata.dccchats);
               chat;
               chat = irlist_get_next(chat))
            {
              if (chat->status != DCCCHAT_UNUSED)
                {
                  writedccchat(chat,0,"Another DCC Chat Request Received\n");
                  shutdowndccchat(chat,1);
                  break;
                }
            }
        }
      if (!chat)
        {
          chat = irlist_get_head(&gdata.dccchats);
          writedccchat(chat,0,"Another DCC Chat Request Received\n");
          shutdowndccchat(chat,1);
        }
    }
  
  chat = irlist_add(&gdata.dccchats, sizeof(dccchat_t));
  
  ip = getpart(line,7);
  port = getpart(line,8);
  
  if ( !ip || !port )
    {
      mydelete(ip);
      mydelete(port);
      return 1;
    }
  
  bzero((char *) &remoteip, sizeof(remoteip));
  
  chat->family = (strchr(ip, ':')) ? AF_INET6 : AF_INET;
  chat->fd = socket(chat->family, SOCK_STREAM, 0);
  if (chat->fd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Socket Error: %s", strerror(errno));
      chat->fd = FD_UNUSED;
      mydelete(ip);
      mydelete(port);
      return 1;
    }
  
  port[strlen(port)-1] = '\0';
  if (chat->family == AF_INET)
    {
      remoteip.sa.sa_len =  sizeof(struct sockaddr_in);
      remoteip.sin.sin_family = AF_INET;
      remoteip.sin.sin_port = htons(atoi(port));
      remoteip.sin.sin_addr.s_addr = htonl(atoul(ip));
    }
  else
    {
      remoteip.sa.sa_len =  sizeof(struct sockaddr_in6);
      remoteip.sin6.sin6_family = AF_INET6;
      remoteip.sin6.sin6_port = htons(atoi(port));
      retval = inet_pton(AF_INET6, ip, &(remoteip.sin6.sin6_addr));
      if (retval != 0)
        outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", ip);
    }
  
  mydelete(port);
  mydelete(ip);
  
  if (gdata.local_vhost)
    {
      bzero((char*)&localaddr, sizeof(localaddr));
      localaddr.sin.sin_family = AF_INET;
      localaddr.sin.sin_port = 0;
      localaddr.sin.sin_addr.s_addr = htonl(gdata.local_vhost);
      
      if (bind(chat->fd, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Bind To Virtual Host: %s", strerror(errno));
          chat->fd = FD_UNUSED;
          return 1;
        }
    }
  
  if (set_socket_nonblocking(chat->fd,1) < 0 )
    {
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
    }
  
  alarm(CTIMEOUT);
  retval = connect(chat->fd, &remoteip.sa, remoteip.sa.sa_len);
  if ((retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Connection to DCC Chat Failed: %s", strerror(errno));
      chat->fd = FD_UNUSED;
      return 1;
    }
  alarm(0);
  
  addrlen = sizeof (localaddr);
  if (getsockname(chat->fd, &localaddr.sa, &addrlen) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't get sock name: %s", strerror(errno));
      chat->fd = FD_UNUSED;
      return 1;
    }
  
  if (gdata.debug > 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"dccchat socket = %d",chat->fd);
    }
  
  gdata.num_dccchats++;
  chat->status = DCCCHAT_CONNECTING;
  chat->nick = mystrdup(nick);
  chat->localport  = get_port(&localaddr);
  chat->connecttime = gdata.curtime;
  chat->lastcontact = gdata.curtime;
  chat->net = gnetwork->net;
  
  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &localaddr.sa, localaddr.sa.sa_len);
  chat->localaddr = mystrdup(msg);
  my_getnameinfo(msg, maxtextlength -1, &remoteip.sa, remoteip.sa.sa_len);
  chat->remoteaddr = mystrdup(msg);
  mydelete(msg);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT received from %s on %s, attempting connection to %s",
          nick, gnetwork->name, chat->remoteaddr);
  return 0;
}

void setupdccchatconnected(dccchat_t *chat)
{
  char *tempstr;
  
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC CHAT connection suceeded, authenticating");
  
  chat->status = DCCCHAT_AUTHENTICATING;
  chat->connecttime = gdata.curtime;
  chat->lastcontact = gdata.curtime;
  ir_boutput_init(&chat->boutput, chat->fd, 0);
  
  tempstr = mycalloc(maxtextlength);
  getuptime(tempstr, 0, gdata.startuptime, maxtextlength);

  writedccchat(chat,0,"Welcome to %s\n",
               save_nick(gnetwork->user_nick));
  writedccchat(chat,0,"iroffer v" VERSIONLONG "%s%s\n",
               gdata.hideos ? "" : " - ",
               gdata.hideos ? "" : gdata.osstring);
  writedccchat(chat,0,"    running %s\n", tempstr);
  writedccchat(chat,0," \n");
  writedccchat(chat,0,"Enter Your Password:\n");

  mydelete(tempstr);
  
}


void parsedccchat(dccchat_t *chat,
                  char* line)
{
  char *linec;
  userinput ui;
  int count;
  
  updatecontext();
  
  linec = mystrdup(line);
  caps(linec);
  
  chat->lastcontact = gdata.curtime;
  
  switch (chat->status)
    {
    case DCCCHAT_AUTHENTICATING:
      if (verifypass2(gdata.adminpass, line))
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT Correct password");
          
          chat->status = DCCCHAT_CONNECTED;
          chat->level = gdata.adminlevel;
          
          writedccchat(chat,0," \n");
          writedccchat(chat,0,"Entering DCC Chat Admin Interface\n");
          writedccchat(chat,0,"For Help type \"help\"\n");
          
          count = irlist_size(&gdata.msglog);
          writedccchat(chat, 0, "** You have %i message%s in the message log%s\n",
                       count,
                       count != 1 ? "s" : "",
                       count ? ", use MSGREAD to read them" : "");
          writedccchat(chat,0," \n");
        }
      else if (verifypass2(gdata.hadminpass, line))
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT Correct password");
          
          chat->status = DCCCHAT_CONNECTED;
          chat->level = gdata.hadminlevel;
          
          writedccchat(chat,0," \n");
          writedccchat(chat,0,"Entering DCC Chat Admin Interface\n");
          writedccchat(chat,0,"For Help type \"help\"\n");
        }
      else
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT: Incorrect password");
          
          writedccchat(chat,0,"Incorrect Password\n");
          shutdowndccchat(chat,1);
          /* caller deletes */
        }
      break;
      
    case DCCCHAT_CONNECTED:
      if (!gdata.attop) gototop();
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_CYAN,">DCC>: %s",line);
        }
      u_fillwith_dcc(&ui,chat,line);
      u_parseit(&ui);
      break;
      
    case DCCCHAT_UNUSED:
      break;
      
    case DCCCHAT_LISTENING:
    case DCCCHAT_CONNECTING:
    default:
      outerror(OUTERROR_TYPE_CRASH,
               "Unexpected dccchat state %d", chat->status);
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
  
  if ((len < 0) || (len >= maxtextlength))
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
  
  if (gdata.debug > 0)
    {
      if (tempstr[len-1] == '\n')
        {
          tempstr[len-1] = '\0';
        }
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_CYAN,"<DCC<: %s",tempstr);
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
   
   tempstr = mycalloc(maxtextlength);
   
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
      
      FD_CLR(chat->fd, &gdata.readset);
      FD_CLR(chat->fd, &gdata.writeset);
      usleep(100*1000);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(chat->fd, SHUT_RDWR);
      close(chat->fd);
      mydelete(chat->nick);
      mydelete(chat->localaddr);
      mydelete(chat->remoteaddr);
      ir_boutput_delete(&chat->boutput);
      memset(chat, 0, sizeof(dccchat_t));
      chat->fd = FD_UNUSED;

      if (chat->status == DCCCHAT_LISTENING)
        ir_listen_port_connected(chat->localport);

      chat->status = DCCCHAT_UNUSED;
      
      gdata.num_dccchats--;
    }
  return;
}


/* End of File */
