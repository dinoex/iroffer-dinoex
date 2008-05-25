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
#define GEX 
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
#include "dinoex_misc.h"

/* local functions */
static void mainloop(void);
static void parseline(char *line);
static void privmsgparse(int type, char* line);
static int  parsecmdline(int argc, char *argv[]);

/* main */
int main(int argc, char *argv[]) {
   int i;
   int first_config_arg;
   
   initvars();
   
   switch( parsecmdline( argc, argv ) ) {
   case PCL_OK: break;
   case PCL_BAD_OPTION:
     printf("\n"
            "iroffer-dinoex " VERSIONLONG ", see http://iroffer.dinoex.net/\n"
            "\n"
            "Usage: %s [-vc] [-bdkn"
#if !defined(_OS_CYGWIN)
            "s"
#endif
            "]"
#if !defined(NO_SETUID)
            " [-u user]"
#endif
#if !defined(NO_CHROOT)
            " [-t dir]"
#endif
            " configfile [ configfile ... ]\n"
            "        -v        Print version and exit.\n"
            "        -c        Generate encrypted password and exit.\n"
            "        -d        Increase debug level\n"
            "        -b        Go to background mode\n"
            "        -k        Attempt to adjust ulimit to allow core files\n"
            "        -n        No colors in foreground mode\n"
#if !defined(_OS_CYGWIN)
            "        -s        No screen manipulation in foreground mode\n"
#endif
#if !defined(NO_SETUID)
            "        -u user   Run as user (you have to start as root).\n"
#endif
#if !defined(NO_CHROOT)
            "        -t dir    Chroot to dir (you have to start as root).\n"
#endif
            "\n",
            argv[0]);
      exit(64);
   case PCL_SHOW_VERSION:
      printf("iroffer-dinoex " VERSIONLONG ", see http://iroffer.dinoex.net/\n");
      exit(0);
   case PCL_GEN_PASSWORD:
      createpassword();
      exit(0);
   default: break;
   }

#if defined(_OS_CYGWIN)
   gdata.noscreen = 1;
#endif
   
   first_config_arg = optind;

   for (i=0; i<MAXCONFIG && i<(argc-first_config_arg); i++)
      gdata.configfile[i] = argv[i+first_config_arg];
   
   startupiroffer();
   
   while ( 1 )
     {
       mainloop();
     }
   
   return(0);
   }

static int parsecmdline(int argc, char *argv[])
{
  int retval;
#if defined(_OS_CYGWIN)
  const char *options = "bndkct:u:v";
#else
  const char *options = "bndkcst:u:v";
#endif

#if 0
  opterr = 0; /* No printed error from getopt() */
#endif
  
  while( (retval = getopt( argc, argv, options )) != -1 )
    {
      switch(retval)
        {
        case 'c':
          return PCL_GEN_PASSWORD;
        case 'v':
          return PCL_SHOW_VERSION;
        case 'b':
          gdata.background = 1;
          break;
        case 'd':
          gdata.debug++;
          break;
        case 'n':
          gdata.nocolor = 1;
          break;
        case 'k':
          gdata.adjustcore = 1;
          break;
        case 's':
          gdata.noscreen = 1;
          break;
        case 't':
#if !defined(NO_CHROOT)
          gdata.chrootdir = optarg;
          break;
#else
          return PCL_BAD_OPTION;
#endif
       case 'u':
#if !defined(NO_SETUID)
         gdata.runasuser = optarg;
         break;
#else
         return PCL_BAD_OPTION;
#endif
        case ':':
        case '?':
	default:
          return PCL_BAD_OPTION;
        }
    }
  
  if (optind >= argc)
    {
      fprintf(stderr,"%s: no configuration file specified\n",
              argv[0]);
      return PCL_BAD_OPTION;
    }
  
  return PCL_OK;
}


static void select_dump(const char *desc, int highests)
{
  int ii;
  
  ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_CYAN,
          "select %s: [read",desc);
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.readset))
        {
          ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_CYAN,
                  " %d",ii);
        }
    }
  ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_CYAN,
          "] [write");
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.writeset))
        {
          ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_CYAN,
                  " %d",ii);
        }
    }
  ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_CYAN, "]");
  
}

static void mainloop (void) {
   /* data is persistant across calls */
   static struct timeval timestruct;
   static int changequartersec,changesec,changemin,changehour;
   static time_t lasttime, lastmin, lasthour, last4sec, last5sec, last20sec;
   static time_t lastautoadd;
   static long last3min, last2min, lastignoredec, lastperiodicmsg;
   static int first_loop = 1;
   static unsigned long long last250ms;

   userinput *pubplist;
   userinput *urehash;
   ir_uint64 xdccsent;
   int i, j;
   int length;
   int highests;
   int ss;
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
       last250ms = ((unsigned long long)lasttime) * 1000;
       lastmin=(lasttime/60)-1;
       lasthour=(lasttime/60/60)-1;
       last4sec = last5sec = last20sec = last2min = last3min = lasttime;
       lastignoredec = lasttime;
       for (ss=0; ss<gdata.networks_online; ss++)
         {
           gdata.networks[ss].lastnotify = lasttime;
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
      
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_get_next(chat))
        {
          if (chat->status == DCCCHAT_CONNECTING)
            {
              FD_SET(chat->con.clientsocket, &gdata.writeset);
              highests = max2(highests, chat->con.clientsocket);
            }
          else if (chat->status == DCCCHAT_LISTENING)
            {
              FD_SET(chat->con.listensocket, &gdata.readset);
              highests = max2(highests, chat->con.listensocket);
            }
          else if (chat->status != DCCCHAT_UNUSED)
            {
              FD_SET(chat->con.clientsocket, &gdata.readset);
              highests = max2(highests, chat->con.clientsocket);
            }
        }
      
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
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
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
                          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                                  "Unable to resolve server %s (%s)",
                                  gdata.networks[ss].curserver.hostname, errstr);
                        }
                      else
#endif
                        {
                          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
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
      
      gdata.curtimems = (((unsigned long long)timestruct.tv_sec) * 1000) + (((unsigned long long)timestruct.tv_usec) / 1000);
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
            ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                    "Closing Server Connection on %s: %s",
                    gnetwork->name, (length<0) ? strerror(errno) : "Closed");
            if (gdata.exiting)
              {
                gnetwork->recentsent = 0;
              }
            close_server();
            mydelete(gnetwork->curserveractualname);
            }
         else
           {
             j=strlen(gnetwork->server_input_line);
             for (i=0; i<length; i++)
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
      
      if (gnetwork->serverstatus == SERVERSTATUS_TRYING && FD_ISSET(gnetwork->ircserver, &gdata.writeset))
        {
          int callval_i;
          int connect_error;
          unsigned int connect_error_len = sizeof(connect_error);
          
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
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Connection Failed: %s on %s", strerror(connect_error), gnetwork->name);
            }
          
          if ((callval_i < 0) || connect_error)
            {
              close_server();
            }
          else
            {
	    SIGNEDSOCK int addrlen; 
          
	    ioutput(CALLTYPE_NORMAL ,OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Server Connection to %s Established, Logging In",  gnetwork->name);
            gnetwork->serverstatus = SERVERSTATUS_CONNECTED;
            gnetwork->connecttime = gdata.curtime;
            FD_CLR(gnetwork->ircserver, &gdata.writeset);
            if (set_socket_nonblocking(gnetwork->ircserver, 0) < 0 )
	      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Blocking");
	    
            addrlen = sizeof(gnetwork->myip);
            bzero((char *) &(gnetwork->myip), sizeof(gnetwork->myip));
            if (getsockname(gnetwork->ircserver, &(gnetwork->myip.sa), &addrlen) >= 0)
              {
                if (gdata.debug > 0)
                  {
                    char *msg;
                    msg = mycalloc(maxtextlength);
                    my_getnameinfo(msg, maxtextlength -1, &(gnetwork->myip.sa), 0);
                    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "using %s", msg);
                    mydelete(msg);
                  }
                if (!gnetwork->usenatip)
                  {
                    gnetwork->ourip = ntohl(gnetwork->myip.sin.sin_addr.s_addr);
                    if (gdata.debug > 0)
                      {
                        ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"ourip = %lu.%lu.%lu.%lu",
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
	    
	    initirc();
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
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
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
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Resolve Timed Out (%d seconds) on %s", timeout, gnetwork->name);
              gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
        }
      
      if (changesec && gnetwork->serverstatus == SERVERSTATUS_TRYING)
        {
          int timeout;
          timeout = CTIMEOUT + (gnetwork->serverconnectbackoff * CBKTIMEOUT);
          
          if (gnetwork->lastservercontact + timeout < gdata.curtime)
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Connection Timed Out (%d seconds) on %s", timeout, gnetwork->name);
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
              unsigned int connect_error_len = sizeof(connect_error);
              
              callval_i = getsockopt(ul->con.clientsocket,
                                     SOL_SOCKET, SO_ERROR,
                                     &connect_error, &connect_error_len);
              
              if (callval_i < 0)
                {
                  int errno2 = errno;
                  outerror(OUTERROR_TYPE_WARN,
                           "Couldn't determine upload connection status on %s: %s",
                           gnetwork->name, strerror(errno));
                  l_closeconn(ul,"Upload Connection Failed status:",errno2);
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
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
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
          gnetwork = &(gdata.networks[chat->net]);
          if ((chat->status == DCCCHAT_CONNECTING) &&
              FD_ISSET(chat->con.clientsocket, &gdata.writeset))
            {
              int callval_i;
              int connect_error;
              unsigned int connect_error_len = sizeof(connect_error);
              
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
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
          if ((chat->status == DCCCHAT_LISTENING) &&
              FD_ISSET(chat->con.listensocket, &gdata.readset))
            {
              setupdccchataccept(chat);
            }
          if ((chat->status != DCCCHAT_UNUSED) &&
              FD_ISSET(chat->con.clientsocket, &gdata.readset))
            {
              char tempbuffa[INPUT_BUFFER_LENGTH];
              switch (chat->status)
                {
                case DCCCHAT_AUTHENTICATING:
                case DCCCHAT_CONNECTED:
                  memset(tempbuffa, 0, INPUT_BUFFER_LENGTH);
                  length = read(chat->con.clientsocket, &tempbuffa, INPUT_BUFFER_LENGTH);
                  
                  if (length < 1)
                    {
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
                      for (i=0; i<length; i++)
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
                  
                case DCCCHAT_CONNECTING:
                case DCCCHAT_UNUSED:
                default:
                  outerror(OUTERROR_TYPE_CRASH,
                           "Unexpected dccchat state %d", chat->status);
                  break;
                }
            }
        }
      gnetwork = NULL;
      
      updatecontext();
      
      i = j = gdata.cursendptr++ % max2(irlist_size(&gdata.trans),1);
      
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
              check_new_connection(tr);
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
                  (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
                {
                  sendaqueue(0, 0, trnick);
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
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
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

            if (localt->tm_hour >= gdata.overallmaxspeeddaytimestart
                && localt->tm_hour < gdata.overallmaxspeeddaytimeend
                && ( gdata.overallmaxspeeddaydays & (1 << localt->tm_wday)) )
               gdata.maxb = gdata.overallmaxspeeddayspeed;
            }
         }
      
      /*----- see if we've hit a transferlimit or need to reset counters */
      if (changesec)
        {
          int ii;
          int transferlimits_over = 0;
          for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
            {
              /* reset counters? */
              if ((!gdata.transferlimits[ii].ends) ||
                  (gdata.transferlimits[ii].ends < gdata.curtime))
                {
                  struct tm *localt;
                  if (gdata.transferlimits[ii].limit && gdata.transferlimits[ii].ends)
                    {
                      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
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
                      outerror(OUTERROR_TYPE_CRASH, "unknown type %d", ii);
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
                      
                      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
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
                  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                          "No longer over any transfer limits. Transfers are now allowed.");
                }
              gdata.transferlimits_over = transferlimits_over;
            }
        }
      
      /*----- gdata.autoignore_threshold seconds ----- */
      if (changesec && (gdata.curtime - lastignoredec > gdata.autoignore_threshold))
        {
          igninfo *ignore;
          
          lastignoredec += gdata.autoignore_threshold;
          
          ignore = irlist_get_head(&gdata.ignorelist);
          
          while(ignore)
            {
              ignore->bucket--;
              if ((ignore->flags & IGN_IGNORING) && (ignore->bucket < 0))
                {
                  ignore->flags &= ~IGN_IGNORING;
                  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                          "Ignore removed for %s",ignore->hostmask);
                  write_statefile();
                }
              if (ignore->bucket < 0)
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
      if (changesec && (gdata.curtime - lastperiodicmsg > gdata.periodicmsg_time*60)) {
         lastperiodicmsg = gdata.curtime;
         
         for (ss=0; ss<gdata.networks_online; ss++) {
            if (gdata.periodicmsg_nick && gdata.periodicmsg_msg
            && (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED) ) {
               gnetwork = &(gdata.networks[ss]);
               privmsg(gdata.periodicmsg_nick,"%s",gdata.periodicmsg_msg);
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
             (gdata.curtime - gnetwork->lastservercontact > SRVRTOUT)) {
            if (gnetwork->servertime < 3)
              {
                const char *servname = gnetwork->curserveractualname ? gnetwork->curserveractualname : gnetwork->curserver.hostname;
                int        len       = 6 + strlen(servname);
                char       *tempstr3 = mycalloc(len + 1);
                snprintf(tempstr3, len + 1, "PING %s\n", servname);
                writeserver_ssl(tempstr3, len);
                if (gdata.debug > 0)
                  {
                    tempstr3[len-1] = '\0';
                    len--;
                    ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<NORES<: %s",tempstr3);
                  }
                mydelete(tempstr3);
                gnetwork->servertime++;
              }
            else if (gnetwork->servertime == 3) {
               ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                       "Closing Server Connection on %s: No Response for %d minutes.",
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
                        ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (pgroup)", ch->name);
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
                    ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Can't send Summary Plist to %s (restrictprivlist is set and no creditline or headline, summary makes no sense!)",tchans);
                  }
                else
                  {
                    ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Plist sent to %s (summary)",tchans);
                    pubplist = mycalloc(sizeof(userinput));
                    u_fillwith_msg(pubplist,tchans,"A A A A A xdl");
                    pubplist->method = method_xdl_channel_sum;
                    pubplist->net = gnetwork->net;
                    pubplist->level = ADMIN_LEVEL_PUBLIC;
                    u_parseit(pubplist);
                    mydelete(pubplist);
                  }
                mydelete(tchans);
              }
            if (tchanf) {
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Plist sent to %s (full)",tchanf);
               pubplist = mycalloc(sizeof(userinput));
               pubplist->method = method_xdl_channel;
               pubplist->net = gnetwork->net;
               pubplist->level = ADMIN_LEVEL_PUBLIC;
               a_fillwith_plist(pubplist, tchanf, NULL);
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanf);
               }
            if (tchanm) {
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Plist sent to %s (minimal)",tchanm);
               pubplist = mycalloc(sizeof(userinput));
               u_fillwith_msg(pubplist,tchanm,"A A A A A xdl");
               pubplist->method = method_xdl_channel_min;
               pubplist->net = gnetwork->net;
               pubplist->level = ADMIN_LEVEL_PUBLIC;
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
             (irlist_size(&gdata.trans) < MAXTRANS))
           {
             sendaqueue(1, 0, NULL);
           }
         write_files();
         }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      /*----- queue notify ----- */
      if (changesec && gdata.notifytime && (!gdata.quietmode) &&
          (gdata.curtime - gnetwork->lastnotify > (gdata.notifytime*60)))
        {
         gnetwork->lastnotify = gdata.curtime;
         
         if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
           {
             if ((irlist_size(&(gnetwork->serverq_fast)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_normal)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_slow)) >= 50))
               {
                 ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D|OUT_L, COLOR_NO_COLOR,
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
           (gdata.curtime - last2min > 119))
        {
          last2min = gdata.curtime;
          logstat();
          
          for (chat = irlist_get_head(&gdata.dccchats);
               chat;
               chat = irlist_get_next(chat))
            {
              if (chat->status == DCCCHAT_CONNECTED)
                {
                  writestatus(chat);
                }
            }
          
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
           if (!gnetwork->user_nick || strcmp(get_config_nick(),gnetwork->user_nick))
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
            
            tostdout("\x1b[s");
            
            getstatusline(tempstr,maxtextlength);
            tempstr[min2(maxtextlength-2,gdata.termcols-4)] = '\0';
            snprintf(tempstr2,maxtextlengthshort,"\x1b[%d;1H[ %%-%ds ]",gdata.termlines-1,gdata.termcols-4);
            tostdout(tempstr2,tempstr);
            
            tostdout("\x1b[%d;%dH]\x1b[u",gdata.termlines,gdata.termcols);
            }
         
         admin_jobs();
         }
      
      updatecontext();
      
      if (changemin)
        {
          reverify_restrictsend();
          update_hour_dinoex(lasthour, lastmin);
          check_idle_queue();
          clean_uploadhost();
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
                  ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"[MD5]: read %ld", (long)howmuch);
                }
              
              if ((howmuch < 0) && (errno != EAGAIN))
                {
                  outerror(OUTERROR_TYPE_WARN,"[MD5]: Can't read data from file '%s': %s",
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
                  
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "[MD5]: is " MD5_PRINT_FMT, MD5_PRINT_DATA(gdata.md5build.xpack->md5sum));
                  if (!gdata.nocrc32)
                    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                            "[CRC32]: is %.8lX", gdata.md5build.xpack->crc32);
                  
                  FD_CLR(gdata.md5build.file_fd, &gdata.readset);
                  close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  if (!gdata.nocrc32 && gdata.auto_crc_check)
                    {
                      const char *crcmsg = validate_crc32(gdata.md5build.xpack, 2);
                      if (crcmsg != NULL)
                        {
                           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                                   "File '%s' %s.", gdata.md5build.xpack->file, crcmsg);
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
          int packnum = 1;
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
         
         mylog(CALLTYPE_NORMAL,"iroffer exited\n\n");
         
         exit_iroffer();
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
                  ioutput(CALLTYPE_NORMAL, OUT_S,COLOR_YELLOW,
                          "Reconnecting to server (%d seconds) on %s",
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
         u_fillwith_msg(urehash,NULL,"A A A A A rehash");
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
          if (changesec && (gdata.curtime - lastautoadd > gdata.autoadd_time))
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
   int i;
   char *tptr;
   channel_t *ch;
   
   updatecontext();
   
   /* we only support lines upto maxtextlength, truncate line */
   line[maxtextlength-1] = '\0';
   
   if (gdata.debug > 0)
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_CYAN, ">IRC>: %d, %s", gnetwork->net + 1, line);
   
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
   if (part3 && gnetwork->caps_nick && !strcmp(caps(part2),"NOTICE") && !strcmp(caps(part3),gnetwork->caps_nick))
     {
       /* nickserv */
       identify_check(line);
       privmsgparse(0, line);
     }
 
 /* :server 001  xxxx :welcome.... */
   if ( !strcmp(part2,"001") )
     {
       ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Server welcome: %s",line);
       mylog(CALLTYPE_NORMAL,"Server welcome: %s",line);
       update_server_welcome(line);
       
       /* update server name */
       mydelete(gnetwork->curserveractualname);
       gnetwork->curserveractualname = getpart(line+1,1);
       
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
       int ii = 4;
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
               int pi;
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
               int ci;
               int cm;
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
   if ( !strcmp(part2,"401") && part3 && !strcmp(part3,"*") && part4 )
     {
       lost_nick(part4);
     }
  
 /* :server 433 old new :Nickname is already in use. */
   if ( !strcmp(part2,"433") && part3 && !strcmp(part3,"*") && part4 )
     {
       ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
               "Nickname %s already in use on %s, trying %s%d",
               part4,
               gnetwork->name,
               get_config_nick(),
               gnetwork->nick_number);
       
       /* generate new nick and retry */
       writeserver(WRITESERVER_NORMAL, "NICK %s%d",
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
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                 "Server Closed Connection on %s: %s",
                 gnetwork->name, line);
         close_server();
         }
      }
   
 /* server ping */
   if (PING_SRVR && (strncmp(line, "PING :", 6) == 0)) {
      if (gdata.debug > 0)
         ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
                 "Server Ping on %s: %s",
                 gnetwork->name, line);
      writeserver(WRITESERVER_NOW, "PO%s", line+2);
      }

 /* JOIN */
   if (!strcmp(part2,"JOIN") && part3a && gnetwork->caps_nick) {
      char* nick;
      int j,found;
      nick = mycalloc(strlen(line)+1);
      j=1;
      gnetwork->nocon = 0;
      found = 0;
      while(line[j] != '!' && j<sstrlen(line)) {
         nick[j-1] = line[j];
         j++;
         }
      nick[j-1]='\0';
      if (!strcmp(caps(nick),gnetwork->caps_nick))
        {
          /* we joined */
          /* clear now, we have succesfully logged in */
          gnetwork->serverconnectbackoff = 0;
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
                  break;
                }
              ch = irlist_get_next(ch);
            }
          

          if (!ch)
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "%s is not a known channel on %s!",
                  part3a, gnetwork->name);
            }

          /* we have joined all channels => restart transfers if needed */
          if (has_joined_channels(0) > 0)
            {
              for (i=0; i<100; i++)
                {
                  if (!gdata.exiting &&
                      irlist_size(&gdata.mainqueue) &&
                      (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
                    {
                      sendaqueue(0, 0, NULL);
                    }
                }
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
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
   if (!strcmp(part2,"PART") && part3a && gnetwork->caps_nick)
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
       
       if (!strcmp(caps(nick),gnetwork->caps_nick))
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
               ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
   if (!strcmp(part2,"QUIT") && gnetwork->caps_nick)
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
       
       if (!strcmp(caps(nick),gnetwork->caps_nick))
	 {
	   /* we quit? */
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
   if (!strcmp(part2,"NICK") && part3a)
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
       
       if (gnetwork->caps_nick && !strcmp(caps(oldnick),gnetwork->caps_nick))
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
   if (!strcmp(part2,"KICK") && part3a && part4 && gnetwork->caps_nick)
     {
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if (!strcmp(caps(part3a),ch->name))
             {
               if(!strcmp(caps(part4),gnetwork->caps_nick))
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
                       joinchannel(ch);
                       ch->flags &= ~CHAN_ONCHAN;
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
           int plus = 0;
           int part = 5;
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
                   int ii;
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
           /* matched lines are skipped */
           if (check_trigger(line2, part4) == 0)
             privmsgparse(1, line2);
           mydelete(line2);
         }
       else
         {
#endif /* WITHOUT_BLOWFISH */
           if (!gdata.fish_only)
             {
               if (check_trigger(line, part4) == 0)
                 privmsgparse(1, line);
             }
#ifndef WITHOUT_BLOWFISH
         }
#endif /* WITHOUT_BLOWFISH */
     }
   
   mydelete(part2);
   mydelete(part3);
   mydelete(part4);
   mydelete(part5);
   }


static const char *type_list[2] = { "NOTICE", "PRIVMSG" };

static void privmsgparse(int type, char* line)
{
   char *nick, *hostname, *hostmask, *wildhost;
   char *msg1, *msg2, *msg3, *msg4, *msg5, *msg6, *dest;
   int i,j,k;
   igninfo *ignore = NULL;
   upload *ul;
   transfer *tr;
   xdcc *xd;
   int line_len;
   
   updatecontext();

   floodchk();
   
   line_len = sstrlen(line);
   
   hostmask = caps(getpart(line,1));
   for (i=1; i<=sstrlen(hostmask); i++)
      hostmask[i-1] = hostmask[i];
   wildhost = NULL;
   
   dest = caps(getpart(line,3));
   msg1 = getpart(line,4);
   msg2 = getpart(line,5);
   msg3 = getpart(line,6);
   msg4 = getpart(line,7);
   msg5 = getpart(line,8);
   msg6 = getpart(line,9);
   
   if (msg1)
     msg1++;  /* point past the ":" */
   
   nick = mycalloc(line_len+1);
   hostname = mycalloc(line_len+1);
   
   
   i=1; j=0;
   while(line[i] != '!' && i<line_len) {
      nick[i-1] = line[i];
      i++;
      }
   nick[i-1]='\0';
   
   
   /* see if it came from a user or server, ignore if from server */
   if (i == line_len)
     goto privmsgparse_cleanup;
   
   while(line[i] != '@' && i<line_len) { i++; }
   i++;
   
   while(line[i] != ' ' && i<line_len) {
      hostname[j] = line[i];
      i++;
      j++;
      }
   hostname[j]='\0';
   
   wildhost = to_hostmask("*", hostname);
   
   if (isthisforme(dest, msg1))
     {
       
       if (verifyshell(&gdata.autoignore_exclude, hostmask))
         {
           /* host matches autoignore_exclude */
           goto noignore;
         }

       /* add/increment ignore list */
       ignore = get_ignore(hostmask);
       
               j=1;
               ignore->bucket++;
               ignore->lastcontact = gdata.curtime;
               
               if (!(ignore->flags & IGN_IGNORING) &&
                    (ignore->bucket >= IGN_ON))
                 {
                   int left;
                   left = gdata.autoignore_threshold*(ignore->bucket+1);
                   
                   ignore->flags |= IGN_IGNORING;
                   
                   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                           "Auto-ignore activated for %s (%s) lasting %i%c%i%c",
                           nick, wildhost,
                           left < 3600 ? left/60 : left/60/60 ,
                           left < 3600 ? 'm' : 'h',
                           left < 3600 ? left%60 : (left/60)%60 ,
                           left < 3600 ? 's' : 'm');
                   
                   notice(nick,
                          "Auto-ignore activated for %s (%s) lasting %i%c%i%c. Further messages will increase duration.",
                          nick, wildhost,
                          left < 3600 ? left/60 : left/60/60 ,
                          left < 3600 ? 'm' : 'h',
                          left < 3600 ? left%60 : (left/60)%60 ,
                          left < 3600 ? 's' : 'm');
                   
                   write_statefile();
                 }
               
               if (ignore->flags & IGN_IGNORING)
                 {
                   goto privmsgparse_cleanup;
                 }
      }
 noignore:
   
   /*----- CLIENTINFO ----- */
   if ( !gdata.ignore && (!strcmp(msg1,"\1CLIENTINFO")
          || !strcmp(msg1,"\1CLIENTINFO\1") )) {
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      if (!msg2) {
         notice(nick,"\1CLIENTINFO DCC PING VERSION XDCC UPTIME "
         ":Use CTCP CLIENTINFO <COMMAND> to get more specific information\1");
         }
      else if (strncmp(caps(msg2),"PING",4) == 0)
         notice(nick,"\1CLIENTINFO PING returns the arguments it receives\1");
      else if (strncmp(caps(msg2),"DCC",3) == 0)
         notice(nick,"\1CLIENTINFO DCC requests a DCC for chatting or file transfer\1");
      else if (strncmp(caps(msg2),"VERSION",7) == 0)
         notice(nick,"\1CLIENTINFO VERSION shows information about this client's version\1");
      else if (strncmp(caps(msg2),"XDCC",4) == 0)
         notice(nick,"\1CLIENTINFO XDCC LIST|SEND list and DCC file(s) to you\1");
      else if (strncmp(caps(msg2),"UPTIME",6) == 0)
         notice(nick,"\1CLIENTINFO UPTIME shows how long this client has been running\1");
      
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "[CTCP] %s in %s: CLIENTINFO",
              nick, gnetwork->name);
      }
   
   /*----- PING ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1PING")
          || !strcmp(msg1, "\1PING\1") ) && (type == 0) ) {
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      if (msg2 && (msg2[strlen(msg2)-1] == '\1'))
        {
          msg2[strlen(msg2)-1] = '\0';
        }
      if (msg3 && (msg3[strlen(msg3)-1] == '\1'))
        {
          msg3[strlen(msg3)-1] = '\0';
        }
      notice(nick, "\1PING%s%s%s%s\1",
             msg2 ? " " : "",
             msg2 ? msg2 : "",
             msg3 ? " " : "",
             msg3 ? msg3 : "");
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "[CTCP] %s on %s: PING",
              nick, gnetwork->name);
      }
   
   /*----- VERSION ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1VERSION")
          || !strcmp(msg1,"\1VERSION\1") )) {
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      notice(nick,"\1VERSION iroffer-dinoex " VERSIONLONG ", http://iroffer.dinoex.net/%s%s\1",
             gdata.hideos ? "" : " - ",
             gdata.hideos ? "" : gdata.osstring);
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "[CTCP] %s on %s: VERSION",
              nick, gnetwork->name);
      }
   
   /*----- UPTIME ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1UPTIME")
          || !strcmp(msg1,"\1UPTIME\1") ))
     {
       char *tempstr2 = mycalloc(maxtextlength);
       gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
       tempstr2 = getuptime(tempstr2, 0, gdata.startuptime, maxtextlength);
       notice(nick,"\1UPTIME %s\1", tempstr2);
       ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
               "[CTCP] %s on %s: UPTIME",
               nick, gnetwork->name);
       mydelete(tempstr2);
     }
   
   /*----- STATUS ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1STATUS")
          || !strcmp(msg1,"\1STATUS\1") ))
     {
       char *tempstr2 = mycalloc(maxtextlength);
       gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
       tempstr2 = getstatuslinenums(tempstr2,maxtextlength);
       notice(nick,"\1%s\1",tempstr2);
       ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
               "[CTCP] %s on %s: STATUS",
               nick, gnetwork->name);
       mydelete(tempstr2);
     }
   
   /*----- DCC SEND/CHAT/RESUME ----- */
   else if ( !gdata.ignore && gnetwork->caps_nick && !strcmp(gnetwork->caps_nick,dest) && !strcmp(caps(msg1),"\1DCC") && msg2) {
      if (!strcmp(caps(msg2),"RESUME") && msg3 && msg4 && msg5)
        {
          gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
          
          clean_quotes(msg3);
          if (msg5[strlen(msg5)-1] == '\1') msg5[strlen(msg5)-1] = '\0';
          
          t_find_resume(nick, msg3, msg4, msg5, msg6);
        }
      else if (!strcmp(caps(msg2), "CHAT"))
        {
          if ( verifyshell(&gdata.adminhost, hostmask) )
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC CHAT attempt authorized from %s on %s",
                      hostmask, gnetwork->name);
              setupdccchat(nick, line);
            }
          else if ( verifyshell(&gdata.hadminhost, hostmask) )
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC CHAT attempt authorized from %s on %s",
                      hostmask, gnetwork->name);
              setupdccchat(nick, line);
            }
          else
           {
             notice(nick,"DCC Chat denied from %s",hostmask);
             ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                     "DCC CHAT attempt denied from %s on %s",
                      hostmask, gnetwork->name);
           }
        }
      
      else if (!strcmp(caps(msg2), "SEND") && msg3 && msg4 && msg5 && msg6)
        {
          char *msg7;
          int down = 0;

          if (msg6[strlen(msg6)-1] == '\1')
            {
              msg6[strlen(msg6)-1] = '\0';
            }
          msg7 = getpart(line, 10);
          if (msg7)
            {
              if (msg7[strlen(msg7)-1] == '\1')
                msg7[strlen(msg7)-1] = '\0';
              down = t_find_transfer(nick, msg3, msg4, msg5, msg7);
            }
          if (!down)
            {
             clean_quotes(msg3);
             removenonprintablefile(msg3);
             upload_start(nick, hostname, hostmask, msg3, msg4, msg5, msg6, msg7);
            }
          mydelete(msg7);
        }
      
      else if (!strcmp(caps(msg2), "ACCEPT") && msg3 && msg4 && msg5)
        {
          off_t len;
          
          if (msg5[strlen(msg5)-1] == '\1')
            {
              msg5[strlen(msg5)-1] = '\0';
            }
          len = atoull(msg5);
          if ( verify_uploadhost( hostmask) )
            {
              notice(nick,"DCC Send Denied, I don't accept transfers from %s", hostmask);
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied from %s on %s",
                      hostmask, gnetwork->name);
            }
          else if (gdata.uploadmaxsize && (len > gdata.uploadmaxsize))
            {
              notice(nick,"DCC Send Denied, I don't accept transfers that big");
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC Send denied from %s on %s (too big)",
                      hostmask, gnetwork->name);
            }
          else if (len > gdata.max_file_size)
            {
              notice(nick,"DCC Send Denied, I can't accept transfers that large");
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC Send denied from %s on %s (too large)",
                      hostmask, gnetwork->name);
            }
          
          ul = irlist_get_head(&gdata.uploads);
          while (ul)
            {
              if ((ul->con.remoteport == atoi(msg4)) && !strcmp(ul->nick, nick))
                {
                  char *tempstr;
                  ul->resumed = 1;
                  tempstr = getsendname(ul->file);
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                          "DCC Send Resumed from %s on %s: %s (%" LLPRINTFMT "u of %" LLPRINTFMT "uKB left)",
                          nick, gnetwork->name, tempstr,
                          ((ul->totalsize - ul->resumesize) / 1024),
                          (ul->totalsize / 1024));
                  mydelete(tempstr);
                  if (ul->con.remoteport > 0)
                    {
                      l_establishcon(ul);
                    }
                  else
                    {
                      /* Passive DCC */
                      l_setup_passive(ul, msg6 ? msg6 : msg4);
                    }
                  break;
                }
              ul = irlist_get_next(ul);
            }
          
          if (!ul)
            {
              notice(nick, "DCC Resume Denied, unable to find transfer");
              outerror(OUTERROR_TYPE_WARN,
                       "Couldn't find upload that %s on %s tried to resume!",
                       nick, gnetwork->name);
            }
        }
   }
   
   /*----- ADMIN ----- */
   else if ( !gdata.ignore && gnetwork->caps_nick && !strcmp(gnetwork->caps_nick,dest) && !strcmp(caps(msg1),"ADMIN") ) {
/*      msg2 = getpart(line,5); */
      if (admin_message(nick, hostmask, msg2, line, line_len) != 0 )
         {
            /* admin commands shouldn't count against ignore */
            if (ignore)
              {
                ignore->bucket--;
              }
         }
      }
   
   /*----- XDCC ----- */
   else if ( !gdata.ignore && gnetwork->caps_nick && (!strcmp(gnetwork->caps_nick,dest) || gdata.respondtochannelxdcc) && (!strcmp(caps(msg1),"XDCC") || !strcmp(msg1,"\1XDCC") || !strcmp(caps(msg1),"CDCC") || !strcmp(msg1,"\1CDCC") )) {
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      
      caps(msg2);
      
      if (msg3 && msg3[strlen(msg3)-1] == '\1')
         msg3[strlen(msg3)-1] = '\0';
      
      if ( msg2 && ( !strcmp(msg2,"LIST") || !strcmp(msg2,"LIST\1"))) {
         /* detect xdcc list group xxx */
         if ((msg3) && (msg4) && (strcmp(caps(msg3), "GROUP") == 0))
            j = parse_xdcc_list(nick, msg4);
         else
            j = parse_xdcc_list(nick, msg3);
	 
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                 "XDCC LIST %s: (%s on %s)",
                 (j==1?"ignored":(j==2?"denied":"queued")),
                 hostmask, gnetwork->name);
         
         }
      else if (gnetwork->caps_nick && !strcmp(gnetwork->caps_nick,dest))
	{
         
         if ( msg2 && msg3 && (!strcmp(msg2,"SEND") || !strcmp(msg2,"GET"))) {
           ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC SEND %s",
                   msg3);
           sendxdccfile(nick, hostname, hostmask, packnumtonum(msg3), NULL, msg4);
         }
         else if ( msg2 && msg3 && (!strcmp(msg2,"INFO"))) {
           ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC INFO %s",
                   msg3);
           sendxdccinfo(nick, hostname, hostmask, packnumtonum(msg3));
         }
         else if ( msg2 && !strcmp(msg2, "QUEUE")) {
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC QUEUE (%s on %s)",
                   hostmask, gnetwork->name);
           notifyqueued_nick(nick);
         }
         else if ( msg2 && !strcmp(msg2, "STOP")) {
           stoplist(nick);
         }
         else if ( msg2 && !strcmp(msg2, "CANCEL")) {
           k = 0;
           /* stop transfers */
           for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
             {
               if ((tr->net == gnetwork->net) && (!strcasecmp(tr->nick,nick)))
                 {
                   if (tr->tr_status != TRANSFER_STATUS_DONE)
                     {
                       k += 1;
                       t_closeconn(tr,"Transfer canceled by user",0);
                     }
                 }
             }
           if (!k) notice(nick, "You don't have a transfer running");
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC CANCEL (%s on %s)",
                   hostmask, gnetwork->name);
         }
	 else if ( msg2 && !strcmp(msg2,"REMOVE")) {
         k=0;
         
         k += queue_xdcc_remove(&gdata.mainqueue, gnetwork->net, nick);
         k += queue_xdcc_remove(&gdata.idlequeue, gnetwork->net, nick);
         if (!k) notice(nick,"You Don't Appear To Be In A Queue");
           
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC REMOVE (%s on %s) ",
                   hostmask, gnetwork->name);
         }
         else if ( msg2 && !strcmp(msg2,"OWNER")) {
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                 "XDCC OWNER (%s on %s) ",
                 hostmask, gnetwork->name);
           notice(nick, "Owner for this bots is: %s",
                  gdata.owner_nick ? gdata.owner_nick : "(unknown)");
         }
         else if ( msg2 && !strcmp(msg2, "HELP")) {
           send_help(nick);
         }
         else if ( msg2 && !strcmp(msg2,"SEARCH") && msg3) {
           char *match;
         
         notice_slow(nick,"Searching for \"%s\"...",msg3);
           match = grep_to_fnmatch(msg3);
         
         i = 1;
         k = 0;
         xd = irlist_get_head(&gdata.xdccs);
         while(xd)
           {
             if (hide_pack(xd) == 0)
               {
                 if (fnmatch_xdcc(match, xd))
                   {
                      notice_slow(nick," - Pack #%i matches, \"%s\"",
                                  i, xd->desc);
                      k++;
                      /* limit matches */
                      if ((gdata.max_find != 0) && (k >= gdata.max_find))
                        {
                           notice_slow(nick, "Search limit exceeded, please check the packlist for more results.");
                           break;
                        }
                   }
               }
             i++;
             xd = irlist_get_next(xd);
           }
         
         mydelete(match);
         if (!k)
           {
             notice_slow(nick,"Sorry, nothing was found, try a XDCC LIST");
           }
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC SEARCH %s (%s on %s)",
                   msg3, hostmask, gnetwork->name);
         
         }
         else if ( msg2 )  {
           ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "XDCC unsupported (%s on %s) ",
                   hostmask, gnetwork->name);
           notice(nick, "Sorry, this command is unsupported" );
         }
      }
   }
   
   /*----- !LIST ----- */
   else if ( !gdata.ignore && gnetwork->caps_nick && gdata.respondtochannellist && msg1 && !strcasecmp(caps(msg1),"!LIST") &&
             ( !msg2 || !strcmp(caps(msg2),gnetwork->caps_nick) ))
     {
      char *tempstr2 = mycalloc(maxtextlength);
      
      /* generate !list styled message */
      
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      
      if (gdata.restrictprivlist)
        strcpy(tempstr2, "");
      else
        snprintf(tempstr2, maxtextlength - 2,
                 "Trigger:\2(\2/MSG %s XDCC LIST\2)\2 ",
                 save_nick(gnetwork->user_nick));
      notice_slow(nick,
             "\2(\2XDCC\2)\2 Packs:\2(\2%d\2)\2 "
             "%s%s"
             "%s"
             "Sends:\2(\2%i/%i\2)\2 "
             "Queues:\2(\2%i/%i\2)\2 "
             "Record:\2(\2%1.1fKB/s\2)\2 "
             "%s%s%s\2=\2iroffer\2=\2",
             irlist_size(&gdata.xdccs),
             (gdata.respondtochannellistmsg ? gdata.respondtochannellistmsg : ""),
             (gdata.respondtochannellistmsg ? " " : ""),
             tempstr2,
             irlist_size(&gdata.trans),gdata.slotsmax,
             irlist_size(&gdata.mainqueue),gdata.queuesize,
             gdata.record,
             gdata.creditline ? "Note:\2(\2" : "",
             gdata.creditline ? gdata.creditline : "",
             gdata.creditline ? "\2)\2 " : "");

       mydelete(tempstr2);

     }
   
   /* iroffer-lamm: @find */
   else if ( !gdata.ignore && gnetwork->caps_nick && gdata.atfind && msg2 && !strcasecmp(caps(msg1),"@FIND") )
     {
      char *msg2e;
      msg2e = getpart_eol(line,5);
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      for (i = k = 0; i < (int)(strlen(msg2e)); i++)
        if (msg2e[i] == ' ') msg2e[i] = '*';
        if ((msg2e[i] == '*') || (msg2e[i] == '#') || (msg2e[i] == '?'))
          k++;
      if ((int)(strlen(msg2e) - k) >= gdata.atfind) {
        char *atfindmatch = grep_to_fnmatch(msg2e);
        k = noticeresults(nick, atfindmatch);
        if (k) {
          ioutput(CALLTYPE_NORMAL, OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
                  "@FIND %s (%s on %s) - %i %s found.",
                  msg2e, hostmask, gnetwork->name, k, k != 1 ? "packs" : "pack");
        }
        mydelete(atfindmatch);
      }
      mydelete(msg2e);
     }
   
   else if ( !gdata.ignore && gnetwork->caps_nick && gdata.new_trigger && !strcasecmp(caps(msg1),"!new") )
     {
      gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
      k = run_new_trigger(nick);
      if (k)
        {
           ioutput(CALLTYPE_NORMAL, OUT_S | OUT_L | OUT_D, COLOR_YELLOW,
                   "!new (%s on %s) - %i packs.",
                   hostmask, gnetwork->name, k);
        }
     }

   else {
      if (dest && gnetwork->caps_nick && !strcmp(dest,gnetwork->caps_nick))
        {
          char *begin;
          int exclude = 0;
          
          begin = line + 5 + strlen(hostmask) + strlen(type_list[type]) + strlen(dest);
          if (verifyshell(&gdata.log_exclude_host, hostmask))
            exclude ++;
          if (verifyshell(&gdata.log_exclude_text, begin))
            exclude ++;
          
          if (((gdata.lognotices && (type == 0)) ||
              (gdata.logmessages && (type != 0)))
              && (exclude == 0))
            {
              msglog_t *ml;
              
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D,COLOR_GREEN,
                      "%s from %s on %s logged, use MSGREAD to display it.",
                      type_list[type], nick, gnetwork->name);
              
              ml = irlist_add(&gdata.msglog, sizeof(msglog_t));
              
              ml->when = gdata.curtime;
              ml->hostmask = mystrdup(hostmask);
              ml->message = mystrdup(begin);
              
              write_statefile();
            }
          else if (gdata.logfile_notices && (type == 0) && (exclude == 0))
            {
              logfile_add(gdata.logfile_notices, line);
            }
          else if (gdata.logfile_messages && (type != 0) && (exclude == 0))
            {
              logfile_add(gdata.logfile_messages, line);
            }
          else
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_GREEN,
                      "%s on %s: %s",
                      type_list[type], gnetwork->name, line);
            }
         }
      }
   
 privmsgparse_cleanup:
   
   if (msg1)
     msg1--;
   
   mydelete(dest);
   mydelete(nick);
   mydelete(hostname);
   mydelete(hostmask);
   mydelete(wildhost);
   mydelete(msg1);
   mydelete(msg2);
   mydelete(msg3);
   mydelete(msg4);
   mydelete(msg5);
   mydelete(msg6);
   
   return;
   }

void get_nick_hostname(char *nick, char *hostname, const char* line)
{
   int i,j;

   i=1; j=0;
   while(line[i] != '!' && i<sstrlen(line) && i<maxtextlengthshort-1) {
      nick[i-1] = line[i];
      i++;
      }
   nick[i-1]='\0';

   while(line[i] != '@' && i<sstrlen(line)) { i++; }
   i++;

   while(line[i] != ' ' && i<sstrlen(line) && j<maxtextlength-1) {
      hostname[j] = line[i];
      i++;
      j++;
      }
   hostname[j]='\0';
}

void autoqueuef(const char* line, int pack, const char *message)
{
   char *nick, *hostname, *hostmask;
   int i;
   
   updatecontext();

   floodchk();
   
   nick = mycalloc(maxtextlengthshort);
   hostname = mycalloc(maxtextlength);
      
   hostmask = caps(getpart(line,1));
   for (i=1; i<=sstrlen(hostmask); i++)
      hostmask[i-1] = hostmask[i];

   get_nick_hostname(nick, hostname, line);

   if ( !gdata.ignore )
     {
       char *tempstr = NULL;
       const char *format = " :** Sending You %s by DCC";
       
       gnetwork->inamnt[gdata.curtime%INAMNT_SIZE]++;
       
       ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"AutoSend ");
       
       if (message)
         {
           tempstr = mycalloc(strlen(message) + strlen(format) - 1);
           snprintf(tempstr, strlen(message) + strlen(format) - 1,
                    format, message);
         }
       
       sendxdccfile(nick, hostname, hostmask, pack, tempstr, NULL);
       
       mydelete(tempstr);
     }
   
   mydelete(hostmask);
   mydelete(nick);
   mydelete(hostname);

   }

void sendxdccfile(const char* nick, const char* hostname, const char* hostmask, int pack, const char* msg, const char* pwd)
{
  int usertrans, userpackok, man;
  int unlimitedhost;
  xdcc *xd;
  transfer *tr;
  
  updatecontext();
  
  usertrans = 0;
  userpackok = 1;
  unlimitedhost = 0;

  xd = get_download_pack(nick, hostname, hostmask, pack, &man, "SEND");
  if (xd == NULL)
    {
      goto done;
    }
  
  if (!man && (check_lock(xd->lock, pwd) != 0))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (pack locked): ");
      notice(nick,"** XDCC SEND denied, this pack is locked");
      goto done;
    }
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if (!man && !strcmp(tr->hostname,hostname))
        {
          usertrans++;
          if (xd == tr->xpack)
            {
              userpackok = 0;
            }
        }
      tr = irlist_get_next(tr);
    }
  
   if (!man && usertrans && !userpackok) {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (dup): ");
      notice(nick,"** You already requested that pack");
      }
   else if (!man && gdata.nonewcons > gdata.curtime ) {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (No New Cons): ");
      notice(nick, "** The Owner Has Requested That No New Connections Are Made In The Next %li Minutes%s%s",
             (gdata.nonewcons-gdata.curtime+1)/60,
             gdata.nosendmsg?", ":"",
             gdata.nosendmsg?gdata.nosendmsg:"");
      }   
   else if (!man && gdata.transferlimits_over)
     {
      char *tempstr = transfer_limit_exceeded_msg(gdata.transferlimits_over - 1);
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Over Transfer Limit): ");
      notice(nick, "** %s", tempstr);
      mydelete(tempstr);
     }
   else if (!man && (xd->dlimit_max != 0) && (xd->gets >= xd->dlimit_used))
     {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Over Pack Transfer Limit): ");
      notice(nick,"** Sorry, This Pack is over download limit for today.  Try again tomorrow.");
      if (xd->dlimit_desc != NULL)
        notice(nick, "%s", xd->dlimit_desc);
     }
   /* if maxtransfersperperson is reached, queue the file, unless queues are used up, which is checked in addtoqueue() */
   else if ( !man && usertrans >= gdata.maxtransfersperperson)
     {
       char *tempstr;
       tempstr = addtoqueue(nick, hostname, pack);
       notice(nick, "** You can only have %d %s at a time, %s",
              gdata.maxtransfersperperson,
              gdata.maxtransfersperperson!=1 ? "transfers" : "transfer",
              tempstr);
       mydelete(tempstr);
     }
   else if ((irlist_size(&gdata.trans) >= MAXTRANS) || (gdata.holdqueue) ||
            (gdata.restrictlist && (has_joined_channels(0) == 0)) ||
            (!man &&
             (((xd->st_size < gdata.smallfilebypass) && (gdata.slotsmax >= MAXTRANS)) ||
              ((xd->st_size >= gdata.smallfilebypass) && (gdata.slotsmax-irlist_size(&gdata.trans) <= 0)))))
     {
       char *tempstr;
       tempstr = addtoqueue(nick, hostname, pack);
       notice(nick, "** All Slots Full, %s",
              tempstr);
       mydelete(tempstr);
     }
   else
     {
       char *sizestrstr;
       
       look_for_file_changes(xd);
       
       xd->file_fd_count++;
       tr = irlist_add(&gdata.trans, sizeof(transfer));
       t_initvalues(tr);
       tr->id = get_next_tr_id();
       tr->nick = mystrdup(nick);
       tr->caps_nick = mystrdup(nick);
       caps(tr->caps_nick);
       tr->hostname = mystrdup(hostname);
       
       tr->xpack = xd;
       tr->maxspeed = xd->maxspeed;
       tr->unlimited = verifyshell(&gdata.unlimitedhost, hostmask);
       tr->nomax = tr->unlimited;
       tr->net = gnetwork->net;
       
       if (!man)
         {
           ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," requested: ");
           ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                   "%s (%s on %s)",
                   nick, hostname, gnetwork->name);
         }
       if (!gdata.quietmode)
         {
           sizestrstr = sizestr(0, tr->xpack->st_size);
           if (!msg)
             {
               notice_fast(nick,"** Sending you pack #%i (\"%s\"), which is %sB (resume supported)",
                      pack, tr->xpack->desc, sizestrstr);
             }
           else
             {
               writeserver(WRITESERVER_FAST, "NOTICE %s%s Which Is %sB. (Resume Supported)",
                           nick, msg, sizestrstr);
             }
           mydelete(sizestrstr);
         }
       
       if (tr->unlimited)
         ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                 "unlimitedhost found: %s (%s on %s)",
                 nick, hostname, gnetwork->name);
       
       t_setup_dcc(tr, nick);
       return;
     }
   
 done:
   
   if (!man)
      ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "%s (%s on %s)",
              nick, hostname, gnetwork->name);
}
   
void sendxdccinfo(const char* nick,
                  const char* hostname,
                  const char* hostmask,
                  int pack)
{
  userinput *pubinfo;
  xdcc *xd;
  char tempstr[maxtextlengthshort];
  
  updatecontext();
  
  if (gdata.disablexdccinfo)
    {
      notice(nick,"** XDCC INFO denied, disabled by configuration");
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," ignored: ");
      goto done;
    }

  xd = get_download_pack(nick, hostname, hostmask, pack, NULL, "INFO");
  if (xd == NULL)
    {
      goto done;
    }

  if (hide_pack(xd) != 0)
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (pack locked): ");
      notice(nick,"** Invalid Pack Number, Try Again");
      goto done;
    }
  
  ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," requested: ");

  pubinfo = mycalloc(sizeof(userinput));
  snprintf(tempstr, sizeof(tempstr), "A A A A A INFO %d", pack);
  u_fillwith_msg(pubinfo, nick, tempstr);
  pubinfo->method = method_xdl_user_notice;
  pubinfo->net = gnetwork->net;
  pubinfo->level = ADMIN_LEVEL_PUBLIC;
  u_parseit(pubinfo);
  mydelete(pubinfo);

 done:
  
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "%s (%s on %s)",
          nick, hostname, gnetwork->name);
  
  return;
}
   
char* addtoqueue(const char* nick, const char* hostname, int pack)
{
   char *tempstr = mycalloc(maxtextlength);
   char *idle;
   ir_pqueue *tempq;
   xdcc *tempx;
   int inq,alreadytrans;
   int inq2;
   int man;
   
   updatecontext();
   
   tempx = irlist_get_nth(&gdata.xdccs, pack-1);
   
   if (!strcmp(hostname,"man"))
      {
        man = 1;
      }
    else
      {
        man = 0;
      }
   
   inq = 0;
   inq2 = 0;
   alreadytrans = queue_count_host(&gdata.mainqueue, &inq, man, nick, hostname, tempx);
   alreadytrans += queue_count_host(&gdata.idlequeue, &inq2, man, nick, hostname, tempx);
   
   if (alreadytrans) {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (queue/dup): ");
      snprintf(tempstr, maxtextlength,
               "Denied, You already have that item queued.");
      return tempstr;
      }
    
   if (!man && (inq >= gdata.maxqueueditemsperperson)) {
      idle = addtoidlequeue(tempstr, nick, hostname, tempx, pack, inq2);
      if (idle != NULL)
        return tempstr;

      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (user/queue): ");
      snprintf(tempstr, maxtextlength,
               "Denied, You already have %i items queued, Try Again Later",
               inq);
      return tempstr;
      }
   
   if (!man && (irlist_size(&gdata.mainqueue) >= gdata.queuesize)) {
      idle = addtoidlequeue(tempstr, nick, hostname, tempx, pack, inq2);
      if (idle != NULL)
        return tempstr;

         ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (slot/queue): ");
         snprintf(tempstr, maxtextlength,
                  "Main queue of size %d is Full, Try Again Later",
                  gdata.queuesize);
         return tempstr;
         }

         ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Queued (slot): ");
         tempq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));
         tempq->queuedtime = gdata.curtime;
         tempq->nick = mystrdup(nick);
         tempq->hostname = mystrdup(hostname);
         tempq->xpack = tempx;
         tempq->net = gnetwork->net;

         snprintf(tempstr,maxtextlength,
                  "Added you to the main queue for pack %d (\"%s\") in position %d. To Remove yourself at "
                  "a later time type \"/MSG %s XDCC REMOVE\".",
                  pack, tempx->desc,
                  irlist_size(&gdata.mainqueue),
                  save_nick(gnetwork->user_nick));

   return tempstr;
   }

void sendaqueue(int type, int pos, char *lastnick)
{
  int usertrans;
  ir_pqueue *pq;
  transfer *tr;
  char *hostmask;
  gnetwork_t *backup;
  
  updatecontext();
  
  if (gdata.holdqueue)
     return;
  
  if (!gdata.balanced_queue)
     lastnick = NULL;

  if (gdata.restrictlist && (has_joined_channels(0) == 0))
     return;
  
  if (irlist_size(&gdata.mainqueue))
    {
      
      /*
       * first determine what the first queue position is that is eligable
       * for a transfer find the first person who has not already maxed out
       * his sends if noone, do nothing and let execution continue to pack
       * queue check
       */
      
      if (pos > 0) {
        /* get specific entry */
        pq = irlist_get_nth(&gdata.mainqueue, pos - 1);
      } else {
        for (pq = irlist_get_head(&gdata.mainqueue); pq; pq = irlist_get_next(pq)) {
          if (gdata.networks[pq->net].serverstatus != SERVERSTATUS_CONNECTED)
            continue;

          /* timeout for restart must be less then Transfer Timeout 180s */
          if (gdata.curtime - gdata.networks[pq->net].lastservercontact > 150)
            continue;

          usertrans=0;
          for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
            if ((!strcmp(tr->hostname, pq->hostname)) || (!strcasecmp(tr->nick, pq->nick))) {
              usertrans++;
            }
          }

          /* usertrans is the number of transfers a user has in progress */
          if (usertrans >= gdata.maxtransfersperperson)
            continue;

          /* skip last trasfering user */
          if (lastnick != NULL) {
            if (!strcasecmp(pq->nick, lastnick))
              continue;
          }

          /* found the person that will get the send */
          break;
        }
      }
      
      if (!pq)
        {
	  if (lastnick != NULL)
            {
              /* try again */
              sendaqueue(type, pos, NULL);
            }
          return;
        }
      
      if (type == 1)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                  "QUEUED SEND (low bandwidth): %s (%s on %s), Pack #%d",
                  pq->nick, pq->hostname, gdata.networks[ pq->net ].name,
                  number_of_pack(pq->xpack));
        }
      else if (type == 2)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                  "QUEUED SEND (manual): %s (%s on %s), Pack #%d",
                  pq->nick, pq->hostname, gdata.networks[ pq->net ].name,
                  number_of_pack(pq->xpack));
        }
      else
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                  "QUEUED SEND: %s (%s on %s), Pack #%d",
                  pq->nick, pq->hostname, gdata.networks[ pq->net ].name,
                  number_of_pack(pq->xpack));
        }
      
      look_for_file_changes(pq->xpack);
      
      backup = gnetwork;
      gnetwork = &(gdata.networks[pq->net]);
      
      pq->xpack->file_fd_count++;
      tr = irlist_add(&gdata.trans, sizeof(transfer));
      t_initvalues(tr);
      tr->id = get_next_tr_id();
      tr->nick = mystrdup(pq->nick);
      tr->caps_nick = mystrdup(pq->nick);
      caps(tr->caps_nick);
      tr->hostname = mystrdup(pq->hostname);
      
      tr->xpack = pq->xpack;
      tr->maxspeed = pq->xpack->maxspeed;
      hostmask = to_hostmask(tr->nick, tr->hostname);
      tr->unlimited = verifyshell(&gdata.unlimitedhost, hostmask);
      if (tr->unlimited)
        ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                "unlimitedhost found: %s (%s on %s)",
                tr->nick, tr->hostname, gdata.networks[ tr->net ].name);
      tr->nomax = tr->unlimited;
      tr->net = pq->net;
      mydelete(hostmask);
      mydelete(pq->nick);
      mydelete(pq->hostname);
      irlist_delete(&gdata.mainqueue, pq);
      
      if (!gdata.quietmode)
        {
          char *sizestrstr;
          sizestrstr = sizestr(0, tr->xpack->st_size);
          notice_fast(tr->nick,
                 "** Sending You Your Queued Pack Which Is %sB. (Resume Supported)",
                 sizestrstr);
          mydelete(sizestrstr);
        }
      
      t_setup_dcc(tr, tr->nick);
      
      gnetwork = backup;
      return;
    }
}

/* End of File */
