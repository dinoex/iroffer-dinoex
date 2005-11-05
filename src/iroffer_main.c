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

/* local functions */
static void mainloop(void);
static void parseline(char *line);
static void privmsgparse(const char* type, char* line);
static void autosendf(char* line);
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
            "iroffer v" VERSIONLONG " by Dinoex, see http://iroffer.dinoex.net/\n"
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
      exit(0);
   case PCL_SHOW_VERSION:
      printf("iroffer v" VERSIONLONG " by Dinoex, see http://iroffer.dinoex.net/\n");
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
      fprintf(stderr,"%s: no configuration file specifed\n",
              argv[0]);
      return PCL_BAD_OPTION;
    }
  
  return PCL_OK;
}


static void select_dump(const char *desc, int highests)
{
  int ii;
  
  if (!gdata.attop) gototop();
  
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

static void admin_line(int fd, const char *line) {
   userinput *uxdl;
   
   if (line == NULL)
      return;
   
   uxdl = mycalloc(sizeof(userinput));
   
   u_fillwith_msg(uxdl,NULL,line);
   uxdl->method = method_fd;
   uxdl->fd = fd;
   
   u_parseit(uxdl);
   
   mydelete(uxdl);
}

static void admin_jobs(const char *job) {
   FILE *fin;
   int fd;
   char *done;
   char *line;
   char *l;
   char *r;
   
   if (job == NULL)
      return;

   fin = fopen(job, "ra" );
   if (fin == NULL)
      return;

   line = mycalloc(maxtextlength);

   done = mycalloc(strlen(job)+6);
   strcpy(done,job);
   strcat(done,".done");
   fd = open(done,
             O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
             CREAT_PERMISSIONS);
   if (fd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Cant Create Admin Job Done File '%s': %s",
               done, strerror(errno));
    }
  else
    {
      while (!feof(fin)) {
         strcpy(line, "A A A A A ");
         l = line + strlen(line);
         r = fgets(l, maxtextlength - 1, fin);
         if (r == NULL )
            break;
         l = line + strlen(line) - 1;
         while (( *l == '\r' ) || ( *l == '\n' ))
            *(l--) = 0;
         admin_line(fd,line);
      }
      close(fd);
    }
   mydelete(line)
   mydelete(done)
   fclose(fin);
   unlink(job);
}

static void mainloop (void) {
   /* data is persistant across calls */
   static char server_input_line[INPUT_BUFFER_LENGTH];
   static struct timeval timestruct;
   static int i,j,length,changequartersec,changesec,changemin,changehour;
   static time_t lasttime, lastmin, lasthour, last4sec, last5sec, last20sec;
   static long lastnotify, last3min, last2min, lastignoredec, lastperiodicmsg;
   static userinput *pubplist;
   static userinput *urehash;
   static int first_loop = 1;
   static unsigned long long last250ms;
   static ir_uint64 xdccsent;
   
   int overlimit;
   int highests;
   upload *ul;
   transfer *tr;
   pqueue *pq;
   channel_t *ch;
   xdcc *xd;
   dccchat_t *chat;
   
   updatecontext();
   
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
       lastperiodicmsg = last4sec = last5sec
	 = lastnotify = last3min = last20sec = last2min = lastignoredec = lasttime;
       server_input_line[0] = '\0';
       
       gdata.cursendptr = 0;
       
       first_loop = 0;
     }
   
      updatecontext();
   
      FD_ZERO(&gdata.readset);
      FD_ZERO(&gdata.writeset);
      highests = 0;
      
      if (gdata.serverstatus == SERVERSTATUS_CONNECTED)
        {
          FD_SET(gdata.ircserver, &gdata.readset);
          highests = max2(highests, gdata.ircserver);
        }
      else if (gdata.serverstatus == SERVERSTATUS_TRYING)
        {
          FD_SET(gdata.ircserver, &gdata.writeset);
          highests = max2(highests, gdata.ircserver);
        }
      else if (gdata.serverstatus == SERVERSTATUS_RESOLVING)
        {
          FD_SET(gdata.serv_resolv.sp_fd[0], &gdata.readset);
          highests = max2(highests, gdata.serv_resolv.sp_fd[0]);
        }
      
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
              FD_SET(chat->fd, &gdata.writeset);
              highests = max2(highests, chat->fd);
            }
          else if (chat->status != DCCCHAT_UNUSED)
            {
              FD_SET(chat->fd, &gdata.readset);
              highests = max2(highests, chat->fd);
            }
        }

      j = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE] 
        + gdata.xdccsent[(gdata.curtime-1)%XDCC_SENT_SIZE]
        + gdata.xdccsent[(gdata.curtime-2)%XDCC_SENT_SIZE]
        + gdata.xdccsent[(gdata.curtime-3)%XDCC_SENT_SIZE];
      
      if ( gdata.maxb && (j >= gdata.maxb*1024))
        {
          overlimit = 1;
        }
      else
        {
          overlimit = 0;
        }
      
      
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          if (tr->tr_status == TRANSFER_STATUS_LISTENING)
            {
              FD_SET(tr->listensocket, &gdata.readset);
              highests = max2(highests, tr->listensocket);
            }
          if (tr->tr_status == TRANSFER_STATUS_SENDING)
            {
              if (!overlimit && !tr->overlimit)
                {
                  FD_SET(tr->clientsocket, &gdata.writeset);
                  highests = max2(highests, tr->clientsocket);
                }
              if (changequartersec || ((tr->bytessent-tr->lastack) > 512*1024))
                {
                  FD_SET(tr->clientsocket, &gdata.readset);
                  highests = max2(highests, tr->clientsocket);
                }
            }
          if (tr->tr_status == TRANSFER_STATUS_WAITING)
            {
              FD_SET(tr->clientsocket, &gdata.readset);
              highests = max2(highests, tr->clientsocket);
            }
          tr = irlist_get_next(tr);
        }
      
      ul = irlist_get_head(&gdata.uploads);
      while(ul)
        {
          if (ul->ul_status == UPLOAD_STATUS_CONNECTING)
            {
              FD_SET(ul->clientsocket, &gdata.writeset);
              highests = max2(highests, ul->clientsocket);
            }
          if (ul->ul_status == UPLOAD_STATUS_GETTING)
            {
              FD_SET(ul->clientsocket, &gdata.readset);
              highests = max2(highests, ul->clientsocket);
            }
          ul = irlist_get_next(ul);
        }
      
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
        }
      
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
              if (gdata.serverstatus == SERVERSTATUS_RESOLVING)
                {
                  if (child == gdata.serv_resolv.child_pid)
                    {
                      /* lookup failed */
#ifdef NO_WSTATUS_CODES
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                              "Unable to resolve server %s (status=0x%.8X)",
                              gdata.curserver.hostname,
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
                                  gdata.curserver.hostname, errstr);
                        }
                      else
#endif
                        {
                          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                                  "Unable to resolve server %s (status=0x%.8X, %s: %d)",
                                  gdata.curserver.hostname,
                                  status,
                                  WIFEXITED(status) ? "exit" : WIFSIGNALED(status) ? "signaled" : "??",
                                  WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status) : 0);
                        }
#endif
                      gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
                    }
                  /* else  this is an old child, ignore */
                }
              
              if (child == gdata.serv_resolv.child_pid)
                {
                  /* cleanup */
                  close(gdata.serv_resolv.sp_fd[0]);
                  FD_CLR(gdata.serv_resolv.sp_fd[0], &gdata.readset);
                  gdata.serv_resolv.sp_fd[0] = 0;
                  gdata.serv_resolv.child_pid = 0;
                }
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
            if (!gdata.attop) gototop();
            outerror(OUTERROR_TYPE_WARN,"System Time Changed Backwards %lim %lis!!\n",
               (long)(lasttime-gdata.curtime)/60,(long)(lasttime-gdata.curtime)%60);
            }
         
         if (gdata.curtime > lasttime+10) {
            if (!gdata.attop) gototop();
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
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsent[i];
         if (((float)xdccsent)/XDCC_SENT_SIZE/1024.0 > gdata.sentrecord)
            gdata.sentrecord = ((float)xdccsent)/XDCC_SENT_SIZE/1024.0;
         gdata.xdccsent[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;
      }

      if (changequartersec)
        {
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ( !tr->nomax &&
                   (tr->xpack->maxspeed > 0))
                {
                  tr->tx_bucket += tr->xpack->maxspeed * (1024 / 4);
                  tr->tx_bucket = min2(tr->tx_bucket, MAX_TRANSFER_TX_BURST_SIZE * tr->xpack->maxspeed * 1024);
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
      /*----- see if gdata.ircserver is sending anything to us ----- */
      if (gdata.serverstatus == SERVERSTATUS_CONNECTED && FD_ISSET(gdata.ircserver, &gdata.readset)) {
         char tempbuffa[INPUT_BUFFER_LENGTH];
         gdata.lastservercontact = gdata.curtime;
         gdata.servertime = 0;
         length = read (gdata.ircserver, &tempbuffa, INPUT_BUFFER_LENGTH);
         
         if (length < 1) {
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_RED,"Closing Server Connection: %s",(length<0) ? strerror(errno) : "Closed");
            if (gdata.exiting)
              {
                gdata.recentsent = 0;
              }
            FD_CLR(gdata.ircserver, &gdata.readset);
            /*
             * cygwin close() is broke, if outstanding data is present
             * it will block until the TCP connection is dead, sometimes
             * upto 10-20 minutes, calling shutdown() first seems to help
             */
            shutdown(gdata.ircserver, SHUT_RDWR);
            close(gdata.ircserver);
            mydelete(gdata.curserveractualname);
            gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
         else
           {
             j=strlen(server_input_line);
             for (i=0; i<length; i++)
               {
                 if ((tempbuffa[i] == '\n') || (j == (INPUT_BUFFER_LENGTH-1)))
                   {
                     if (j && (server_input_line[j-1] == 0x0D))
                       {
                         j--;
                       }
                     server_input_line[j] = '\0';
                     parseline(removenonprintable(server_input_line));
                     j = 0;
                   }
                 else
                   {
                     server_input_line[j] = tempbuffa[i];
                     j++;
                   }
               }
             server_input_line[j] = '\0';
           }
        }
      
      if (gdata.serverstatus == SERVERSTATUS_TRYING && FD_ISSET(gdata.ircserver, &gdata.writeset))
        {
          int callval_i;
          int connect_error;
          int connect_error_len = sizeof(connect_error);
          
          callval_i = getsockopt(gdata.ircserver,
                                 SOL_SOCKET, SO_ERROR,
                                 &connect_error, &connect_error_len);
          
          if (callval_i < 0)
            {
              outerror(OUTERROR_TYPE_WARN,
                       "Couldn't determine connection status: %s",
                       strerror(errno));
            }
          else if (connect_error)
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Server Connection Failed: %s", strerror(connect_error));
            }
          
          if ((callval_i < 0) || connect_error)
            {
              FD_CLR(gdata.ircserver, &gdata.writeset);
              /*
               * cygwin close() is broke, if outstanding data is present
               * it will block until the TCP connection is dead, sometimes
               * upto 10-20 minutes, calling shutdown() first seems to help
               */
              shutdown(gdata.ircserver, SHUT_RDWR);
              close(gdata.ircserver);
              gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
          else
            {
	    SIGNEDSOCK int addrlen; 
	    struct sockaddr_in localaddr;
          
	    ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Server Connection Established, Logging In");
            gdata.serverstatus = SERVERSTATUS_CONNECTED;
            FD_CLR(gdata.ircserver, &gdata.writeset);
	    if (set_socket_nonblocking(gdata.ircserver, 0) < 0 )
	      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Blocking");
	    
            if (!gdata.usenatip)
              {
                addrlen = sizeof (localaddr);
                bzero ((char *) &localaddr, sizeof (localaddr));
                if (getsockname(gdata.ircserver,(struct sockaddr *) &localaddr, &addrlen) >= 0)
                  {
                    gdata.ourip = ntohl(localaddr.sin_addr.s_addr);
                    if (gdata.debug > 0)
                      {
                        ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"ourip = %lu.%lu.%lu.%lu",
                                (gdata.ourip >> 24) & 0xFF,
                                (gdata.ourip >> 16) & 0xFF,
                                (gdata.ourip >>  8) & 0xFF,
                                (gdata.ourip      ) & 0xFF
                                );
                      }
                  }
                else
                  outerror(OUTERROR_TYPE_WARN,"couldn't get ourip");
              }
	    
	    initirc();
            }
         }
      
      if ((gdata.serverstatus == SERVERSTATUS_RESOLVING) &&
          FD_ISSET(gdata.serv_resolv.sp_fd[0], &gdata.readset))
        {
          struct in_addr remote;
          length = read(gdata.serv_resolv.sp_fd[0],
                        &remote, sizeof(struct in_addr));
          
          kill(gdata.serv_resolv.child_pid, SIGKILL);
          FD_CLR(gdata.serv_resolv.sp_fd[0], &gdata.readset);
          
          if (length != sizeof(struct in_addr))
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
                      "Error resolving server %s",
                      gdata.curserver.hostname);
              gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
          else
            {
              /* continue with connect */
              if (connectirc2(&remote))
                {
                  /* failed */
                  gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
                }
            }
        }
      
      if (changesec && gdata.serverstatus == SERVERSTATUS_RESOLVING)
        {
          int timeout;
          timeout = CTIMEOUT + (gdata.serverconnectbackoff * CBKTIMEOUT);
          
          if (gdata.lastservercontact + timeout < gdata.curtime)
            {
              kill(gdata.serv_resolv.child_pid, SIGKILL);
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Server Resolve Timed Out (%d seconds)",timeout);
              gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
        }
      
      if (changesec && gdata.serverstatus == SERVERSTATUS_TRYING)
        {
          int timeout;
          timeout = CTIMEOUT + (gdata.serverconnectbackoff * CBKTIMEOUT);
          
          if (gdata.lastservercontact + timeout < gdata.curtime)
            {
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Server Connection Timed Out (%d seconds)",timeout);
              FD_CLR(gdata.ircserver, &gdata.readset);
              /*
               * cygwin close() is broke, if outstanding data is present
               * it will block until the TCP connection is dead, sometimes
               * upto 10-20 minutes, calling shutdown() first seems to help
               */
              shutdown(gdata.ircserver, SHUT_RDWR);
              close(gdata.ircserver);
              gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
            }
        }
      
      if (gdata.needsswitch)
        {
          gdata.needsswitch = 0;
          switchserver(-1);
        }
      
      updatecontext();
      
      ul = irlist_get_head(&gdata.uploads);
      while(ul)
        {
          /*----- see if uploads are sending anything to us ----- */
          if (ul->ul_status == UPLOAD_STATUS_GETTING && FD_ISSET(ul->clientsocket, &gdata.readset))
            {
              l_transfersome(ul);
            }
          
          if (ul->ul_status == UPLOAD_STATUS_CONNECTING && FD_ISSET(ul->clientsocket, &gdata.writeset))
            {
              int callval_i;
              int connect_error;
              int connect_error_len = sizeof(connect_error);
              
              callval_i = getsockopt(ul->clientsocket,
                                     SOL_SOCKET, SO_ERROR,
                                     &connect_error, &connect_error_len);
              
              if (callval_i < 0)
                {
                  outerror(OUTERROR_TYPE_WARN,
                           "Couldn't determine upload connection status: %s",
                           strerror(errno));
                  l_closeconn(ul,"Upload Connection Failed status:",errno);
                }
              else if (connect_error)
                {
                  l_closeconn(ul,"Upload Connection Failed",connect_error);
                }
              
              if ((callval_i < 0) || connect_error)
                {
                  FD_CLR(ul->clientsocket, &gdata.writeset);
                }
              else
                {
                  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"Upload Connection Established");
                  ul->ul_status = UPLOAD_STATUS_GETTING;
                  FD_CLR(ul->clientsocket, &gdata.writeset);
                  notice(ul->nick,"DCC Connection Established");
                  ul->connecttime = gdata.curtime;
                  if (set_socket_nonblocking(ul->clientsocket,0) < 0 )
                    {
                      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Blocking");
                    }
                }
            }
          
          if (changesec && ul->ul_status == UPLOAD_STATUS_CONNECTING && ul->lastcontact + CTIMEOUT < gdata.curtime)
            {
              FD_CLR(ul->clientsocket, &gdata.readset);
              l_closeconn(ul,"Upload Connection Timed Out",0);
            }
          
          if (changesec)
            {            
              l_istimeout(ul);
            }
          
          if (changesec && ul->ul_status == UPLOAD_STATUS_DONE)
            {
              mydelete(ul->nick);
              mydelete(ul->hostname);
              mydelete(ul->file);
              ul = irlist_delete(&gdata.uploads, ul);
            }
          else
            {
              ul = irlist_get_next(ul);
            }
        }
      
      updatecontext();
      /*----- see if dccchat is sending anything to us ----- */
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_get_next(chat))
        {
          if ((chat->status == DCCCHAT_CONNECTING) &&
              FD_ISSET(chat->fd, &gdata.writeset))
            {
              int callval_i;
              int connect_error;
              int connect_error_len = sizeof(connect_error);
              
              callval_i = getsockopt(chat->fd,
                                     SOL_SOCKET, SO_ERROR,
                                     &connect_error, &connect_error_len);
              
              if (callval_i < 0)
                {
                  outerror(OUTERROR_TYPE_WARN,
                           "Couldn't determine dcc connection status: %s",
                           strerror(errno));
                  notice(chat->nick, "DCC Chat Connect Attempt Failed: %s", strerror(errno));
                }
              else if (connect_error)
                {
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "DCC Chat Connect Attempt Failed: %s",
                          strerror(connect_error));
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
          if ((chat->status != DCCCHAT_UNUSED) &&
              FD_ISSET(chat->fd, &gdata.readset))
            {
              char tempbuffa[INPUT_BUFFER_LENGTH];
              switch (chat->status)
                {
                case DCCCHAT_LISTENING:
                  setupdccchataccept(chat);
                  break;
                  
                case DCCCHAT_AUTHENTICATING:
                case DCCCHAT_CONNECTED:
                  memset(tempbuffa, 0, INPUT_BUFFER_LENGTH);
                  length = read(chat->fd, &tempbuffa, INPUT_BUFFER_LENGTH);
                  
                  if (length < 1)
                    {
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "DCC Chat Lost: %s",
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
      
      updatecontext();
      
      i = j = gdata.cursendptr++ % max2(irlist_size(&gdata.trans),1);
      
      tr = irlist_get_nth(&gdata.trans, i);
      
      /* first: do from cur to end */
      while(tr)
        {
          if (tr->tr_status == TRANSFER_STATUS_SENDING)
            {
              /*----- look for transfer some -----  send at least once a second, or more if necessary */
              if (changequartersec || FD_ISSET(tr->clientsocket, &gdata.writeset))
                {
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
              if (changequartersec || FD_ISSET(tr->clientsocket, &gdata.writeset))
                {
                  t_transfersome(tr);
                }
            }
          tr = irlist_get_next(tr);
        }
      
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          /*----- look for listen reminders ----- */
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->lastcontact) >= 30) &&
              (tr->reminded == 0))
            {
              t_remind(tr);
            }
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->lastcontact) >= 90) &&
              (tr->reminded == 1) && !gdata.quietmode)
            {
              t_remind(tr);
            }
          if (changesec &&
              (tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              ((gdata.curtime - tr->lastcontact) >= 150) &&
              (tr->reminded == 2) && !gdata.quietmode)
            {
              t_remind(tr);
            }
          
          /*----- look for listen->connected ----- */
          if ((tr->tr_status == TRANSFER_STATUS_LISTENING) &&
              FD_ISSET(tr->listensocket, &gdata.readset))
            {
              t_establishcon(tr);
              if ((gdata.ignoreduplicateip) && (gdata.maxtransfersperperson > 0))
                {
                  check_duplicateip(tr);
                }
            }
          
          /*----- look for junk to read ----- */
          if (((tr->tr_status == TRANSFER_STATUS_SENDING) ||
               (tr->tr_status == TRANSFER_STATUS_WAITING)) &&
              FD_ISSET(tr->clientsocket, &gdata.readset))
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
              mydelete(tr->nick);
              mydelete(tr->caps_nick);
              mydelete(tr->hostname);
              tr = irlist_delete(&gdata.trans, tr);
              
              if (!gdata.exiting &&
                  irlist_size(&gdata.mainqueue) &&
                  (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
                {
                  sendaqueue(0);
                }
            }
          else
            {
              tr = irlist_get_next(tr);
            }
         }
      
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
      /*----- send server stuff ----- */
      if (changesec) {
         sendserver();
         if (gdata.curtime%INAMNT_SIZE == (INAMNT_SIZE-1))
            gdata.inamnt[0] = 0;
         else
            gdata.inamnt[gdata.curtime%INAMNT_SIZE+1] = 0;
         }
      
      /*----- see if we can send out some xdcc lists */
      if (changesec && gdata.serverstatus == SERVERSTATUS_CONNECTED) {
         if (!irlist_size(&gdata.serverq_normal) && !irlist_size(&gdata.serverq_slow))
            sendxdlqueue();
         }
      
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
                      reset_download_limits();
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
                }
              
              if (!transferlimits_over &&
                  gdata.transferlimits[ii].limit &&
                  (gdata.transferlimits[ii].used >= gdata.transferlimits[ii].limit))
                {
                  transferlimits_over = 1;
                  
                  if (!gdata.transferlimits_over)
                    {
                      char *tempstr = mycalloc(maxtextlength);
                      char *tempstr2 = mycalloc(maxtextlength);
                      
                      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                              "All %" LLPRINTFMT "uMB of the %s transfer limit used. Stopping transfers.",
                              gdata.transferlimits[ii].limit / 1024 / 1024,
                              transferlimit_type_to_string(ii));
                      
                      getdatestr(tempstr2, gdata.transferlimits[ii].ends, maxtextlength);
                      
                      snprintf(tempstr, maxtextlength,
                               "Sorry, I have exceeded my %s transfer limit of %" LLPRINTFMT "uMB.  Try again after %s.",
                               transferlimit_type_to_string(ii),
                               gdata.transferlimits[ii].limit / 1024 / 1024,
                               tempstr2);
                      
                      /* remove queued users */
                      for (pq = irlist_get_head(&gdata.mainqueue); pq; pq = irlist_delete(&gdata.mainqueue, pq))
                        {
                          notice_slow(pq->nick, tempstr);
                          mydelete(pq->nick);
                          mydelete(pq->hostname);
                        }
                      
                      /* stop transfers */
                      for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
                        {
                          if (tr->tr_status != TRANSFER_STATUS_DONE)
                            {
                              t_closeconn(tr,tempstr,0);
                            }
                        }
                      
                      mydelete(tempstr);
                      mydelete(tempstr2);
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
              if (!gdata.attop) gototop();
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
                  if (ignore->regexp)
                    {
                      regfree(ignore->regexp);
                    }
                  mydelete(ignore->regexp);
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
         
         if (gdata.periodicmsg_nick && gdata.periodicmsg_msg
	     && (gdata.serverstatus == SERVERSTATUS_CONNECTED) )
            privmsg(gdata.periodicmsg_nick,"%s",gdata.periodicmsg_msg);
         
         }
      
      updatecontext();
      
      /*----- 5 seconds ----- */
      if (changesec && (gdata.curtime - last5sec > 4)) {
         last5sec = gdata.curtime;
         
	 updatecontext();
         /*----- server timeout ----- */
         if ((gdata.serverstatus == SERVERSTATUS_CONNECTED) &&
             (gdata.curtime - gdata.lastservercontact > SRVRTOUT)) {
            if (gdata.servertime < 3)
              {
                const char *servname = gdata.curserveractualname ? gdata.curserveractualname : gdata.curserver.hostname;
                int        len       = 6 + strlen(servname);
                char       *tempstr3 = mycalloc(len + 1);
                snprintf(tempstr3, len + 1, "PING %s\n", servname);
                write(gdata.ircserver, tempstr3, len);
                if (gdata.debug > 0)
                  {
                    tempstr3[len-1] = '\0';
                    len--;
                    ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<NORES<: %s",tempstr3);
                  }
                mydelete(tempstr3);
                gdata.servertime++;
              }
            else if (gdata.servertime == 3) {
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_RED,"Closing Server Connection: No Response for %d minutes.",SRVRTOUT/60);
               FD_CLR(gdata.ircserver, &gdata.readset);
               /*
                * cygwin close() is broke, if outstanding data is present
                * it will block until the TCP connection is dead, sometimes
                * upto 10-20 minutes, calling shutdown() first seems to help
                */
               shutdown(gdata.ircserver, SHUT_RDWR);
               close(gdata.ircserver);
               gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
               gdata.servertime = 0;
               }
            }
         
         
         /*----- ping server ----- */
         if (gdata.recentsent) {
            pingserver();
            gdata.recentsent--;
            }
         
         }
      
      /*----- 4 seconds ----- */
      if (changesec && (gdata.curtime - last4sec > 3))
        {
         
         /*----- update lastspeed, check minspeed ----- */
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ( tr->connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
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
              if ( ul->connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
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
      
      /*----- plist stuff ----- */
      if ((gdata.serverstatus == SERVERSTATUS_CONNECTED) &&
          changemin &&
          irlist_size(&gdata.xdccs) &&
          !gdata.transferlimits_over &&
          (!gdata.queuesize || irlist_size(&gdata.mainqueue) < gdata.queuesize) &&
          (gdata.nolisting <= gdata.curtime))
        {
            char *tchanf = NULL, *tchanm = NULL, *tchans = NULL;
            
            ch = irlist_get_head(&gdata.channels);
            while(ch)
              {
                if ((ch->flags & CHAN_ONCHAN) &&
                    ch->plisttime &&
                    (((gdata.curtime / 60) % ch->plisttime) == ch->plistoffset))
                  {
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
               ch = irlist_get_next(ch);
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
                    u_parseit(pubplist);
                    mydelete(pubplist);
                  }
                mydelete(tchans);
              }
            if (tchanf) {
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Plist sent to %s (full)",tchanf);
               pubplist = mycalloc(sizeof(userinput));
               u_fillwith_msg(pubplist,tchanf,"A A A A A xdl");
               pubplist->method = method_xdl_channel;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanf);
               }
            if (tchanm) {
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Plist sent to %s (minimal)",tchanm);
               pubplist = mycalloc(sizeof(userinput));
               u_fillwith_msg(pubplist,tchanm,"A A A A A xdl");
               pubplist->method = method_xdl_channel_min;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanm);
               }
            
         }
      
      updatecontext();
      /*----- low bandwidth send, save state file ----- */
      if (changesec && (gdata.curtime - last3min > 180)) {
         last3min = gdata.curtime;
         
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsent[i];
         xdccsent /= XDCC_SENT_SIZE*1024;
         
         if ((xdccsent < gdata.lowbdwth) &&
             !gdata.exiting &&
             irlist_size(&gdata.mainqueue) &&
             (irlist_size(&gdata.trans) < MAXTRANS))
           {
             sendaqueue(1);
           }
         write_statefile();
         xdccsavetext();
         }
      
      updatecontext();
      /*----- queue notify ----- */
      if (changesec && gdata.notifytime && (!gdata.quietmode) &&
          (gdata.curtime - lastnotify > (gdata.notifytime*60)))
        {
         lastnotify = gdata.curtime;
         
	 if (gdata.serverstatus == SERVERSTATUS_CONNECTED)
	   {
             if ((irlist_size(&gdata.serverq_fast) >= 10) ||
                 (irlist_size(&gdata.serverq_normal) >= 10) ||
                 (irlist_size(&gdata.serverq_slow) >= 50))
               {
                 ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,
                         "notifications skipped, server queue is rather large");
               }
             else
               {
                 notifyqueued();
                 notifybandwidth();
                 notifybandwidthtrans();
               }
	   }
	 
         }
      
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
      
      /*----- 20 seconds ----- */
      if (changesec && (gdata.curtime - last20sec > 19)) {
         
         if (gdata.logfd != FD_UNUSED)
           {
             /* cycle */
             close(gdata.logfd);
             gdata.logfd = FD_UNUSED;
           }
         
         updatecontext();
         
         /* look to see if any files changed */
         look_for_file_remove();
         
         updatecontext();
         
         /* try rejoining channels not on */
         ch = irlist_get_head(&gdata.channels);
         while(ch)
           {
             if ((gdata.serverstatus == SERVERSTATUS_CONNECTED) &&
                 !(ch->flags & CHAN_ONCHAN))
               {
                 joinchannel(ch);
               }
             ch = irlist_get_next(ch);
           }
         
         last20sec = gdata.curtime;
         
         updatecontext();
         
         /* try to regain nick */
         if (!gdata.user_nick || strcmp(gdata.config_nick,gdata.user_nick))
           {
             writeserver(WRITESERVER_NORMAL, "NICK %s", gdata.config_nick);
           }
         
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
         
         admin_jobs(gdata.admin_job_file);
         }
      
      updatecontext();
      
      if (changemin)
        {
          reverify_restrictsend();
        }

      updatecontext();
      
      if ((gdata.md5build.file_fd != FD_UNUSED) &&
          FD_ISSET(gdata.md5build.file_fd, &gdata.readset))
        {
          ssize_t howmuch;
          int reads_per_loop = 64;
          
          assert(gdata.md5build.xpack);
          
          while (reads_per_loop--)
            {
              howmuch = read(gdata.md5build.file_fd, gdata.sendbuff, BUFFERSIZE);
              
              if (gdata.debug > 4)
                {
                  if (!gdata.attop) { gototop(); }
                  ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"[MD5]: read %d",howmuch);
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
                  gdata.md5build.xpack->has_md5sum = 1;
                  
                  if (!gdata.attop) { gototop(); }
                  
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "[MD5]: is " MD5_PRINT_FMT, MD5_PRINT_DATA(gdata.md5build.xpack->md5sum));
                  
                  FD_CLR(gdata.md5build.file_fd, &gdata.readset);
                  close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  gdata.md5build.xpack = NULL;
                  break;
                }
              /* else got data */
              MD5Update(&gdata.md5build.md5sum, gdata.sendbuff, howmuch);
            }
        }
      
      if (!gdata.nomd5sum && changesec && (!gdata.md5build.xpack))
        {
          int packnum = 1;
          /* see if any pack needs a md5sum calculated */
          for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd), packnum++)
            {
              if (!xd->has_md5sum)
                {
                  if (!gdata.attop) { gototop(); }
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "[MD5]: Calculating pack %d",packnum);
                  
                  gdata.md5build.file_fd = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
                  if (gdata.md5build.file_fd >= 0)
                    {
                      gdata.md5build.xpack = xd;
                      MD5Init(&gdata.md5build.md5sum);
                      if (set_socket_nonblocking(gdata.md5build.file_fd, 1) < 0)
                        {
                          outerror(OUTERROR_TYPE_WARN,"[MD5]: Couldn't Set Non-Blocking");
                        }
                      break;
                    }
                  else
                    {
                      outerror(OUTERROR_TYPE_WARN,
                               "[MD5]: Cant Access Offered File '%s': %s",
                               xd->file, strerror(errno));
                      gdata.md5build.file_fd = FD_UNUSED;
                    }
                }
            }
        }
      
      updatecontext();
      
      if (gdata.exiting && gdata.serverstatus != SERVERSTATUS_CONNECTED) {
         
         for (chat = irlist_get_head(&gdata.dccchats);
              chat;
              chat = irlist_delete(&gdata.dccchats,chat))
           {
             writedccchat(chat, 0, "iroffer exited, Closing DCC Chat\n");
             shutdowndccchat(chat,1);
           }
         
         mylog(CALLTYPE_NORMAL,"iroffer exited\n\n");
         
         tostdout_disable_buffering(1);
         uninitscreen();
         if (gdata.pidfile) unlink(gdata.pidfile);
         exit(0);
         }
      
      updatecontext();
      if (gdata.serverstatus == SERVERSTATUS_NEED_TO_CONNECT)
        {
          int timeout;
          timeout = CTIMEOUT + (gdata.serverconnectbackoff * CBKTIMEOUT);
          
          if (gdata.lastservercontact + timeout < gdata.curtime)
            {
              if (gdata.debug > 0)
                {
                  ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"Reconnecting to server (%d seconds)",timeout);
                }
              switchserver(-1);
            }
        }
      
      if (gdata.needsrehash) {
         gdata.needsrehash = 0;
         urehash = mycalloc(sizeof(userinput));
         u_fillwith_msg(urehash,NULL,"A A A A A rehash");
         urehash->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
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
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_CYAN,">IRC>: %s",line);
   
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
   if (part3 && gdata.caps_nick && !strcmp(caps(part2),"NOTICE") && !strcmp(caps(part3),gdata.caps_nick))
     {
       /* nickserv */
       if (gdata.nickserv_pass)
         {
           if (strstr(line, "please choose a different nick.") != NULL)
             {
               privmsg("nickserv","IDENTIFY %s",gdata.nickserv_pass);
               ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"nickserv identify send.");
             }
         }
       privmsgparse("NOTICE",line);
     }
 
 /* :server 001  xxxx :welcome.... */
   if ( !strcmp(part2,"001") )
     {
       ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Server welcome: %s",line);
       mylog(CALLTYPE_NORMAL,"Server welcome: %s",line);
       if (gdata.getipfromserver)
         {
           tptr = strchr(line, '@');
           if (tptr != NULL)
             {
             ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"IP From Server: %s",tptr+1);
	     update_natip(tptr+1);
             }
         }
       
       /* update server name */
       mydelete(gdata.curserveractualname);
       gdata.curserveractualname = getpart(line+1,1);
       
       /* update nick */
       mydelete(gdata.user_nick);
       mydelete(gdata.caps_nick);
       gdata.user_nick = mymalloc(strlen(part3)+1);
       gdata.caps_nick = mymalloc(strlen(part3)+1);
       strcpy(gdata.user_nick,part3);
       strcpy(gdata.caps_nick,part3);
       caps(gdata.caps_nick);
       gdata.nick_number = 0;
       gdata.needsclear = 1;
       
       if (gdata.user_modes && strlen(gdata.user_modes))
         {
           writeserver(WRITESERVER_NOW, "MODE %s %s",
                       gdata.user_nick, gdata.user_modes);
         }
       
       /* server connected raw command */
       tptr = irlist_get_head(&gdata.server_connected_raw);
       while(tptr)
         {
           writeserver(WRITESERVER_NORMAL, "%s", tptr);
           tptr = irlist_get_next(tptr);
         }
       
       /* nickserv */
       if (gdata.nickserv_pass)
         {
           privmsg("nickserv","IDENTIFY %s",gdata.nickserv_pass);
         }
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
               memset(&gdata.prefixes, 0, sizeof(gdata.prefixes));
               for (pi = 0; (ptr[pi] && (ptr[pi] != ')') && (pi < MAX_PREFIX)); pi++)
                 {
                   gdata.prefixes[pi].p_mode = ptr[pi];
                 }
               if (ptr[pi] == ')')
                 {
                   ptr += pi + 1;
                   for (pi = 0; (ptr[pi] && (pi < MAX_PREFIX)); pi++)
                     {
                       gdata.prefixes[pi].p_symbol = ptr[pi];
                     }
                 }
               for (pi = 0; pi < MAX_PREFIX; pi++)
                 {
                   if ((gdata.prefixes[pi].p_mode && !gdata.prefixes[pi].p_symbol) ||
                       (!gdata.prefixes[pi].p_mode && gdata.prefixes[pi].p_symbol))
                     {
                       outerror(OUTERROR_TYPE_WARN,
                                "Server prefix list doesn't make sense, using defaults: %s",
                                item);
                       initprefixes();
                     }
                 }
             }
           
           if (!strncmp("CHANMODES=", item, 10))
             {
               char *ptr = item+10;
               int ci;
               int cm;
               memset(&gdata.chanmodes, 0, sizeof(gdata.chanmodes));
               for (ci = cm = 0; (ptr[ci] && (cm < MAX_CHANMODES)); ci++)
                 {
                   if (ptr[ci+1] == ',')
                     {
                       /* we only care about ones with arguments */
                       gdata.chanmodes[cm++] = ptr[ci++];
                     }
                 }
             }
           
           mydelete(item);
         }
     }

 /* :server 433 old new :Nickname is already in use. */
   if ( !strcmp(part2,"433") && part3 && !strcmp(part3,"*") && part4 )
     {
       ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR,
               "Nickname %s already in use, trying %s%d",
               part4,
               gdata.config_nick,
               gdata.nick_number);
       
       /* generate new nick and retry */
       writeserver(WRITESERVER_NORMAL, "NICK %s%d",
                   gdata.config_nick,
                   gdata.nick_number++);
     }

   /* names list for a channel */
   /* :server 353 our_nick = #channel :nick @nick +nick nick */
   if ( !strcmp(part2,"353") && part3 && part4 && part5 )
     {
       caps(part5);
       
       ch = irlist_get_head(&gdata.channels);
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
           ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                   "Got name data for %s which is not a known channel!",part5);
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
         gdata.recentsent = 0;
      else {
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_RED,"Server Closed Connection: %s",line);
         
          FD_CLR(gdata.ircserver, &gdata.readset);
          /*
           * cygwin close() is broke, if outstanding data is present
           * it will block until the TCP connection is dead, sometimes
           * upto 10-20 minutes, calling shutdown() first seems to help
           */
          shutdown(gdata.ircserver, SHUT_RDWR);
          close(gdata.ircserver);
          gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
         }
      }
   
 /* server ping */
   if (PING_SRVR && (strncmp(line, "PING :", 6) == 0)) {
      if (gdata.debug > 0)
         ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Server Ping: %s",line);
      writeserver(WRITESERVER_NOW, "PO%s", line+2);
      }

 /* JOIN */
   if (!strcmp(part2,"JOIN") && part3a && gdata.caps_nick) {
      char* nick;
      int j,found;
      nick = mycalloc(strlen(line)+1);
      j=1;
      gdata.nocon = 0;
      found = 0;
      while(line[j] != '!' && j<sstrlen(line)) {
         nick[j-1] = line[j];
         j++;
         }
      nick[j-1]='\0';
      if (!strcmp(caps(nick),gdata.caps_nick))
        {
          /* we joined */
          /* clear now, we have succesfully logged in */
          gdata.serverconnectbackoff = 0;
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Joined %s",caps(part3a));
          
          ch = irlist_get_head(&gdata.channels);
          while(ch)
            {
              if (!strcmp(part3a,ch->name))
                {
                  ch->flags |= CHAN_ONCHAN;
                  break;
                }
              ch = irlist_get_next(ch);
            }
          

          if (!ch)
            {
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"%s is not a known channel!",part3a);
            }

          /* we have joined all channels => restart transfers if needed */
          if (has_joined_channels(1) > 0)
            {
              for (i=0; i<100; i++)
                {
                  if (!gdata.exiting &&
                      irlist_size(&gdata.mainqueue) &&
                      (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
                    {
                      sendaqueue(0);
                    }
                }
            }
        }
      else
	{
	  /* someone else joined */
	  caps(part3a);
          ch = irlist_get_head(&gdata.channels);
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
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"%s is not a known channel!",part3a);
            }
	  else
            {
              addtomemberlist(ch,nick);
            }
	}
	
      mydelete(nick);
      }

 /* PART */
   if (!strcmp(part2,"PART") && part3a && gdata.caps_nick)
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
       
       if (!strcmp(caps(nick),gdata.caps_nick))
	 {
	   /* we left? */
	   ;
	 }
       else
	 {
	   /* someone else left */
	   caps(part3a);
           ch = irlist_get_head(&gdata.channels);
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
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"%s is not a known channel!",part3a);
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
   if (!strcmp(part2,"QUIT") && gdata.caps_nick)
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
       
       if (!strcmp(caps(nick),gdata.caps_nick))
	 {
	   /* we quit? */
	   ;
	 }
       else
	 {
	   /* someone else quit */
           ch = irlist_get_head(&gdata.channels);
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
       
       if (gdata.caps_nick && !strcmp(caps(oldnick),gdata.caps_nick))
	 {
           /* nickserv */
           if (gdata.nickserv_pass)
             {
               privmsg("nickserv","IDENTIFY %s",gdata.nickserv_pass);
             }
           
           /* we changed, update nick */
           mydelete(gdata.user_nick);
           mydelete(gdata.caps_nick);
           gdata.user_nick = mymalloc(strlen(part3a)+1);
           gdata.caps_nick = mymalloc(strlen(part3a)+1);
           strcpy(gdata.user_nick,part3a);
           strcpy(gdata.caps_nick,part3a);
           caps(gdata.caps_nick);
           gdata.nick_number = 0;
           gdata.needsclear = 1;
         }
       
       ch = irlist_get_head(&gdata.channels);
       while(ch)
         {
           changeinmemberlist_nick(ch, oldnick, newnick);
           ch = irlist_get_next(ch);
         }
       
       user_changed_nick(oldnick, newnick);
       
       mydelete(oldnick);
     }
   
 /* KICK */
   if (!strcmp(part2,"KICK") && part3a && part4 && gdata.caps_nick)
     {
       ch = irlist_get_head(&gdata.channels);
       while(ch)
         {
           if (!strcmp(caps(part3a),ch->name))
             {
               if(!strcmp(caps(part4),gdata.caps_nick))
                 {
                   /* we were kicked */
                   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Kicked, Rejoining: %s",line);
                   joinchannel(ch);
                   ch->flags &= ~CHAN_ONCHAN;
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
       for (ch = irlist_get_head(&gdata.channels); ch; ch = irlist_get_next(ch))
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
                   for (ii = 0; (ii < MAX_PREFIX && gdata.prefixes[ii].p_mode); ii++)
                     {
                       if (*ptr == gdata.prefixes[ii].p_mode)
                         {
                           /* found a nick mode */
                           char *nick = getpart(line, part++);
                           if (nick)
                             {
                               if (nick[strlen(nick)-1] == '\1')
                                 {
                                   nick[strlen(nick)-1] = '\0';
                                 }
                               changeinmemberlist_mode(ch, nick,
                                                       gdata.prefixes[ii].p_symbol,
                                                       plus);
                               mydelete(nick);
                             }
                           break;
                         }
                     }
                   for (ii = 0; (ii < MAX_CHANMODES && gdata.chanmodes[ii]); ii++)
                     {
                       if (*ptr == gdata.chanmodes[ii])
                         {
                           /* found a channel mode that has an argument */
                           part++;
                           break;
                         }
                     }
                 }
             }
         }
     }

 /* PRIVMSG */
   if (!strcmp(part2,"PRIVMSG"))
     {
       if (gdata.autosend.word && part4 && !strcmp(caps(part4+1),caps(gdata.autosend.word)))
         {
           autosendf(line);
         }
       else
         {
           privmsgparse("PRIVMSG",line);
         }
     }
   
   mydelete(part2);
   mydelete(part3);
   mydelete(part4);
   mydelete(part5);
   }


static void privmsgparse(const char* type, char* line) {
   char *nick, *hostname, *hostmask, *wildhost;
   char *msg1, *msg2, *msg3, *msg4, *msg5, *msg6, *dest;
   int i,j,k;
   userinput ui;
   igninfo *ignore = NULL;
   upload *ul;
   transfer *tr;
   pqueue *pq;
   xdcc *xd;
   int line_len;
   
   updatecontext();

   floodchk();
   
   line_len = sstrlen(line);
   
   hostmask = caps(getpart(line,1));
   for (i=1; i<=sstrlen(hostmask); i++)
      hostmask[i-1] = hostmask[i];
   
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
   wildhost = mycalloc(line_len+2);
   
   
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
   
   snprintf(wildhost,line_len+2,"*!%s",hostmask + strlen(nick) + 1);
   
   if (isthisforme(dest, msg1))
     {
       
       if (verifyhost(&gdata.autoignore_exclude, hostmask))
         {
           /* host matches autoignore_exclude */
           goto noignore;
         }

       /* add/increment ignore list */
       ignore = irlist_get_head(&gdata.ignorelist);
       
       while(ignore)
         {
           if ((ignore->regexp != NULL) &&
               !regexec(ignore->regexp,hostmask,0,NULL,0))
             {
               /* already in list */
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
               break;
             }
           ignore = irlist_get_next(ignore);
         }
       
       if (!ignore)
         {
           char *tempr;
           
           /* not in list */
           ignore = irlist_add(&gdata.ignorelist,sizeof(igninfo));
           ignore->regexp = mycalloc(sizeof(regex_t));
           
           ignore->hostmask = mymalloc(strlen(wildhost)+1);
           strcpy(ignore->hostmask,wildhost);
           
           tempr = hostmasktoregex(wildhost);
           if (regcomp(ignore->regexp,tempr,REG_ICASE|REG_NOSUB))
             {
               ignore->regexp = NULL;
             }
           
           ignore->bucket = 1;
           ignore->flags &= ~IGN_MANUAL & ~IGN_IGNORING;
           ignore->lastcontact = gdata.curtime;
           
           mydelete(tempr);
         }
      }
 noignore:
   
   /*----- CLIENTINFO ----- */
   if ( !gdata.ignore && (!strcmp(msg1,"\1CLIENTINFO")
          || !strcmp(msg1,"\1CLIENTINFO\1") )) {
      gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
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
      
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"[CTCP] %s: CLIENTINFO",nick);
      }
   
   /*----- PING ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1PING")
          || !strcmp(msg1,"\1PING\1") )) {
      gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
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
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"[CTCP] %s: PING",nick);
      }
   
   /*----- VERSION ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1VERSION")
          || !strcmp(msg1,"\1VERSION\1") )) {
      gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
      notice(nick,"\1VERSION iroffer v" VERSIONLONG ", http://iroffer.dinoex.net/%s%s\1",
             gdata.hideos ? "" : " - ",
             gdata.hideos ? "" : gdata.osstring);
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"[CTCP] %s: VERSION",nick);
      }
   
   /*----- UPTIME ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1UPTIME")
          || !strcmp(msg1,"\1UPTIME\1") ))
     {
       char *tempstr2 = mycalloc(maxtextlength);
       gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
       tempstr2 = getuptime(tempstr2, 0, gdata.startuptime, maxtextlength);
       notice(nick,"\1UPTIME %s\1", tempstr2);
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"[CTCP] %s: UPTIME",nick);
       mydelete(tempstr2);
     }
   
   /*----- STATUS ----- */
   else if ( !gdata.ignore && (!strcmp(msg1,"\1STATUS")
          || !strcmp(msg1,"\1STATUS\1") ))
     {
       char *tempstr2 = mycalloc(maxtextlength);
       gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
       tempstr2 = getstatuslinenums(tempstr2,maxtextlength);
       notice(nick,"\1%s\1",tempstr2);
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"[CTCP] %s: STATUS",nick);
       mydelete(tempstr2);
     }
   
   /*----- DCC SEND/CHAT/RESUME ----- */
   else if ( !gdata.ignore && gdata.caps_nick && !strcmp(gdata.caps_nick,dest) && !strcmp(caps(msg1),"\1DCC") && msg2) {
      if (!gdata.attop) gototop();
      if (!strcmp(caps(msg2),"RESUME") && msg3 && msg4 && msg5)
        {
          gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
          
          caps(nick);
          
          if (msg5[strlen(msg5)-1] == '\1') msg5[strlen(msg5)-1] = '\0';
          
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ((tr->tr_status == TRANSFER_STATUS_LISTENING) && 
                  !strcmp(tr->caps_nick,nick) &&
                  (strstrnocase(tr->xpack->file,msg3) || (tr->listenport == atoi(msg4))))
                {
                  if (atoull(msg5) >= (unsigned long long)tr->xpack->st_size)
                    {
                      notice(nick,"You can't resume the transfer at a point greater than the size of the file");
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                              "XDCC [%02i:%s]: Resume attempted beyond end of file ( %" LLPRINTFMT "u >= %" LLPRINTFMT "u )",
                              tr->id, tr->nick, atoull(msg5),
                              (unsigned long long)tr->xpack->st_size);
                    }
                  else
                    {
                      t_setresume(tr,msg5);
                      privmsg_fast(nick,"\1DCC ACCEPT %s %s %s\1",msg3,msg4,msg5);
                      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                              "XDCC [%02i:%s]: Resumed at %" LLPRINTFMT "iK", tr->id,
                              tr->nick, (long long)(tr->startresume / 1024));
                    }
                  break;
                }
              tr = irlist_get_next(tr);
            }
          
          if (!tr)
            {
              outerror(OUTERROR_TYPE_WARN,"Couldn't find transfer that %s tried to resume!",nick);
              tr = irlist_get_head(&gdata.trans);
              while(tr)
                {
                  if (gdata.debug > 0)
                    {
                      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,
                              "resume trying %i: %s == %s, %s == %s, %i == %i\n",
                              tr->tr_status,
                              tr->caps_nick,nick,
                              tr->xpack->file,msg3,
                              tr->listenport,atoi(msg4));
                    }
                  tr = irlist_get_next(tr);
                }
            }
        }
      else if (!strcmp(caps(msg2), "CHAT"))
        {
          if ( verifyhost(&gdata.adminhost, hostmask) )
            {
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC CHAT attempt authorized from %s", hostmask);
              setupdccchat(nick, line);
            }
          else
           {
             notice(nick,"DCC Chat denied from %s",hostmask);
             ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                     "DCC CHAT attempt denied from %s", hostmask);
           }
        }
      
      else if (!strcmp(caps(msg2), "SEND") && msg3 && msg4 && msg5 && msg6)
        {
          if (msg6[strlen(msg6)-1] == '\1')
            {
              msg6[strlen(msg6)-1] = '\0';
            }
          if ( !verifyhost(&gdata.uploadhost, hostmask) )
            {
              notice(nick,"DCC Send Denied, I don't accept transfers from %s", hostmask);
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied from %s",hostmask);
            }
          else if ( gdata.uploadmaxsize && atoull(msg6) > gdata.uploadmaxsize)
            {
              notice(nick,"DCC Send Denied, I don't accept transfers that big");
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied (Too Big) from %s",hostmask);
            }
          else if ( atoull(msg6) > gdata.max_file_size)
            {
              notice(nick,"DCC Send Denied, I can't accept transfers that large");
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied (Too Large) from %s",hostmask);
            }
          else if (irlist_size(&gdata.uploads) >= MAXUPLDS)
            {
              notice(nick,"DCC Send Denied, I'm already getting too many files");
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied (too many uploads) from %s",hostmask);
            }
          else
            {
              ul = irlist_add(&gdata.uploads, sizeof(upload));
              l_initvalues(ul);
              removenonprintablefile(msg3);
              ul->file = mymalloc(strlen(msg3)+1);
              strcpy(ul->file,msg3);
              ul->remoteip = atoul(msg4);
              ul->remoteport = atoi(msg5);
              ul->totalsize = (off_t)atoull(msg6);
              ul->nick = mymalloc(strlen(nick)+1);
              strcpy(ul->nick,nick);
              ul->hostname = mymalloc(strlen(hostname)+1);
              strcpy(ul->hostname,hostname);
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                      "DCC Send Accepted from %s: %s (%" LLPRINTFMT "iKB)", nick, ul->file,
                      (long long)(ul->totalsize / 1024));
              l_establishcon(ul);
            }
        }
      
      else if (!strcmp(caps(msg2), "ACCEPT") && msg3 && msg4 && msg5)
        {
          if (msg5[strlen(msg5)-1] == '\1')
            {
              msg5[strlen(msg5)-1] = '\0';
            }
          if ( !verifyhost(&gdata.uploadhost, hostmask) )
            {
              notice(nick,"DCC Send Denied, I don't accept transfers from %s", hostmask);
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
                      "DCC Send Denied from %s",hostmask);
            }
          else if ( gdata.uploadmaxsize && atoull(msg5) > gdata.uploadmaxsize)
            {
              notice(nick,"DCC Send Denied, I don't accept transfers that big");
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC Send denied from %s (too big)", hostmask);
            }
          else if ( atoull(msg5) > gdata.max_file_size)
            {
              notice(nick,"DCC Send Denied, I can't accept transfers that large");
              ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                      "DCC Send denied from %s (too large)", hostmask);
            }
          
          ul = irlist_get_head(&gdata.uploads);
          while (ul)
            {
              if ((ul->remoteport == atoi(msg4)) && !strcmp(ul->nick, nick))
                {
                  ul->resumed = 1;
                  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"DCC Send Resumed from %s: %s (%" LLPRINTFMT "i of %" LLPRINTFMT "iKB left)",
                          nick, ul->file,
                          (long long)((ul->totalsize - ul->resumesize) / 1024),
                          (long long)(ul->totalsize / 1024));
                  l_establishcon(ul);
                  break;
                }
              ul = irlist_get_next(ul);
            }
          
          if (!ul)
            {
              notice(nick, "DCC Resume Denied, unable to find transfer");
              outerror(OUTERROR_TYPE_WARN, "Couldn't find upload that %s tried to resume!", nick);
            }
        }
   }
   
   /*----- ADMIN ----- */
   else if ( !gdata.ignore && gdata.caps_nick && !strcmp(gdata.caps_nick,dest) && !strcmp(caps(msg1),"ADMIN") ) {
/*      msg2 = getpart(line,5); */
      if (!gdata.attop) gototop();
      
      if ( verifyhost(&gdata.adminhost, hostmask) ) {
         if ( verifypass(msg2) ) {
            if (line[line_len-1] == '\n')
              {
                line[line_len-1] = '\0';
                line_len--;
              }
            u_fillwith_msg(&ui,nick,line);
            u_parseit(&ui);
            
            /* admin commands shouldn't count against ignore */
            if (ignore)
              {
                ignore->bucket--;
              }
            }
         else {
            notice(nick,"ADMIN: Incorrect Password");
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"Incorrect ADMIN Password (%s)",hostmask);
            }
         }
      else {
         notice(nick,"ADMIN: %s is not allowed to issue admin commands",hostmask);
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"Incorrect ADMIN Hostname (%s)",hostmask);
         }
      }
   
   /*----- XDCC ----- */
   else if ( !gdata.ignore && gdata.caps_nick && (!strcmp(gdata.caps_nick,dest) || gdata.respondtochannelxdcc) && (!strcmp(caps(msg1),"XDCC") || !strcmp(msg1,"\1XDCC") || !strcmp(caps(msg1),"CDCC") || !strcmp(msg1,"\1CDCC") )) {
      gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
      
      caps(msg2);
      
      if (msg3 && msg3[strlen(msg3)-1] == '\1')
         msg3[strlen(msg3)-1] = '\0';
      
      if ( msg2 && ( !strcmp(msg2,"LIST") || !strcmp(msg2,"LIST\1"))) {
         if (!gdata.attop) gototop();
         
         if (gdata.restrictprivlist)
	   {
	     j = 2; /* deny */
	     if (gdata.restrictprivlistmsg)
               {
                 notice(nick,"XDCC LIST Denied. %s", gdata.restrictprivlistmsg);
               }
	     else
               {
                 notice(nick,"XDCC LIST Denied. Wait for the public list in the channel.");
               }
	   }
	 else if (gdata.restrictlist && (!isinmemberlist(nick)))
	   {
	     j = 2; /* deny */
	     if (gdata.need_voice != 0)
	        notice(nick,"XDCC LIST Denied. You must have voice or more on this channel to request a list");
             else
	        notice(nick,"XDCC LIST Denied. You must be on a known channel to request a list");
	   }
	 else
	   {
             char *user;
             user = irlist_get_head(&gdata.xlistqueue);
             
             while (user)
               {
                 if (!strcmp(user,nick))
                   {
                     break;
                   }
                 user = irlist_get_next(user);
               }
             
             if (!user)
               {
                 if (irlist_size(&gdata.xlistqueue) >= MAXXLQUE)
                   {
                     j = 2; /* deny */
                     notice_slow(nick,"XDCC LIST Denied. I'm rather busy at the moment, try again later");
                   }
                 else
                   {
                     if (msg3)
                       {
                         userinput *pubplist;
                         char *tempstr = mycalloc(maxtextlength);
                         
                         j = 3; /* msg3 */
                         /* detect xdcc list group xxx */
                         if ((msg4) && (strcmp(caps(msg3),"GROUP") == 0))
                           {
                             char *msg0;
                             msg0 = msg4;
                             msg4 = msg3;
                             msg3 = msg0;
                           }
                         if ((msg3) && (strcmp(caps(msg3),"ALL") == 0))
                           {
                             if (gdata.restrictprivlistfull)
                               {
                                 j = 2; /* deny */
                                 if (gdata.restrictprivlistmsg)
                                   {
                                     notice(nick,"XDCC LIST Denied. %s", gdata.restrictprivlistmsg);
                                   }
                                 else
                                   {
                                     notice(nick,"XDCC LIST Denied. Wait for the public list in the channel.");
                                   }
                               }
                             else
                               {
                                 snprintf(tempstr,maxtextlength-1,"A A A A A xdlfull %s",msg3);
                               }
                           }
                         else
                           {
                             snprintf(tempstr,maxtextlength-1,"A A A A A xdlgroup %s",msg3);
                           }
                         if ( j == 3 )
                           {
                             pubplist = mycalloc(sizeof(userinput));
                             u_fillwith_msg(pubplist,nick,tempstr);
                             pubplist->method = method_xdl_user_notice;
                             u_parseit(pubplist);
                             mydelete(pubplist);
                             mydelete(tempstr);
                           }
                       }
                     else
                       {
                         if (gdata.restrictprivlistmain)
                           {
                             j = 2; /* deny */
                             if (gdata.restrictprivlistmsg)
                               {
                                 notice(nick,"XDCC LIST Denied. %s", gdata.restrictprivlistmsg);
                               }
                             else
                               {
                                 notice(nick,"XDCC LIST Denied. Wait for the public list in the channel.");
                               }
                           }
                         else
                           {
                             user = irlist_add(&gdata.xlistqueue, strlen(nick) + 1);
                             strcpy(user,nick);
                           }
                       }
                   }
               }
	   }
	 
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC LIST %s: (%s)",
                 (j==1?"ignored":(j==2?"denied":(j==3?msg3:"queued"))),hostmask);
         
         }
      else if (gdata.caps_nick && !strcmp(gdata.caps_nick,dest))
	{
         
         if ( msg2 && msg3 && (!strcmp(msg2,"SEND") || !strcmp(msg2,"GET"))) {
         if (!gdata.attop) gototop();
         ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC SEND %s",msg3);
         sendxdccfile(nick, hostname, hostmask, packnumtonum(msg3), NULL, msg4);
         }
         else if ( msg2 && msg3 && (!strcmp(msg2,"INFO"))) {
         if (!gdata.attop) gototop();
         ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC INFO %s",msg3);
         sendxdccinfo(nick, hostname, hostmask, packnumtonum(msg3), NULL);
         }
         else if ( msg2 && !strcmp(msg2,"QUEUE")) {
         if (!gdata.attop) gototop();
         ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC QUEUE (%s)\n",hostmask);
         notifyqueued_nick(nick);
         }
         else if ( msg2 && !strcmp(msg2,"STOP")) {
         stoplist(nick);
         }
         else if ( msg2 && !strcmp(msg2,"CANCEL")) {
         if (!gdata.attop) gototop();
         /* stop transfers */
         for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
           {
             if (!strcmp(tr->nick,nick))
               {
                 if (tr->tr_status != TRANSFER_STATUS_DONE)
                   {
                     t_closeconn(tr,"Tranfer cancelled by user",0);
                   }
               }
           }
         ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC CANCEL (%s)\n",hostmask);
         }
	 else if ( msg2 && !strcmp(msg2,"REMOVE")) {
         if (!gdata.attop) gototop();
         k=0;
         
         pq = irlist_get_head(&gdata.mainqueue);
         while (pq)
           {
             if (!strcmp(pq->nick,nick))
               {
                 notice(nick,
                        "Removed you from the queue for \"%s\", you waited %li minute%s.",
                        pq->xpack->desc,
                        (long)(gdata.curtime-pq->queuedtime)/60,
                        ((gdata.curtime-pq->queuedtime)/60) != 1 ? "s" : "");
                 mydelete(pq->nick);
                 mydelete(pq->hostname);
                 pq = irlist_delete(&gdata.mainqueue, pq);
                 k=1;
               }
             else
               {
                 pq = irlist_get_next(pq);
               }
           }
         if (!k) notice(nick,"You Don't Appear To Be In A Queue");
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC REMOVE (%s) ",hostmask);
         
         }
      else if ( msg2 && !strcmp(msg2,"SEARCH") && msg3) {
         if (!gdata.attop) gototop();
         
         notice_slow(nick,"Searching for \"%s\"...",msg3);
         
         i = 1;
         k = 0;
         xd = irlist_get_head(&gdata.xdccs);
         while(xd)
           {
             if (strstrnocase(xd->file,msg3) ||
                 strstrnocase(xd->desc,msg3) ||
                 strstrnocase(xd->note,msg3))
               {
                 notice_slow(nick," - Pack #%i matches, \"%s\"",
                             i, xd->desc);
                 k++;
               }
             i++;
             xd = irlist_get_next(xd);
           }
         
         if (!k)
           {
             notice_slow(nick,"Sorry, nothing was found, try a XDCC LIST");
           }
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC SEARCH %s (%s)",msg3,hostmask);
         
         }
      }
   }
   
   /*----- !LIST ----- */
   else if ( !gdata.ignore && gdata.caps_nick && gdata.respondtochannellist && msg1 && !strcasecmp(caps(msg1),"!LIST") &&
             ( !msg2 || !strcmp(caps(msg2),gdata.caps_nick) ))
     {
      gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
      
      /* generate !list styled message */
      
      notice_slow(nick,
             "\2(\2XDCC\2)\2 Packs:\2(\2%d\2)\2 "
             "Trigger:\2(\2/msg %s xdcc list\2)\2 "
             "Sends:\2(\2%i/%i\2)\2 "
             "Queues:\2(\2%i/%i\2)\2 "
             "Record:\2(\2%1.1fKB/s\2)\2 "
             "%s%s%s\2=\2iroffer\2=\2",
             irlist_size(&gdata.xdccs),
             (gdata.user_nick ? gdata.user_nick : "??"),
             irlist_size(&gdata.trans),gdata.slotsmax,
             irlist_size(&gdata.mainqueue),gdata.queuesize,
             gdata.record,
             gdata.creditline ? "Note:\2(\2" : "",
             gdata.creditline ? gdata.creditline : "",
             gdata.creditline ? "\2)\2 " : "");
     }
   
   /* iroffer-lamm: @find */
   else if ( !gdata.ignore && gdata.caps_nick && gdata.atfind && msg2 && !strcasecmp(caps(msg1),"@FIND") )
     {
      gdata.inamnt[gdata.curtime % 10]++;
      for (i = k = 0; i < strlen(msg2); i++)
        if ((msg2[i] == '*') || (msg2[i] == '#') || (msg2[i] == '?'))
          k++;
      if ((strlen(msg2) - k) >= gdata.atfind) {
        char *atfindmatch = mycalloc(maxtextlength);
        snprintf(atfindmatch, maxtextlength - 2, "*%s*", msg2);
        k = noticeresults(nick, atfindmatch);
        if (k) {
          if (!gdata.attop)
            gototop();
          ioutput(CALLTYPE_NORMAL, OUT_S | OUT_L | OUT_D, COLOR_YELLOW, "@FIND %s (%s) - %i pack%s found.", msg2, hostmask, k, k != 1 ? "s" : "");
        }
        mydelete(atfindmatch);
      }
     }
   
   else {
      if (dest && gdata.caps_nick && !strcmp(dest,gdata.caps_nick))
        {
          if ((gdata.lognotices && !strcmp(type,"NOTICE")) ||
              (gdata.logmessages && !strcmp(type,"PRIVMSG")))
            {
              msglog_t *ml;
              char *begin;
              
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_GREEN,"%s from %s logged, use MSGREAD to display it.",type,nick);
              
              ml = irlist_add(&gdata.msglog, sizeof(msglog_t));
              
              begin = line + 5 + strlen(hostmask) + strlen(type) + strlen(dest);
              
              ml->when = gdata.curtime;
              ml->hostmask = mymalloc(strlen(hostmask)+1);
              strcpy(ml->hostmask, hostmask);
              ml->message = mymalloc(strlen(begin)+1);
              strcpy(ml->message, begin);
              
              write_statefile();
            }
          else
            {
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_GREEN,"%s: %s",type,line);
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

static void autosendf(char* line) {
   char *nick, *hostname, *hostmask;
   int i,j;
   
   updatecontext();

   floodchk();
   
   nick = mycalloc(maxtextlengthshort);
   hostname = mycalloc(maxtextlength);
      
   hostmask = caps(getpart(line,1));
   for (i=1; i<=sstrlen(hostmask); i++)
      hostmask[i-1] = hostmask[i];
   
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
   
   if ( !gdata.ignore )
     {
       char *tempstr;
       const char *format = " :** Sending You %s by DCC";
       
       gdata.inamnt[gdata.curtime%INAMNT_SIZE]++;
       
       if (!gdata.attop) gototop();
       
       ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"AutoSend ");
       
       tempstr = mycalloc(strlen(gdata.autosend.message) + strlen(format) - 1);
       snprintf(tempstr, strlen(gdata.autosend.message) + strlen(format) - 1,
                       format, gdata.autosend.message);
       
       sendxdccfile(nick, hostname, hostmask, gdata.autosend.pack, tempstr, NULL);
       
       mydelete(tempstr);
     }
   
   mydelete(nick);
   mydelete(hostname);

   }

static int check_lock(const char* lockstr, const char* pwd)
{
  if (lockstr == NULL)
    return 0; /* no lock */
  if (pwd == NULL)
    return 1; /* locked */
  return strcmp(lockstr, pwd);
}

void sendxdccfile(const char* nick, const char* hostname, const char* hostmask, int pack, const char* msg, const char* pwd)
{
  int usertrans, userpackok, man;
  xdcc *xd;
  transfer *tr;
  
  updatecontext();
  
  usertrans = 0;
  userpackok = 1;
  
  if (!strcmp(hostname,"man"))
    {
      man = 1;
    }
  else
    {
      man = 0;
    }
  
  if (!man && (!verifyhost(&gdata.downloadhost, hostmask)) )
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (host denied): ");
      notice(nick,"** XDCC SEND denied, I don't send transfers to %s", hostmask);
      goto done;
    }
  else if (!man && gdata.restrictsend && !isinmemberlist(nick))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (restricted): ");
      if (gdata.need_voice != 0)
        notice(nick,"** XDCC SEND denied, you must have voice on a known channel to request a pack");
      else
        notice(nick,"** XDCC SEND denied, you must be on a known channel to request a pack");
      goto done;
    }
  else if (!man && gdata.enable_nick && !isinmemberlist(gdata.enable_nick))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (offline): ");
      notice(nick,"** XDCC SEND denied, owner of this bot is not online");
      goto done;
    }
  else if ((pack > irlist_size(&gdata.xdccs)) || (pack < 1))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Bad Pack Number): ");
      notice(nick,"** Invalid Pack Number, Try Again");
      goto done;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, pack-1);
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
      notice(nick,"** The Owner Has Requested That No New Connections Are Made In The Next %li Minute%s",(gdata.nonewcons-gdata.curtime+1)/60,((gdata.nonewcons-gdata.curtime+1)/60)!=1?"s":"");
      }   
   else if (!man && gdata.transferlimits_over)
     {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Over Transfer Limit): ");
      notice(nick,"** Sorry, I have exceeded my transfer limit for today.  Try again tomorrow.");
     }
   else if (!man && (xd->dlimit_max != 0) && (xd->gets >= xd->dlimit_used))
     {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Over Pack Transfer Limit): ");
      notice(nick,"** Sorry, This Pack is over download limit for today.  Try again tomorrow.");
      if (xd->dlimit_desc != NULL)
        notice(nick,xd->dlimit_desc);
     }
   /* if maxtransfersperperson is reached, queue the file, unless queues are used up, which is checked in addtoqueue() */
   else if ( !man && usertrans >= gdata.maxtransfersperperson)
     {
       char *tempstr;
       tempstr = addtoqueue(nick, hostname, pack);
       notice(nick, "** You can only have %d transfer%s at a time, %s",
              gdata.maxtransfersperperson,
              gdata.maxtransfersperperson!=1?"s":"",
              tempstr);
       mydelete(tempstr);
     }
   else if ((irlist_size(&gdata.trans) >= MAXTRANS) || (gdata.holdqueue) ||
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
       char *sendnamestr;
       
       look_for_file_changes(xd);
       
       xd->file_fd_count++;
       tr = irlist_add(&gdata.trans, sizeof(transfer));
       t_initvalues(tr);
       tr->id = get_next_tr_id();
       tr->nick = mymalloc(strlen(nick)+1);
       strcpy(tr->nick,nick);
       tr->caps_nick = mymalloc(strlen(nick)+1);
       strcpy(tr->caps_nick,nick);
       caps(tr->caps_nick);
       tr->hostname = mymalloc(strlen(hostname)+1);
       strcpy(tr->hostname,hostname);
       
       tr->xpack = xd;
       
       if (!man)
         {
           ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," requested: ");
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
       
       t_setuplisten(tr);
       
       if (tr->tr_status == TRANSFER_STATUS_LISTENING)
         {
           sendnamestr = getsendname(tr->xpack->file);
           
           privmsg_fast(nick,"\1DCC SEND %s %lu %i %" LLPRINTFMT "u\1",
                        sendnamestr,
                        gdata.ourip,
                        tr->listenport,
                        (unsigned long long)tr->xpack->st_size);
           
           mydelete(sendnamestr);
         }
     }
   
 done:
   
   if (!man)
      ioutput(CALLTYPE_MULTI_END,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"%s (%s)",nick,hostname);

   }
   
void sendxdccinfo(const char* nick,
                  const char* hostname,
                  const char* hostmask,
                  int pack,
                  const char* msg)
{
  xdcc *xd;
  char *sizestrstr;
  char *sendnamestr;
  char tempstr[maxtextlengthshort];
  
  updatecontext();
  
  if (gdata.disablexdccinfo)
    {
      notice(nick,"** XDCC INFO denied, disabled by configuration");
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," ignored: ");
      goto done;
    }
  
  if (!verifyhost(&gdata.downloadhost, hostmask))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (host denied): ");
      notice(nick,"** XDCC INFO denied, I don't send transfers to %s", hostmask);
      goto done;
    }
  else if (gdata.restrictsend && !isinmemberlist(nick))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (restricted): ");
      if (gdata.need_voice != 0)
        notice(nick,"** XDCC INFO denied, you must have voice on a known channel to request a pack");
      else
        notice(nick,"** XDCC INFO denied, you must be on a known channel to request a pack");
      goto done;
    }
  else if ((pack > irlist_size(&gdata.xdccs)) || (pack < 1))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (Bad Pack Number): ");
      notice(nick,"** Invalid Pack Number, Try Again");
      goto done;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, pack-1);
  
  if ((gdata.hidelockedpacks != 0) && (xd->lock != NULL))
    {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (locked pack): ");
      notice(nick,"** Invalid Pack Number, Try Again");
      goto done;
    }
  
  ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," requested: ");
  
  notice_slow(nick, "Pack Info for Pack #%i:", pack);
  
  sendnamestr = getsendname(xd->file);
  notice_slow(nick, " Filename       %s", sendnamestr);
  
  if (strcmp(sendnamestr, xd->desc))
    {
      notice_slow(nick, " Description    %s", xd->desc);
    }
  mydelete(sendnamestr);
  
  if (xd->note[0])
    {
      notice_slow(nick, " Note           %s", xd->note);
    }
  
  sizestrstr = sizestr(1, xd->st_size);
  notice_slow(nick, " Filesize       %" LLPRINTFMT "i [%sB]",
            (long long)xd->st_size, sizestrstr);
  mydelete(sizestrstr);
  
  getdatestr(tempstr, xd->mtime, maxtextlengthshort);
  notice_slow(nick, " Last Modified  %s", tempstr);
  
  notice_slow(nick, " Gets           %d", xd->gets);
  if (xd->minspeed)
    {
      notice_slow(nick, " Minspeed       %1.1fKB/sec", xd->minspeed);
    }
  if (xd->maxspeed)
    {
      notice_slow(nick, " Maxspeed       %1.1fKB/sec", xd->maxspeed);
    }
  
  if (xd->has_md5sum)
    {
      notice_slow(nick, " md5sum         " MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
    }
  
 done:
  
  ioutput(CALLTYPE_MULTI_END,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"%s (%s)",nick,hostname);
  
  return;
}
   
char* addtoqueue(const char* nick, const char* hostname, int pack)
{
   char *tempstr = mycalloc(maxtextlength);
   pqueue *tempq;
   xdcc *tempx;
   int inq,alreadytrans;
   int man;
   pqueue *pq;
   
   updatecontext();

   tempx = irlist_get_nth(&gdata.xdccs, pack-1);
   
   alreadytrans = inq = 0;
   pq = irlist_get_head(&gdata.mainqueue);
   while(pq)
     {
       if (!strcmp(pq->hostname,hostname) || !strcasecmp(pq->nick,nick))
         {
           inq++;
           if (pq->xpack == tempx)
             {
               alreadytrans++;
             }
         }
       pq = irlist_get_next(pq);
     }
   
   if (alreadytrans) {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (queue/dup): ");
      snprintf(tempstr, maxtextlength,
               "Denied, You already have that item queued.");
      return tempstr;
      }
    
   if (!strcmp(hostname,"man"))
      {
        man = 1;
      }
    else
      {
        man = 0;
      }
   
   if (!man && (inq >= gdata.maxqueueditemsperperson)) {
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (user/queue): ");
      snprintf(tempstr, maxtextlength,
               "Denied, You already have %i items queued, Try Again Later",
               inq);
      return tempstr;
      }
   
      if (!man && (irlist_size(&gdata.mainqueue) >= gdata.queuesize)) {
         ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (slot/queue): ");
         snprintf(tempstr, maxtextlength,
                  "Main queue of size %d is Full, Try Again Later",
                  gdata.queuesize);
         }
      else {
         ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Queued (slot): ");
         tempq = irlist_add(&gdata.mainqueue, sizeof(pqueue));
         tempq->queuedtime = gdata.curtime;
         tempq->nick = mymalloc(strlen(nick)+1);
         strcpy(tempq->nick,nick);
         tempq->hostname = mymalloc(strlen(hostname)+1);
         strcpy(tempq->hostname,hostname);
         tempq->xpack = tempx;

         snprintf(tempstr,maxtextlength,
                  "Added you to the main queue in position %d. To Remove youself at "
                  "a later time type \"/msg %s xdcc remove\".",
                  irlist_size(&gdata.mainqueue),
                  (gdata.user_nick ? gdata.user_nick : "??"));
         }
   return tempstr;
   }

void sendaqueue(int type)
{
  int usertrans;
  pqueue *pq;
  transfer *tr;
  char *sendnamestr;
  
  updatecontext();
  
  if (gdata.serverstatus != SERVERSTATUS_CONNECTED)
     return;

  /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gdata.lastservercontact > 150)
     return;
  
  if (gdata.holdqueue)
     return;
  
  if (gdata.restrictlist && (has_joined_channels(1) == 0))
     return;
  
  if (!gdata.attop) gototop();
  
  if (irlist_size(&gdata.mainqueue))
    {
      
      /*
       * first determine what the first queue position is that is eligable
       * for a transfer find the first person who has not already maxed out
       * his sends if noone, do nothing and let execution continue to pack
       * queue check
       */
      
      pq = irlist_get_head(&gdata.mainqueue);
      if (type != 2)
        {
          while (pq)
            {
              usertrans=0;
              tr = irlist_get_head(&gdata.trans);
              while(tr)
                {
                  if ((!strcmp(tr->hostname,pq->hostname)) || (!strcasecmp(tr->nick,pq->nick)))
                    {
                      usertrans++;
                    }
                  tr = irlist_get_next(tr);
                }
              
              /* usertrans is the number of transfers a user has in progress */
              if (usertrans < gdata.maxtransfersperperson)
                {
                  break; /* found the person that will get the send */
                }
              pq = irlist_get_next(pq);
            }
        }
      
      if (!pq)
        {
          return;
        }
      
      if (type == 1)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
                  "QUEUED SEND (low bandwidth): %s (%s)",
                  pq->nick, pq->hostname);
        }
      else if (type == 2)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
                  "QUEUED SEND (manual): %s (%s)",
                  pq->nick, pq->hostname);
        }
      else
        {
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
                  "QUEUED SEND: %s (%s)",
                  pq->nick, pq->hostname);
        }
      
      look_for_file_changes(pq->xpack);
      
      pq->xpack->file_fd_count++;
      tr = irlist_add(&gdata.trans, sizeof(transfer));
      t_initvalues(tr);
      tr->id = get_next_tr_id();
      tr->nick = mymalloc(strlen(pq->nick)+1);
      strcpy(tr->nick,pq->nick);
      tr->caps_nick = mymalloc(strlen(pq->nick)+1);
      strcpy(tr->caps_nick,pq->nick);
      caps(tr->caps_nick);
      tr->hostname = mymalloc(strlen(pq->hostname)+1);
      strcpy(tr->hostname,pq->hostname);
      
      tr->xpack = pq->xpack;
      
      if (!gdata.quietmode)
        {
          char *sizestrstr;
          sizestrstr = sizestr(0, tr->xpack->st_size);
          notice_fast(pq->nick,
                 "** Sending You Your Queued Pack Which Is %sB. (Resume Supported)",
                 sizestrstr);
          mydelete(sizestrstr);
        }
      
      t_setuplisten(tr);
      
      sendnamestr = getsendname(tr->xpack->file);
      
      privmsg_fast(pq->nick,"\1DCC SEND %s %lu %i %" LLPRINTFMT "u\1",
              sendnamestr,
              gdata.ourip,
              tr->listenport,
              (unsigned long long)tr->xpack->st_size);
      
      mydelete(sendnamestr);
      
      mydelete(pq->nick);
      mydelete(pq->hostname);
      irlist_delete(&gdata.mainqueue, pq);
      
      return;
    }
}

/* End of File */







   
