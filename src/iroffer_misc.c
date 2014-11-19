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
 * @(#) iroffer_misc.c 1.251@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_misc.c|20051123201144|15577
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_kqueue.h"
#include "dinoex_config.h"
#include "dinoex_ssl.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"
#include "dinoex_jobs.h"
#include "dinoex_http.h"
#include "dinoex_main.h"
#include "dinoex_misc.h"


#include <locale.h>

void getconfig (void) {
   
   updatecontext();
   
   a_read_config_files(NULL);
   
   printf("** Checking for completeness of config file ...\n");
   
   if ( !irlist_size(&gdata.networks[0].servers) )
      outerror(OUTERROR_TYPE_CRASH, "Missing vital information: %s",
               "server"); /* NOTRANSLATE */

   if ( gdata.config_nick == NULL )
      outerror(OUTERROR_TYPE_CRASH, "Missing vital information: %s",
               "user_nick"); /* NOTRANSLATE */

   if ( gdata.user_realname == NULL )
      outerror(OUTERROR_TYPE_CRASH, "Missing vital information: %s",
               "user_realname"); /* NOTRANSLATE */

   if ( gdata.user_modes == NULL )
      outerror(OUTERROR_TYPE_CRASH, "Missing vital information: %s",
               "user_modes"); /* NOTRANSLATE */

   if ( gdata.slotsmax == 0 )
      outerror(OUTERROR_TYPE_CRASH, "Missing vital information: %s",
               "slotsmax"); /* NOTRANSLATE */

   if ( irlist_size(&gdata.uploadhost) && ( gdata.uploaddir == NULL || strlen(gdata.uploaddir) < 2U ) )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Upload Information");
      
   if ( !irlist_size(&gdata.downloadhost) )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Download Host Information");
      
   if ( !gdata.statefile )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing State File Information");
      
   if (gdata.background) gdata.debug = 0;
   
   if ( !gdata.background ) {
      gototop();
      }
   
   }



static int connectirc (server_t *tserver) {
   char *tempstr;
   char *vhost;
   int callval;
   int family;
   
   updatecontext();

   gnetwork->serverbucket = gnetwork->server_send_max;
   
   if (!tserver) return 1;
   
   gnetwork->lastservercontact=gdata.curtime;
   
   gnetwork->nocon++;
   
   mydelete(gnetwork->curserver.hostname);
   mydelete(gnetwork->curserver.password);
   mydelete(gnetwork->curserveractualname);
   gnetwork->curserver.hostname = mystrdup(tserver->hostname);
   gnetwork->curserver.port = tserver->port;
   if (tserver->password)
     {
       gnetwork->curserver.password = mystrdup(tserver->password);
     }
   
   tempstr = mymalloc(maxtextlength);
   
   switch (gnetwork->connectionmethod.how)
     {
     case how_direct:
       snprintf(tempstr, maxtextlength, " (direct)");
       gnetwork->serv_resolv.to_ip = gnetwork->curserver.hostname;
       gnetwork->serv_resolv.to_port = gnetwork->curserver.port;
       break;
       
     case how_ssl:
       snprintf(tempstr, maxtextlength, " (ssl)");
       gnetwork->serv_resolv.to_ip = gnetwork->curserver.hostname;
       gnetwork->serv_resolv.to_port = gnetwork->curserver.port;
       break;
       
     case how_bnc:
       if (gnetwork->connectionmethod.vhost)
         {
           snprintf(tempstr, maxtextlength,
                    " (bnc at %s:%i with %s)",
                    gnetwork->connectionmethod.host,
                    gnetwork->connectionmethod.port,
                    gnetwork->connectionmethod.vhost);
         }
       else
         {
           snprintf(tempstr, maxtextlength,
                    " (bnc at %s:%i)",
                    gnetwork->connectionmethod.host,
                    gnetwork->connectionmethod.port);
         }
       gnetwork->serv_resolv.to_ip = gnetwork->connectionmethod.host;
       gnetwork->serv_resolv.to_port = gnetwork->connectionmethod.port;
       break;
       
     case how_wingate:
       snprintf(tempstr, maxtextlength,
                " (wingate at %s:%i)",
                gnetwork->connectionmethod.host,
                gnetwork->connectionmethod.port);
       gnetwork->serv_resolv.to_ip = gnetwork->connectionmethod.host;
       gnetwork->serv_resolv.to_port = gnetwork->connectionmethod.port;
       break;
       
     case how_custom:
       snprintf(tempstr, maxtextlength,
                " (custom at %s:%i)",
                gnetwork->connectionmethod.host,
                gnetwork->connectionmethod.port);
       gnetwork->serv_resolv.to_ip = gnetwork->connectionmethod.host;
       gnetwork->serv_resolv.to_port = gnetwork->connectionmethod.port;
       break;
       
     default:
       mydelete(tempstr);
       return 1; /* error */
     }
      
   vhost = get_local_vhost();
   if (vhost)
     {
       ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
               "Attempting Connection to %s:%u from %s%s",
               gnetwork->curserver.hostname,
               gnetwork->curserver.port,
               vhost,
               tempstr);
       if (strchr(vhost, ':') == NULL) 
         family = AF_INET;
       else
         family = AF_INET6;
     }
   else
     {
       family = 0;
       ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Attempting Connection to %s:%u%s",
               gnetwork->curserver.hostname,
               gnetwork->curserver.port, tempstr);
     }
   
   mydelete(tempstr);
   
   if (gnetwork->serv_resolv.child_pid)
     {
       /* old resolv still outstanding, cleanup */
       event_close(gnetwork->serv_resolv.sp_fd[0]);
       gnetwork->serv_resolv.sp_fd[0] = 0;
       gnetwork->serv_resolv.child_pid = 0;
     }
   
   callval = socketpair(AF_UNIX, SOCK_DGRAM, 0, gnetwork->serv_resolv.sp_fd);
   if (callval < 0)
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,"socketpair(): %s", strerror(errno));
       return 1;
     }
   
#ifndef DEBUG_VALGRIND
   callval = fork();
   if (callval < 0)
#endif
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,"fork(): %s", strerror(errno));
       return 1;
     }
   if (callval == 0)
     {
       /* child */
       child_resolver(family);
       sleep(60);
       _exit(0);
     }
   
   /* parent */
   gnetwork->serv_resolv.child_pid = callval;
   event_close(gnetwork->serv_resolv.sp_fd[1]);
   gnetwork->serv_resolv.sp_fd[1] = 0;
   gnetwork->serv_resolv.next ++;
   
   gnetwork->serverstatus = SERVERSTATUS_RESOLVING;
   
   return 0;
}

void initirc(void)
{
  int i,j,k;
  channel_t *ch;
  char *pi;
  char *tptr;
  
  updatecontext();
  
  pi = irlist_get_head(&(gnetwork->proxyinfo));
  while((gnetwork->connectionmethod.how == how_custom) && pi)
    {
      size_t len;
      char *tempstr;
      int found_s = 0;
      int found_p = 0;
      char portstr[maxtextlengthshort];
      
      snprintf(portstr, maxtextlengthshort, "%d", gnetwork->curserver.port);
      
      len = strlen(pi) + strlen(gnetwork->curserver.hostname) + strlen(portstr);
      
      tempstr = mymalloc(len+1);
      
      for (i=j=0; pi[i]; i++,j++)
        {
          if (!found_s &&
              (pi[i] == '$') &&
              (pi[i+1] == 's'))
            {
              for (k=0, i++; gnetwork->curserver.hostname[k]; k++, j++)
                {
                  tempstr[j] = gnetwork->curserver.hostname[k];
                }
              j--;
              found_s=1;
            }
          else if (!found_p &&
                   (pi[i] == '$') &&
                   (pi[i+1] == 'p'))
            {
              for (k=0,i++; portstr[k]; k++,j++)
                {
                  tempstr[j] = portstr[k];
                }
              j--;
              found_p=1;
            }
          else
            {
              tempstr[j] = pi[i];
            }
        }
      tempstr[j] = '\0';
      writeserver(WRITESERVER_NOW, "%s", tempstr);
      
      mydelete(tempstr);
      
      pi = irlist_get_next(pi);
    }
      
   if (gnetwork->connectionmethod.how == how_wingate) {
      writeserver(WRITESERVER_NOW, "%s %u",
                  gnetwork->curserver.hostname, gnetwork->curserver.port);
      }

   if (gnetwork->curserver.password)
     {
       writeserver(WRITESERVER_NOW, "PASS %s", gnetwork->curserver.password);
     }
   writeserver(WRITESERVER_NOW, "NICK %s", get_config_nick());
   writeserver(WRITESERVER_NOW, "USER %s 32 . :%s",
               gdata.loginname, gdata.user_realname);
   
   if (gnetwork->connectionmethod.how == how_bnc) {
      writeserver(WRITESERVER_NOW, "PASS %s",
                  gnetwork->connectionmethod.password);
      if (gnetwork->connectionmethod.vhost) {
         writeserver(WRITESERVER_NOW, "VIP %s",
                     gnetwork->connectionmethod.vhost);
         }
      writeserver(WRITESERVER_NOW, "CONN %s %d",
                  gnetwork->curserver.hostname, gnetwork->curserver.port);
      }
   
   /* server join raw command */
  tptr = irlist_get_head(&(gnetwork->server_join_raw));
  while(tptr)
    {
      writeserver(WRITESERVER_NORMAL, "%s", tptr);
      tptr = irlist_get_next(tptr);
    }
   
   ch = irlist_get_head(&(gnetwork->channels));
   while(ch)
     {
       ch->flags &= ~CHAN_ONCHAN;
       ch = irlist_get_next(ch);
     }
   
   gnetwork->recentsent = 0;
   
   }

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
writeserver (writeserver_type_e type, const char *format, ... )
{
  va_list args;
  va_start(args, format);
  vwriteserver(type, format, args);
  va_end(args);
}

void vwriteserver(writeserver_type_e type, const char *format, va_list ap)
{
  char *msg;
  int len;
  
  msg = mymalloc(maxtextlength+1);
  
  len = vsnprintf(msg,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"WRITESERVER: Output too large, ignoring!");
      mydelete(msg);
      return;
    }
  
  if ((type == WRITESERVER_NOW) &&
      (gnetwork->serverstatus == SERVERSTATUS_CONNECTED))
    {
      if (gdata.debug > 14)
        {
          ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<SND<: %u: %s", gnetwork->net +1, msg);
        }
      msg[len] = '\n';
      len++;
      msg[len] = '\0';
      writeserver_ssl(msg, len);
      if ( len > gnetwork->serverbucket )
        {
          gnetwork->serverbucket = 0;
        }
      else
        {
          gnetwork->serverbucket -= len;
        }
    }
  else if (gdata.exiting || (gnetwork->serverstatus != SERVERSTATUS_CONNECTED))
    {
      mydelete(msg);
      return;
    }
  else
    {
      if (type == WRITESERVER_FAST)
        {
          if (gdata.debug > 12)
            {
              ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<QUEF<: %s", msg);
            }
          
          if (len > gnetwork->server_send_max)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[gnetwork->server_send_max] = '\0';
              len = gnetwork->server_send_max;
            }
          
          if (irlist_size(&(gnetwork->serverq_fast)) < MAXSENDQ)
            {
              irlist_add_string(&(gnetwork->serverq_fast), msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else if (type == WRITESERVER_NORMAL)
        {
          if (gdata.debug > 12)
            {
              ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<QUEN<: %s", msg);
            }
          
          if (len > gnetwork->server_send_max)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[gnetwork->server_send_max] = '\0';
              len = gnetwork->server_send_max;
            }
          
          if (irlist_size(&(gnetwork->serverq_normal)) < MAXSENDQ)
            {
              irlist_add_string(&(gnetwork->serverq_normal), msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else if (type == WRITESERVER_SLOW)
        {
          if (gdata.debug > 12)
            {
              ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<QUES<: %s", msg);
            }
          
          if (len > gnetwork->server_send_max)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[gnetwork->server_send_max] = '\0';
              len = gnetwork->server_send_max;
            }
          
          if (irlist_size(&(gnetwork->serverq_slow)) < MAXSENDQ)
            {
              irlist_add_string(&(gnetwork->serverq_slow), msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else
        {
          outerror(OUTERROR_TYPE_CRASH, "Unknown type %u", type);
        }
    }
  
  mydelete(msg);
  return;
}

void sendserver(void)
{
  char *item;
  unsigned int clean;
  
  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
    {
      return;
    }
  
  if ( !gdata.background )
    gototop();
  
  gnetwork->lastsend = gdata.curtime + 1;
  sendannounce();
  gnetwork->serverbucket += gnetwork->server_send_rate;
  if (gnetwork->serverbucket > gnetwork->server_send_max)
    gnetwork->serverbucket = gnetwork->server_send_max;
  
  clean = (irlist_size(&(gnetwork->serverq_fast)) +
           irlist_size(&(gnetwork->serverq_normal)) +
           irlist_size(&(gnetwork->serverq_slow)));
  
  if (clean == 0)
    {
      if (gdata.exiting)
        {
          close_server();
          gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
          ioutput(OUT_S|OUT_D, COLOR_NO_COLOR,
                  "Connection to %s (%s) Closed",
                  gnetwork->curserver.hostname,
                  gnetwork->curserveractualname ? gnetwork->curserveractualname : "<unknown>");
        }
      return;
    }
  
  item = irlist_get_head(&(gnetwork->serverq_fast));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 14)
        {
          ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<IRC<: %u, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->recentsent = 0;
      gnetwork->serverbucket -= strlen(item);
      gnetwork->lastfast = gdata.curtime;
      
      item = irlist_delete(&(gnetwork->serverq_fast), item);
    }
  
  if (item)
    {
      return;
    }
  
  item = irlist_get_head(&(gnetwork->serverq_normal));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 14)
        {
          ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<IRC<: %u, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->recentsent = 0;
      gnetwork->serverbucket -= strlen(item);
      gnetwork->lastnormal = gdata.curtime;
      
      item = irlist_delete(&(gnetwork->serverq_normal), item);
    }
  
  if (item)
    {
      return;
    }
  
  if ((unsigned)gdata.curtime <= (gnetwork->lastslow + gnetwork->slow_privmsg))
    {
      return;
    }
  
  item = irlist_get_head(&(gnetwork->serverq_slow));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 14)
        {
          ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<IRC<: %u, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->recentsent = 0;
      gnetwork->serverbucket -= strlen(item);
      gnetwork->lastslow = gdata.curtime;
      
      item = irlist_delete(&(gnetwork->serverq_slow), item);
      if (gnetwork->slow_privmsg)
        break;
    }
  
  if (item)
    {
      return;
    }
  
  gnetwork->recentsent = 6;
  return;
}

void pingserver(void) {
   updatecontext();

   writeserver(WRITESERVER_NOW, "PING %s",
               gnetwork->curserveractualname ? gnetwork->curserveractualname : gnetwork->curserver.hostname);
   }

void xdccsavetext(void)
{
  char *xdcclistfile_tmp, *xdcclistfile_bkup;
  int fd;
  userinput *uxdl;
  gnetwork_t *backup;
  
  updatecontext();
  
  if (!gdata.xdcclistfile)
    {
      return;
    }
  
  xdcclistfile_tmp = mystrsuffix(gdata.xdcclistfile, ".tmp");
  xdcclistfile_bkup = mystrsuffix(gdata.xdcclistfile, "~");
  
  fd = open(xdcclistfile_tmp,
            O_WRONLY | O_CREAT | O_TRUNC | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  if (fd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Cant Create XDCC List File '%s': %s",
               xdcclistfile_tmp, strerror(errno));
      goto error_out;
    }
  
  uxdl = mycalloc(sizeof(userinput));
  
  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
  if (gdata.xdcclist_grouponly)
    a_fillwith_msg2(uxdl, NULL, "XDL");
  else
    a_fillwith_msg2(uxdl, NULL, "XDLFULL");
  uxdl->method = method_fd; 
  uxdl->fd = fd;
  uxdl->net = 0;
  
  u_parseit(uxdl);
  
  mydelete(uxdl);
  
  close(fd);
  gnetwork = backup;
  
  rename_with_backup(gdata.xdcclistfile, xdcclistfile_bkup, xdcclistfile_tmp, "XDCC List");
  
 error_out:
  mydelete(xdcclistfile_tmp);
  mydelete(xdcclistfile_bkup);
  return;
}
  
void writepidfile (const char *filename) {
   char *tempstr2 = mymalloc(maxtextlengthshort);
   int filedescriptor;
   
   updatecontext();

   ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Writing pid file...");
   
   filedescriptor=open(filename, O_WRONLY | O_TRUNC | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS);
   if (filedescriptor < 0) outerror(OUTERROR_TYPE_CRASH,"Cant Create PID File '%s': %s",filename,strerror(errno));
   
   snprintf(tempstr2, maxtextlengthshort, "%i\n", (int)getpid());
   write(filedescriptor,tempstr2,strlen(tempstr2));
   
   close(filedescriptor);
   
   mydelete(tempstr2);
   }

void gobackground(void) {
   int s,i;

   updatecontext();

   printf("\n** Entering Background Mode\n"
          "** All Commands must be issued by remote administration\n");
   fflush(stdout);
   fflush(stderr);
   
   /* parent forks */
   s = fork();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH, "Unable to Fork: %s", strerror(errno));
   else if (s > 0) {
      /* parent exits */
      exit(0);
      }

/*   struct rlimit r = { 0 }; */
/*   r.rlim_max = 0; */
/*   s = getrlimit(RLIMIT_NOFILE, &r); */
/*   if ( r.rlim_max < 1 || s < 0) */
/*      outerror(OUTERROR_TYPE_CRASH,"Couldn't get rlimit"); */
   
   for (i=0; i<3; i++) event_close(i);
/*   for (i=0; i< r.rlim_max; i++) close(i); */
   
   s = setsid();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH, "Couldn't setsid: %s", strerror(errno));
   
   /* parent forks */
   s = fork();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH, "Unable to Fork: %s", strerror(errno));
   else if (s > 0)
      /* parent exits */
      exit(0);
   
   
   /* background continues... */
   
/*   umask(0); */
   s = open("/dev/null", O_RDWR); /* stdin */
   dup(s);                        /* stdout */
   dup(s);                        /* stderr */
   
   mylog("Entered Background Mode");
   gdata.background = 2;
   
/*   execlp(program,"iroffer","--background-mode--",config,NULL); */
/*   exit(0); */
   
   }

#if !defined(NO_SIGINFO)
static void iroffer_signal_handler(int signo, siginfo_t *sinfo, void * UNUSED(unused))
#else
static void iroffer_signal_handler(int signo)
#endif
{
  switch (signo)
    {
    case SIGTERM:
    case SIGINT:
      if (gdata.needsshutdown)
        {
          /* must be stuck in a loop somewhere, force exit here */
          
          uninitscreen();
          
          mylog("iroffer exited (signal forced!)\n\n");
          printf("iroffer exited (signal forced!)\n");
          
          exit_iroffer(1);
        }
      else
        {
          gdata.needsshutdown++;
        }
      break;
      
    case SIGUSR1:
      gdata.needsswitch++;
      break;
      
    case SIGUSR2:
      gdata.needsrehash++;
      break;
      
    case SIGCHLD:
      gdata.needsreap++;
      break;
      
    case SIGABRT:
    case SIGBUS:
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    default:
      {
        sigset_t ss;
#if !defined(NO_SIGINFO)
#if !defined(NO_SIGCODES)
        const char *code;
#endif
#endif
        
        ++(gdata.crashing);
        
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                "!!! iroffer has received a fatal signal. !!!");
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                "Signal %d (%s)", signo, strsignal(signo));
        
#if !defined(NO_SIGINFO)
        switch (signo)
          {
          case SIGBUS:
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
#if !defined(NO_SIGCODES)
            switch (sinfo->si_code)
              {
              case BUS_ADRALN:
                code = "invalid address alignment";
                break;
                
              case BUS_ADRERR:
                code = "non-existent physical address";
                break;
                
              case BUS_OBJERR:
                code = "object specific hardware error";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Code: %s", code);
#endif
            break;
            
          case SIGILL:
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
#if !defined(NO_SIGCODES)
            switch (sinfo->si_code)
              {
              case ILL_ILLOPC:
                code = "illegal opcode";
                break;
                
              case ILL_ILLOPN:
                code = "illegal operand";
                break;
                
              case ILL_ILLADR:
                code = "illegal addressing mode";
                break;
                
              case ILL_ILLTRP:
                code = "illegal trap";
                break;
                
              case ILL_PRVOPC:
                code = "privileged opcode";
                break;
                
              case ILL_PRVREG:
                code = "privileged register";
                break;
                
              case ILL_COPROC:
                code = "co-processor error";
                break;
                
              case ILL_BADSTK:
                code = "internal stack error";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGFPE:
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
#if !defined(NO_SIGCODES)
            switch (sinfo->si_code)
              {
              case FPE_INTDIV:
                code = "integer divide by zero";
                break;
                
              case FPE_INTOVF:
                code = "integer overflow";
                break;
                
              case FPE_FLTDIV:
                code = "floating point divide by zero";
                break;
                
              case FPE_FLTOVF:
                code = "floating point overflow";
                break;
                
              case FPE_FLTUND:
                code = "floating point underflow";
                break;
                
              case FPE_FLTRES:
                code = "floating point inexact result";
                break;
                
              case FPE_FLTINV:
                code = "floating point invalid operation";
                break;
                
              case FPE_FLTSUB:
                code = "subscript out of range";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGSEGV:
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
#if !defined(NO_SIGCODES)
            switch (sinfo->si_code)
              {
              case SEGV_MAPERR:
                code = "address not mapped to object";
                break;
                
              case SEGV_ACCERR:
                code = "invalid permissions for mapped object";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGABRT:
          default:
            break;
            
          }
#endif
        
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Context Trace:");
        
        if ( gdata.crashing == 1 ) {
        dumpcontext();
        dumpgdata();
        
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Crashing... Please report this problem to dinoex");
        }
        
        tostdout_disable_buffering();
        
        uninitscreen();
        
        signal(signo, SIG_DFL);
        sigemptyset(&ss);
        sigaddset(&ss, signo);
        sigprocmask(SIG_UNBLOCK, &ss, NULL);
        raise(signo);
        /* shouldn't get here */
        exit(70);
      }
    }
  return;
}


void floodchk(void) {
   unsigned int i;
   unsigned int count;
   int last;
   
   updatecontext();

   count = 0;
   last = gdata.ignore;
   
   for (i=0; i<INAMNT_SIZE; i++) {
      count += gnetwork->inamnt[i];
      }

   if (count > gdata.flood_protection_rate)
      gdata.ignore = 1;
   else
      gdata.ignore = 0;
   
   if (last - gdata.ignore == -1) {
      outerror(OUTERROR_TYPE_WARN,"Flood Protection Activated");
      }
   if (last - gdata.ignore == 1) {
      outerror(OUTERROR_TYPE_WARN,"Flood Protection Deactivated");
      }
   
   }

void logstat(void)
{
  char *tempstr;
  
  if (gdata.logfile == NULL)
    {
      return;
    }
  
  if (gdata.no_status_log)
    {
      return;
    }
  
  tempstr = mymalloc(maxtextlength);
  getstatusline(tempstr,maxtextlength);
  
  mylog("%s", tempstr);
  
  mydelete(tempstr);
  
}

void joinchannel(channel_t *c)
{
  char *tptr;
  
  updatecontext();
  
  if (!c) return;
  
  if (c->flags & CHAN_KICKED) return;

  if (c->nextjoin > gdata.curtime)
    return;

  if ((gnetwork->connecttime + c->waitjoin) > gdata.curtime)
    return;

  if (c->key)
    {
      writeserver(WRITESERVER_NORMAL, "JOIN %s %s", c->name, c->key);
    }
  else
    {
      writeserver(WRITESERVER_NORMAL, "JOIN %s", c->name);
    }
  
  tptr = irlist_get_head(&(gnetwork->channel_join_raw));
  while(tptr)
    {
      writeserver(WRITESERVER_NORMAL, "%s", tptr);
      tptr = irlist_get_next(tptr);
    }
  
  clearmemberlist(c);
  reverify_restrictsend();
}

void shutdowniroffer(void) {
   char *tempstr2;
   char *msg;
   upload *ul;
   ir_pqueue *old;
   transfer *tr;
   dccchat_t *chat;
   gnetwork_t *backup;
   unsigned int ss;
   
   updatecontext();

   backup = gnetwork;
   
   if (gdata.exiting || has_closed_servers()) {
      if (gdata.exiting)
         ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "Shutting Down (FORCED)");
      else
         ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "Shutting Down");
      
      if ( SAVEQUIT )
         write_statefile();
      
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_delete(&gdata.dccchats,chat))
        {
          writedccchat(chat, 0, "iroffer exited (shutdown), Closing DCC Chat\n");
          shutdowndccchat(chat,1);
        }
      
      mylog("iroffer exited (shutdown)\n\n");
   
      exit_iroffer(0);
      }
   
   
   ioutput(OUT_S, COLOR_NO_COLOR, "Shutting Down... (Issue \"SHUTDOWN\" again to force quit)");
   
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       /* empty queue */
       gnetwork = &(gdata.networks[ss]);
       clean_send_buffers();
     }
   
   old = NULL;
   /* close connections */
   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       gnetwork = &(gdata.networks[tr->net]);
       if (gdata.requeue_sends)
         old = requeue(tr, old);
       t_closeconn(tr, "Server Shutting Down. (Resume Supported)", 0);
       tr = irlist_get_next(tr);
     }
   
   /* close upload connections */
   ul = irlist_get_head(&gdata.uploads);
   while (ul)
     {
       gnetwork = &(gdata.networks[ul->net]);
       l_closeconn(ul, "Server Shutting Down. (Resume Supported)", 0);
       ul = irlist_get_next(ul);
     }
   
   if ( SAVEQUIT )
      write_statefile();
   
   /* quit */
   if (has_closed_servers() == 0)
     {
       msg = mymalloc(maxtextlength);
       tempstr2 = mymalloc(maxtextlengthshort);
       getuptime(tempstr2, 1, gdata.startuptime, maxtextlengthshort);
       snprintf(msg, maxtextlength,
                "QUIT :iroffer-dinoex " VERSIONLONG "%s%s - running %s",
                gdata.hideos ? "" : " - ",
                gdata.hideos ? "" : gdata.osstring,
                tempstr2);
       mydelete(tempstr2);
       for (ss=0; ss<gdata.networks_online; ss++)
         {
           gnetwork = &(gdata.networks[ss]);
           if (gdata.debug > 14)
              {
                ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<IRC<: %u, %s", gnetwork->net + 1, msg);
              }
           writeserver_ssl(msg, strlen(msg));
           writeserver_ssl("\n", 1);
         }
       mydelete(msg);
     }
   
   gdata.exiting = 1;
   gdata.delayedshutdown = 0;
   
   ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Waiting for Server Queue To Flush...");
   gnetwork = backup;
   
   }

void quit_server(void)
{
  updatecontext();
  
  /* quit */
  if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
    {
      writeserver(WRITESERVER_NOW, "QUIT :Changing Servers");
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
              "Changing Servers on %s", gnetwork->name);
      close_server();
    }
  
  if (gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE)
    {
      close_server();
    }
  
  if (gnetwork->serverstatus == SERVERSTATUS_TRYING)
    {
      close_server();
    }
  
  /* delete the slow queue */
  clean_send_buffers();
  
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
}

void switchserver(int which)
{
  server_t *ss;
  
  quit_server();
  updatecontext();
   
  if (which < 0)
    {
      unsigned int i;
      
      i = irlist_size(&(gnetwork->servers));
      
      which = (int) get_random_uint( i );
    }
  
  if (gnetwork->offline)
    return;
  
  ss = irlist_get_nth(&(gnetwork->servers), (unsigned int)which);
  
  connectirc(ss);
  
  gnetwork->serverconnectbackoff++;
  gnetwork->servertime = 0;
}

char* getstatusline(char *str, size_t len)
{
  size_t i;
  unsigned int srvq;
  unsigned int netq;
  ir_uint64 xdccsent;
  ir_uint64 xdccrecv;
  ir_uint64 xdccsum;
  unsigned int ss;
  
  updatecontext();
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsent[i];
    }
  
  netq = 0;
  if (gnetwork != NULL)
    {
  netq += irlist_size(&(gnetwork->serverq_fast))
    + irlist_size(&(gnetwork->serverq_normal))
    + irlist_size(&(gnetwork->serverq_slow));
    }
  
  srvq = 0;
  for (ss=0; ss<gdata.networks_online; ss++)
    {
  srvq += irlist_size(&gdata.networks[ss].serverq_fast)
    + irlist_size(&gdata.networks[ss].serverq_normal)
    + irlist_size(&gdata.networks[ss].serverq_slow);
    }
  
  if (gdata.extend_status_line)
    {
       xdccrecv = 0;
       for (i=0; i<XDCC_SENT_SIZE; i++)
         {
           xdccrecv += (ir_uint64)gdata.xdccrecv[i];
         }
       xdccsum = 0;
       for (i=0; i<XDCC_SENT_SIZE; i++)
         {
           xdccsum += (ir_uint64)gdata.xdccsum[i];
         }
       
       i = add_snprintf(str, len,
               "Stat: %u/%u Sls, %u/%u Q, %u/%u I, %u/%u SrQ (Bdw: %" LLPRINTFMT "uK, %1.1fK/s, %1.1fK/s Up, %1.1fK/s Down)",
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               irlist_size(&gdata.idlequeue),
               gdata.idlequeuesize,
               netq,
               srvq,
               xdccsent/1024,
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               ((float)xdccrecv)/XDCC_SENT_SIZE/1024.0,
               ((float)xdccsum)/XDCC_SENT_SIZE/1024.0);
    }
  else
    {
       i = add_snprintf(str, len,
               "Stat: %u/%u Sls, %u/%u Q, %1.1fK/s Rcd, %u SrQ (Bdw: %" LLPRINTFMT "uK, %1.1fK/s, %1.1fK/s Rcd)",
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               gdata.record,
               srvq,
               xdccsent/1024,
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               gdata.sentrecord);
    }
  
  if ((i < 0) || ((size_t)i >= len))
    {
      str[0] = '\0';
    }
  return str;
}

char* getstatuslinenums(char *str, size_t len)
{
  size_t i;
  unsigned int gcount, srvq;
  float scount,ocount;
  xdcc *xd;
  ir_uint64 xdccsent;
  unsigned int ss;
  
  updatecontext();
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsum[i];
    }
  
  srvq = 0;
  for (ss=0; ss<gdata.networks_online; ss++)
    {
  srvq += irlist_size(&gdata.networks[ss].serverq_fast)
    + irlist_size(&gdata.networks[ss].serverq_normal)
    + irlist_size(&gdata.networks[ss].serverq_slow);
    }
  
  gcount = 0;
  scount = ocount = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      gcount += xd->gets;
      ocount += (float)xd->st_size;
      scount += ((float)xd->gets)*((float)xd->st_size);
      xd = irlist_get_next(xd);
    }
  
  i = add_snprintf(str, len,
               "stat %u %1.0f %u %1.0f %u %u %u %u %u %u %1.1f %u %" LLPRINTFMT "u %1.1f %1.1f",
               irlist_size(&gdata.xdccs),
               ocount/1024/1024,
               gcount,
               scount/1024/1024,
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               irlist_size(&gdata.idlequeue),
               gdata.idlequeuesize,
               gdata.record,
               srvq,
               xdccsent/1024,
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               gdata.sentrecord);
   if ((i < 0) || ((size_t)i >= len))
    {
      str[0] = '\0';
    }
  return str;
}

void sendxdlqueue (void)
{
  char *tempstr;
  char *group;
  char *cmd;
  xlistqueue_t *user;
  size_t len;
  userinput ui;
  
  updatecontext();
  
  if (!irlist_size(&(gnetwork->xlistqueue)))
    {
      return;
    }
  
  len = 0;
  user = irlist_get_head(&(gnetwork->xlistqueue));
  while (user)
    {
      len += strlen(user->nick) + 1;
      user = irlist_get_next(user);
    }
  
  tempstr = mymalloc(len);
  
  len = 0;
  user = irlist_get_head(&(gnetwork->xlistqueue));
  strcpy(tempstr+len, user->nick);
  len += strlen(tempstr+len);
  group = user->msg;
  mydelete(user->nick);
  
  user = irlist_delete(&(gnetwork->xlistqueue), user);
  while (user)
    {
      if (strcmp_null(user->msg, group) != 0)
        {
          user = irlist_get_next(user);
          continue;
        }
      strcpy(tempstr+len, ",");
      len += strlen(tempstr+len);
      strcpy(tempstr+len, user->nick);
      len += strlen(tempstr+len);
      mydelete(user->nick);
      mydelete(user->msg);
      
      user = irlist_delete(&(gnetwork->xlistqueue), user);
    }
  
  if (gdata.nolisting > gdata.curtime)
    {
      ioutput(OUT_S|OUT_D, COLOR_YELLOW, "Not Sending XDCC LIST to: %s (nolist set)", tempstr);
      
      notice(tempstr,
             "The Owner Has Requested That No Lists Be Sent In The Next %li %s",
             1L+(gdata.nolisting-gdata.curtime)/60,
             ((1L+(gdata.nolisting-gdata.curtime)/60) != 1L ? "minutes" : "minute"));
    }
  else
    {
      ioutput(OUT_S|OUT_D, COLOR_YELLOW, "Sending XDCC LIST to: %s", tempstr);
      
      bzero((char *)&ui, sizeof(userinput));
      cmd = NULL;
      if (group)
        {
           if (strcasecmp(group, "ALL") == 0)
             {
                a_fillwith_msg2(&ui, tempstr, "XDLFULL");
             }
           else
             {
                cmd = mymalloc(maxtextlength);
                snprintf(cmd, maxtextlength, "XDLGROUP %s", group);
                a_fillwith_msg2(&ui, tempstr, cmd);
             }
           mydelete(group);
        }
      else
        {
          a_fillwith_msg2(&ui, tempstr, "XDL");
        }
      if (gdata.xdcclist_by_privmsg)
        ui.method = method_xdl_user_privmsg;
      else
        ui.method = method_xdl_user_notice;
      u_parseit(&ui);
      mydelete(cmd);
    }
  
  mydelete(tempstr);
}

void initprefixes(void)
{
  memset(&(gnetwork->prefixes), 0, sizeof(gnetwork->prefixes));
  gnetwork->prefixes[0].p_mode   = 'o';
  gnetwork->prefixes[0].p_symbol = '@';
  gnetwork->prefixes[1].p_mode   = 'v';
  gnetwork->prefixes[1].p_symbol = '+';
}

static void initchanmodes(void)
{
  memset(&(gnetwork->chanmodes), 0, sizeof(gnetwork->chanmodes));
  gnetwork->chanmodes[0] = 'b';
  gnetwork->chanmodes[1] = 'k';
  gnetwork->chanmodes[1] = 'l';
}

void initvars(void)
{
  struct timeval timestruct;
  unsigned int ss;

  memset(&gdata, 0, sizeof(gdata_t));
  
  config_reset();
  
  for (ss=0; ss<MAX_NETWORKS; ss++)
    {
      gnetwork = &(gdata.networks[ss]);
      initprefixes();
      initchanmodes();
      
      gdata.networks[ss].serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
    }
  gnetwork = NULL;
  
  gdata.logfd = FD_UNUSED;
  gdata.termcols = 80;
  gdata.termlines = 24;
  
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
  /* 4GB max size */
  gdata.max_file_size = (1024*1024*1024*1024ULL)-1;
#else
  /* 2GB max size */
  gdata.max_file_size = (2*1024*1024*1024UL)-1;
  outerror(OUTERROR_TYPE_WARN, "max_file_size limited to 2GB");
#endif
  
  gettimeofday(&timestruct, NULL);
  gdata.curtimems = timeval_to_ms(&timestruct);
  gdata.curtime = timestruct.tv_sec;
  gdata.startuptime = timestruct.tv_sec;
  
  gdata.sendbuff = mycalloc(BUFFERSIZE);
  gdata.console_input_line = mycalloc(INPUT_BUFFER_LENGTH);
  
  gdata.last_logrotate = gdata.curtime;
  gdata.last_update = gdata.curtime;
  
  if (!setlocale(LC_CTYPE, "")) {
    fprintf(stderr, "Can't set the specified locale! "
            "Check LANG, LC_CTYPE, LC_ALL.\n");
    return;
  }
  return;
}
   
void startupiroffer(void) {
   char *tempstr23;
#if !defined(NO_SETUID)
   uid_t runasuid = 0;
   gid_t runasgid = 0;
   gid_t *groups = NULL;
   int ngroups = 0;
   int ii;
#endif
   struct sigaction sa;
   struct rlimit rlim;
   int callval;
   unsigned int ss;
   unsigned int save;
   
   updatecontext();
   
   srand((unsigned int)( (getpid()*5000) + (gdata.curtime%5000) ));
   
   if (!gdata.background) {
      initscreen(1, 1);
      gotobot();
      gototop();
      }
   
   printf("\n");
   if (!gdata.background && !gdata.nocolor) printf(IRVT_COLOR_YELLOW);
   printf("Welcome to iroffer-dinoex - " "http://iroffer.dinoex.net/" FEATURES "\n"
          "Version " VERSIONLONG "\n");
   if (!gdata.background && !gdata.nocolor) printf(IRVT_COLOR_RESET);
   printf("\n");
   
   /* signal handling */
   memset(&sa, 0, sizeof(sa));
   
#if !defined(NO_SIGINFO)
   sa.sa_sigaction = iroffer_signal_handler;
   sa.sa_flags = SA_SIGINFO;
#else
   sa.sa_handler = iroffer_signal_handler;
#endif
   sigfillset(&sa.sa_mask);
   
   sigaction(SIGBUS, &sa, NULL);
   sigaction(SIGABRT, &sa, NULL);
   sigaction(SIGILL, &sa, NULL);
   sigaction(SIGFPE, &sa, NULL);
   sigaction(SIGSEGV, &sa, NULL);
   
   sigaction(SIGTERM, &sa, NULL);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGUSR1, &sa, NULL);
   sigaction(SIGUSR2, &sa, NULL);
   sigaction(SIGCHLD, &sa, NULL);

   signal(SIGPIPE,SIG_IGN);
   signal(SIGALRM,SIG_IGN);
   signal(SIGHUP,SIG_IGN);
   signal(SIGTSTP,SIG_IGN);
   
   printf("** iroffer-dinoex is distributed under the GNU General Public License.\n"
          "**    please see the LICENSE for more information.\n");

   printf("\n** Starting up...\n");
   
#if !defined(NO_SETUID)
   if (gdata.runasuser) {
      struct passwd *pw_ent = NULL;
      if( (pw_ent = getpwnam( gdata.runasuser )) == NULL)
         if( (pw_ent = getpwuid( atoi( gdata.runasuser ) )) == NULL)
            outerror( OUTERROR_TYPE_CRASH, "Can't lookup user: %s", strerror(errno) );
      runasuid = pw_ent->pw_uid;
      runasgid = pw_ent->pw_gid;

#if defined(NO_GETGROUPLIST)
      ngroups = 1;
      groups = mycalloc(sizeof (gid_t));
      groups[0] = runasgid;
#else
      ngroups = 16;
      groups = mycalloc(ngroups * sizeof (gid_t));
      
      if (getgrouplist (pw_ent->pw_name, pw_ent->pw_gid, groups, &ngroups) < 0)
        {
          mydelete(groups);
          groups = mycalloc(ngroups * sizeof (gid_t));
          if (getgrouplist(pw_ent->pw_name, pw_ent->pw_gid, groups, &ngroups) < 0)
            {
              outerror( OUTERROR_TYPE_CRASH, "Can't lookup group list: %s", strerror(errno) );
            }
        }
#endif

   }
#endif

#if !defined(NO_CHROOT)
   if ((geteuid() == 0) && (gdata.chrootdir)) {
      printf( "** Changing root filesystem to '%s'\n", gdata.chrootdir );
      if( chroot( gdata.chrootdir ) < 0 ) {
		   outerror( OUTERROR_TYPE_CRASH, "Can't chroot: %s", strerror(errno));
      }
      if( chdir("/") < 0 ) {
        outerror( OUTERROR_TYPE_CRASH, "Can't chdir: %s", strerror(errno) );
      }
   }
#endif
   if (gdata.workdir) {
      if( chdir(gdata.workdir) < 0 ) {
        outerror( OUTERROR_TYPE_CRASH, "Can't chdir: %s", strerror(errno) );
      }
   }
#if !defined(NO_SETUID)
   if (gdata.runasuser) {
      
      printf( "** Dropping root privileges to user %s (%u/%u, groups:",
              gdata.runasuser, (unsigned int)runasuid, (unsigned int)runasgid );
      for (ii=0; ii<ngroups; ii++)
        {
          printf(" %d", (int)groups[ii]);
        }
      printf( ").\n");
      
      if (setgid(runasgid) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"Can't change group: %s", strerror(errno));
        }
      if (geteuid() == 0)
        {
      if (setgroups(ngroups,groups) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"Can't set group list: %s", strerror(errno));
        }
        }
      if (setuid(runasuid) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"Can't change to user: %s", strerror(errno));
        }
      mydelete(groups);
   }
#endif

   if (geteuid() == 0)
     {
       outerror(
#if BLOCKROOT && !defined(_OS_CYGWIN)
		OUTERROR_TYPE_CRASH,
#else
		OUTERROR_TYPE_WARN_LOUD,
#endif
		"iroffer should not be run as root!"
		);
     }
   
   if (!gdata.background)
      printf("** Window Size: %ux%u\n", gdata.termcols, gdata.termlines);
   
   tempstr23 = mymalloc(maxtextlength);
   printf("** Started on: %s\n", user_getdatestr(tempstr23, 0, maxtextlength));
   mydelete(tempstr23);
   
   set_loginname();
   
   /* we can't work with fds higher than FD_SETSIZE so set rlimit to force it */
   
   gdata.max_fds_from_rlimit = FD_SETSIZE;

#ifdef USE_OFILE
   callval = getrlimit(RLIMIT_OFILE, &rlim);
#else
   callval = getrlimit(RLIMIT_NOFILE, &rlim);
#endif
   if (callval >= 0)
     {
       rlim.rlim_cur = min2(rlim.rlim_max, FD_SETSIZE);
       gdata.max_fds_from_rlimit = rlim.rlim_cur;
#ifdef USE_OFILE
       callval = setrlimit(RLIMIT_OFILE, &rlim);
#else
       callval = setrlimit(RLIMIT_NOFILE, &rlim);
#endif
       if (callval < 0)
         {
           outerror(OUTERROR_TYPE_CRASH, "Unable to adjust fd limit to %u: %s",
                    gdata.max_fds_from_rlimit, strerror(errno));
         }
     }
   else
     {
       outerror(OUTERROR_TYPE_CRASH, "Unable to read fd limit: %s", strerror(errno));
     }
   
   if (gdata.max_fds_from_rlimit < (RESERVED_FDS + RESERVED_FDS))
     {
       outerror(OUTERROR_TYPE_CRASH, "fd limit of %u is too small",
                gdata.max_fds_from_rlimit);
     }
   gdata.maxtrans = (((gdata.max_fds_from_rlimit) - RESERVED_FDS)/2);
   
   if (gdata.adjustcore)
     {
       callval = getrlimit(RLIMIT_CORE, &rlim);
       if (callval >= 0)
         {
           rlim.rlim_cur = rlim.rlim_max;
           if (rlim.rlim_cur == 0)
             {
               outerror(OUTERROR_TYPE_WARN, "Unable to set core limit: hard limit is 0");
             }
           
           callval = setrlimit(RLIMIT_CORE, &rlim);
           if (callval < 0)
             {
               outerror(OUTERROR_TYPE_WARN, "Unable to adjust core limit: %s",
                        strerror(errno));
             }
         }
       else
         {
           outerror(OUTERROR_TYPE_WARN, "Unable to read core limit: %s", strerror(errno));
         }
     }
   
   startup_dinoex();
   
   getconfig();
   
   mylog("iroffer-dinoex started " VERSIONLONG FEATURES);

   getos();
   
   if (gdata.statefile)
     {
       save = read_statefile();
       save += import_xdccfile();
       if (save > 0)
         write_statefile();
       autotrigger_rebuild();
       start_main_queue();
     }
   
   /* fork to background if in background mode */
   if (gdata.background) gobackground();
   
   if (gdata.pidfile)
      writepidfile(gdata.pidfile);
   
   /* start stdout buffered I/O */
   fflush(stdout);
   if (gdata.background)
     {
     }
   else if (set_socket_nonblocking(fileno(stdout),1) < 0)
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,"Cant set console non-blocking!: %s",strerror(errno));
     }
   else
     {
       ir_boutput_init(&gdata.stdout_buffer, fileno(stdout), BOUTPUT_NO_LIMIT);
       gdata.stdout_buffer_init = 1;
       gotobot();
       drawbot();
       gototop();
     }
   
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       gnetwork = &(gdata.networks[ss]);
       gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
       switchserver(-1);
     }
   gnetwork = NULL;
   config_dinoex();
   }


void isrotatelog(void)
{
  char *newname;
  int call_val;
  
  updatecontext();
  
  /* check time for logrotate */
  if (compare_logrotate_time() == 0)
    return;
  
  gdata.last_logrotate = gdata.curtime;
  write_statefile();
  rotatelog(gdata.logfile_httpd);
  rotatelog(gdata.http_access_log);
  newname = new_logfilename(gdata.logfile);
  if (newname == NULL)
    return;
  
  mylog("Rotating Log to '%s'", newname);
  call_val = link(gdata.logfile, newname);
  
  if (call_val < 0)
    {
      outerror(OUTERROR_TYPE_WARN,
               "Unable to rename log '%s' -> '%s': %s",
               gdata.logfile,
               newname,
               strerror(errno));
    }
  else
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
              "Logfile rotated to '%s'", newname);
      
      if (gdata.logfd != FD_UNUSED)
        {
          close(gdata.logfd);
          gdata.logfd = FD_UNUSED;
        }
      
      call_val = unlink(gdata.logfile);
      if (call_val < 0)
        {
          outerror(OUTERROR_TYPE_WARN,
                   "Unable to remove old log '%s' : %s",
                   gdata.logfile,
                   strerror(errno));
        }
      
      mylog("Rotated Log to '%s'", newname);
    }
  
  mydelete(newname);
  expire_logfiles(gdata.logfile);
  
  return;
}


void createpassword(void) {
#ifndef NO_CRYPT
   char pw1[maxtextlengthshort], pw2[maxtextlengthshort];
   int len;
   unsigned int ok;
   unsigned int saltnum;
   char salt[6], *pwout;
   
   printf("\niroffer-dinoex " VERSIONLONG "\n"
          "  Configuration File Password Generator\n"
          "\n"
          "This will take a password of your choosing and encrypt it.\n"
          "You should place the output this program generates in your config file.\n"
          "You can then use your password you enter here over irc.\n"
          "\n"
          "Your password must be between 5 and 59 characters\n");
   
   
   ok = 0;
   while ( !ok ) {
      printf("Please Enter Your Password: "); fflush(stdout);
      
      if ( (len = read(0,pw1,maxtextlengthshort-1)) < 0 )
         { fprintf(stderr, "Couldn't Read Your Password, Try Again\n"); exit(66); }
      if (pw1[len-1] == '\n') { pw1[len-1] = '\0'; len--;}
      
      if ( len < 5 )
         printf("Wrong Length, Try Again\n");
      else
         ok = 1;
   }
   
   printf("And Again for Verification: ");
   fflush(stdout);

   if ( (len = read(0,pw2,maxtextlengthshort-1)) < 0 )
      { fprintf(stderr, "Couldn't Read Your Password, Try Again\n"); exit(66); }
   if (pw2[len-1] == '\n') { pw2[len-1] = '\0'; len--;}
   
   if ( strcmp(pw1,pw2) )
      { fprintf(stderr, "The Password Didn't Match, Try Again\n"); exit(65); }
   
   
   srand((unsigned int)( (getpid()*5000) + (time(NULL)%5000) ));
   saltnum = get_random_uint( 4096 );
#if !defined(_OS_CYGWIN)
   salt[0] = '$';
   salt[1] = '1';
   salt[2] = '$';
   salt[3] = inttosaltchar((saltnum>>6) %64);
   salt[4] = inttosaltchar( saltnum     %64);
   salt[5] = '\0';
#else
   salt[0] = inttosaltchar((saltnum>>6) %64);
   salt[1] = inttosaltchar( saltnum     %64);
   salt[2] = '\0';
#endif
   
   pwout = crypt(pw1,salt);
   
   if (pwout && strlen(pwout) >= 13U)
     {
       if (add_password(pwout) != 0)
      printf("\n"
             "To use \"%s\" as your password use the following in your config file:\n"
             "adminpass %s\n"
             "\n"
             ,pw1,pwout);
     }
   else
      printf("\n"
             "The crypt() function does not appear to be working correctly\n"
             "\n");
   
#else
   printf("iroffer was compiled without encrypted password support.  You do not need to encrypt your password.\n");
#endif
   }

/* 0 .. 63 */
int inttosaltchar (int n) {
   
   if ( n < 26 )
      return  n + 'a';
   else if ( n < 26*2 )
      return  n - 26 + 'A';
   else if ( n < 26*2+10 )
      return  n - 26 - 26 + '0';
   else if ( n < 26*2+10+1 )
      return  '.';
   else if ( n < 26*2+10+2 )
      return  '/';
   
   return 0; /* error */
   
   }

void notifybandwidth(void)
{
  unsigned int i;
  transfer *tr;
  gnetwork_t *backup;
  ir_uint64 xdccsent;
  
  updatecontext();
  
  if (gdata.exiting) return;
  if (!gdata.maxb) return;
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsent[i];
    }
  xdccsent /= 1024;
  
  /* send if over 90% */
  if ( (xdccsent*10) > (gdata.maxb*30*9) )
    {
      backup = gnetwork;
      for (tr = irlist_get_head(&gdata.trans);
           tr;
           tr = irlist_get_next(tr))
        {
          if (gnetwork->net != tr->net)
            continue;
          notice_slow(tr->nick, "%s bandwidth limit: %2.1f of %2.1fkB/sec used. Your share: %2.1fkB/sec.",
                      get_user_nick(),
                      ((float)xdccsent)/XDCC_SENT_SIZE,
                      ((float)gdata.maxb)/4.0,
                      tr->lastspeed);
        }
      gnetwork = backup;
    }
  
}

void notifybandwidthtrans(void)
{
  transfer *tr;
  gnetwork_t *backup;
  
  updatecontext();
  
  if (gdata.exiting) return;
  
  backup = gnetwork;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr))
    {
      if (gnetwork->net != tr->net)
        continue;
      if (!tr->nomax &&
          (tr->maxspeed > 0) &&
          (gdata.curtime > tr->con.connecttime + MIN_TL) &&
          (tr->lastspeed*10 > tr->maxspeed*9))
        {
          /* send if over 90% */
          gnetwork = &(gdata.networks[tr->net]);
          notice_slow(tr->nick, "Pack bandwidth limit: %2.1f of %2.1fkB/sec used.",
                      tr->lastspeed,
                      tr->maxspeed);
        }
    }
  gnetwork = backup;
}


int look_for_file_changes(xdcc *xpack)
{
  struct stat st;
  transfer *tr;
  
  if (stat(xpack->file,&st) < 0)
    {
      outerror(OUTERROR_TYPE_WARN,
               "Pack %u: File '%s' can no longer be accessed: %s",
                number_of_pack(xpack),
                xpack->file, strerror(errno));
      if (errno != ENOENT)
        return 0;
      return 1;
    }
  
  if (st.st_size == 0)
    {
      outerror(OUTERROR_TYPE_WARN,
               "File '%s' has size of 0 byte", xpack->file);
    }
  
  if ((st.st_size > gdata.max_file_size) || (st.st_size < 0))
    {
      outerror(OUTERROR_TYPE_WARN,
               "File '%s' is too large", xpack->file);
    }
  
  /*
   * Certain filesystem types dont have constant dev/inode
   * numbers so we can't compare against them.  Only compare
   * mtime and size.
   */
  xpack->st_dev = st.st_dev;
  xpack->st_ino = st.st_ino;
  
  if ((xpack->mtime   != st.st_mtime) ||
      (xpack->st_size != st.st_size))
    {
      if ((!gdata.xdcclistfile || strcmp(xpack->file, gdata.xdcclistfile)) &&
          (!gdata.send_listfile || (gdata.send_listfile != number_of_pack(xpack))))
        {
          outerror(OUTERROR_TYPE_WARN,
                   "File '%s' has changed", xpack->file);
        }
      
      xpack->mtime    = st.st_mtime;
      xpack->st_size  = st.st_size;
      
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
              (tr->xpack == xpack))
            {
              t_closeconn(tr,"Pack file changed",0);
            }
          tr = irlist_get_next(tr);
        }
      
      cancel_md5_hash(xpack, "file changed");
    }
  
  return 0;
}

void user_changed_nick(const char *oldnick, const char *newnick)
{
  transfer *tr;
  ir_pqueue *pq;
  ir_pqueue *old;
  unsigned int userinqueue = 0;
  
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
    {
      if (!strcasecmp(tr->nick, oldnick))
        {
          mydelete(tr->nick);
          tr->nick = mystrdup(newnick);
        }
    }
  
  queue_update_nick(&gdata.mainqueue, oldnick, newnick);
  queue_update_nick(&gdata.idlequeue, oldnick, newnick);
  
  for (pq = irlist_get_head(&gdata.mainqueue); pq; )
    {
      if (!strcasecmp(pq->nick, newnick))
        {
          if (strcmp(pq->hostname, "man"))
            {
              userinqueue++;
              if ( userinqueue > gdata.maxqueueditemsperperson )
                {
                  notice(pq->nick, "** Removed From Queue: To many requests");
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                          "Removed From Queue: To many requests for %s on %s.",
                          pq->nick, gdata.networks[ pq->net ].name);
                  mydelete(pq->nick);
                  mydelete(pq->hostname);
                  old = pq;
                  pq = irlist_get_next(pq);
                  irlist_delete(&gdata.mainqueue, old);
                  continue;
                }
            }
        }
      pq = irlist_get_next(pq);
    }
  return;
}

void reverify_restrictsend(void)
{
  gnetwork_t *backup;
  transfer *tr;
  
  backup = gnetwork;
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
    {
      gnetwork = &(gdata.networks[tr->net]);
      if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
         continue;
      if (!get_restrictsend())
         continue;
      if (gdata.curtime < gnetwork->next_restrict)
         continue;
      if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
          strcmp(tr->hostname,"man"))
        {
          if (isinmemberlist(tr->nick))
            {
              tr->restrictsend_bad = 0;
            }
          else if (!tr->restrictsend_bad)
            {
              tr->restrictsend_bad = gdata.curtime;
              if (gdata.restrictsend_warning)
                {
                  notice(tr->nick, "You are no longer on a known channel");
                }
            }
          else if ((unsigned)gdata.curtime >= (tr->restrictsend_bad + gdata.restrictsend_timeout))
            {
              t_closeconn(tr, "You are no longer on a known channel", 0);
            }
        }
    }
  
  gnetwork = backup;
  queue_reverify_restrictsend(&gdata.mainqueue);
  queue_reverify_restrictsend(&gdata.idlequeue);
  return;
}

/* End of File */
