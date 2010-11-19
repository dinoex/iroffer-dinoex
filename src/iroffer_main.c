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
 * @(#) iroffer_main.c 1.245@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_main.c|20050313225819|23935
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_http.h"
#include "dinoex_upload.h"
#include "dinoex_transfer.h"
#include "dinoex_ssl.h"
#include "dinoex_curl.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"
#include "dinoex_telnet.h"
#include "dinoex_badip.h"
#include "dinoex_jobs.h"
#include "dinoex_ruby.h"
#include "dinoex_main.h"
#include "dinoex_user.h"
#include "dinoex_chat.h"
#include "dinoex_misc.h"

/* local functions */
static void mainloop(void);
static void parseline(char *line);

/* main */
int
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
main(int argc, char *const *argv) {
   
   initvars();
   
   command_options(argc, argv);
   
#if defined(_OS_CYGWIN)
   gdata.noscreen = 1;
#endif
   
   startupiroffer();
   
   while ( 1 )
     {
       mainloop();
     }
   }

static void select_dump(const char *desc, int highests)
{
  char buffer[maxtextlength];
  size_t len;
  int ii;
  
  len = 0;
  len += snprintf(buffer + len, maxtextlength - len, "select %s: [read", desc);
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.readset))
        {
          len += snprintf(buffer + len, maxtextlength - len, " %d", ii);
        }
    }
  len += snprintf(buffer + len, maxtextlength - len, "] [write");
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.writeset))
        {
          len += snprintf(buffer + len, maxtextlength - len, " %d", ii);
        }
    }
  len += snprintf(buffer + len, maxtextlength - len, "]");
  ioutput(OUT_S, COLOR_CYAN, "%s", buffer);
  
}

static void mainloop (void) {
   /* data is persistant across calls */
   static struct timeval timestruct;
   static int changequartersec, changesec, changemin, changehour;
   static time_t lasttime, lastmin, lasthour, last4sec, last5sec, last20sec;
   static time_t lastautoadd;
   static time_t last3min, last2min, lastignoredec, lastperiodicmsg;
   static int first_loop = 1;
   static ir_uint64 last250ms;

   userinput *pubplist;
   userinput *urehash;
   ir_uint64 xdccsent;
   unsigned int i, j;
   int length;
   int highests;
   unsigned int ss;
   upload *ul;
   transfer *tr;
   channel_t *ch;
   xdcc *xd;
   dccchat_t *chat;
   
   updatecontext();
   gnetwork = NULL;
   
   if (first_loop)
     {
       /* init if first time called */
       FD_ZERO(&gdata.readset);
       FD_ZERO(&gdata.writeset);
       changehour=changemin=changesec=changequartersec=0;
       lasttime=gdata.curtime;
       last250ms = ((ir_uint64)lasttime) * 1000;
       lastmin=(lasttime/60)-1;
       lasthour=(lasttime/60/60)-1;
       last4sec = last5sec = last20sec = last2min = last3min = lasttime;
       lastignoredec = lasttime;
       for (ss=0; ss<gdata.networks_online; ss++)
         {
           gdata.networks[ss].lastnotify = lasttime;
           gdata.networks[ss].lastslow = lasttime;
           gdata.networks[ss].server_input_line[0] = '\0';
         }
       
       gdata.cursendptr = 0;
       lastautoadd = gdata.curtime + 60;
       
       first_loop = 0;
     }
   
      updatecontext();
   
      FD_ZERO(&gdata.readset);
      FD_ZERO(&gdata.writeset);
      FD_ZERO(&gdata.execset);
      highests = 0;
#ifdef USE_CURL
      fetch_multi_fdset(&gdata.readset, &gdata.writeset, &gdata.execset, &highests);
#endif /* USE_CURL */
      
      highests = irc_select(highests);
      
      if (!gdata.background)
        {
          FD_SET(fileno(stdin), &gdata.readset);
          highests = max2(highests, fileno(stdin));
        }
      
      highests = chat_select_fdset(highests);
      highests = t_select_fdset(highests, changequartersec);
      highests = l_select_fdset(highests);
#ifndef WITHOUT_TELNET
      highests = telnet_select_fdset(highests);
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
      highests = h_select_fdset(highests);
#endif /* WITHOUT_HTTP */
      
      if (gdata.md5build.file_fd != FD_UNUSED)
        {
          assert(gdata.md5build.xpack);
          FD_SET(gdata.md5build.file_fd, &gdata.readset);
          highests = max2(highests, gdata.md5build.file_fd);
        }
      
      updatecontext();
   
      timestruct.tv_sec = 0;
      timestruct.tv_usec = 250*1000;
      
      if (gdata.debug > 3)
        {
          select_dump("try", highests);
        }
      
      if (gdata.attop) gotobot();
      
      tostdout_write();
      
      if (select(highests+1, &gdata.readset, &gdata.writeset, NULL, &timestruct) < 0)
        {
          if (errno != EINTR)
            {
              outerror(OUTERROR_TYPE_WARN,"Select returned an error: %s",strerror(errno));
              usleep(10000); /* prevent fast spinning */
            }
          
          /* data is undefined on error, zero and continue */
          FD_ZERO(&gdata.readset);
          FD_ZERO(&gdata.writeset);
          FD_ZERO(&gdata.execset);
        }
      
#ifdef USE_CURL
      fetch_perform();
#endif /* USE_CURL */
      
      if (gdata.debug > 3)
        {
          select_dump("got", highests);
        }
      
      if (gdata.needsshutdown)
        {
          gdata.needsshutdown = 0;
          shutdowniroffer();
        }
      
      if (gdata.needsreap)
        {
          pid_t child;
          int status;
          
          gdata.needsreap = 0;
          
          while ((child = waitpid(-1, &status, WNOHANG)) > 0)
            {
              for (ss=0; ss<gdata.networks_online; ss++)
                {
                  if (gdata.networks[ss].serverstatus == SERVERSTATUS_RESOLVING)
                    {
                      if (child == gdata.networks[ss].serv_resolv.child_pid)
                        {
                      /* lookup failed */
#ifdef NO_WSTATUS_CODES
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                              "Unable to resolve server %s (status=0x%.8X)",
                              gdata.networks[ss].curserver.hostname,
                              status);
#else
#ifndef NO_HOSTCODES
                      if (WIFEXITED(status) &&
                          (WEXITSTATUS(status) >= 20) &&
                          (WEXITSTATUS(status) <= 23))
                        {
                          const char *errstr;
                          switch (WEXITSTATUS(status))
                            {
                            case 20:
                              errstr = "host not found";
                              break;
                              
                            case 21:
                              errstr = "no ip address";
                              break;
                              
                            case 22:
                              errstr = "non-recoverable name server";
                              break;
                              
                            case 23:
                              errstr = "try again later";
                              break;
                              
                            default:
                              errstr = "unknown";
                              break;
                            }
                          ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                                  "Unable to resolve server %s (%s)",
                                  gdata.networks[ss].curserver.hostname, errstr);
                        }
                      else
#endif
                        {
                          ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                                  "Unable to resolve server %s (status=0x%.8X, %s: %d)",
                                  gdata.networks[ss].curserver.hostname,
                                  status,
                                  WIFEXITED(status) ? "exit" : WIFSIGNALED(status) ? "signaled" : "??",
                                  WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status) : 0);
                        }
#endif
                          gdata.networks[ss].serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
                        }
                      /* else  this is an old child, ignore */
                    }
              
                  if (child == gdata.networks[ss].serv_resolv.child_pid)
                    {
                  /* cleanup */
                  close(gdata.networks[ss].serv_resolv.sp_fd[0]);
                  FD_CLR(gdata.networks[ss].serv_resolv.sp_fd[0], &gdata.readset);
                  gdata.networks[ss].serv_resolv.sp_fd[0] = 0;
                  gdata.networks[ss].serv_resolv.child_pid = 0;
                    }
                } /* networks */
            }
        }
      
      /*----- one second check ----- */
      
      updatecontext();
      
      if (gettimeofday(&timestruct, NULL) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"gettimeofday() failed! %s\n",strerror(errno));
        }
      
      gdata.curtimems = timeval_to_ms(&timestruct);
      gdata.curtime = timestruct.tv_sec;
      
      /* adjust for drift and cpu usage */
      if ((gdata.curtimems > (last250ms+1000)) ||
          (gdata.curtimems < last250ms))
        {
          /* skipped forward or backwards, correct */
          last250ms = gdata.curtimems-250;
        }
      
      if (gdata.curtimems >= (last250ms+250))
        {
          changequartersec = 1;
          /* note bandwidth limiting requires no drift! */
          last250ms += 250;
        }
      else
        {
          changequartersec = 0;
        }
      
      changesec = 0;
      if (gdata.curtime != lasttime) {
         
         if (gdata.curtime < lasttime-3) {
            outerror(OUTERROR_TYPE_WARN,"System Time Changed Backwards %lim %lis!!\n",
               (long)(lasttime-gdata.curtime)/60,(long)(lasttime-gdata.curtime)%60);
            }
         
         if (gdata.curtime > lasttime+10) {
            outerror(OUTERROR_TYPE_WARN,"System Time Changed Forward or Mainloop Skipped %lim %lis!!\n",
               (long)(gdata.curtime-lasttime)/60,(long)(gdata.curtime-lasttime)%60);
            if (gdata.debug > 0)
              {
                dumpcontext();
              }
            }
         
         lasttime = gdata.curtime;
         changesec = 1;
         
         }
      
      if (changesec && lasttime/60/60 != lasthour) {
         lasthour = lasttime/60/60;
         changehour = 1;
         }
      
      if (changesec && lasttime/60 != lastmin) {
         lastmin = lasttime/60;
         changemin = 1;
         }
      
      updatecontext();
      
      if (changesec) {
         gdata.totaluptime++;
         gdata.xdccsent[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;
         gdata.xdccrecv[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;
         
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsum[i];
         if (((float)xdccsent)/XDCC_SENT_SIZE/1024.0 > gdata.sentrecord)
            gdata.sentrecord = ((float)xdccsent)/XDCC_SENT_SIZE/1024.0;
         gdata.xdccsum[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;

         run_delayed_jobs();
      }

      if (changequartersec)
        {
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ( !tr->nomax &&
                   (tr->maxspeed > 0))
                {
                  tr->tx_bucket += tr->maxspeed * (1024 / 4);
                  tr->tx_bucket = min2(tr->tx_bucket, MAX_TRANSFER_TX_BURST_SIZE * tr->maxspeed * 1024);
                }
              tr = irlist_get_next(tr);
            }
        }
      
      updatecontext();
      
      /*----- see if anything waiting on console ----- */
      gdata.needsclear = 0;
      if (!gdata.background && FD_ISSET(fileno(stdin), &gdata.readset))
         parseconsole();
      
      updatecontext();
      
      for (ss=0; ss<gdata.networks_online; ss++)
        {
          gnetwork = &(gdata.networks[ss]);
      /*----- see if gdata.ircserver is sending anything to us ----- */
      if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED && FD_ISSET(gnetwork->ircserver, &gdata.readset)) {
         char tempbuffa[INPUT_BUFFER_LENGTH];
         gnetwork->lastservercontact = gdata.curtime;
         gnetwork->servertime = 0;
         memset(&tempbuffa, 0, INPUT_BUFFER_LENGTH);
         length = readserver_ssl(&tempbuffa, INPUT_BUFFER_LENGTH);
         
         if (length < 1) {
            if (errno != EAGAIN) {
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                    "Closing Server Connection on %s: %s",
                    gnetwork->name, (length<0) ? strerror(errno) : "Closed");
            if (gdata.exiting)
              {
                gnetwork->recentsent = 0;
              }
            close_server();
            mydelete(gnetwork->curserveractualname);
            }
            }
         else
           {
             j=strlen(gnetwork->server_input_line);
             for (i=0; i<(unsigned int)length; i++)
               {
                 if ((tempbuffa[i] == '\n') || (j == (INPUT_BUFFER_LENGTH-1)))
                   {
                     if (j && (gnetwork->server_input_line[j-1] == 0x0D))
                       {
                         j--;
                       }
                     gnetwork->server_input_line[j] = '\0';
                     parseline(removenonprintable(gnetwork->server_input_line));
                     j = 0;
                   }
                 else
                   {
                     gnetwork->server_input_line[j] = tempbuffa[i];
                     j++;
                   }
               }
             gnetwork->server_input_line[j] = '\0';
           }
        }
      
      if ((gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE) &&
	((FD_ISSET(gnetwork->ircserver, &gdata.writeset)) || (FD_ISSET(gnetwork->ircserver, &gdata.readset))))
        {
          handshake_ssl();
        }
      
      if (gnetwork->serverstatus == SERVERSTATUS_TRYING && FD_ISSET(gnetwork->ircserver, &gdata.writeset))
        {
          int callval_i;
          int connect_error;
          SIGNEDSOCK int connect_error_len = sizeof(connect_error);
          
          callval_i = getsockopt(gnetwork->ircserver,
                                 SOL_SOCKET, SO_ERROR,
                                 &connect_error, &connect_error_len);
          
          if (callval_i < 0)
            {
              outerror(OUTERROR_TYPE_WARN,
                       "Couldn't determine connection status: %s on %s",
                       strerror(errno), gnetwork->name);
            }
          else if (connect_error)
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Connection Failed: %s on %s", strerror(connect_error), gnetwork->name);
            }
          
          if ((callval_i < 0) || connect_error)
            {
              close_server();
            }
          else
            {
	    SIGNEDSOCK int addrlen; 
          
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Server Connection to %s Established, Logging In",  gnetwork->name);
            gnetwork->serverstatus = SERVERSTATUS_CONNECTED;
            gnetwork->connecttime = gdata.curtime;
            gnetwork->botstatus = BOTSTATUS_LOGIN;
            ch = irlist_get_head(&(gnetwork->channels));
            if (ch == NULL)
              {
                gnetwork->botstatus = BOTSTATUS_JOINED;
                start_sends();
              }
            FD_CLR(gnetwork->ircserver, &gdata.writeset);
#if 0
            if (set_socket_nonblocking(gnetwork->ircserver, 0) < 0 )
	      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Blocking");
#endif
	    
            addrlen = sizeof(gnetwork->myip);
            bzero((char *) &(gnetwork->myip), sizeof(gnetwork->myip));
            if (getsockname(gnetwork->ircserver, &(gnetwork->myip.sa), &addrlen) >= 0)
              {
                if (gdata.debug > 0)
                  {
                    char *msg;
                    msg = mycalloc(maxtextlength);
                    my_getnameinfo(msg, maxtextlength -1, &(gnetwork->myip.sa));
                    ioutput(OUT_S, COLOR_YELLOW, "using %s", msg);
                    mydelete(msg);
                  }
                if (!gnetwork->usenatip)
                  {
                    gnetwork->ourip = ntohl(gnetwork->myip.sin.sin_addr.s_addr);
                    if (gdata.debug > 0)
                      {
                        ioutput(OUT_S, COLOR_YELLOW, "ourip = %lu.%lu.%lu.%lu",
                                (gnetwork->ourip >> 24) & 0xFF,
                                (gnetwork->ourip >> 16) & 0xFF,
                                (gnetwork->ourip >>  8) & 0xFF,
                                (gnetwork->ourip      ) & 0xFF
                                );
                      }
                  }
              }
            else
              outerror(OUTERROR_TYPE_WARN, "couldn't get ourip on %s", gnetwork->name);
	    
            handshake_ssl();
            }
         }
      
      if ((gnetwork->serverstatus == SERVERSTATUS_RESOLVING) &&
          FD_ISSET(gnetwork->serv_resolv.sp_fd[0], &gdata.readset))
        {
          res_addrinfo_t remote;
          length = read(gnetwork->serv_resolv.sp_fd[0],
                        &remote, sizeof(res_addrinfo_t));
          
          kill(gnetwork->serv_resolv.child_pid, SIGKILL);
          FD_CLR(gnetwork->serv_resolv.sp_fd[0], &gdata.readset);
          
          if (length != sizeof(res_addrinfo_t))
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                      "Error resolving server %s on %s",
                      gnetwork->curserver.hostname, gnetwork->name);
              gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
          else
            {
              /* continue with connect */
              if (connectirc2(&remote))
                {
                  /* failed */
                  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
                }
            }
        }
      
      if (changesec && gnetwork->serverstatus == SERVERSTATUS_RESOLVING)
        {
          int timeout;
          timeout = CTIMEOUT + (gnetwork->serverconnectbackoff * CBKTIMEOUT);
          
          if (gnetwork->lastservercontact + timeout < gdata.curtime)
            {
              kill(gnetwork->serv_resolv.child_pid, SIGKILL);
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Resolve Timed Out (%u seconds) on %s", timeout, gnetwork->name);
              gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
        }
      
      if (changesec && ((gnetwork->serverstatus == SERVERSTATUS_TRYING) || (gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE)))
        {
          int timeout;
          timeout = CTIMEOUT + (gnetwork->serverconnectbackoff * CBKTIMEOUT);
          
          if (gnetwork->lastservercontact + timeout < gdata.curtime)
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Connection Timed Out (%u seconds) on %s", timeout, gnetwork->name);
              close_server();
            }
        }
      
      if (gdata.needsswitch)
        {
          gdata.needsswitch = 0;
          switchserver(-1);
        }
        } /* networks */
      gnetwork = NULL;
      
      updatecontext();
      
      ul = irlist_get_head(&gdata.uploads);
      while(ul)
        {
          gnetwork = &(gdata.networks[ul->net]);
          /*----- see if uploads are sending anything to us ----- */
          if (ul->ul_status == UPLOAD_STATUS_GETTING && FD_ISSET(ul->con.clientsocket, &gdata.readset))
            {
              l_transfersome(ul);
            }

          if (ul->ul_status == UPLOAD_STATUS_LISTENING && FD_ISSET(ul->con.listensocket, &gdata.readset))
            {
              l_setup_accept(ul);
            }

          if (ul->ul_status == UPLOAD_STATUS_CONNECTING && FD_ISSET(ul->con.clientsocket, &gdata.writeset))
            {
              int callval_i;
              int connect_error;
              SIGNEDSOCK int connect_error_len = sizeof(connect_error);
              
              callval_i = getsockopt(ul->con.clientsocket,
                                     SOL_SOCKET, SO_ERROR,
                                     &connect_error, &connect_error_len);
              
              if (callval_i < 0)
                {
                  int errno2 = errno;
                  outerror(OUTERROR_TYPE_WARN,
                           "Couldn't determine upload connection status on %s: %s",
                           gnetwork->name, strerror(errno));
                  l_closeconn(ul, "Upload Connection Failed status:", errno2);
                }
              else if (connect_error)
                {
                  l_closeconn(ul,"Upload Connection Failed",connect_error);
                }
              
              if ((callval_i < 0) || connect_error)
                {
                  FD_CLR(ul->con.clientsocket, &gdata.writeset);
                }
              else
                {
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                          "Upload Connection Established on %s", gnetwork->name);
                  ul->ul_status = UPLOAD_STATUS_GETTING;
                  FD_CLR(ul->con.clientsocket, &gdata.writeset);
                  notice(ul->nick,"DCC Connection Established");
                  ul->con.connecttime = gdata.curtime;
                }
            }
          
          if (changesec && ul->ul_status == UPLOAD_STATUS_CONNECTING && ul->con.lastcontact + CTIMEOUT < gdata.curtime)
            {
              FD_CLR(ul->con.clientsocket, &gdata.readset);
              l_closeconn(ul,"Upload Connection Timed Out",0);
            }
          
          if (changesec)
            {            
              l_istimeout(ul);
            }
          
          if (changesec && ul->ul_status == UPLOAD_STATUS_DONE)
            {
              close_qupload(ul->net, ul->nick);
              mydelete(ul->nick);
              mydelete(ul->hostname);
              mydelete(ul->uploaddir);
              mydelete(ul->file);
              mydelete(ul->con.remoteaddr);
              ul = irlist_delete(&gdata.uploads, ul);
            }
          else
            {
              ul = irlist_get_next(ul);
            }
        }
      gnetwork = NULL;
      
      updatecontext();
      /*----- see if dccchat is sending anything to us ----- */
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_get_next(chat))
        {
          char tempbuffa[INPUT_BUFFER_LENGTH];
          gnetwork = &(gdata.networks[chat->net]);
          switch (chat->status)
          {
          case DCCCHAT_CONNECTING:
            if (FD_ISSET(chat->con.clientsocket, &gdata.writeset))
            {
              int callval_i;
              int connect_error;
              SIGNEDSOCK int connect_error_len = sizeof(connect_error);
              
              callval_i = getsockopt(chat->con.clientsocket,
                                     SOL_SOCKET, SO_ERROR,
                                     &connect_error, &connect_error_len);
              
              if (callval_i < 0)
                {
                  int errno2 = errno;
                  outerror(OUTERROR_TYPE_WARN,
                           "Couldn't determine dcc connection status on %s: %s",
                           gnetwork->name, strerror(errno));
                  notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(errno2));
                }
              else if (connect_error)
                {
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "DCC Chat Connect Attempt Failed on %s: %s",
                          gnetwork->name, strerror(connect_error));
                  notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(connect_error));
                }
              
              if ((callval_i < 0) || connect_error)
                {
                  shutdowndccchat(chat,0);
                }
              else
                {
                  setupdccchatconnected(chat);
                }
            }
            break;
          case DCCCHAT_LISTENING:
            if (FD_ISSET(chat->con.listensocket, &gdata.readset))
              {
                setupdccchataccept(chat);
              }
            break;
          case DCCCHAT_AUTHENTICATING:
          case DCCCHAT_CONNECTED:
            if (FD_ISSET(chat->con.clientsocket, &gdata.readset))
              {
                  memset(tempbuffa, 0, INPUT_BUFFER_LENGTH);
                  length = recv(chat->con.clientsocket, &tempbuffa, INPUT_BUFFER_LENGTH, MSG_DONTWAIT);
                  
                  if (length < 1)
                    {
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "DCC Chat Lost on %s: %s",
                              gnetwork->name,
                              (length<0) ? strerror(errno) : "Closed");
                      notice(chat->nick, "DCC Chat Lost: %s", (length<0) ? strerror(errno) : "Closed");
                      shutdowndccchat(chat,0);
                      /* deleted below */
                    }
                  else
                    {
                      j=strlen(chat->dcc_input_line);
                      for (i=0; i<(unsigned int)length; i++)
                        {
                          if ((tempbuffa[i] == '\n') || (j == (INPUT_BUFFER_LENGTH-1)))
                            {
                              if (j && (chat->dcc_input_line[j-1] == 0x0D))
                                {
                                  j--;
                                }
                              chat->dcc_input_line[j] = '\0';
                              parsedccchat(chat, chat->dcc_input_line);
                              j = 0;
                            }
                          else
                            {
                              chat->dcc_input_line[j] = tempbuffa[i];
                              j++;
                            }
                        }
                      chat->dcc_input_line[j] = '\0';
                    }
                  break;
                  
              }
            break;
          case DCCCHAT_UNUSED:
          default:
            break;
          }
        }
      gnetwork = NULL;
      
      updatecontext();
      
      i = j = select_starting_transfer(irlist_size(&gdata.trans));
      
      tr = irlist_get_nth(&gdata.trans, i);
      
      /* first: do from cur to end */
      while(tr)
        {
          if (tr->tr_status == TRANSFER_STATUS_SENDING)
            {
              /*----- look for transfer some -----  send at least once a second, or more if necessary */
              if (changequartersec || FD_ISSET(tr->con.clientsocket, &gdata.writeset))
                {
                  gnetwork = &(gdata.networks[tr->net]);
                  t_transfersome(tr);
                }
            }
          tr = irlist_get_next(tr);
        }
      
      /* second: do from begin to cur-1 */
      tr = irlist_get_head(&gdata.trans);
      while(tr && j--)
        {
          if (tr->tr_status == TRANSFER_STATUS_SENDING)
            {
              /*----- look for transfer some -----  send at least once a second, or more if necessary */
              if (changequartersec || FD_ISSET(tr->con.clientsocket, &gdata.writeset))
                {
                  gnetwork = &(gdata.networks[tr->net]);
                  t_transfersome(tr);
                }
            }
          tr = irlist_get_next(tr);
        }
      
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          gnetwork = &(gdata.networks[tr->net]);
          /*----- look for listen reminders ----- */
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->con.lastcontact) >= 30) &&
              (tr->reminded == 0))
            {
              t_remind(tr);
            }
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->con.lastcontact) >= 90) &&
              (tr->reminded == 1) && !gdata.quietmode)
            {
              t_remind(tr);
            }
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->con.lastcontact) >= 150) &&
              (tr->reminded == 2) && !gdata.quietmode)
            {
              t_remind(tr);
            }

         if (changesec &&
             (tr->tr_status == TRANSFER_STATUS_CONNECTING) &&
             FD_ISSET(tr->con.clientsocket, &gdata.writeset))
            {
              t_connected(tr);
            }
  
          /*----- look for listen->connected ----- */
          if ((tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              FD_ISSET(tr->con.listensocket, &gdata.readset))
            {
              t_establishcon(tr);
              t_check_new_connection(tr);
            }
          
          /*----- look for junk to read ----- */
          if (((tr->tr_status == TRANSFER_STATUS_SENDING) ||
               (tr->tr_status == TRANSFER_STATUS_WAITING)) &&
              FD_ISSET(tr->con.clientsocket, &gdata.readset))
            {
              t_readjunk(tr);
            }
          
          /*----- look for done flushed status ----- */
          if (tr->tr_status == TRANSFER_STATUS_WAITING)
            {
              t_flushed(tr);
            }
          
          /*----- look for lost transfers ----- */
          if (changesec && (tr->tr_status != TRANSFER_STATUS_DONE))
            {
              t_istimeout(tr);
            }
          
          /*----- look for finished transfers ----- */
          if (tr->tr_status == TRANSFER_STATUS_DONE)
            {
              char *trnick;

              trnick = tr->nick;
              mydelete(tr->caps_nick);
              mydelete(tr->hostname);
              mydelete(tr->con.localaddr);
              mydelete(tr->con.remoteaddr);
              tr = irlist_delete(&gdata.trans, tr);
              
              if (!gdata.exiting &&
                  irlist_size(&gdata.mainqueue) &&
                  (irlist_size(&gdata.trans) < gdata.slotsmax))
                {
                  check_idle_queue();
                  send_from_queue(0, 0, trnick);
                }
              mydelete(trnick);
            }
          else
            {
              tr = irlist_get_next(tr);
            }
         }
      gnetwork = NULL;

#ifndef WITHOUT_TELNET
      telnet_perform();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
      h_perform(changesec);
#endif /* WITHOUT_HTTP */
      
      /*----- time for a delayed shutdown? ----- */
      if (changesec && gdata.delayedshutdown)
        {
          if (!irlist_size(&gdata.trans))
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Delayed Shutdown Activated, No Transfers Remaining");
              shutdowniroffer();
            }
        }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
        /*----- send server stuff ----- */
        if (changesec) {
           sendserver();
           if (gdata.curtime%INAMNT_SIZE == (INAMNT_SIZE-1))
              gnetwork->inamnt[0] = 0;
           else
              gnetwork->inamnt[gdata.curtime%INAMNT_SIZE+1] = 0;
           }
        /*----- see if we can send out some xdcc lists */
        if (changesec && gnetwork->serverstatus == SERVERSTATUS_CONNECTED) {
          if (!irlist_size((&gnetwork->serverq_normal)) && !irlist_size(&(gnetwork->serverq_slow)))
            sendxdlqueue();
          }
        }
      gnetwork = NULL;
      
      /*----- see if its time to change maxb */
      if (changehour) {
         gdata.maxb = gdata.overallmaxspeed;
         if (gdata.overallmaxspeeddayspeed != gdata.overallmaxspeed) {
            struct tm *localt;
            localt = localtime(&gdata.curtime);

            if ((unsigned int)localt->tm_hour >= gdata.overallmaxspeeddaytimestart
                && (unsigned int)localt->tm_hour < gdata.overallmaxspeeddaytimeend
                && ( gdata.overallmaxspeeddaydays & (1 << (unsigned int)localt->tm_wday)) )
               gdata.maxb = gdata.overallmaxspeeddayspeed;
            }
         }
      
      /*----- see if we've hit a transferlimit or need to reset counters */
      if (changesec)
        {
          unsigned int ii;
          unsigned int transferlimits_over = 0;
          for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
            {
              /* reset counters? */
              if ((!gdata.transferlimits[ii].ends) ||
                  (gdata.transferlimits[ii].ends < gdata.curtime))
                {
                  struct tm *localt;
                  if (gdata.transferlimits[ii].limit && gdata.transferlimits[ii].ends)
                    {
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "Resetting %s transfer limit, used %" LLPRINTFMT "uMB of the %" LLPRINTFMT "uMB limit",
                              transferlimit_type_to_string(ii),
                              gdata.transferlimits[ii].used / 1024 / 1024,
                              gdata.transferlimits[ii].limit / 1024 / 1024);
                    }
                  
                  /* find our next end time */
                  localt = localtime(&gdata.curtime);
                  localt->tm_sec = localt->tm_min = localt->tm_hour = 0; /* midnight */
                  switch (ii)
                    {
                    case TRANSFERLIMIT_DAILY:
                      /* tomorrow */
                      localt->tm_mday++;
                      break;
                      
                    case TRANSFERLIMIT_WEEKLY:
                      /* next sunday morning */
                      localt->tm_mday += 7 - localt->tm_wday;
                      break;
                      
                    case TRANSFERLIMIT_MONTHLY:
                      /* next month */
                      localt->tm_mday = gdata.start_of_month;
                      localt->tm_mon++;
                      break;
                      
                    default:
                      outerror(OUTERROR_TYPE_CRASH, "unknown type %u", ii);
                    }
                  /* tm_wday and tm_yday are ignored in mktime() */
                  gdata.transferlimits[ii].ends = mktime(localt);
                  gdata.transferlimits[ii].used = 0;
                  if ( ii == TRANSFERLIMIT_DAILY )
                    reset_download_limits();
                }
              
              if (!transferlimits_over &&
                  gdata.transferlimits[ii].limit &&
                  (gdata.transferlimits[ii].used >= gdata.transferlimits[ii].limit))
                {
                  transferlimits_over = 1 + ii;
                  
                  if (!gdata.transferlimits_over)
                    {
                      char *tempstr = transfer_limit_exceeded_msg(ii);
                      
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "All %" LLPRINTFMT "uMB of the %s transfer limit used. Stopping transfers.",
                              gdata.transferlimits[ii].limit / 1024 / 1024,
                              transferlimit_type_to_string(ii));
                      
                      /* remove queued users */
                      queue_all_remove(&gdata.mainqueue, tempstr);
                      queue_all_remove(&gdata.idlequeue, tempstr);
                      
                      /* stop transfers */
                      for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
                        {
                          if (tr->tr_status != TRANSFER_STATUS_DONE)
                            {
                              gnetwork = &(gdata.networks[tr->net]);
                              t_closeconn(tr,tempstr,0);
                            }
                        }
                      
                      gnetwork = NULL;
                      mydelete(tempstr);
                    }
                }
            }
          
          if (gdata.transferlimits_over != transferlimits_over)
            {
              if (!transferlimits_over)
                {
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "No longer over any transfer limits. Transfers are now allowed.");
                }
              gdata.transferlimits_over = transferlimits_over;
            }
        }
      
      /*----- gdata.autoignore_threshold seconds ----- */
      if (changesec && (gdata.curtime > lastignoredec + gdata.autoignore_threshold))
        {
          igninfo *ignore;
          
          lastignoredec += gdata.autoignore_threshold;
          
          ignore = irlist_get_head(&gdata.ignorelist);
          
          while(ignore)
            {
              ignore->bucket--;
              if ((ignore->flags & IGN_IGNORING) && (ignore->bucket == 0))
                {
                  ignore->flags &= ~IGN_IGNORING;
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "Ignore removed for %s",ignore->hostmask);
                  write_statefile();
                }
              if (ignore->bucket == 0)
                {
                  mydelete(ignore->hostmask);
                  ignore = irlist_delete(&gdata.ignorelist, ignore);
                }
              else
                {
                  ignore = irlist_get_next(ignore);
                }
            }
        }
      
      /*----- periodicmsg_time seconds ----- */
      if (changesec && (gdata.curtime > lastperiodicmsg + gdata.periodicmsg_time*60)) {
         lastperiodicmsg = gdata.curtime;
         
         for (ss=0; ss<gdata.networks_online; ss++) {
            if (gdata.periodicmsg_nick && gdata.periodicmsg_msg
            && (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED) ) {
               gnetwork = &(gdata.networks[ss]);
               privmsg(gdata.periodicmsg_nick, "%s", gdata.periodicmsg_msg);
            }
         }
         
         }
      gnetwork = NULL;
      
      updatecontext();
      
      /*----- 5 seconds ----- */
      if (changesec && (gdata.curtime - last5sec > 4)) {
         last5sec = gdata.curtime;
         
         updatecontext();
         /*----- server timeout ----- */
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
           if (gdata.needsshutdown)
             continue;
         if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
             (gdata.curtime > gnetwork->lastservercontact + SRVRTOUT)) {
            if (gnetwork->servertime < 3)
              {
                const char *servname = gnetwork->curserveractualname ? gnetwork->curserveractualname : gnetwork->curserver.hostname;
                size_t     len       = 6 + strlen(servname);
                char       *tempstr3 = mycalloc(len + 1);
                snprintf(tempstr3, len + 1, "PING %s\n", servname);
                writeserver_ssl(tempstr3, len);
                if (gdata.debug > 0)
                  {
                    tempstr3[len-1] = '\0';
                    len--;
                    ioutput(OUT_S, COLOR_MAGENTA, "<NORES<: %s", tempstr3);
                  }
                mydelete(tempstr3);
                gnetwork->servertime++;
              }
            else if (gnetwork->servertime == 3) {
               ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                       "Closing Server Connection on %s: No Response for %u minutes.",
                       gnetwork->name, SRVRTOUT/60);
               close_server();
               gnetwork->servertime = 0;
               }
            }
         
         
         /*----- ping server ----- */
         if (gnetwork->recentsent) {
            pingserver();
            gnetwork->recentsent--;
            }
         
         }
         } /* networks */
      gnetwork = NULL;
      
      /*----- 4 seconds ----- */
      if (changesec && (gdata.curtime - last4sec > 3))
        {
         
         /*----- update lastspeed, check minspeed ----- */
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ( tr->con.connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
                {
                  tr->lastspeed = 
                    (tr->lastspeed)*DCL_SPDW_I + 
                    (((float)(tr->bytessent-tr->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_I)/((float)(gdata.curtime-last4sec)*1.0);
                }
              else                                              /* ongoing */
                {
                  tr->lastspeed = 
                    (tr->lastspeed)*DCL_SPDW_O + 
                    (((float)(tr->bytessent-tr->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_O)/((float)(gdata.curtime-last4sec)*1.0);
                  }
              
              tr->lastspeedamt = tr->bytessent;
              
              t_checkminspeed(tr);
              
              tr = irlist_get_next(tr);
            }
         
          ul = irlist_get_head(&gdata.uploads);
          while(ul)
            {
              if ( ul->con.connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
                {
                  ul->lastspeed = 
                    (ul->lastspeed)*DCL_SPDW_I + 
                    (((float)(ul->bytesgot-ul->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_I)/((float)(gdata.curtime-last4sec)*1.0);
                }
              else                                                /* ongoing */
                {
                  ul->lastspeed = 
                    (ul->lastspeed)*DCL_SPDW_O + 
                    (((float)(ul->bytesgot-ul->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_O)/((float)(gdata.curtime-last4sec)*1.0);
                }
              ul->lastspeedamt = ul->bytesgot;
              
              ul = irlist_get_next(ul);
            }
          
          last4sec = gdata.curtime;
        }

      updatecontext();
      /*----- check for size change ----- */
      if (changesec)
         checktermsize();
      
      updatecontext();
      
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      /*----- plist stuff ----- */
      if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
          changemin &&
          irlist_size(&gdata.xdccs) &&
          !gdata.transferlimits_over &&
          (irlist_size(&(gnetwork->serverq_channel)) < irlist_size(&gdata.xdccs)) &&
          (!gdata.queuesize || irlist_size(&gdata.mainqueue) < gdata.queuesize) &&
          (gdata.nolisting <= gdata.curtime))
        {
            char *tchanf = NULL, *tchanm = NULL, *tchans = NULL;
            
            for(ch = irlist_get_head(&(gnetwork->channels));
                ch;
                ch = irlist_get_next(ch))
              {
                if ((ch->flags & CHAN_ONCHAN) &&
                    (ch->nextann < gdata.curtime) &&
                    ch->plisttime &&
                    (((gdata.curtime / 60) % ch->plisttime) == ch->plistoffset))
                  {
                    if (ch->pgroup != NULL)
                      {
                        ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (pgroup)", ch->name);
                        pubplist = mycalloc(sizeof(userinput));
                        pubplist->method = method_xdl_channel;
                        pubplist->net = gnetwork->net;
                        pubplist->level = ADMIN_LEVEL_PUBLIC;
                        a_fillwith_plist(pubplist, ch->name, ch);
                        u_parseit(pubplist);
                        mydelete(pubplist);
                        continue;
                      }
                    if (ch->flags & CHAN_MINIMAL)
                      {
                        if (tchanm)
                          {
                            strncat(tchanm,",",maxtextlength-strlen(tchanm)-1);
                            strncat(tchanm,ch->name,maxtextlength-strlen(tchanm)-1);
                          }
                        else
                          {
                            tchanm = mycalloc(maxtextlength);
                            strncpy(tchanm,ch->name,maxtextlength-1);
                          }
                      }
                    else if (ch->flags & CHAN_SUMMARY)
                      {
                        if (tchans)
                          {
                            strncat(tchans,",",maxtextlength-strlen(tchans)-1);
                            strncat(tchans,ch->name,maxtextlength-strlen(tchans)-1);
                          }
                        else
                          {
                            tchans = mycalloc(maxtextlength);
                            strncpy(tchans,ch->name,maxtextlength-1);
                          }
                      }
                    else
                      {
                        if (tchanf)
                          {
                            strncat(tchanf,",",maxtextlength-strlen(tchanf)-1);
                            strncat(tchanf,ch->name,maxtextlength-strlen(tchanf)-1);
                          }
                        else
                          {
                            tchanf = mycalloc(maxtextlength);
                            strncpy(tchanf,ch->name,maxtextlength-1);
                          }
                      }
                  }
              }
            
            if (tchans)
              {
                if (gdata.restrictprivlist && !gdata.creditline && !gdata.headline)
                  {
                    ioutput(OUT_S|OUT_D, COLOR_NO_COLOR,
                            "Can't send Summary Plist to %s (restrictprivlist is set and no creditline or headline, summary makes no sense!)", tchans);
                  }
                else
                  {
                    ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (summary)", tchans);
                    pubplist = mycalloc(sizeof(userinput));
                    a_fillwith_msg2(pubplist, tchans, "XDL");
                    pubplist->method = method_xdl_channel_sum;
                    u_parseit(pubplist);
                    mydelete(pubplist);
                  }
                mydelete(tchans);
              }
            if (tchanf) {
               ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (full)", tchanf);
               pubplist = mycalloc(sizeof(userinput));
               a_fillwith_plist(pubplist, tchanf, NULL);
               pubplist->method = method_xdl_channel;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanf);
               }
            if (tchanm) {
               ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (minimal)", tchanm);
               pubplist = mycalloc(sizeof(userinput));
               a_fillwith_msg2(pubplist, tchanm, "XDL");
               pubplist->method = method_xdl_channel_min;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanm);
               }
            
         }
         } /* networks */
      gnetwork = NULL;
      
      updatecontext();
      /*----- low bandwidth send, save state file ----- */
      if (changesec && (gdata.curtime - last3min > 180)) {
         last3min = gdata.curtime;
         
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsent[i];
         xdccsent /= XDCC_SENT_SIZE*1024;
         
         if ((xdccsent < (unsigned)gdata.lowbdwth) &&
             !gdata.exiting &&
             irlist_size(&gdata.mainqueue) &&
             (irlist_size(&gdata.trans) < gdata.maxtrans))
           {
             check_idle_queue();
             send_from_queue(1, 0, NULL);
           }
         write_files();
         }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      /*----- queue notify ----- */
      if (changesec && gdata.notifytime && (!gdata.quietmode) &&
          (gdata.curtime > gnetwork->lastnotify + (gdata.notifytime*60)))
        {
         gnetwork->lastnotify = gdata.curtime;
         
         if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
           {
             if ((irlist_size(&(gnetwork->serverq_fast)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_normal)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_slow)) >= 50))
               {
                 ioutput(OUT_S|OUT_D|OUT_L, COLOR_NO_COLOR,
                         "notifications skipped on %s, server queue is rather large",
                         gnetwork->name);
               }
             else
               {
                 notifyqueued();
                 notifybandwidth();
                 notifybandwidthtrans();
               }
           }
         
         }
         } /* networks */
      gnetwork = NULL;
      
      updatecontext();
      /*----- log stats / remote admin stats ----- */
      if ( gdata.logstats && changesec && gdata.logfile &&
           (gdata.curtime >= last2min + gdata.status_time_dcc_chat))
        {
          last2min = gdata.curtime;
          logstat();
          
          chat_writestatus();
          
          isrotatelog();
        }
      
      updatecontext();

      /* look to see if any files changed */
      if (changesec)
        look_for_file_remove();

      updatecontext();
      
      /*----- 20 seconds ----- */
      if (changesec && (gdata.curtime - last20sec > 19)) {
         expire_badip();
         
         if (gdata.logfd != FD_UNUSED)
           {
             /* cycle */
             close(gdata.logfd);
             gdata.logfd = FD_UNUSED;
           }
         
         updatecontext();
         
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
         /* try rejoining channels not on */
         ch = irlist_get_head(&(gnetwork->channels));
         while(ch)
           {
             if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
                 !(ch->flags & CHAN_ONCHAN))
               {
                 joinchannel(ch);
               }
             ch = irlist_get_next(ch);
           }
         } /* networks */
         gnetwork = NULL;
         
         last20sec = gdata.curtime;
         
         updatecontext();
         
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
           /* try to regain nick */
           if (!gnetwork->user_nick || strcmp(get_config_nick(), gnetwork->user_nick))
             {
               writeserver(WRITESERVER_NORMAL, "NICK %s", get_config_nick());
             }
           } /* networks */
         gnetwork = NULL;
         
         updatecontext();
         
         /* update status line */
         if (!gdata.background && !gdata.noscreen) {
            char tempstr[maxtextlength];
            char tempstr2[maxtextlengthshort];
            
            if (gdata.attop) gotobot();
            
            tostdout(IRVT_SAVE_CURSOR);
            
            getstatusline(tempstr,maxtextlength);
            tempstr[min2(maxtextlength-2,gdata.termcols-4)] = '\0';
            snprintf(tempstr2, maxtextlengthshort, IRVT_CURSOR_HOME1 "[ %%-%us ]", gdata.termlines - 1, gdata.termcols - 4);
            tostdout(tempstr2,tempstr);
            
            tostdout(IRVT_CURSOR_HOME2 IRVT_UNSAVE_CURSOR, gdata.termlines, gdata.termcols);
            }
         
         admin_jobs();
#ifdef USE_RUBY
	 rehash_myruby(1);
#endif /* USE_RUBY */
         }
      
      updatecontext();
      
      if (changemin)
        {
          reverify_restrictsend();
          update_hour_dinoex(lastmin);
          check_idle_queue();
          clean_uploadhost();
          auto_rehash();
        }

      updatecontext();
      
      if ((gdata.md5build.file_fd != FD_UNUSED) &&
          FD_ISSET(gdata.md5build.file_fd, &gdata.readset))
        {
          ssize_t howmuch;
#if defined(_OS_CYGWIN)
          int reads_per_loop = 32;
#else /* _OS_CYGWIN */
          int reads_per_loop = 64;
#endif /* _OS_CYGWIN */
          
          assert(gdata.md5build.xpack);
          
          while (reads_per_loop--)
            {
              howmuch = read(gdata.md5build.file_fd, gdata.sendbuff, BUFFERSIZE);
              
              if (gdata.debug > 4)
                {
                  ioutput(OUT_S, COLOR_YELLOW, "[MD5 Pack %u]: read %ld",
                          number_of_pack(gdata.md5build.xpack), (long)howmuch);
                }
              
              if ((howmuch < 0) && (errno != EAGAIN))
                {
                  outerror(OUTERROR_TYPE_WARN, "[MD5 Pack %u]: Can't read data from file '%s': %s",
                           number_of_pack(gdata.md5build.xpack),
                           gdata.md5build.xpack->file, strerror(errno));
                  
                  FD_CLR(gdata.md5build.file_fd, &gdata.readset);
                  close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  gdata.md5build.xpack = NULL;
                  break;
                }
              else if (howmuch < 0)
                {
                  break;
                }
              else if (howmuch == 0)
                {
                  /* EOF */
                  MD5Final(gdata.md5build.xpack->md5sum, &gdata.md5build.md5sum);
                  if (!gdata.nocrc32)
                    crc32_final(gdata.md5build.xpack);
                  gdata.md5build.xpack->has_md5sum = 1;
                  
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "[MD5 Pack %u]: is " MD5_PRINT_FMT,
                          number_of_pack(gdata.md5build.xpack),
                          MD5_PRINT_DATA(gdata.md5build.xpack->md5sum));
                  if (!gdata.nocrc32)
                    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                            "[CRC32 Pack %u]: is %.8lX",
                            number_of_pack(gdata.md5build.xpack), gdata.md5build.xpack->crc32);
                  
                  FD_CLR(gdata.md5build.file_fd, &gdata.readset);
                  close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  if (!gdata.nocrc32 && gdata.auto_crc_check)
                    {
                      const char *crcmsg = validate_crc32(gdata.md5build.xpack, 2);
                      if (crcmsg != NULL)
                        {
                           ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                                   "[CRC32 Pack %u]: File '%s' %s.",
                                   number_of_pack(gdata.md5build.xpack),
                                   gdata.md5build.xpack->file, crcmsg);
                        }
                    }
                  gdata.md5build.xpack = NULL;
                  break;
                }
              /* else got data */
              MD5Update(&gdata.md5build.md5sum, gdata.sendbuff, howmuch);
              if (!gdata.nocrc32)
                crc32_update((char *)gdata.sendbuff, howmuch);
            }
        }
      
      if (!gdata.nomd5sum && changesec && (!gdata.md5build.xpack))
        {
          unsigned int packnum = 1;
          /* see if any pack needs a md5sum calculated */
          if (gdata.nomd5_start <= gdata.curtime)
          for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd), packnum++)
            {
              if (!gdata.nocrc32)
                {
                  if (!xd->has_crc32)
                    xd->has_md5sum = 0; /* force recheck with crc */
                }
              if (!xd->has_md5sum)
                {
                  start_md5_hash(xd, packnum);
                  break;
                }
            }
        }
      
      updatecontext();
      
      if (gdata.exiting && has_closed_servers()) {
         
         for (chat = irlist_get_head(&gdata.dccchats);
              chat;
              chat = irlist_delete(&gdata.dccchats,chat))
           {
             writedccchat(chat, 0, "iroffer exited, Closing DCC Chat\n");
             shutdowndccchat(chat,1);
           }
         
         mylog("iroffer exited\n\n");
         
         exit_iroffer(0);
         }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      if (gnetwork->serverstatus == SERVERSTATUS_NEED_TO_CONNECT)
        {
          int timeout;
          timeout = CTIMEOUT + (gnetwork->serverconnectbackoff * CBKTIMEOUT);
          
          if (gnetwork->lastservercontact + timeout < gdata.curtime)
            {
              if (gdata.debug > 0)
                {
                  ioutput(OUT_S, COLOR_YELLOW,
                          "Reconnecting to server (%u seconds) on %s",
                          timeout, gnetwork->name);
                }
              switchserver(-1);
            }
        }
      } /* networks */
      gnetwork = NULL;
      
      if (gdata.needsrehash) {
         gdata.needsrehash = 0;
         urehash = mycalloc(sizeof(userinput));
         a_fillwith_msg2(urehash, NULL, "REHASH");
         urehash->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
         urehash->net = 0;
         urehash->level = ADMIN_LEVEL_FULL;
         u_parseit(urehash);
         mydelete(urehash);
         }
      
      updatecontext();
      
      chat = irlist_get_head(&gdata.dccchats);
      while (chat)
        {
          if (chat->status == DCCCHAT_UNUSED)
            {
              chat = irlist_delete(&gdata.dccchats,chat);
            }
          else
            {
              flushdccchat(chat);
              chat = irlist_get_next(chat);
            }
        }
      
      if (gdata.autoadd_time > 0)
        {
          if (changesec && (gdata.curtime > lastautoadd + gdata.autoadd_time))
            {
              lastautoadd = gdata.curtime;
              autoadd_all();
            }
        }
      
      /* END */
      updatecontext();
      if (gdata.needsclear) drawbot();
      if (gdata.attop) gotobot();
      
      changehour=changemin=0;
      
   }

static void parseline(char *line) {
   char *part2, *part3, *part4, *part5;
   char *part3a;
   char *t;
   unsigned int i;
   char *tptr;
   channel_t *ch;
   
   updatecontext();
   
#ifdef USE_RUBY
   if (do_myruby_server(line))
     return;
#endif /* USE_RUBY */
   /* we only support lines upto maxtextlength, truncate line */
   line[maxtextlength-1] = '\0';
   
   if (gdata.debug > 0)
      ioutput(OUT_S, COLOR_CYAN, ">IRC>: %u, %s", gnetwork->net + 1, line);
   
   part2 = getpart(line,2);
   if (part2 == NULL)
     {
       return;
     }
   part3 = getpart(line,3);
   part4 = getpart(line,4);
   part5 = getpart(line,5);
   
   
   if (part3 && part3[0] == ':')
     {
       part3a = part3+1;
     }
   else
     {
       part3a = part3;
     }
   
 /* NOTICE nick */
   if (part3 && gnetwork->caps_nick && !strcmp(caps(part2), "NOTICE") && !strcmp(caps(part3), gnetwork->caps_nick))
     {
       /* nickserv */
       identify_check(line);
#ifdef USE_RUBY
       if (do_myruby_notice(line) == 0)
#endif /* USE_RUBY */
       privmsgparse(0, 0, line);
     }
 
 /* :server 001  xxxx :welcome.... */
   if ( !strcmp(part2,"001") )
     {
       ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "Server welcome: %s", line);
       update_server_welcome(line);
       
       /* update server name */
       mydelete(gnetwork->curserveractualname);
       gnetwork->curserveractualname = getpart(line+1, 1);
       
       /* update nick */
       mydelete(gnetwork->user_nick);
       mydelete(gnetwork->caps_nick);
       gnetwork->user_nick = mystrdup(part3);
       gnetwork->caps_nick = mystrdup(part3);
       caps(gnetwork->caps_nick);
       gnetwork->nick_number = 0;
       gnetwork->next_restrict = gdata.curtime + gdata.restrictsend_delay;
       gdata.needsclear = 1;
       
       tptr = get_user_modes();
       if (tptr && strlen(tptr))
         {
           writeserver(WRITESERVER_NOW, "MODE %s %s",
                       gnetwork->user_nick, tptr);
         }
       
       /* server connected raw command */
       tptr = irlist_get_head(&(gnetwork->server_connected_raw));
       while(tptr)
         {
           writeserver(WRITESERVER_NORMAL, "%s", tptr);
           tptr = irlist_get_next(tptr);
         }
       
       /* nickserv */
       identify_needed(0);
     }

   /* :server 005 xxxx aaa bbb=x ccc=y :are supported... */
   if ( !strcmp(part2,"005") )
     {
       unsigned int ii = 4;
       char *item;
       
       while((item = getpart(line, ii++)))
         {
           if (item[0] == ':')
             {
               mydelete(item);
               break;
             }
           
           if (!strncmp("PREFIX=(", item, 8))
             {
               char *ptr = item+8;
               unsigned int pi;
               memset(&(gnetwork->prefixes), 0, sizeof(gnetwork->prefixes));
               for (pi = 0; (ptr[pi] && (ptr[pi] != ')') && (pi < MAX_PREFIX)); pi++)
                 {
                   gnetwork->prefixes[pi].p_mode = ptr[pi];
                 }
               if (ptr[pi] == ')')
                 {
                   ptr += pi + 1;
                   for (pi = 0; (ptr[pi] && (pi < MAX_PREFIX)); pi++)
                     {
                       gnetwork->prefixes[pi].p_symbol = ptr[pi];
                     }
                 }
               for (pi = 0; pi < MAX_PREFIX; pi++)
                 {
                   if ((gnetwork->prefixes[pi].p_mode && !gnetwork->prefixes[pi].p_symbol) ||
                       (!gnetwork->prefixes[pi].p_mode && gnetwork->prefixes[pi].p_symbol))
                     {
                       outerror(OUTERROR_TYPE_WARN,
                                "Server prefix list on %s doesn't make sense, using defaults: %s",
                                gnetwork->name, item);
                       initprefixes();
                     }
                 }
             }
           
           if (!strncmp("CHANMODES=", item, 10))
             {
               char *ptr = item+10;
               unsigned int ci;
               unsigned int cm;
               memset(&(gnetwork->chanmodes), 0, sizeof(gnetwork->chanmodes));
               for (ci = cm = 0; (ptr[ci] && (cm < MAX_CHANMODES)); ci++)
                 {
                   if (ptr[ci+1] == ',')
                     {
                       /* we only care about ones with arguments */
                       gnetwork->chanmodes[cm++] = ptr[ci++];
                     }
                 }
             }
           
           mydelete(item);
         }
     }
  
 /* :server 401 botnick usernick :No such nick/channel */
   if ( !strcmp(part2, "401") && part3 && !strcmp(part3, "*") && part4 )
     {
       lost_nick(part4);
     }
  
 /* :server 433 old new :Nickname is already in use. */
   if ( !strcmp(part2,"433") && part3 && !strcmp(part3,"*") && part4 )
     {
       ioutput(OUT_S, COLOR_NO_COLOR,
               "Nickname %s already in use on %s, trying %s%u",
               part4,
               gnetwork->name,
               get_config_nick(),
               gnetwork->nick_number);
       
       /* generate new nick and retry */
       writeserver(WRITESERVER_NORMAL, "NICK %s%u",
                   get_config_nick(),
                   gnetwork->nick_number++);
     }

 /* :server 470 botnick #channel :(you are banned) transfering you to #newchannel */
   if ( !strcmp(part2, "470") && part3 && part4 && part5 )
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,
                "channel on %s: %s", gnetwork->name, strstr(line, "470"));
       for (ch = irlist_get_head(&(gnetwork->channels));
            ch;
            ch = irlist_get_next(ch))
         {
           if (strcmp(caps(part3), gnetwork->caps_nick))
             continue;
           if (strcmp(caps(part4), ch->name))
             continue;
           ch->flags |= CHAN_KICKED;
         }
     }

   /* names list for a channel */
   /* :server 353 our_nick = #channel :nick @nick +nick nick */
   if ( !strcmp(part2,"353") && part3 && part4 && part5 )
     {
       caps(part5);
       
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if (!strcmp(part5,ch->name))
             {
               break;
             }
           ch = irlist_get_next(ch);
         }
       
       if (!ch)
         {
           ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                   "Got name data for %s which is not a known channel on %s!",
                   part5, gnetwork->name);
           writeserver(WRITESERVER_NORMAL, "PART %s", part5);
         }
       else
	 {
	   for (i=0; (t = getpart(line,6+i)); i++)
	     {
               addtomemberlist(ch, i == 0 ? t+1 : t);
	       mydelete(t);
	     }
	 }
     }
  
 /* ERROR :Closing Link */
   if (strncmp(line, "ERROR :Closing Link", strlen("ERROR :Closing Link")) == 0) {
      if (gdata.exiting)
         gnetwork->recentsent = 0;
      else {
         ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                 "Server Closed Connection on %s: %s",
                 gnetwork->name, line);
         close_server();
         }
      }
   
 /* server ping */
   if (PING_SRVR && (strncmp(line, "PING :", 6) == 0)) {
      if (gdata.debug > 0)
         ioutput(OUT_S, COLOR_NO_COLOR,
                 "Server Ping on %s: %s",
                 gnetwork->name, line);
      writeserver(WRITESERVER_NOW, "PO%s", line+2);
      }

   if (gnetwork->lastping != 0) {
     if (strcmp(part2, "PONG") == 0) {
       lag_message();
     }
   }

 /* JOIN */
   if (!strcmp(part2, "JOIN") && part3a && gnetwork->caps_nick) {
      char* nick;
      int j;
      nick = mycalloc(strlen(line)+1);
      j=1;
      gnetwork->nocon = 0;
      while(line[j] != '!' && j<sstrlen(line)) {
         nick[j-1] = line[j];
         j++;
         }
      nick[j-1]='\0';
      if (!strcmp(caps(nick), gnetwork->caps_nick))
        {
          /* we joined */
          /* clear now, we have succesfully logged in */
          gnetwork->serverconnectbackoff = 0;
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "Joined %s on %s", caps(part3a), gnetwork->name);
          
          ch = irlist_get_head(&(gnetwork->channels));
          while(ch)
            {
              if (!strcmp(part3a,ch->name))
                {
                  ch->flags |= CHAN_ONCHAN;
                  ch->lastjoin = gdata.curtime;
                  ch->nextann = gdata.curtime + gdata.waitafterjoin;
                  if (ch->joinmsg)
                    {
                      writeserver(WRITESERVER_NOW, "PRIVMSG %s :%s", ch->name, ch->joinmsg);
                    }
                  gnetwork->botstatus = BOTSTATUS_JOINED;
                  start_sends();
                  break;
                }
              ch = irlist_get_next(ch);
            }
          

          if (!ch)
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "%s is not a known channel on %s!",
                  part3a, gnetwork->name);
            }
        }
      else
	{
	  /* someone else joined */
	  caps(part3a);
          ch = irlist_get_head(&(gnetwork->channels));
          while(ch)
            {
              if (!strcmp(part3a,ch->name))
                {
                  break;
                }
              ch = irlist_get_next(ch);
            }
	  
	  if (!ch)
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "%s is not a known channel on %s!",
                      part3a, gnetwork->name);
            }
	  else
            {
              addtomemberlist(ch,nick);
            }
	}
	
      mydelete(nick);
      }

 /* PART */
   if (!strcmp(part2, "PART") && part3a && gnetwork->caps_nick)
     {
       char* nick;
       int j;
       nick = mycalloc(strlen(line)+1);
       j=1;
       while(line[j] != '!' && j<sstrlen(line))
	 {
	   nick[j-1] = line[j];
	   j++;
	 }
       nick[j-1]='\0';
       
       if (!strcmp(caps(nick), gnetwork->caps_nick))
	 {
	   /* we left? */
	   ;
	 }
       else
	 {
	   /* someone else left */
	   caps(part3a);
           ch = irlist_get_head(&(gnetwork->channels));
           while(ch)
             {
               if (!strcmp(part3a,ch->name))
                 {
                   break;
                 }
               ch = irlist_get_next(ch);
             }
	   
	   if (!ch)
             {
               ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                       "%s is not a known channel on %s!",
                       part3a, gnetwork->name);
             }
	   else
             {
               removefrommemberlist(ch,nick);
             }
           reverify_restrictsend();
	 }
       
       mydelete(nick);
     }
   
 /* QUIT */
   if (!strcmp(part2, "QUIT") && gnetwork->caps_nick)
     {
       char* nick;
       int j;
       nick = mycalloc(strlen(line)+1);
       j=1;
       while(line[j] != '!' && j<sstrlen(line))
	 {
	   nick[j-1] = line[j];
	   j++;
	 }
       nick[j-1]='\0';
       
       if (!strcmp(caps(nick), gnetwork->caps_nick))
	 {
	   /* we quit? */
           outerror(OUTERROR_TYPE_WARN_LOUD,
                    "Forced quit on %s: %s", gnetwork->name, line);
           close_server();
           clean_send_buffers();
           /* do not reconnect */
           gnetwork->serverstatus = SERVERSTATUS_EXIT;
	 }
       else
	 {
	   /* someone else quit */
           ch = irlist_get_head(&(gnetwork->channels));
           while(ch)
             {
               removefrommemberlist(ch,nick);
               ch = irlist_get_next(ch);
             }
           reverify_restrictsend();
	 }
       
       mydelete(nick);
     }
   
 /* NICK */
   if (!strcmp(part2, "NICK") && part3a)
     {
       char *oldnick, *newnick;
       int j;
       oldnick = mycalloc(strlen(line)+1);
       j=1;
       while(line[j] != '!' && j<sstrlen(line))
	 {
	   oldnick[j-1] = line[j];
	   j++;
	 }
       oldnick[j-1]='\0';

       newnick = part3a;
       
       if (gnetwork->caps_nick && !strcmp(caps(oldnick), gnetwork->caps_nick))
	 {
           /* nickserv */
           identify_needed(0);
           
           /* we changed, update nick */
           mydelete(gnetwork->user_nick);
           mydelete(gnetwork->caps_nick);
           gnetwork->user_nick = mystrdup(part3a);
           gnetwork->caps_nick = mystrdup(part3a);
           caps(gnetwork->caps_nick);
           gnetwork->nick_number = 0;
           gdata.needsclear = 1;
         }
       
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           changeinmemberlist_nick(ch, oldnick, newnick);
           ch = irlist_get_next(ch);
         }
       
       user_changed_nick(oldnick, newnick);
       
       mydelete(oldnick);
     }
   
 /* KICK */
   if (!strcmp(part2, "KICK") && part3a && part4 && gnetwork->caps_nick)
     {
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if (!strcmp(caps(part3a),ch->name))
             {
               if(!strcmp(caps(part4), gnetwork->caps_nick))
                 {
                   /* we were kicked */
                   if ( gdata.noautorejoin )
                     {
                       outerror(OUTERROR_TYPE_WARN_LOUD,
                               "Kicked on %s: %s", gnetwork->name, line);
                       ch->flags |= CHAN_KICKED;
                       clearmemberlist(ch);
                       reverify_restrictsend();
                     }
                   else
                     {
                       outerror(OUTERROR_TYPE_WARN_LOUD,
                               "Kicked on %s, Rejoining: %s", gnetwork->name, line);
                       ch->flags &= ~CHAN_ONCHAN;
                       joinchannel(ch);
                     }
                 }
               else
                 {
                   /* someone else was kicked */
                   removefrommemberlist(ch,part4);
                 }
             }
           ch = irlist_get_next(ch);
         }
       reverify_restrictsend();
     }
   
   /* MODE #channel +x ... */
   if (!strcmp(part2,"MODE") && part3 && part4)
     {
       /* find channel */
       for (ch = irlist_get_head(&(gnetwork->channels)); ch; ch = irlist_get_next(ch))
         {
           if (!strcasecmp(ch->name, part3))
             {
               break;
             }
         }
       if (ch)
         {
           unsigned int plus = 0;
           unsigned int part = 5;
           char *ptr;
           
           for (ptr = part4; *ptr; ptr++)
             {
               if (*ptr == '+')
                 {
                   plus = 1;
                 }
               else if (*ptr == '-')
                 {
                   plus = 0;
                 }
               else
                 {
                   unsigned int ii;
                   for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_mode); ii++)
                     {
                       if (*ptr == gnetwork->prefixes[ii].p_mode)
                         {
                           /* found a nick mode */
                           char *nick = getpart(line, part++);
                           if (nick)
                             {
                               if (nick[strlen(nick)-1] == '\1')
                                 {
                                   nick[strlen(nick)-1] = '\0';
                                 }
                               if (plus == 0)
                                 {
                                   if (strcasecmp(nick, get_config_nick()) == 0)
                                     {
                                       identify_needed(0);
                                     }
                                 }
                               changeinmemberlist_mode(ch, nick,
                                                       gnetwork->prefixes[ii].p_symbol,
                                                       plus);
                               mydelete(nick);
                             }
                           break;
                         }
                     }
                   for (ii = 0; (ii < MAX_CHANMODES && gnetwork->chanmodes[ii]); ii++)
                     {
                       if (*ptr == gnetwork->chanmodes[ii])
                         {
                           /* found a channel mode that has an argument */
                           part++;
                           break;
                         }
                     }
                 }
             }
         }
       else
         {
           if (strcasecmp(part3, get_config_nick()) == 0)
             {
               if (part4[0] == '-')
                 identify_needed(0);
             }
         }
     }

 /* PRIVMSG */
   if (!strcmp(part2,"PRIVMSG"))
     {
#ifndef WITHOUT_BLOWFISH
       char *line2;

       line2 = test_fish_message(line, part3, part4, part5);
       if (line2)
         {
#ifdef USE_RUBY
           if (do_myruby_privmsg(line2) == 0)
#endif /* USE_RUBY */
           privmsgparse(1, 1, line2);
           mydelete(line2);
         }
       else
         {
#endif /* WITHOUT_BLOWFISH */
#ifdef USE_RUBY
           if (do_myruby_privmsg(line) == 0)
#endif /* USE_RUBY */
           privmsgparse(1, 0, line);
#ifndef WITHOUT_BLOWFISH
         }
#endif /* WITHOUT_BLOWFISH */
     }
   
   mydelete(part2);
   mydelete(part3);
   mydelete(part4);
   mydelete(part5);
   }

/* End of File */
