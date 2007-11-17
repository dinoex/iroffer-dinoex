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
#include "dinoex_config.h"
#include "dinoex_ssl.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"
#include "dinoex_jobs.h"
#include "dinoex_misc.h"


void getconfig (void) {
   char *templine = mycalloc(maxtextlength);
   int h, filedescriptor;
   
   updatecontext();

   for (h=0; h<MAXCONFIG && gdata.configfile[h]; h++)
     {
       convert_to_unix_slash(gdata.configfile[h]);
       
       printf("** Loading %s ... \n",gdata.configfile[h]);
       
       filedescriptor=open(gdata.configfile[h], O_RDONLY | ADDED_OPEN_FLAGS);
       if (filedescriptor < 0)
         {
           outerror(OUTERROR_TYPE_CRASH,"Cant Access Config File '%s': %s",gdata.configfile[h],strerror(errno));
         }
       
       gdata.bracket = 0;
       while (getfline(templine,maxtextlength,filedescriptor,1))
         {
           if ((templine[0] != '#') && templine[0])
             {
               getconfig_set(templine,0);
             }
         }
       
       close(filedescriptor);
     }
   
   printf("** Checking for completeness of config file ...\n");
   
   if ( !irlist_size(&gdata.networks[0].servers)
        || gdata.config_nick == NULL || gdata.user_realname == NULL
        || gdata.slotsmax == 0)
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Necessary Information");

   gdata.networks_online ++;

   if ( irlist_size(&gdata.uploadhost) && ( gdata.uploaddir == NULL || strlen(gdata.uploaddir) < 2 ) )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Upload Information");
      
   if ( !irlist_size(&gdata.downloadhost) )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Download Host Information");
      
   if ( !gdata.statefile )
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing State File Information");
      
   if (gdata.background) gdata.debug = 0;
   
   if ( !gdata.noscreen && !gdata.background) {
      printf("\x1b[%i;12H%s) >",gdata.termlines,"");
      printf("\x1b[%i;%iH",gdata.termlines, 16);
      gototop();
      }
   
   mydelete(templine);
   }



void getconfig_set (const char *line, int rehash)
{
  char *type;
  char *var;
  char *a,*b;
  const char *found;
  int i,j;
  
  updatecontext();
  
  i = strlen(line);
  type = mycalloc(i+1);
  var = mycalloc(i+1);

  /* ignore leading spaces */
  for (i=0; line[i] == ' '; i++)
    continue;
  
  for (j=0; ; j++,i++)
    {
      if (line[i] == ' ')
        {
          type[j] = '\0';
          i++;
          break;
        }
      type[j] = line[i];
      if (line[i] == '\0')
        {
          break;
        }
    }
  
  for (j=0; ; j++,i++)
    {
      var[j] = line[i];
      if (line[i] == '\0')
        {
          break;
        }
    }
  
  if (gdata.bracket == 0)
    {
      if (set_config_bool(type, var) == 0)
        {
          mydelete(type);
          mydelete(var);
          return;
        }
  
      if (set_config_int(type, var) == 0)
        {
          mydelete(type);
          mydelete(var);
          return;
        }

      if (set_config_string(type, var) == 0)
        {
          mydelete(type);
          return;
        }

      if (set_config_list(type, var) == 0)
        {
          mydelete(type);
          return;
        }
    }

  if (set_config_func(type, var) == 0)
    {
      mydelete(type);
      return;
    }

   /* parse it */
   if ( ! strcmp(type,"server"))
     {
       server_t *ss;
       char *portstr;

       set_default_network_name();
       ss = irlist_add(&gdata.networks[gdata.networks_online].servers, sizeof(*ss));
       ss->hostname = getpart(var,1);
       portstr = getpart(var,2);
       if (!ss->hostname)
         {
           outerror(OUTERROR_TYPE_WARN,"ignoring invalid server line");
           mydelete(portstr);
           irlist_delete(&gdata.networks[gdata.networks_online].servers, ss);
         }
       else if (portstr)
         {
           int totallen = strlen(var);
           int passoffset = strlen(ss->hostname) + strlen(portstr) + 2;
           
           if (totallen > passoffset)
             {
               /* has a password */
               ss->password = mycalloc(1 + totallen - passoffset);
               strcpy(ss->password, var+passoffset);
             }
           
           ss->port = atoi(portstr);
           mydelete(portstr);
         }
       else
         {
           ss->port = 6667;
           ss->password = NULL;
         }
       mydelete(var);
     }
   else if ( ! strcmp(type,"proxyinfo"))
     {
       char *pi;
       pi = irlist_add(&gdata.networks[gdata.networks_online].proxyinfo, strlen(var) + 1);
       strcpy(pi, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"channel_join_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.networks[gdata.networks_online].channel_join_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"server_join_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.networks[gdata.networks_online].server_join_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"server_connected_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.networks[gdata.networks_online].server_connected_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( ! strcmp(type,"channel")) {
      char *tptr = NULL, *tptr2 = NULL, *tname;
      int ok=1;
      channel_t *cptr = NULL;
      
      if (!rehash)
        {
          cptr = irlist_add(&gdata.networks[gdata.networks_online].channels, sizeof(channel_t));
        }
      else
        {
          cptr = irlist_add(&gdata.networks[gdata.networks_online].r_channels, sizeof(channel_t));
        }
      
      tname = getpart(var,1);
      caps(tname);
      cptr->name = mystrdup(tname);
      cptr->headline = NULL;
      cptr->pgroup = NULL;
      cptr->noannounce = 0;
      
      for (i=2; i<20 && ok && (tptr = getpart(var,i)); i++) {
         if (!strcmp(tptr,"-plist")) {
            i++;
            if ((tptr2 = getpart(var,i)))
               cptr->plisttime = atoi(tptr2);
            else ok=0;
            }
         else if (!strcmp(tptr,"-pformat")) {
            i++;
            if ((tptr2 = getpart(var,i))) {
               if (!strcmp(tptr2,"full"))
                  ;
               else if (!strcmp(tptr2,"minimal"))
                  cptr->flags |= CHAN_MINIMAL;
               else if (!strcmp(tptr2,"summary"))
                  cptr->flags |= CHAN_SUMMARY;
               else ok=0;
               }
            else ok=0;
            }
          else if (!strcmp(tptr, "-plistoffset")) {
            i++;
            if ((tptr2 = getpart(var, i)))
              cptr->plistoffset = atoi(tptr2);
            else
              cptr->plistoffset = 0;
            }
         else if (!strcmp(tptr,"-key")) {
            i++;
            if ((tptr2 = getpart(var,i)))
              {
                cptr->key = mystrdup(tptr2);
              }
            else ok=0;
            }
         else if (!strcmp(tptr,"-delay")) {
            i++;
            if ((tptr2 = getpart(var, i)))
              cptr->delay = atoi(tptr2);
            else
              cptr->delay = 0;
            }
         else if (!strcmp(tptr,"-noannounce")) {
            cptr->noannounce = 1;
            }
         else if (!strcmp(tptr,"-pgroup")) {
            i++;
            if ((tptr2 = getpart(var,i)))
              {
                cptr->pgroup = mystrdup(tptr2);
              }
            else ok=0;
            }
         else if (!strcmp(tptr,"-headline")) {
            i++;
            found = strstr(line, "-headline");
            if (found != NULL)
              {
		found += strlen( "-headline" );
		found ++;
                cptr->headline = mystrdup(found);
                mydelete(tptr);
		break;
              }
            else ok=0;
            }
         else ok=0;
         
         mydelete(tptr);
         mydelete(tptr2);
         }
      
      if (!ok) ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                       " !!! Bad syntax for channel %s on %s in config file !!!",
                       tname, gdata.networks[gdata.networks_online].name);
      
      if (cptr->plisttime && (cptr->plistoffset >= cptr->plisttime))
        {
          outerror(OUTERROR_TYPE_WARN,"plistoffset must be less than plist time, ignoring offset");
          cptr->plistoffset = 0;
        }
      
      mydelete(tptr);
      mydelete(tptr2);
      mydelete(tname);
      mydelete(var);
      }
   else if ( ! strcmp(type,"slotsmax")) {
      gdata.slotsmax = between(1,atoi(var),MAXTRANS);
      if (gdata.slotsmax != atoi(var))
        {
          outerror(OUTERROR_TYPE_WARN,"unable to have slotsmax of %d, using %d instead",atoi(var),gdata.slotsmax);
        }
      mydelete(var);
      }
   else if ( ! strcmp(type,"transferminspeed")) {
      gdata.transferminspeed = atof(var);
      mydelete(var);
      }
   else if ( ! strcmp(type,"transfermaxspeed")) {
      gdata.transfermaxspeed = max2(0,atof(var));
      mydelete(var);
      }
   else if ( ! strcmp(type,"overallmaxspeeddaytime")) {
      a = getpart(var,1); b = getpart(var,2);
      if (a && b) {
         gdata.overallmaxspeeddaytimestart = between(0,atoi(a),23);
         gdata.overallmaxspeeddaytimeend   = between(0,atoi(b),23);
         }
      mydelete(a); mydelete(b);
      mydelete(var);
      }
   else if ( ! strcmp(type,"overallmaxspeeddaydays")) {
      gdata.overallmaxspeeddaydays = 0;
      for (i=0; (i<sstrlen(var) && i<8); i++)
         gdata.overallmaxspeeddaydays |= dayofweektomask(var[i]);
      mydelete(var);
      }
   else if ( ! strcmp(type,"transferlimits"))
     {
       char *part;
       int ii;
       for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
         {
           part = getpart(var, ii+1);
           if (part)
             {
               gdata.transferlimits[ii].limit = max2(0,atoi(part));
               gdata.transferlimits[ii].limit *= 1024 * 1024;
               mydelete(part);
             }
         }
       mydelete(var);
     }
   else if ( ! strcmp(type,"logrotate")) {
      if (!strcmp(var,"daily")) gdata.logrotate = 24*60*60;
      if (!strcmp(var,"weekly")) gdata.logrotate = 7*24*60*60;
      if (!strcmp(var,"monthly")) gdata.logrotate = 30*24*60*60;
      mydelete(var);
      }
   else if ( ! strcmp(type,"statefile")) {
      mydelete(gdata.statefile);
      gdata.statefile = var;
      convert_to_unix_slash(gdata.statefile);
      i = open(gdata.statefile, O_RDWR | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS );
      if (i >= 0) close(i);
      }
   else if ( ! strcmp(type,"periodicmsg")) {
      char *tnum;
      int offset;
      mydelete(gdata.periodicmsg_nick);
      mydelete(gdata.periodicmsg_msg);
      
      gdata.periodicmsg_nick = getpart(var,1);
      tnum = getpart(var,2);
      gdata.periodicmsg_msg = getpart(var,3);
      
      if (!gdata.periodicmsg_nick || !tnum || !gdata.periodicmsg_msg) {
         outerror(OUTERROR_TYPE_WARN_LOUD,"Syntax Error In periodicmsg, Ignoring");
         mydelete(gdata.periodicmsg_nick);
         mydelete(gdata.periodicmsg_msg);
         mydelete(var);
         }
      else {
         gdata.periodicmsg_time = max2(1,atoi(tnum));
         
         offset = strlen(gdata.periodicmsg_nick) + strlen(tnum) + 2;
         for (i=offset; i<=(int)(strlen(var)); i++)
            var[i-offset] = var[i];
         
         mydelete(gdata.periodicmsg_msg);
         gdata.periodicmsg_msg = var;
         
         }
      
      mydelete(tnum);
      }
   else if ( ! strcmp(type,"uploadmaxsize")) {
      gdata.uploadmaxsize = atoull(var)*1024*1024;
      mydelete(var);
      }
   else if ( ! strcmp(type,"connectionmethod"))
     {
       char *thow = getpart(var,1);
       char *targ1 = getpart(var,2);
       char *targ2 = getpart(var,3);
       char *targ3 = getpart(var,4);
       char *targ4 = getpart(var,5);
       
       mydelete(gdata.networks[gdata.networks_online].connectionmethod.host);
       mydelete(gdata.networks[gdata.networks_online].connectionmethod.password);
       mydelete(gdata.networks[gdata.networks_online].connectionmethod.vhost);
       bzero((char *) &gdata.networks[gdata.networks_online].connectionmethod, sizeof(connectionmethod_t));
       
       if (thow && !strcmp(thow,"direct"))
         {
           gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
         }
       else if (thow && !strcmp(thow, "ssl"))
         {
#ifdef USE_SSL
           gdata.networks[gdata.networks_online].connectionmethod.how = how_ssl;
#else
           gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
           outerror(OUTERROR_TYPE_WARN_LOUD,"connectionmethod ssl not compiled, defaulting to direct");
#endif /* USE_SSL */
         }
       else if (thow && targ1 && targ2 && targ3 && !strcmp(thow,"bnc"))
         {
           gdata.networks[gdata.networks_online].connectionmethod.how = how_bnc;
           
           gdata.networks[gdata.networks_online].connectionmethod.host = mystrdup(targ1);
           
           gdata.networks[gdata.networks_online].connectionmethod.port = atoi(targ2);
           
           gdata.networks[gdata.networks_online].connectionmethod.password = mystrdup(targ3);
           
           if (targ4)
             {
               gdata.networks[gdata.networks_online].connectionmethod.vhost = mystrdup(targ4);
             }
         }
       else if (thow && targ1 && targ2 && !strcmp(thow,"wingate"))
         {
           gdata.networks[gdata.networks_online].connectionmethod.how = how_wingate;
           
           gdata.networks[gdata.networks_online].connectionmethod.host = mystrdup(targ1);
           
           gdata.networks[gdata.networks_online].connectionmethod.port = atoi(targ2);
         }
       else if (thow && targ1 && targ2 && !strcmp(thow,"custom"))
         {
           gdata.networks[gdata.networks_online].connectionmethod.how = how_custom;
           
           gdata.networks[gdata.networks_online].connectionmethod.host = mystrdup(targ1);
           
           gdata.networks[gdata.networks_online].connectionmethod.port = atoi(targ2);
         }
       else
         {
           gdata.networks[gdata.networks_online].connectionmethod.how = how_direct;
           outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid connectionmethod in config file, defaulting to direct");
         }
       
      mydelete(thow);
      mydelete(targ1);
      mydelete(targ2);
      mydelete(targ3);
      mydelete(targ4);
      mydelete(var);
      }
   else {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Ignored invalid line in config file: %s",type);
      mydelete(var);
      }

   mydelete(type);
   }

static int connectirc (server_t *tserver) {
   char *tempstr;
   char *vhost;
   int callval;
   
   updatecontext();

   gnetwork->serverbucket = EXCESS_BUCKET_MAX;
   
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
   
   tempstr = mycalloc(maxtextlength);
   
   switch (gnetwork->connectionmethod.how)
     {
     case how_direct:
       snprintf(tempstr,maxtextlength-1," (direct)");
       gnetwork->serv_resolv.to_ip = gnetwork->curserver.hostname;
       gnetwork->serv_resolv.to_port = gnetwork->curserver.port;
       break;
       
     case how_ssl:
       snprintf(tempstr,maxtextlength-1," (ssl)");
       gnetwork->serv_resolv.to_ip = gnetwork->curserver.hostname;
       gnetwork->serv_resolv.to_port = gnetwork->curserver.port;
       break;
       
     case how_bnc:
       if (gnetwork->connectionmethod.vhost)
         {
           snprintf(tempstr, maxtextlength-1,
                    " (bnc at %s:%i with %s)",
                    gnetwork->connectionmethod.host,
                    gnetwork->connectionmethod.port,
                    gnetwork->connectionmethod.vhost);
         }
       else
         {
           snprintf(tempstr, maxtextlength-1,
                    " (bnc at %s:%i)",
                    gnetwork->connectionmethod.host,
                    gnetwork->connectionmethod.port);
         }
       gnetwork->serv_resolv.to_ip = gnetwork->connectionmethod.host;
       gnetwork->serv_resolv.to_port = gnetwork->connectionmethod.port;
       break;
       
     case how_wingate:
       snprintf(tempstr, maxtextlength-1,
                " (wingate at %s:%i)",
                gnetwork->connectionmethod.host,
                gnetwork->connectionmethod.port);
       gnetwork->serv_resolv.to_ip = gnetwork->connectionmethod.host;
       gnetwork->serv_resolv.to_port = gnetwork->connectionmethod.port;
       break;
       
     case how_custom:
       snprintf(tempstr, maxtextlength-1,
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
       ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
               "Attempting Connection to %s:%u from %s%s",
               gnetwork->curserver.hostname,
               gnetwork->curserver.port,
               vhost,
               tempstr);
     }
   else
     {
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Attempting Connection to %s:%u%s",
               gnetwork->curserver.hostname,
               gnetwork->curserver.port,tempstr);
     }
   
   mydelete(tempstr);
   
   if (gnetwork->serv_resolv.child_pid)
     {
       /* old resolv still outstanding, cleanup */
       close(gnetwork->serv_resolv.sp_fd[0]);
       FD_CLR(gnetwork->serv_resolv.sp_fd[0], &gdata.readset);
       gnetwork->serv_resolv.sp_fd[0] = 0;
       gnetwork->serv_resolv.child_pid = 0;
     }
   
   callval = socketpair(AF_UNIX, SOCK_DGRAM, 0, gnetwork->serv_resolv.sp_fd);
   if (callval < 0)
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,"socketpair(): %s", strerror(errno));
       return 1;
     }
   
   callval = fork();
   if (callval < 0)
     {
       outerror(OUTERROR_TYPE_WARN_LOUD,"fork(): %s", strerror(errno));
       return 1;
     }
   if (callval == 0)
     {
       /* child */
       child_resolver();
       sleep(60);
       exit(0);
     }
   
   /* parent */
   gnetwork->serv_resolv.child_pid = callval;
   close(gnetwork->serv_resolv.sp_fd[1]);
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
  
  if (setup_ssl())
    return;

  pi = irlist_get_head(&(gnetwork->proxyinfo));
  while((gnetwork->connectionmethod.how == how_custom) && pi)
    {
      int len;
      char *tempstr;
      int found_s = 0;
      int found_p = 0;
      char portstr[maxtextlengthshort];
      
      snprintf(portstr, maxtextlengthshort-1, "%u", gnetwork->curserver.port);
      portstr[maxtextlengthshort-1] = '\0';
      
      len = strlen(pi) + strlen(gnetwork->curserver.hostname) + strlen(portstr);
      
      tempstr = mycalloc(len+1);
      
      for (i=j=0; pi[i]; i++,j++)
        {
          if (!found_s &&
              (pi[i] == '$') &&
              (pi[i+1] == 's'))
            {
              for (k=0,i++; gnetwork->curserver.hostname[k]; k++,j++)
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
  char *item;
  int len;
  
  msg = mycalloc(maxtextlength+1);
  
  len = vsnprintf(msg,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"WRITESERVER: Output too large, ignoring!");
      mydelete(msg);
      return;
    }
  
  if ((type == WRITESERVER_NOW) &&
      (gnetwork->serverstatus == SERVERSTATUS_CONNECTED))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<SND<: %d: %s", gnetwork->net +1, msg);
        }
      msg[len] = '\n';
      len++;
      msg[len] = '\0';
      writeserver_ssl(msg, len);
      gnetwork->serverbucket -= len;
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
          if (gdata.debug > 0)
            {
              ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<QUEF<: %s",msg);
            }
          
          if (len > EXCESS_BUCKET_MAX)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[EXCESS_BUCKET_MAX] = '\0';
              len = EXCESS_BUCKET_MAX;
            }
          
          if (irlist_size(&(gnetwork->serverq_fast)) < MAXSENDQ)
            {
              item = irlist_add(&(gnetwork->serverq_fast), len + 1);
              strcpy(item, msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else if (type == WRITESERVER_NORMAL)
        {
          if (gdata.debug > 0)
            {
              ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<QUEN<: %s",msg);
            }
          
          if (len > EXCESS_BUCKET_MAX)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[EXCESS_BUCKET_MAX] = '\0';
              len = EXCESS_BUCKET_MAX;
            }
          
          if (irlist_size(&(gnetwork->serverq_normal)) < MAXSENDQ)
            {
              item = irlist_add(&(gnetwork->serverq_normal), len + 1);
              strcpy(item, msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else if (type == WRITESERVER_SLOW)
        {
          if (gdata.debug > 0)
            {
              ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<QUES<: %s",msg);
            }
          
          if (len > EXCESS_BUCKET_MAX)
            {
              outerror(OUTERROR_TYPE_WARN,"Message Truncated!");
              msg[EXCESS_BUCKET_MAX] = '\0';
              len = EXCESS_BUCKET_MAX;
            }
          
          if (irlist_size(&(gnetwork->serverq_slow)) < MAXSENDQ)
            {
              item = irlist_add(&(gnetwork->serverq_slow), len + 1);
              strcpy(item, msg);
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
            }
        }
      else
        {
          outerror(OUTERROR_TYPE_CRASH,"Unknown type %d",type);
        }
    }
  
  mydelete(msg);
  return;
}

void sendserver(void)
{
  char *item;
  
  sendannounce();
  gnetwork->serverbucket += EXCESS_BUCKET_ADD;
  gnetwork->serverbucket = min2(gnetwork->serverbucket,EXCESS_BUCKET_MAX);
  
  if ((irlist_size(&(gnetwork->serverq_fast)) == 0) &&
      (irlist_size(&(gnetwork->serverq_normal)) == 0) &&
      (irlist_size(&(gnetwork->serverq_slow)) == 0) &&
      gdata.exiting &&
      !gnetwork->recentsent)
    {
      close_server();
      gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_NO_COLOR,
              "Connection to %s (%s) Closed",
              gnetwork->curserver.hostname,
              gnetwork->curserveractualname ? gnetwork->curserveractualname : "<unknown>");
      return;
    }
  
  if (((irlist_size(&(gnetwork->serverq_fast)) == 0) &&
       (irlist_size(&(gnetwork->serverq_normal)) == 0) &&
       (irlist_size(&(gnetwork->serverq_slow)) == 0)) ||
      (gnetwork->serverstatus != SERVERSTATUS_CONNECTED))
    {
      return;
    }
  
  
  item = irlist_get_head(&(gnetwork->serverq_fast));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<IRC<: %d, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->serverbucket -= strlen(item);
      
      item = irlist_delete(&(gnetwork->serverq_fast), item);
    }
  
  if (item)
    {
      gnetwork->recentsent = 0;
      return;
    }
  
  item = irlist_get_head(&(gnetwork->serverq_normal));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<IRC<: %d, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->serverbucket -= strlen(item);
      
      item = irlist_delete(&(gnetwork->serverq_normal), item);
    }
  
  if (item)
    {
      gnetwork->recentsent = 0;
      return;
    }
  
  item = irlist_get_head(&(gnetwork->serverq_slow));
  
  while (item && (((int)strlen(item)) < gnetwork->serverbucket))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_MAGENTA, "<IRC<: %d, %s", gnetwork->net + 1, item);
        }
      writeserver_ssl(item, strlen(item));
      writeserver_ssl("\n", 1);
      
      gnetwork->serverbucket -= strlen(item);
      
      item = irlist_delete(&(gnetwork->serverq_slow), item);
    }
  
  if (item)
    {
      gnetwork->recentsent = 0;
    }
  else
    {
      gnetwork->recentsent = 6;
    }
  
  return;
}

/* 'Sanitize' the filename in full, putting the sanitized copy into copy. */
char* getsendname(char * const full)
{
  char *copy;
  int i, lastslash;
  int spaced;
  int len;
  
  updatecontext();
  
  len = sstrlen(full);
  lastslash = -1;
  spaced = 0;
  for (i = 0 ; i < len ; i++)
    {
      switch (full[i]) {
      case '/':
      case '\\':
        lastslash = i;
        spaced = 0;
        break;
      case ' ':
        spaced = 2;
        break;
      }
    }
  
  len -= lastslash + 1;
  copy = mycalloc(len + 1 + spaced);
  
  if ((spaced != 0) && (gdata.spaces_in_filenames != 0))
    sprintf(copy, "\"%s\"", full + lastslash + 1);
  else
    strcpy(copy, full + lastslash + 1);
  
  /* replace any evil characters in the filename with underscores */
  for (i = 0; i < len; i++)
    {
     if (copy[i] == ' ')
        {
          if (gdata.spaces_in_filenames == 0)
            copy[i] = '_';
          continue;
        }
      if (copy[i] == '|' || copy[i] == ':' || copy[i] == '*' ||
          copy[i] == '?' || copy[i] == '<' || copy[i] == '>')
        {
          copy[i] = '_';
        }
    }
  return copy;
}

const char* getfilename(const char * const full) {
   int i,lastslash;
   
   updatecontext();

   lastslash = -1;
   for (i=0; i<sstrlen(full); i++)
      if (full[i] == '/' || full[i] == '\\')
         lastslash=i;
   
   return full+lastslash+1;
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
  
  xdcclistfile_tmp = mycalloc(strlen(gdata.xdcclistfile)+5);
  xdcclistfile_bkup = mycalloc(strlen(gdata.xdcclistfile)+2);
  
  sprintf(xdcclistfile_tmp,  "%s.tmp", gdata.xdcclistfile);
  sprintf(xdcclistfile_bkup, "%s~",    gdata.xdcclistfile);
  
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
    u_fillwith_msg(uxdl,NULL,"A A A A A xdl");
  else
    u_fillwith_msg(uxdl,NULL,"A A A A A xdlfull");
  uxdl->method = method_fd; 
  uxdl->fd = fd;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_PUBLIC;
  
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
   char *tempstr2 = mycalloc(maxtextlengthshort);
   int filedescriptor;
   
   updatecontext();

   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Writing pid file...");
   
   filedescriptor=open(filename, O_WRONLY | O_TRUNC | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS);
   if (filedescriptor < 0) outerror(OUTERROR_TYPE_CRASH,"Cant Create PID File '%s': %s",filename,strerror(errno));
   
   snprintf(tempstr2,maxtextlengthshort-1,"%i\n",(int)getpid());
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
      outerror(OUTERROR_TYPE_CRASH,"Unable to Fork: %s", strerror(errno));
   else if (s > 0) {
      /* parent exits */
      exit(69);
      }

/*   struct rlimit r = { 0 }; */
/*   r.rlim_max = 0; */
/*   s = getrlimit(RLIMIT_NOFILE, &r); */
/*   if ( r.rlim_max < 1 || s < 0) */
/*      outerror(OUTERROR_TYPE_CRASH,"Couldn't get rlimit"); */
   
   for (i=0; i<3; i++) close(i);
/*   for (i=0; i< r.rlim_max; i++) close(i); */
   
   s = setsid();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH,"Couldn't setsid: %s", strerror(errno));
   
   /* parent forks */
   s = fork();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH,"Unable to Fork: %s", strerror(errno));
   else if (s > 0)
      /* parent exits */
      exit(69);
   
   
   /* background continues... */
   
/*   umask(0); */
   s = open("/dev/null", O_RDWR); /* stdin */
   dup(s);                        /* stdout */
   dup(s);                        /* stderr */
   
   mylog(CALLTYPE_NORMAL,"Entered Background Mode");
   gdata.background = 2;
   
/*   execlp(program,"iroffer","--background-mode--",config,NULL); */
/*   exit(0); */
   
   }

#if !defined(NO_SIGINFO)
static void iroffer_signal_handler(int signo, siginfo_t *sinfo, void *unused)
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
          
          mylog(CALLTYPE_NORMAL,"iroffer exited (signal forced!)\n\n");
          printf("iroffer exited (signal forced!)\n");
          
          if (gdata.pidfile)
            {
              unlink(gdata.pidfile);
            }
          
          exit_iroffer();
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
        
        gdata.crashing = 1;
        
        ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                "!!! iroffer has received a fatal signal. !!!");
        ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                "Signal %d (%s)", signo, strsignal(signo));
        
#if !defined(NO_SIGINFO)
        switch (signo)
          {
          case SIGBUS:
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
#if !defined(NO_SIGCODES)
            switch (sinfo->si_code)
              {
              case BUS_ADRALN:
                code = "invalid address alignment";
                break;
                
              case BUS_ADRERR:
                code = "non-existant physical address";
                break;
                
              case BUS_OBJERR:
                code = "object specific hardware error";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGILL:
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
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
                code = "coprocessor error";
                break;
                
              case ILL_BADSTK:
                code = "internal stack error";
                break;
                
              default:
                code = "Unknown";
                break;
                
              }
            
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGFPE:
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
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
            
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGSEGV:
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Faulting Address: 0x%.8lX", (unsigned long)sinfo->si_addr);
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
            
            ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                    "Code: %s",code);
#endif
            break;
            
          case SIGABRT:
          default:
            break;
            
          }
#endif
        
        ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Context Trace:");
        
        dumpcontext();
        dumpgdata();
        
        ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Crashing... Please report this problem to dinoex");
        
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
   int i,count,last;
   
   updatecontext();

   count = 0;
   last = gdata.ignore;
   
   for (i=0; i<INAMNT_SIZE; i++) {
      count += gnetwork->inamnt[i];
      }

   if (count > 6)
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
  
  tempstr = mycalloc(maxtextlength);
  getstatusline(tempstr,maxtextlength);
  
  mylog(CALLTYPE_NORMAL, "%s", tempstr);
  
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
   upload *ul;
   transfer *tr;
   dccchat_t *chat;
   gnetwork_t *backup;
   int ss;
   
   updatecontext();

   backup = gnetwork;
   
   if ( SAVEQUIT )
      write_statefile();
   
   if (gdata.exiting || has_closed_servers()) {
      if (gdata.exiting)
         ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Shutting Down (FORCED)");
      else
         ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Shutting Down");
      
      for (chat = irlist_get_head(&gdata.dccchats);
           chat;
           chat = irlist_delete(&gdata.dccchats,chat))
        {
          writedccchat(chat, 0, "iroffer exited (shutdown), Closing DCC Chat\n");
          shutdowndccchat(chat,1);
        }
      
      mylog(CALLTYPE_NORMAL,"iroffer exited (shutdown)\n\n");

      tostdout_disable_buffering();
      uninitscreen();
      shutdown_dinoex();
      if (gdata.pidfile) unlink(gdata.pidfile);
      exit_iroffer();
      }
   
   
   ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Shutting Down... (Issue \"SHUTDOWN\" again to force quit)");
   
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       /* empty queue */
       gnetwork = &(gdata.networks[ss]);
       cleanannounce();
       irlist_delete_all(&gdata.networks[ss].serverq_fast);
       irlist_delete_all(&gdata.networks[ss].serverq_normal);
       irlist_delete_all(&gdata.networks[ss].serverq_slow);
       irlist_delete_all(&gdata.networks[ss].serverq_channel);
     }
   
   /* close connections */
   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       gnetwork = &(gdata.networks[tr->net]);
       notice(tr->nick,"** Shutting Down. Closing Connection. (Resume Supported)");
       
       FD_CLR(tr->con.clientsocket, &gdata.writeset);
       FD_CLR(tr->con.clientsocket, &gdata.readset);
       if (tr->con.listensocket != FD_UNUSED)
         {
           close(tr->con.listensocket);
         }
       if (tr->con.clientsocket != FD_UNUSED)
         {
           shutdown_close(tr->con.clientsocket);
         }
       tr->xpack->file_fd_count--;
       if (!tr->xpack->file_fd_count && (tr->xpack->file_fd != FD_UNUSED))
         {
           close(tr->xpack->file_fd);
           tr->xpack->file_fd = FD_UNUSED;
           tr->xpack->file_fd_location = 0;
         }
       tr->tr_status = TRANSFER_STATUS_DONE;
       
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_YELLOW,"XDCC Transfer to %s Closed",tr->nick);
       
       tr = irlist_get_next(tr);
     }
   
   /* close upload connections */
   ul = irlist_get_head(&gdata.uploads);
   while (ul)
     {
       gnetwork = &(gdata.networks[ul->net]);
       notice(ul->nick,"** Shutting Down. Closing Upload Connection. (Resume Supported)");
       
       FD_CLR(ul->con.clientsocket, &gdata.writeset);
       FD_CLR(ul->con.clientsocket, &gdata.readset);
       if (ul->con.clientsocket != FD_UNUSED)
         {
           shutdown_close(ul->con.clientsocket);
         }
       if (ul->filedescriptor != FD_UNUSED)
         {
           close(ul->filedescriptor);
         }
       ul->ul_status = UPLOAD_STATUS_DONE;
       
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_YELLOW,
               "Upload Transfer from %s Closed",ul->nick);
       ul = irlist_get_next(ul);
     }
   
   /* quit */
   if (has_closed_servers() == 0)
     {
       tempstr2 = mycalloc(maxtextlengthshort);
       tempstr2 = getuptime(tempstr2, 1, gdata.startuptime, maxtextlengthshort);
       for (ss=0; ss<gdata.networks_online; ss++)
         {
	   gnetwork = &(gdata.networks[ss]);
       writeserver(WRITESERVER_NORMAL,
                   "QUIT :iroffer-dinoex " VERSIONLONG "%s%s - running %s",
                   gdata.hideos ? "" : " - ",
                   gdata.hideos ? "" : gdata.osstring,
                   tempstr2);
         }
       mydelete(tempstr2);
     }
   
   gdata.exiting = 1;
   gdata.delayedshutdown = 0;
   
   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Waiting for Server Queue To Flush...");
   gnetwork = backup;
   
   }

void quit_server(void)
{
  updatecontext();
  
  /* quit */
  if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
    {
      writeserver(WRITESERVER_NOW, "QUIT :Changing Servers");
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_RED,
              "Changing Servers on %s", gnetwork->name);
      close_server();
    }
  
  if (gnetwork->serverstatus == SERVERSTATUS_TRYING)
    {
      close_server();
    }
  
  /* delete the slow queue */
  cleanannounce();
  irlist_delete_all(&(gnetwork->serverq_slow));
  irlist_delete_all(&(gnetwork->serverq_normal));
  irlist_delete_all(&(gnetwork->serverq_fast));
  
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
}

void switchserver(int which)
{
  server_t *ss;
  
  quit_server();
  updatecontext();
   
  if (which < 0)
    {
      int i;
      
      i = irlist_size(&(gnetwork->servers));
      
      which = (int) (((float)i)*rand()/(RAND_MAX+0.0));
      
      while (which > i || which < 0)
        {
          if (which < 0) which += i;
          if (which > i) which -= i;
        }
    }
  
  ss = irlist_get_nth(&(gnetwork->servers), which);
  
  connectirc(ss);
  
  gnetwork->serverconnectbackoff++;
  gnetwork->servertime = 0;
}

char* getstatusline(char *str, int len)
{
  int i,srvq;
  ir_uint64 xdccsent;
  ir_uint64 xdccrecv;
  ir_uint64 xdccsum;
  int ss;
  
  updatecontext();
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsent[i];
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
       
       i = snprintf(str, len,
               "Stat: %i/%i Sls, %i/%i Q, %i/%i I, %i SrQ (Bdw: %" LLPRINTFMT "uK, %1.1fK/s, %1.1fK/s Up, %1.1fK/s Down)",
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               irlist_size(&gdata.idlequeue),
               gdata.idlequeuesize,
               srvq,
               (unsigned long long)(xdccsent/1024),
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               ((float)xdccrecv)/XDCC_SENT_SIZE/1024.0,
               ((float)xdccsum)/XDCC_SENT_SIZE/1024.0);
    }
  else
    {
       i = snprintf(str, len,
               "Stat: %i/%i Sls, %i/%i Q, %1.1fK/s Rcd, %i SrQ (Bdw: %" LLPRINTFMT "uK, %1.1fK/s, %1.1fK/s Rcd)",
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               gdata.record,
               srvq,
               (unsigned long long)(xdccsent/1024),
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               gdata.sentrecord);
    }
  
  if ((i < 0) || (i >= len))
    {
      str[0] = '\0';
    }
  return str;
}

char* getstatuslinenums(char *str, int len)
{
  int i,gcount,srvq;
  float scount,ocount;
  xdcc *xd;
  ir_uint64 xdccsent;
  int ss;
  
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
  
  i = snprintf(str, len,
               "stat %i %1.0f %i %1.0f %i %i %i %i %i %i %1.1f %i %" LLPRINTFMT "u %1.1f %1.1f",
               irlist_size(&gdata.xdccs),
               ocount/1024/1024,
               gcount,
               scount/1024/1024,
               irlist_size(&gdata.trans),
               gdata.slotsmax,
               irlist_size(&gdata.mainqueue),
               gdata.queuesize,
               0, 0,
               gdata.record,
               srvq,
               (unsigned long long)(xdccsent/1024),
               ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
               gdata.sentrecord);
   if ((i < 0) || (i >= len))
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
  int len;
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
  
  tempstr = mycalloc(len);
  
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
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_YELLOW,"Not Sending XDCC LIST to: %s (nolist set)",tempstr);
      
      notice(tempstr,
             "The Owner Has Requested That No Lists Be Sent In The Next %li Minute%s",
             1+(gdata.nolisting-gdata.curtime)/60,
             ((1+(gdata.nolisting-gdata.curtime)/60) != 1 ? "s" : ""));
    }
  else
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_YELLOW,"Sending XDCC LIST to: %s",tempstr);
      
      cmd = NULL;
      if (group)
        {
           if (strcasecmp(group, "ALL") == 0)
             {
                u_fillwith_msg(&ui,tempstr, "A A A A A xdlfull");
             }
           else
             {
                cmd = mycalloc(maxtextlength);
                snprintf(cmd, maxtextlength-1, "A A A A A xdlgroup %s", group);
                u_fillwith_msg(&ui,tempstr, cmd);
             }
           mydelete(group);
        }
      else
        {
          u_fillwith_msg(&ui,tempstr,"A A A A A xdl");
        }
      if (gdata.xdcclist_by_privmsg)
        ui.method = method_xdl_user_privmsg;
      else
        ui.method = method_xdl_user_notice;
      ui.net = gnetwork->net;
      ui.level = ADMIN_LEVEL_PUBLIC;
      u_parseit(&ui);
      mydelete(cmd);
    }
  
  mydelete(tempstr);
}

int isthisforme (const char *dest, char *msg1) {
   if (!msg1 || !dest) { outerror(OUTERROR_TYPE_WARN_LOUD,"isthisforme() got NULL value"); return 1; }
   
   if (!gnetwork->caps_nick)
     {
       return 0;
     }

   if (
         !strcmp(msg1,"\1CLIENTINFO") || !strcmp(msg1,"\1CLIENTINFO\1")
      || !strcmp(msg1,"\1PING") || !strcmp(msg1,"\1PING\1") 
      || !strcmp(msg1,"\1VERSION") || !strcmp(msg1,"\1VERSION\1")
      || !strcmp(msg1,"\1UPTIME") || !strcmp(msg1,"\1UPTIME\1") 
      || !strcmp(msg1,"\1STATUS") || !strcmp(msg1,"\1STATUS\1")
      || (!strcmp(gnetwork->caps_nick,dest) && !strcmp(caps(msg1),"\1DCC"))
      || (!strcmp(gnetwork->caps_nick,dest) && !strcmp(caps(msg1),"ADMIN"))
      || (!strcmp(gnetwork->caps_nick,dest) && (!strcmp(caps(msg1),"XDCC") || !strcmp(msg1,"\1XDCC") || !strcmp(caps(msg1),"CDCC") || !strcmp(msg1,"\1CDCC")))
      || !strcmp(dest,gnetwork->caps_nick)
      ) return 1;
   
   return 0;
   
   }

void reinit_config_vars(void)
{
  autoqueue_t *aq;
  regex_t *rh;
  int si;
  
  /* clear old config items */
  for (aq = irlist_get_head(&gdata.autoqueue);
       aq;
       aq = irlist_delete(&gdata.autoqueue, aq))
    {
       mydelete(aq->word);
       mydelete(aq->message);
    }
  for (rh = irlist_get_head(&gdata.autoignore_exclude);
       rh;
       rh = irlist_delete(&gdata.autoignore_exclude, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.adminhost);
       rh;
       rh = irlist_delete(&gdata.adminhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.hadminhost);
       rh;
       rh = irlist_delete(&gdata.hadminhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.uploadhost);
       rh;
       rh = irlist_delete(&gdata.uploadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.downloadhost);
       rh;
       rh = irlist_delete(&gdata.downloadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.nodownloadhost);
       rh;
       rh = irlist_delete(&gdata.nodownloadhost, rh))
    {
      regfree(rh);
    }
  for (rh = irlist_get_head(&gdata.unlimitedhost);
       rh;
       rh = irlist_delete(&gdata.unlimitedhost, rh))
    {
      regfree(rh);
    }
  mydelete(gdata.pidfile);
  mydelete(gdata.config_nick);
  gdata.networks_online = 0;
  for (si=0; si<MAX_NETWORKS; si++)
  {
    server_t *ss;
    for (ss = irlist_get_head(&gdata.networks[si].servers);
         ss;
         ss = irlist_delete(&gdata.networks[si].servers, ss))
      {
        mydelete(ss->hostname);
        mydelete(ss->password);
      }
    mydelete(gdata.networks[si].nickserv_pass);
    mydelete(gdata.networks[si].config_nick);
    mydelete(gdata.networks[si].user_modes);
    mydelete(gdata.networks[si].local_vhost);
    irlist_delete_all(&gdata.networks[si].r_channels);
    irlist_delete_all(&gdata.networks[si].server_join_raw);
    irlist_delete_all(&gdata.networks[si].server_connected_raw);
    irlist_delete_all(&gdata.networks[si].channel_join_raw);
    irlist_delete_all(&gdata.networks[si].proxyinfo);
    gdata.networks[si].connectionmethod.how = how_direct;
    gdata.networks[si].usenatip = 0;
    gdata.networks[si].getip_net = -1;
  } /* networks */
  mydelete(gdata.logfile);
  gdata.logrotate = 0;
  gdata.logstats = 1;
  mydelete(gdata.user_realname);
  mydelete(gdata.user_modes);
  gdata.tcprangestart = 0;
  gdata.tcprangelimit = 65535;
  gdata.slotsmax = gdata.queuesize = 0;
  gdata.idlequeuesize = 0;
  gdata.maxtransfersperperson = 1;
  gdata.maxqueueditemsperperson = 1;
  gdata.maxidlequeuedperperson = 1;
  gdata.autoignore_threshold = 10;
  gdata.lowbdwth = 0;
  gdata.punishslowusers = 0;
  gdata.nomd5sum = 0;
  gdata.getipfromserver = 0;
  gdata.noduplicatefiles = 0;
  irlist_delete_all(&gdata.adddir_exclude);
  irlist_delete_all(&gdata.geoipcountry);
  irlist_delete_all(&gdata.nogeoipcountry);
  irlist_delete_all(&gdata.geoipexcludenick);
  irlist_delete_all(&gdata.autoadd_dirs);
  irlist_delete_all(&gdata.autocrc_exclude);
  irlist_delete_all(&gdata.filedir);
  irlist_delete_all(&gdata.http_vhost);
  irlist_delete_all(&gdata.telnet_vhost);
  irlist_delete_all(&gdata.weblist_info);
  irlist_delete_all(&gdata.mime_type);
  mydelete(gdata.enable_nick);
  mydelete(gdata.owner_nick);
  mydelete(gdata.geoipdatabase);
  mydelete(gdata.respondtochannellistmsg);
  gdata.need_voice = 0;
  gdata.need_level = 0;
  gdata.hide_list_info = 0;
  gdata.xdcclist_grouponly = 0;
  gdata.auto_default_group = 0;
  gdata.auto_path_group = 0;
  gdata.start_of_month = 1;
  gdata.restrictprivlistmain = 0;
  gdata.restrictprivlistfull = 0;
  gdata.groupsincaps = 0;
  gdata.ignoreuploadbandwidth = 0;
  gdata.holdqueue = 0;
  gdata.removelostfiles = 0;
  gdata.ignoreduplicateip = 0;
  gdata.hidelockedpacks = 0;
  gdata.disablexdccinfo = 0;
  gdata.atfind = 0;
  gdata.waitafterjoin = 200;
  gdata.noautorejoin = 0;
  gdata.auto_crc_check = 0;
  gdata.nocrc32 = 0;
  gdata.direct_file_access = 0;
  gdata.autoaddann_short = 0;
  gdata.spaces_in_filenames = 0;
  gdata.autoadd_time = 0;
  gdata.restrictsend_warning = 0;
  gdata.restrictsend_timeout = RESTRICTSEND_TIMEOUT;
  gdata.send_statefile_minute = 0;
  gdata.send_statefile_minute = 0;
  gdata.extend_status_line = 0;
  gdata.include_subdirs = 0;
  gdata.max_uploads = 65000;
  gdata.max_upspeed = 65000;
  gdata.max_find = 0;
  gdata.restrictsend_delay = 0;
  gdata.adminlevel = ADMIN_LEVEL_FULL;
  gdata.hadminlevel = ADMIN_LEVEL_HALF;
  gdata.monitor_files = 20;
  gdata.xdcclist_by_privmsg = 0;
  gdata.autoadd_delay = 0;
  gdata.balanced_queue = 0;
  gdata.http_port = 0;
  gdata.passive_dcc = 0;
  gdata.telnet_port = 0;
  gdata.remove_dead_users = 0;
  gdata.upnp_router = 0;
  gdata.http_search = 0;
  gdata.send_listfile = 0;
  mydelete(gdata.admin_job_file);
  mydelete(gdata.autoaddann);
  mydelete(gdata.autoadd_group);
  mydelete(gdata.send_statefile);
  mydelete(gdata.xdccremovefile);
  mydelete(gdata.autoadd_sort);
  mydelete(gdata.http_admin);
  mydelete(gdata.http_dir);
  mydelete(gdata.group_seperator);
  mydelete(gdata.usenatip);
  mydelete(gdata.logfile_notices);
  mydelete(gdata.logfile_messages);
  mydelete(gdata.trashcan_dir);
  mydelete(gdata.xdccxmlfile);
  gdata.transferminspeed = gdata.transfermaxspeed = 0.0;
  gdata.overallmaxspeed = gdata.overallmaxspeeddayspeed = 0;
  gdata.overallmaxspeeddaytimestart = gdata.overallmaxspeeddaytimeend = 0;
  gdata.overallmaxspeeddaydays = 0x7F; /* all days */
  {
    int ii;
    for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
      {
        gdata.transferlimits[ii].limit = 0;
      }
  }
  gdata.hideos = 0;
  gdata.lognotices = 0;
  gdata.logmessages = 0;
  gdata.timestampconsole = 0;
  gdata.quietmode = 0;
  gdata.notifytime = 5;
  gdata.respondtochannelxdcc = 0;
  gdata.respondtochannellist = 0;
  gdata.smallfilebypass = 0;
  mydelete(gdata.creditline);
  mydelete(gdata.headline);
  mydelete(gdata.nickserv_pass);
  mydelete(gdata.periodicmsg_nick);
  mydelete(gdata.periodicmsg_msg);
  gdata.periodicmsg_time = 0;
  gdata.uploadmaxsize = 0;
  gdata.uploadminspace = 0;
  mydelete(gdata.uploaddir);
  gdata.restrictlist = gdata.restrictsend = gdata.restrictprivlist = 0;
  mydelete(gdata.restrictprivlistmsg);
  mydelete(gdata.loginname);
  mydelete(gdata.statefile);
  mydelete(gdata.xdcclistfile);
  gdata.xdcclistfileraw = 0;
  mydelete(gdata.adminpass);
  mydelete(gdata.hadminpass);
  return;
}

void initprefixes(void)
{
  memset(&(gnetwork->prefixes), 0, sizeof(gnetwork->prefixes));
  gnetwork->prefixes[0].p_mode   = 'o';
  gnetwork->prefixes[0].p_symbol = '@';
  gnetwork->prefixes[1].p_mode   = 'v';
  gnetwork->prefixes[1].p_symbol = '+';
}

void initchanmodes(void)
{
  memset(&(gnetwork->chanmodes), 0, sizeof(gnetwork->chanmodes));
  gnetwork->chanmodes[0] = 'b';
  gnetwork->chanmodes[1] = 'k';
  gnetwork->chanmodes[1] = 'l';
}

void initvars(void)
{
  int ss;

  memset(&gdata, 0, sizeof(gdata_t));
  
  reinit_config_vars();
  
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
#endif
  
  gdata.startuptime = gdata.curtime = time(NULL);
  gdata.curtimems = ((unsigned long long)gdata.curtime) * 1000;
  
  gdata.sendbuff = mycalloc(BUFFERSIZE);
  gdata.console_input_line = mycalloc(INPUT_BUFFER_LENGTH);
  
  gdata.last_logrotate = gdata.curtime;
  
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
   int ss;
   
   updatecontext();
   
   srand((unsigned int)( (getpid()*5000) + (gdata.curtime%5000) ));
   
   if (!gdata.background) {
      initscreen(1, 1);
      gotobot();
      gototop();
      }
   
   printf("\n");
   if (!gdata.background && !gdata.nocolor) printf("\x1b[1;33m");
   printf("Welcome to iroffer-dinoex - http://iroffer.dinoex.net/\n"
          "Version " VERSIONLONG "\n");
   if (!gdata.background && !gdata.nocolor) printf("\x1b[0m");
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
   if (gdata.chrootdir) {
      printf( "** Changing root filesystem to '%s'\n", gdata.chrootdir );
      if( chroot( gdata.chrootdir ) < 0 ) {
		   outerror( OUTERROR_TYPE_CRASH, "Can't chroot: %s", strerror(errno));
      }
      if( chdir("/") < 0 ) {
        outerror( OUTERROR_TYPE_CRASH, "Can't chdir: %s", strerror(errno) );
      }
   }
#endif
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
      if (setgroups(ngroups,groups) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"Can't set group list: %s", strerror(errno));
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
#if BLOCKROOT
		OUTERROR_TYPE_CRASH,
#else
		OUTERROR_TYPE_WARN_LOUD,
#endif
		"iroffer should not be run as root!"
		);
     }
   
   if (!gdata.background)
      printf("** Window Size: %ix%i\n",gdata.termcols,gdata.termlines);
   
   tempstr23 = mycalloc(maxtextlength);
   printf("** Started on: %s\n",getdatestr(tempstr23,0,maxtextlength));
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
   
   if (MAXTRANS < 1)
     {
       outerror(OUTERROR_TYPE_CRASH, "fd limit of %u is too small",
                gdata.max_fds_from_rlimit);
     }
   
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
   
   mylog(CALLTYPE_NORMAL,"iroffer-dinoex started " VERSIONLONG);

   getos();
   
   if (gdata.statefile)
     {
       read_statefile();
       write_statefile();
       autotrigger_rebuild();
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
       ir_boutput_init(&gdata.stdout_buffer, fileno(stdout), 0);
       gdata.stdout_buffer_init = 1;
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
  struct tm *lthen;
  char *newname;
  int call_val;
  
  updatecontext();
  
  if (!gdata.logrotate ||
      !gdata.logfile ||
      (gdata.last_logrotate + gdata.logrotate) > gdata.curtime)
    {
      return;
    }
  
  lthen = localtime(&gdata.curtime);
  
  newname = mymalloc(strlen(gdata.logfile) + 12);
  sprintf(newname, "%s.%04i-%02i-%02i",
          gdata.logfile,
          lthen->tm_year+1900,
          lthen->tm_mon+1,
          lthen->tm_mday);
  
  mylog(CALLTYPE_NORMAL,"Rotating Log to '%s'", newname);
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
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
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
      
      gdata.last_logrotate = gdata.curtime;
      write_statefile();
      mylog(CALLTYPE_NORMAL,"Rotated Log to '%s'", newname);
    }
  
  mydelete(newname);
  
  return;
}


void createpassword(void) {
#ifndef NO_CRYPT
   char pw1[maxtextlengthshort], pw2[maxtextlengthshort];
   int len, ok, saltnum;
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
         { fprintf(stderr,"Couldn't Read Your Password, Try Again\n"); exit(66); }
      if (pw1[len-1] == '\n') { pw1[len-1] = '\0'; len--;}
      
      if ( len < 5 )
         printf("Wrong Length, Try Again\n");
      else
         ok = 1;
   }
   
   printf("And Again for Verification: ");
   fflush(stdout);

   if ( (len = read(0,pw2,maxtextlengthshort-1)) < 0 )
      { fprintf(stderr,"Couldn't Read Your Password, Try Again\n"); exit(66); }
   if (pw2[len-1] == '\n') { pw2[len-1] = '\0'; len--;}
   
   if ( strcmp(pw1,pw2) )
      { fprintf(stderr,"The Password Didn't Match, Try Again\n"); exit(65); }
   
   
   srand((unsigned int)( (getpid()*5000) + (time(NULL)%5000) ));
   saltnum = (int)(4096.0*rand()/(RAND_MAX+0.0));
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
   
   if (pwout && strlen(pwout) >= 13)
      printf("\n"
             "To use \"%s\" as your password use the following in your config file:\n"
             "adminpass %s\n"
             "\n"
             ,pw1,pwout);
   else
      printf("\n"
             "The crypt() function does not appear to be working correctly\n"
             "\n");
   
#else
   printf("iroffer was compiled without encrypted password support.  You do not need to ecrypt your password.\n");
#endif
   }

/* 0 .. 63 */
char inttosaltchar (int n) {
   
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

void notifyqueued(void)
{
  int i;
  ir_uint64 xdccsent;
  
  updatecontext();
  
  if ( !gdata.exiting && irlist_size(&gdata.mainqueue))
    {
      if (gdata.lowbdwth)
        {
          xdccsent = 0;
          for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += gdata.xdccsent[i];
          
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D,COLOR_YELLOW,
                  "Notifying %d Queued People (%.1fK/sec used, %dK/sec limit)",
                  irlist_size(&gdata.mainqueue),
                  ((float)xdccsent)/XDCC_SENT_SIZE/1024.0,
                  gdata.lowbdwth);
        }
      else
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D,COLOR_YELLOW,
                  "Notifying %d Queued People",
                  irlist_size(&gdata.mainqueue));
        }
    }
  
  notifyqueued_nick(NULL);
}

void notifybandwidth(void)
{
  int i;
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
          notice_slow(tr->nick,"%s bandwidth limit: %2.1f of %2.1fKB/sec used. Your share: %2.1fKB/sec.",
                      save_nick(gnetwork->user_nick),
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
          (gdata.curtime-tr->con.connecttime > MIN_TL) &&
          (tr->lastspeed*10 > tr->maxspeed*9))
        {
          /* send if over 90% */
          gnetwork = &(gdata.networks[tr->net]);
          notice_slow(tr->nick,"Pack bandwidth limit: %2.1f of %2.1fKB/sec used.",
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
               "Pack %d: File '%s' can no longer be accessed: %s",
                number_of_pack(xpack),
                xpack->file, strerror(errno));
      if ((gdata.removelostfiles) && (errno == ENOENT))
        return 1;
      return 0;
    }
  
  if (st.st_size == 0)
    {
      outerror(OUTERROR_TYPE_WARN,
               "File '%s' has size of 0 bytes", xpack->file);
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
      if (!gdata.xdcclistfile || strcmp(xpack->file, gdata.xdcclistfile))
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
  int userinqueue = 0;
  
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
          if (strcmp(pq->hostname,"man"))
            {
	      userinqueue++;
              if ( userinqueue > gdata.maxqueueditemsperperson )
                {
                  notice(pq->nick,"** Removed From Queue: To many requests");
                  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
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
  
  if (!gdata.restrictsend)
    {
      return;
    }
  
  backup = gnetwork;
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
    {
      gnetwork = &(gdata.networks[tr->net]);
      if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
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
          else if ((gdata.curtime - tr->restrictsend_bad) >= gdata.restrictsend_timeout)
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
