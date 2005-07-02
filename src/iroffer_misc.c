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
 * @(#) iroffer_misc.c 1.247@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_misc.c|20050116225153|27533
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"


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
   
   if ( !irlist_size(&gdata.servers)
        || gdata.config_nick == NULL || gdata.user_realname == NULL
        || gdata.slotsmax == 0)
      outerror(OUTERROR_TYPE_CRASH,"Config File Missing Necessary Information");

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


typedef struct
{
  const char *name;
  int *value_normal;
  int *value_rehash;
} config_parse_bool_t;

static const config_parse_bool_t config_parse_bool[] = {
  {"logstats",             &gdata.logstats,             &gdata.logstats },
  {"hideos",               &gdata.hideos,               &gdata.hideos },
  {"lognotices",           &gdata.lognotices,           &gdata.lognotices },
  {"logmessages",          &gdata.logmessages,          &gdata.logmessages },
  {"timestampconsole",     &gdata.timestampconsole,     &gdata.timestampconsole },
  {"respondtochannelxdcc", &gdata.respondtochannelxdcc, &gdata.respondtochannelxdcc },
  {"respondtochannellist", &gdata.respondtochannellist, &gdata.respondtochannellist },
  {"quietmode",            &gdata.quietmode,            &gdata.quietmode },
  {"restrictlist",         &gdata.restrictlist,         &gdata.restrictlist },
  {"restrictprivlist",     &gdata.restrictprivlist,     &gdata.restrictprivlist },
  {"restrictsend",         &gdata.restrictsend,         &gdata.restrictsend },
  {"nomd5sum",             &gdata.nomd5sum,             &gdata.nomd5sum },
  {"getipfromserver",      &gdata.getipfromserver,      &gdata.getipfromserver },
  {"noduplicatefiles",     &gdata.noduplicatefiles,     &gdata.noduplicatefiles },
  {"need_voice",           &gdata.need_voice,           &gdata.need_voice },
  {"hide_list_info",       &gdata.hide_list_info,       &gdata.hide_list_info },
  {"xdcclist_grouponly",   &gdata.xdcclist_grouponly,   &gdata.xdcclist_grouponly },
  {"auto_default_group",   &gdata.auto_default_group,   &gdata.auto_default_group },
  {"restrictprivlistmain", &gdata.restrictprivlistmain, &gdata.restrictprivlistmain },
  {"restrictprivlistfull", &gdata.restrictprivlistfull, &gdata.restrictprivlistfull },
  {"groupsincaps",         &gdata.groupsincaps,         &gdata.groupsincaps },
  {"ignoreuploadbandwidth", &gdata.ignoreuploadbandwidth, &gdata.ignoreuploadbandwidth },
  {"holdqueue",            &gdata.holdqueue,            &gdata.holdqueue },
  {"removelostfiles",      &gdata.removelostfiles,      &gdata.removelostfiles },
};

typedef struct
{
  const char *name;
  int *value_normal;
  int *value_rehash;
  int min;
  int max;
  int mult;
} config_parse_int_t;

static const config_parse_int_t config_parse_int[] = {
  {"notifytime",      &gdata.notifytime,      &gdata.notifytime,      0, 1000000, 1 },
  {"smallfilebypass", &gdata.smallfilebypass, &gdata.smallfilebypass, 0, 1024*1024, 1024 },
  {"tcprangestart",   &gdata.tcprangestart,   &gdata.tcprangestart,   1024, 65000, 1 },
  {"overallmaxspeed", &gdata.overallmaxspeed, &gdata.overallmaxspeed, 0, 1000000, 4 },
  {"overallmaxspeeddayspeed", &gdata.overallmaxspeeddayspeed, &gdata.overallmaxspeeddayspeed, 0, 1000000, 4 },
  {"queuesize",       &gdata.queuesize,       &gdata.queuesize,       0, 1000000, 1 },
  {"lowbdwth",        &gdata.lowbdwth,        &gdata.lowbdwth,        0, 1000000, 1 },
  {"maxqueueditemsperperson", &gdata.maxqueueditemsperperson, &gdata.maxqueueditemsperperson, 1, 1000000, 1 },
  {"maxtransfersperperson", &gdata.maxtransfersperperson, &gdata.maxtransfersperperson, 1, 1000000, 1 },
  {"punishslowusers", &gdata.punishslowusers, &gdata.punishslowusers, 0, 1000000, 1 },
  {"autoignore_threshold", &gdata.autoignore_threshold, &gdata.autoignore_threshold, 10, 600, 1 },
  {"start_of_month",  &gdata.start_of_month,  &gdata.start_of_month,  1, 31, 1 },
};

typedef struct
{
  const char *name;
  char **value_normal;
  char **value_rehash;
} config_parse_str_t;

static const config_parse_str_t config_parse_str[] = {
  {"user_nick",            &gdata.config_nick,          &gdata.r_config_nick },
  {"user_realname",        &gdata.user_realname,        &gdata.user_realname },
  {"user_modes",           &gdata.user_modes,           &gdata.user_modes },
  {"headline",             &gdata.headline,             &gdata.headline },
  {"creditline",           &gdata.creditline,           &gdata.creditline },
  {"loginname",            &gdata.loginname,            &gdata.loginname },
  {"nickserv_pass",        &gdata.nickserv_pass,        &gdata.nickserv_pass },
  {"restrictprivlistmsg",  &gdata.restrictprivlistmsg,  &gdata.restrictprivlistmsg },
  {"enable_nick",          &gdata.enable_nick,          &gdata.enable_nick },
  {"admin_job_file",       &gdata.admin_job_file,       &gdata.admin_job_file },
};

void update_natip (const char *var)
{
  struct hostent *hp;
  struct in_addr old;
  struct in_addr in;
  char *oldtxt;

  if (var == NULL)
    return;

  bzero((char *)&in, sizeof(in));
  if (inet_aton(var, &in) == 0)
    {
      hp = gethostbyname2(var, AF_INET);
      if (hp == NULL)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid NAT Host, Ignoring: %s",hstrerror(h_errno));
          return;
        }
      if ((unsigned)hp->h_length > sizeof(in) || hp->h_length < 0)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid DNS response, Ignoring: %s",hstrerror(h_errno));
          return;
        }
      memcpy(&in, hp->h_addr_list[0], sizeof(in));
    }
  
  old.s_addr = htonl(gdata.ourip);
  gdata.usenatip = 1;
  if (old.s_addr == in.s_addr)
    return;
  
  gdata.ourip = ntohl(in.s_addr);
  oldtxt = strdup(inet_ntoa(old));
  mylog(CALLTYPE_NORMAL,"DCC IP changed from %s to %s", oldtxt, inet_ntoa(in));
  free(oldtxt);
  
  if (gdata.debug > 0) ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"ip=0x%8.8lX\n",gdata.ourip);
  
  /* check for 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 */
  if (((gdata.ourip & 0xFF000000UL) == 0x0A000000UL) ||
      ((gdata.ourip & 0xFFF00000UL) == 0xAC100000UL) ||
      ((gdata.ourip & 0xFFFF0000UL) == 0xC0A80000UL))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"usenatip of %lu.%lu.%lu.%lu looks wrong, this is probably not what you want to do",
                (gdata.ourip >> 24) & 0xFF,
                (gdata.ourip >> 16) & 0xFF,
                (gdata.ourip >>  8) & 0xFF,
                (gdata.ourip      ) & 0xFF);
     }
}

void getconfig_set (const char *line, int rehash)
{
  char *type;
  char *var;
  char *a,*b,*c;
  const char *found;
  int i,j;
  
  updatecontext();
  
  i = strlen(line);
  type = mycalloc(i+1);
  var = mycalloc(i+1);
  
  for (i=0; ; i++)
    {
      if (line[i] == ' ')
        {
          type[i] = '\0';
          i++;
          break;
        }
      type[i] = line[i];
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
  
  for (i=0; i<(sizeof(config_parse_bool)/sizeof(config_parse_bool_t)); i++)
    {
      if (!strcmp(config_parse_bool[i].name,type))
        {
          if (!strcmp("no", var))
            {
              if (rehash)
                {
                  *config_parse_bool[i].value_rehash = 0;
                }
              else
                {
                  *config_parse_bool[i].value_normal = 0;
                }
            }
          else if (!strcmp("yes", var) || (var[0] == 0))
            {
              if (rehash)
                {
                  *config_parse_bool[i].value_rehash = 1;
                }
              else
                {
                  *config_parse_bool[i].value_normal = 1;
                }
            }
          else
            {
              outerror(OUTERROR_TYPE_WARN,
                       "ignored '%s' because it has unknown args: '%s'",
                       type, var);
            }
          mydelete(var);
          goto done;
        }
    }
  
  for (i=0; i<(sizeof(config_parse_int)/sizeof(config_parse_int_t)); i++)
    {
      if (!strcmp(config_parse_int[i].name,type))
        {
          int rawval;
          char *endptr;
          
          rawval = (int)strtol(var, &endptr, 0);
          
          if (var[0] == 0)
            {
              outerror(OUTERROR_TYPE_WARN,
                       "ignored '%s' because it has no args.",
                       type);
            }
          else if (endptr[0] != 0)
            {
              outerror(OUTERROR_TYPE_WARN,
                       "ignored '%s' because it has invalid args: '%s'",
                       type, var);
            }
          else
            {
              if ((rawval < config_parse_int[i].min) || (rawval > config_parse_int[i].max))
                {
                  outerror(OUTERROR_TYPE_WARN,
                           "'%s': %d is out-of-range",
                           type, rawval);
                }
              rawval = between(config_parse_int[i].min, rawval, config_parse_int[i].max);
              rawval *= config_parse_int[i].mult;
              
              if (rehash)
                {
                  *config_parse_int[i].value_rehash = rawval;
                }
              else
                {
                  *config_parse_int[i].value_normal = rawval;
                }
            }
          mydelete(var);
          goto done;
        }
    }
  
  for (i=0; i<(sizeof(config_parse_str)/sizeof(config_parse_str_t)); i++)
    {
      if (!strcmp(config_parse_str[i].name,type))
        {
          if (var[0] == 0)
            {
              outerror(OUTERROR_TYPE_WARN,
                       "ignored '%s' because it has no args.",
                       type);
              mydelete(var);
            }
          else if (rehash)
            {
              mydelete(*config_parse_str[i].value_rehash);
              *config_parse_str[i].value_rehash = var;
            }
          else
            {
              mydelete(*config_parse_str[i].value_normal);
              *config_parse_str[i].value_normal = var;
            }
          goto done;
        }
    }
  
   /* parse it */
   if ( ! strcmp(type,"server"))
     {
       server_t *ss;
       char *portstr;
       ss = irlist_add(&gdata.servers, sizeof(*ss));
       ss->hostname = getpart(var,1);
       portstr = getpart(var,2);
       if (!ss->hostname)
         {
           outerror(OUTERROR_TYPE_WARN,"ignoring invalid server line");
           mydelete(portstr);
           irlist_delete(&gdata.servers, ss);
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
       pi = irlist_add(&gdata.proxyinfo, strlen(var) + 1);
       strcpy(pi, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"channel_join_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.channel_join_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"server_join_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.server_join_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"server_connected_raw"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.server_connected_raw, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( !strcmp(type,"adddir_exclude"))
     {
       char *cjr;
       cjr = irlist_add(&gdata.adddir_exclude, strlen(var) + 1);
       strcpy(cjr, var);
       mydelete(var);
     }
   else if ( ! strcmp(type,"channel")) {
      char *tptr = NULL, *tptr2 = NULL, *tname;
      int ok=1;
      channel_t *cptr = NULL;
      
      if (!rehash)
        {
          cptr = irlist_add(&gdata.channels, sizeof(channel_t));
        }
      else
        {
          cptr = irlist_add(&gdata.r_channels, sizeof(channel_t));
        }
      
      tname = getpart(var,1);
      caps(tname);
      cptr->name = mymalloc(strlen(tname)+1);
      strcpy(cptr->name,tname);
      cptr->headline = NULL;
      
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
                cptr->key = mymalloc(strlen(tptr2)+1);
                strcpy(cptr->key,tptr2);
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
                cptr->headline = mycalloc(strlen(found)+1);
                strcpy(cptr->headline,found);
                mydelete(tptr);
		break;
              }
            else ok=0;
            }
         else ok=0;
         
         mydelete(tptr);
         mydelete(tptr2);
         }
      
      if (!ok) ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR," !!! Bad syntax for channel %s in config file !!!",tname);
      
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
   else if ( ! strcmp(type,"adminhost"))
     {
       regex_t *ah;
       caps(var);
       
       for (i=strlen(var)-1; i>=0; i--)
         {
           if (var[i] == '@')
             {
               i = 0;
             }
           else if ((var[i] != '*') && (var[i] != '?') && (var[i] != '#'))
             {
               break;
             }
         }
       
       if ((i<0) || (!strchr(var,'@')))
         {
           outerror(OUTERROR_TYPE_WARN,"adminhost '%s' ignored because it's way too vague", var);
         }
       else
         {
           char *varr;
           varr = hostmasktoregex(var);
           ah = irlist_add(&gdata.adminhost, sizeof(regex_t));
           if (regcomp(ah,varr,REG_ICASE|REG_NOSUB))
             {
               irlist_delete(&gdata.adminhost, ah);
             }
           mydelete(varr);
         }
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
     }
   else if ( ! strcmp(type,"autosendpack"))
     {
       a = getpart(var,1); b = getpart(var,2); c = getpart(var,3);
       if (a && b && c)
         {
           gdata.autosend.pack = between(0,atoi(a),100000);
           mydelete(gdata.autosend.word);
           gdata.autosend.word = b;
           mydelete(gdata.autosend.message);
           
           gdata.autosend.message = mymalloc(strlen(var) - 2 - strlen(a) - strlen(b) + 1);
           strcpy(gdata.autosend.message, var + strlen(a) + strlen(b) + 2);
           mydelete(a); mydelete(c);
         }
       else
         {
           mydelete(a); mydelete(b); mydelete(c);
         }
       mydelete(var);
     }
   else if ( ! strcmp(type,"logrotate")) {
      if (!strcmp(var,"daily")) gdata.logrotate = 24*60*60;
      if (!strcmp(var,"weekly")) gdata.logrotate = 7*24*60*60;
      if (!strcmp(var,"monthly")) gdata.logrotate = 30*24*60*60;
      mydelete(var);
      }
   else if ( ! strcmp(type,"adminpass")) {
      mydelete(gdata.adminpass);
      gdata.adminpass = var;
      checkadminpass();
      }
   else if ( ! strcmp(type,"pidfile") && !rehash) {
      mydelete(gdata.pidfile);
      gdata.pidfile = var;
      convert_to_unix_slash(gdata.pidfile);
      }
   else if ( ! strcmp(type,"pidfile") && rehash) {
      mydelete(gdata.r_pidfile);
      gdata.r_pidfile = var;
      convert_to_unix_slash(gdata.r_pidfile);
      }
   else if ( ! strcmp(type,"filedir")) {
      mydelete(gdata.filedir);
      gdata.filedir = var;
      convert_to_unix_slash(gdata.filedir);
      }
   else if ( ! strcmp(type,"statefile")) {
      mydelete(gdata.statefile);
      gdata.statefile = var;
      convert_to_unix_slash(gdata.statefile);
      i = open(gdata.statefile, O_RDWR | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS );
      if (i >= 0) close(i);
      }
   else if ( ! strcmp(type,"xdcclistfile")) {
      mydelete(gdata.xdcclistfile);
      gdata.xdcclistfile = var;
      convert_to_unix_slash(gdata.xdcclistfile);
      }
   else if ( ! strcmp(type,"logfile")) {
      mydelete(gdata.logfile);
      gdata.logfile = var;
      convert_to_unix_slash(gdata.logfile);
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
         for (i=offset; i<=strlen(var); i++)
            var[i-offset] = var[i];
         
         mydelete(gdata.periodicmsg_msg);
         gdata.periodicmsg_msg = var;
         
         }
      
      mydelete(tnum);
      }
   else if ( ! strcmp(type,"usenatip"))
     {
      update_natip(var);
      mydelete(var);
     }
   else if ( ! strcmp(type,"local_vhost"))
     {
      unsigned long ipparts[4];
      if (sscanf(var, "%lu.%lu.%lu.%lu", &ipparts[0], &ipparts[1], &ipparts[2], &ipparts[3]) < 4)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid VHost, Ignoring");
        }
      else if ((ipparts[0] > 255) || (ipparts[1] > 255) ||
               (ipparts[2] > 255) || (ipparts[3] > 255))
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid VHost, Ignoring");
        }
      else
        {
          if (rehash)
            {
              gdata.r_local_vhost = (ipparts[0] << 24) | (ipparts[1] << 16) | (ipparts[2] << 8) | ipparts[3];
            }
          else
            {
              gdata.local_vhost = (ipparts[0] << 24) | (ipparts[1] << 16) | (ipparts[2] << 8) | ipparts[3];
            }
        }
      mydelete(var);
     }
   else if ( ! strcmp(type,"autoignore_exclude"))
     {
       regex_t *uh;
       char *varr;
       varr = hostmasktoregex(caps(var));
       uh = irlist_add(&gdata.autoignore_exclude, sizeof(regex_t));
       if (regcomp(uh,varr,REG_ICASE|REG_NOSUB))
         {
           irlist_delete(&gdata.autoignore_exclude, uh);
         }
       mydelete(varr);
       mydelete(var);
     }
   else if ( ! strcmp(type,"uploaddir")) {
      mydelete(gdata.uploaddir);
      gdata.uploaddir = var;
      convert_to_unix_slash(gdata.uploaddir);
      }
   else if ( ! strcmp(type,"uploadmaxsize")) {
      gdata.uploadmaxsize = (off_t)(max2(0,atoull(var)*1024*1024));
      mydelete(var);
      }
   else if ( ! strcmp(type,"uploadhost"))
     {
       regex_t *uh;
       char *varr;
       varr = hostmasktoregex(caps(var));
       uh = irlist_add(&gdata.uploadhost, sizeof(regex_t));
       if (regcomp(uh,varr,REG_ICASE|REG_NOSUB))
         {
           irlist_delete(&gdata.uploadhost, uh);
         }
       mydelete(varr);
       mydelete(var);
     }
   else if ( ! strcmp(type,"downloadhost"))
     {
       regex_t *uh;
       char *varr;
       varr = hostmasktoregex(caps(var));
       uh = irlist_add(&gdata.downloadhost, sizeof(regex_t));
       if (regcomp(uh,varr,REG_ICASE|REG_NOSUB))
         {
           irlist_delete(&gdata.downloadhost, uh);
         }
       mydelete(varr);
       mydelete(var);
     }
   else if ( ! strcmp(type,"connectionmethod"))
     {
       char *thow = getpart(var,1);
       char *targ1 = getpart(var,2);
       char *targ2 = getpart(var,3);
       char *targ3 = getpart(var,4);
       char *targ4 = getpart(var,5);
       
       mydelete(gdata.connectionmethod.host);
       mydelete(gdata.connectionmethod.password);
       mydelete(gdata.connectionmethod.vhost);
       bzero((char *) &gdata.connectionmethod,sizeof(connectionmethod_t));
       
       if (thow && !strcmp(thow,"direct"))
         {
           gdata.connectionmethod.how = how_direct;
         }
       else if (thow && targ1 && targ2 && targ3 && !strcmp(thow,"bnc"))
         {
           gdata.connectionmethod.how = how_bnc;
           
           gdata.connectionmethod.host = mymalloc(strlen(targ1)+1);
           strcpy(gdata.connectionmethod.host,targ1);
           
           gdata.connectionmethod.port = atoi(targ2);
           
           gdata.connectionmethod.password = mymalloc(strlen(targ3)+1);
           strcpy(gdata.connectionmethod.password,targ3);
           
           if (targ4)
             {
               gdata.connectionmethod.vhost = mymalloc(strlen(targ4)+1);
               strcpy(gdata.connectionmethod.vhost,targ4);
             }
         }
       else if (thow && targ1 && targ2 && !strcmp(thow,"wingate"))
         {
           gdata.connectionmethod.how = how_wingate;
           
           gdata.connectionmethod.host = mymalloc(strlen(targ1)+1);
           strcpy(gdata.connectionmethod.host,targ1);
           
           gdata.connectionmethod.port = atoi(targ2);
         }
       else if (thow && targ1 && targ2 && !strcmp(thow,"custom"))
         {
           gdata.connectionmethod.how = how_custom;
           
           gdata.connectionmethod.host = mymalloc(strlen(targ1)+1);
           strcpy(gdata.connectionmethod.host,targ1);
           
           gdata.connectionmethod.port = atoi(targ2);
         }
       else
         {
           gdata.connectionmethod.how = how_direct;
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

 done:
   mydelete(type);
   }

static int connectirc (server_t *tserver) {
   char *tempstr;
   int callval;
   
   updatecontext();

   gdata.serverbucket = EXCESS_BUCKET_MAX;
   
   if (!tserver) return 1;
   
   gdata.lastservercontact=gdata.curtime;
   
   gdata.nocon++;
   
   mydelete(gdata.curserver.hostname);
   mydelete(gdata.curserver.password);
   mydelete(gdata.curserveractualname);
   gdata.curserver.hostname = mymalloc(strlen(tserver->hostname)+1);
   strcpy(gdata.curserver.hostname, tserver->hostname);
   gdata.curserver.port = tserver->port;
   if (tserver->password)
     {
       gdata.curserver.password = mymalloc(strlen(tserver->password)+1);
       strcpy(gdata.curserver.password, tserver->password);
     }
   
   tempstr = mycalloc(maxtextlength);
   
   switch (gdata.connectionmethod.how)
     {
     case how_direct:
       snprintf(tempstr,maxtextlength-1," (direct)");
       gdata.serv_resolv.to_ip = gdata.curserver.hostname;
       gdata.serv_resolv.to_port = gdata.curserver.port;
       break;
       
     case how_bnc:
       if (gdata.connectionmethod.vhost)
         {
           snprintf(tempstr, maxtextlength-1,
                    " (bnc at %s:%i with %s)",
                    gdata.connectionmethod.host,
                    gdata.connectionmethod.port,
                    gdata.connectionmethod.vhost);
         }
       else
         {
           snprintf(tempstr, maxtextlength-1,
                    " (bnc at %s:%i)",
                    gdata.connectionmethod.host,
                    gdata.connectionmethod.port);
         }
       gdata.serv_resolv.to_ip = gdata.connectionmethod.host;
       gdata.serv_resolv.to_port = gdata.connectionmethod.port;
       break;
       
     case how_wingate:
       snprintf(tempstr, maxtextlength-1,
                " (wingate at %s:%i)",
                gdata.connectionmethod.host,
                gdata.connectionmethod.port);
       gdata.serv_resolv.to_ip = gdata.connectionmethod.host;
       gdata.serv_resolv.to_port = gdata.connectionmethod.port;
       break;
       
     case how_custom:
       snprintf(tempstr, maxtextlength-1,
                " (custom at %s:%i)",
                gdata.connectionmethod.host,
                gdata.connectionmethod.port);
       gdata.serv_resolv.to_ip = gdata.connectionmethod.host;
       gdata.serv_resolv.to_port = gdata.connectionmethod.port;
       break;
       
     default:
       mydelete(tempstr);
       return 1; /* error */
     }
      
   if (gdata.local_vhost)
     {
       ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
               "Attempting Connection to %s:%u from %ld.%ld.%ld.%ld%s",
               gdata.curserver.hostname,
               gdata.curserver.port,
               gdata.local_vhost>>24,
               (gdata.local_vhost>>16) & 0xFF,
               (gdata.local_vhost>>8) & 0xFF,
               gdata.local_vhost & 0xFF,
               tempstr);
     }
   else
     {
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Attempting Connection to %s:%u%s",gdata.curserver.hostname,gdata.curserver.port,tempstr);
     }
   
   mydelete(tempstr);
   
   if (gdata.serv_resolv.child_pid)
     {
       /* old resolv still outstanding, cleanup */
       close(gdata.serv_resolv.sp_fd[0]);
       FD_CLR(gdata.serv_resolv.sp_fd[0], &gdata.readset);
       gdata.serv_resolv.sp_fd[0] = 0;
       gdata.serv_resolv.child_pid = 0;
     }
   
   callval = socketpair(AF_UNIX, SOCK_DGRAM, 0, gdata.serv_resolv.sp_fd);
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
   else if (callval == 0)
     {
       struct hostent *remotehost;
       int i;
       
       /* child */
       for (i=3; i<FD_SETSIZE; i++)
         {
           /* include [0], but not [1] */
           if (i != gdata.serv_resolv.sp_fd[1])
             {
               close(i);
             }
         }
       
       remotehost = gethostbyname(gdata.serv_resolv.to_ip);
       if (remotehost == NULL)
         {
#ifdef NO_HOSTCODES
           exit(10);
#else
           extern int h_errno;
           switch (h_errno)
             {
             case HOST_NOT_FOUND:
               exit(20);
             case NO_ADDRESS:
#if NO_ADDRESS != NO_DATA
             case NO_DATA:
#endif
               exit(21);
             case NO_RECOVERY:
               exit(22);
             case TRY_AGAIN:
               exit(23);
             default:
               exit(12);
             }
#endif
         }
       
       callval = write(gdata.serv_resolv.sp_fd[1],
                       remotehost->h_addr_list[0],
                       sizeof(struct in_addr));
       
       if (callval != sizeof(struct in_addr))
         {
           exit(11);
         }
       
       sleep(60);
       exit(0);
     }
   
   /* parent */
   gdata.serv_resolv.child_pid = callval;
   close(gdata.serv_resolv.sp_fd[1]);
   gdata.serv_resolv.sp_fd[1] = 0;
   
   gdata.serverstatus = SERVERSTATUS_RESOLVING;
   
   return 0;
}

int connectirc2 (struct in_addr *remote) {
   struct sockaddr_in ircserverip;
   struct sockaddr_in localaddr;
   int retval;
   
   bzero ((char *) &ircserverip, sizeof (ircserverip));
   
   gdata.ircserver = socket( AF_INET, SOCK_STREAM, 0);
   if (gdata.ircserver < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Socket Error");
      return 1;
      }
   
   ircserverip.sin_family = AF_INET;
   ircserverip.sin_port = htons(gdata.serv_resolv.to_port);
   
   memcpy(&ircserverip.sin_addr, remote, sizeof(struct in_addr));
   
   if (gdata.debug > 0)
     {
       unsigned long to_ip = ntohl(ircserverip.sin_addr.s_addr);
       ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"Connecting to %lu.%lu.%lu.%lu:%d",
               (to_ip >> 24) & 0xFF,
               (to_ip >> 16) & 0xFF,
               (to_ip >>  8) & 0xFF,
               (to_ip      ) & 0xFF,
               gdata.serv_resolv.to_port);
     }
   
   if (gdata.local_vhost) {
      bzero((char*)&localaddr, sizeof(struct sockaddr_in));
      localaddr.sin_family = AF_INET;
      localaddr.sin_port = 0;
      localaddr.sin_addr.s_addr = htonl(gdata.local_vhost);
      
      if (bind(gdata.ircserver, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0) {
         outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Bind To Virtual Host");
         close(gdata.ircserver);
         return 1;
         }
      }
   
   if (set_socket_nonblocking(gdata.ircserver,1) < 0 )
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
   
   alarm(CTIMEOUT);
   retval = connect(gdata.ircserver, (struct sockaddr *) &ircserverip, sizeof(ircserverip));
   if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) ) {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Connection to Server Failed");
      alarm(0);
      close(gdata.ircserver);
      return 1;
      }
   alarm(0);
   
   if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"ircserver socket = %d",gdata.ircserver);
      }

   gdata.lastservercontact=gdata.curtime;
   
   /* good */
   gdata.serverstatus = SERVERSTATUS_TRYING;
   
   return 0;
   }

void initirc(void)
{
  int i,j,k;
  channel_t *ch;
  char *pi;
  char *tptr;
  
  updatecontext();
  
  pi = irlist_get_head(&gdata.proxyinfo);
  while((gdata.connectionmethod.how == how_custom) && pi)
    {
      int len;
      char *tempstr;
      int found_s = 0;
      int found_p = 0;
      char portstr[maxtextlengthshort];
      
      snprintf(portstr, maxtextlengthshort-1, "%u", gdata.curserver.port);
      portstr[maxtextlengthshort-1] = '\0';
      
      len = strlen(pi) + strlen(gdata.curserver.hostname) + strlen(portstr);
      
      tempstr = mycalloc(len+1);
      
      for (i=j=0; pi[i]; i++,j++)
        {
          if (!found_s &&
              (pi[i] == '$') &&
              (pi[i+1] == 's'))
            {
              for (k=0,i++; gdata.curserver.hostname[k]; k++,j++)
                {
                  tempstr[j] = gdata.curserver.hostname[k];
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
      
   if (gdata.connectionmethod.how == how_wingate) {
      writeserver(WRITESERVER_NOW, "%s %u",
                  gdata.curserver.hostname, gdata.curserver.port);
      }
   
   if (gdata.curserver.password)
     {
       writeserver(WRITESERVER_NOW, "PASS %s", gdata.curserver.password);
     }
   writeserver(WRITESERVER_NOW, "NICK %s", gdata.config_nick);
   writeserver(WRITESERVER_NOW, "USER %s 32 . :%s",
               gdata.loginname, gdata.user_realname);
   
   if (gdata.connectionmethod.how == how_bnc) {
      writeserver(WRITESERVER_NOW, "PASS %s",
                  gdata.connectionmethod.password);
      if (gdata.connectionmethod.vhost) {
         writeserver(WRITESERVER_NOW, "VIP %s",
                     gdata.connectionmethod.vhost);
         }
      writeserver(WRITESERVER_NOW, "CONN %s %d",
                  gdata.curserver.hostname, gdata.curserver.port);
      }
   
   /* server join raw command */
  tptr = irlist_get_head(&gdata.server_join_raw);
  while(tptr)
    {
      writeserver(WRITESERVER_NORMAL, "%s", tptr);
      tptr = irlist_get_next(tptr);
    }
   
   ch = irlist_get_head(&gdata.channels);
   while(ch)
     {
       ch->flags &= ~CHAN_ONCHAN;
       ch = irlist_get_next(ch);
     }
   
   gdata.recentsent = 0;
   
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
      (gdata.serverstatus == SERVERSTATUS_CONNECTED))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<SND<: %s",msg);
        }
      msg[len] = '\n';
      len++;
      msg[len] = '\0';
      write(gdata.ircserver, msg, len);
      gdata.serverbucket -= len;
    }
  else if (gdata.exiting || (gdata.serverstatus != SERVERSTATUS_CONNECTED))
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
          
          if (irlist_size(&gdata.serverq_fast) < MAXSENDQ)
            {
              item = irlist_add(&gdata.serverq_fast, len + 1);
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
          
          if (irlist_size(&gdata.serverq_normal) < MAXSENDQ)
            {
              item = irlist_add(&gdata.serverq_normal, len + 1);
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
          
          if (irlist_size(&gdata.serverq_slow) < MAXSENDQ)
            {
              item = irlist_add(&gdata.serverq_slow, len + 1);
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
  
  gdata.serverbucket += EXCESS_BUCKET_ADD;
  gdata.serverbucket = min2(gdata.serverbucket,EXCESS_BUCKET_MAX);
  
  if ((irlist_size(&gdata.serverq_fast) == 0) &&
      (irlist_size(&gdata.serverq_normal) == 0) &&
      (irlist_size(&gdata.serverq_slow) == 0) &&
      gdata.exiting &&
      !gdata.recentsent)
    {
      FD_CLR(gdata.ircserver, &gdata.readset);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(gdata.ircserver, SHUT_RDWR);
      close(gdata.ircserver);
      gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_NO_COLOR,
              "Connection to %s (%s) Closed",
              gdata.curserver.hostname,
              gdata.curserveractualname ? gdata.curserveractualname : "<unknown>");
      return;
    }
  
  if (((irlist_size(&gdata.serverq_fast) == 0) &&
       (irlist_size(&gdata.serverq_normal) == 0) &&
       (irlist_size(&gdata.serverq_slow) == 0)) ||
      (gdata.serverstatus != SERVERSTATUS_CONNECTED))
    {
      return;
    }
  
  
  item = irlist_get_head(&gdata.serverq_fast);
  
  while (item && (strlen(item) < gdata.serverbucket))
    {
      if (!gdata.attop) gototop();
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<IRC<: %s",item);
        }
      write(gdata.ircserver, item, strlen(item));
      write(gdata.ircserver, "\n", 1);
      
      gdata.serverbucket -= strlen(item);
      
      item = irlist_delete(&gdata.serverq_fast, item);
    }
  
  if (item)
    {
      gdata.recentsent = 0;
      return;
    }
  
  item = irlist_get_head(&gdata.serverq_normal);
  
  while (item && (strlen(item) < gdata.serverbucket))
    {
      if (!gdata.attop) gototop();
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<IRC<: %s",item);
        }
      write(gdata.ircserver, item, strlen(item));
      write(gdata.ircserver, "\n", 1);
      
      gdata.serverbucket -= strlen(item);
      
      item = irlist_delete(&gdata.serverq_normal, item);
    }
  
  if (item)
    {
      gdata.recentsent = 0;
      return;
    }
  
  item = irlist_get_head(&gdata.serverq_slow);
  
  while (item && (strlen(item) < gdata.serverbucket))
    {
      if (!gdata.attop) gototop();
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_MAGENTA,"<IRC<: %s",item);
        }
      write(gdata.ircserver, item, strlen(item));
      write(gdata.ircserver, "\n", 1);
      
      gdata.serverbucket -= strlen(item);
      
      item = irlist_delete(&gdata.serverq_slow, item);
    }
  
  if (item)
    {
      gdata.recentsent = 0;
    }
  else
    {
      gdata.recentsent = 6;
    }
  
  return;
}


/* 'Sanitize' the filename in full, putting the sanitized copy into copy. */
char* getsendname(char * const full)
{
  char *copy;
  int i, lastslash;
  int len;
  
  updatecontext();
  
  len = sstrlen(full);
  lastslash = -1;
  for (i = 0 ; i < len ; i++)
    {
      if (full[i] == '/' || full[i] == '\\')
        {
          lastslash = i;
        }
    }
  
  len -= lastslash + 1;
  copy = mycalloc(len + 1);
  
  strcpy(copy, full + lastslash + 1);
  
  /* replace any evil characters in the filename with underscores */
  for (i = 0; i < len; i++)
    {
      if (copy[i] == ' ' || copy[i] == '|' || copy[i] == ':' || copy[i] == '*' ||
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
               gdata.curserveractualname ? gdata.curserveractualname : gdata.curserver.hostname);
   }

void xdccsavetext(void)
{
  char *xdcclistfile_tmp, *xdcclistfile_bkup;
  int fd;
  userinput *uxdl;
  int callval;
  
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
  
  if (gdata.xdcclist_grouponly)
    u_fillwith_msg(uxdl,NULL,"A A A A A xdl");
  else
    u_fillwith_msg(uxdl,NULL,"A A A A A xdlfull");
  uxdl->method = method_fd; 
  uxdl->fd = fd;
  
  u_parseit(uxdl);
  
  mydelete(uxdl);
  
  close(fd);
  
  /* remove old bkup */
  callval = unlink(xdcclistfile_bkup);
  if ((callval < 0) && (errno != ENOENT))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Remove Old XDCC List File '%s': %s",
               xdcclistfile_bkup, strerror(errno));
      /* ignore, continue */
    }
  
  /* backup old -> bkup */
  callval = link(gdata.xdcclistfile, xdcclistfile_bkup);
  if ((callval < 0) && (errno != ENOENT))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Backup Old XDCC List File '%s' -> '%s': %s",
               gdata.xdcclistfile, xdcclistfile_bkup, strerror(errno));
      /* ignore, continue */
    }
  
  /* rename new -> current */
  callval = rename(xdcclistfile_tmp, gdata.xdcclistfile);
  if (callval < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Save New XDCC List File '%s': %s",
               gdata.xdcclistfile, strerror(errno));
      /* ignore, continue */
    }
  
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
      outerror(OUTERROR_TYPE_CRASH,"Unable to Fork");
   else if (s > 0) {
      /* parent exits */
      exit(0);
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
      outerror(OUTERROR_TYPE_CRASH,"Couldn't setsid");
   
   /* parent forks */
   s = fork();
   if (s < 0)
      outerror(OUTERROR_TYPE_CRASH,"Unable to Fork");
   else if (s > 0)
      /* parent exits */
      exit(0);
   
   
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
          
          exit(0);
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
        
        ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,"Crashing... Please report this problem to PMG");
        
        tostdout_disable_buffering(1);
        
        uninitscreen();
        
        signal(signo, SIG_DFL);
        sigemptyset(&ss);
        sigaddset(&ss, signo);
        sigprocmask(SIG_UNBLOCK, &ss, NULL);
        raise(signo);
        /* shouldn't get here */
        exit(1);
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
      count += gdata.inamnt[i];
      }

   if (count > 6)
      gdata.ignore = 1;
   else
      gdata.ignore = 0;
   
   if (last - gdata.ignore == -1) {
      if (!gdata.attop) gototop();
      outerror(OUTERROR_TYPE_WARN,"Flood Protection Activated");
      }
   if (last - gdata.ignore == 1) {
      if (!gdata.attop) gototop();
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
  
  if (c->key)
    {
      writeserver(WRITESERVER_NORMAL, "JOIN %s %s", c->name, c->key);
    }
  else
    {
      writeserver(WRITESERVER_NORMAL, "JOIN %s", c->name);
    }
  
  tptr = irlist_get_head(&gdata.channel_join_raw);
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
   
   updatecontext();

   if (!gdata.attop) gototop();
   
   if ( SAVEQUIT )
      write_statefile();
   
   if (gdata.exiting || gdata.serverstatus != SERVERSTATUS_CONNECTED) {
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

      tostdout_disable_buffering(1);
      uninitscreen();
      if (gdata.pidfile) unlink(gdata.pidfile);
      exit(0);
      }
   
   
   ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,"Shutting Down... (Issue \"SHUTDOWN\" again to force quit)");
   
   /* empty queue */
   irlist_delete_all(&gdata.serverq_fast);
   irlist_delete_all(&gdata.serverq_normal);
   irlist_delete_all(&gdata.serverq_slow);
   
   /* close connections */
   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       notice(tr->nick,"** Shutting Down. Closing Connection. (Resume Supported)");
       
       FD_CLR(tr->clientsocket, &gdata.writeset);
       FD_CLR(tr->clientsocket, &gdata.readset);
       if (tr->listensocket != FD_UNUSED)
         {
           close(tr->listensocket);
         }
       if (tr->clientsocket != FD_UNUSED)
         {
           /*
            * cygwin close() is broke, if outstanding data is present
            * it will block until the TCP connection is dead, sometimes
            * upto 10-20 minutes, calling shutdown() first seems to help
            */
           shutdown(tr->clientsocket, SHUT_RDWR);
           close(tr->clientsocket);
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
       notice(ul->nick,"** Shutting Down. Closing Upload Connection. (Resume Supported)");
       
       FD_CLR(ul->clientsocket, &gdata.writeset);
       FD_CLR(ul->clientsocket, &gdata.readset);
       if (ul->clientsocket != FD_UNUSED)
         {
           /*
            * cygwin close() is broke, if outstanding data is present
            * it will block until the TCP connection is dead, sometimes
            * upto 10-20 minutes, calling shutdown() first seems to help
            */
           shutdown(ul->clientsocket, SHUT_RDWR);
           close(ul->clientsocket);
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
   if (gdata.serverstatus == SERVERSTATUS_CONNECTED)
     {
       tempstr2 = mycalloc(maxtextlengthshort);
       tempstr2 = getuptime(tempstr2, 1, gdata.startuptime, maxtextlengthshort);
       writeserver(WRITESERVER_NORMAL,
                   "QUIT :iroffer v" VERSIONLONG "%s%s - running %s",
                   gdata.hideos ? "" : " - ",
                   gdata.hideos ? "" : gdata.osstring,
                   tempstr2);
       mydelete(tempstr2);
     }
   
   gdata.exiting = 1;
   
   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_D,COLOR_NO_COLOR,"Waiting for Server Queue To Flush...");
   
   }

void switchserver(int which)
{
  server_t *ss;
  
  updatecontext();
  
  /* quit */
  if (gdata.serverstatus == SERVERSTATUS_CONNECTED)
    {
      writeserver(WRITESERVER_NOW, "QUIT :Changing Servers");
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_RED,"Changing Servers");
      
      FD_CLR(gdata.ircserver, &gdata.readset);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(gdata.ircserver, SHUT_RDWR);
      close(gdata.ircserver);
    }
  
  if (gdata.serverstatus == SERVERSTATUS_TRYING)
    {
      FD_CLR(gdata.ircserver, &gdata.readset);
      close(gdata.ircserver);
    }
  
  /* delete the slow queue */
  irlist_delete_all(&gdata.serverq_slow);
  
  gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
   
  if (which < 0)
    {
      int i;
      
      i = irlist_size(&gdata.servers);
      
      which = (int) (((float)i)*rand()/(RAND_MAX+0.0));
      
      while (which > i || which < 0)
        {
          if (which < 0) which += i;
          if (which > i) which -= i;
        }
    }
  
  ss = irlist_get_nth(&gdata.servers, which);
  
  connectirc(ss);
  
  gdata.serverconnectbackoff++;
  gdata.servertime = 0;
}

char* getstatusline(char *str, int len)
{
  int i,srvq;
  ir_uint64 xdccsent;
  
  updatecontext();
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsent[i];
    }
  
  srvq = irlist_size(&gdata.serverq_fast)
    + irlist_size(&gdata.serverq_normal)
    + irlist_size(&gdata.serverq_slow);
  
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
  
  updatecontext();
  
  xdccsent = 0;
  for (i=0; i<XDCC_SENT_SIZE; i++)
    {
      xdccsent += (ir_uint64)gdata.xdccsent[i];
    }
  
  srvq = irlist_size(&gdata.serverq_fast)
    + irlist_size(&gdata.serverq_normal)
    + irlist_size(&gdata.serverq_slow);
  
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
  char *user;
  int len;
  userinput ui;
  
  updatecontext();
  
  if (!irlist_size(&gdata.xlistqueue))
    {
      return;
    }
  
  len = 0;
  user = irlist_get_head(&gdata.xlistqueue);
  while (user)
    {
      len += strlen(user) + 1;
      user = irlist_get_next(user);
    }
  
  tempstr = mycalloc(len);
  
  len = 0;
  user = irlist_get_head(&gdata.xlistqueue);
  strcpy(tempstr+len, user);
  len += strlen(tempstr+len);
  
  user = irlist_delete(&gdata.xlistqueue, user);
  while (user)
    {
      strcpy(tempstr+len, ",");
      len += strlen(tempstr+len);
      strcpy(tempstr+len, user);
      len += strlen(tempstr+len);
      
      user = irlist_delete(&gdata.xlistqueue, user);
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
      
      u_fillwith_msg(&ui,tempstr,"A A A A A xdl");
      ui.method = method_xdl_user_notice;
      u_parseit(&ui);
    }
  
  mydelete(tempstr);
}

int isthisforme (const char *dest, char *msg1) {
   if (!msg1 || !dest) { outerror(OUTERROR_TYPE_WARN_LOUD,"isthisforme() got NULL value"); return 1; }
   
   if (!gdata.caps_nick)
     {
       return 0;
     }

   if (
         !strcmp(msg1,"\1CLIENTINFO") || !strcmp(msg1,"\1CLIENTINFO\1")
      || !strcmp(msg1,"\1PING") || !strcmp(msg1,"\1PING\1") 
      || !strcmp(msg1,"\1VERSION") || !strcmp(msg1,"\1VERSION\1")
      || !strcmp(msg1,"\1UPTIME") || !strcmp(msg1,"\1UPTIME\1") 
      || !strcmp(msg1,"\1STATUS") || !strcmp(msg1,"\1STATUS\1")
      || (!strcmp(gdata.caps_nick,dest) && !strcmp(caps(msg1),"\1DCC"))
      || (!strcmp(gdata.caps_nick,dest) && !strcmp(caps(msg1),"ADMIN"))
      || (!strcmp(gdata.caps_nick,dest) && (!strcmp(caps(msg1),"XDCC") || !strcmp(msg1,"\1XDCC") || !strcmp(caps(msg1),"CDCC") || !strcmp(msg1,"\1CDCC")))
      || !strcmp(dest,gdata.caps_nick)
      ) return 1;
   
   return 0;
   
   }

void reinit_config_vars(void)
{
  regex_t *rh;
  
  /* clear old config items */
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
  {
    server_t *ss;
    for (ss = irlist_get_head(&gdata.servers);
         ss;
         ss = irlist_delete(&gdata.servers, ss))
      {
        mydelete(ss->hostname);
        mydelete(ss->password);
      }
  }
  irlist_delete_all(&gdata.r_channels);
  mydelete(gdata.logfile);
  gdata.logrotate = 0;
  gdata.logstats = 1;
  mydelete(gdata.user_realname);
  mydelete(gdata.user_modes);
  gdata.tcprangestart = 0;
  irlist_delete_all(&gdata.proxyinfo);
  irlist_delete_all(&gdata.server_join_raw);
  irlist_delete_all(&gdata.server_connected_raw);
  irlist_delete_all(&gdata.channel_join_raw);
  gdata.usenatip = 0;
  gdata.slotsmax = gdata.queuesize = 0;
  gdata.maxtransfersperperson = 1;
  gdata.maxqueueditemsperperson = 1;
  gdata.autoignore_threshold = 10;
  mydelete(gdata.filedir);
  gdata.lowbdwth = 0;
  gdata.punishslowusers = 0;
  gdata.nomd5sum = 0;
  gdata.getipfromserver = 0;
  gdata.noduplicatefiles = 0;
  irlist_delete_all(&gdata.adddir_exclude);
  mydelete(gdata.enable_nick);
  gdata.need_voice = 0;
  gdata.hide_list_info = 0;
  gdata.xdcclist_grouponly = 0;
  gdata.auto_default_group = 0;
  gdata.start_of_month = 1;
  gdata.restrictprivlistmain = 0;
  gdata.restrictprivlistfull = 0;
  gdata.groupsincaps = 0;
  gdata.ignoreuploadbandwidth = 0;
  gdata.holdqueue = 0;
  gdata.removelostfiles = 0;
  mydelete(gdata.admin_job_file);
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
  gdata.autosend.pack = 0;
  mydelete(gdata.autosend.word);
  mydelete(gdata.autosend.message);
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
  mydelete(gdata.uploaddir);
  gdata.restrictlist = gdata.restrictsend = gdata.restrictprivlist = 0;
  mydelete(gdata.restrictprivlistmsg);
  mydelete(gdata.loginname);
  gdata.connectionmethod.how = how_direct;
  mydelete(gdata.statefile);
  mydelete(gdata.xdcclistfile);
  return;
}

void initprefixes(void)
{
  memset(&gdata.prefixes, 0, sizeof(gdata.prefixes));
  gdata.prefixes[0].p_mode   = 'o';
  gdata.prefixes[0].p_symbol = '@';
  gdata.prefixes[1].p_mode   = 'v';
  gdata.prefixes[1].p_symbol = '+';
}

void initchanmodes(void)
{
  memset(&gdata.chanmodes, 0, sizeof(gdata.chanmodes));
  gdata.chanmodes[0] = 'b';
  gdata.chanmodes[1] = 'k';
  gdata.chanmodes[1] = 'l';
}

void initvars(void)
{
  memset(&gdata, 0, sizeof(gdata_t));
  
  reinit_config_vars();
  initprefixes();
  initchanmodes();
  
  gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
  gdata.logfd = FD_UNUSED;
  gdata.termcols = 80;
  gdata.termlines = 24;
  
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
  /* 4GB max size */
  /* NOTE: DCC protocol can't handle more than 32bit file size, 4GB is it, sorry */
  gdata.max_file_size = (4*1024*1024*1024UL)-1;
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
   
   updatecontext();
   
   srand((unsigned int)( (getpid()*5000) + (gdata.curtime%5000) ));
   
   if (!gdata.background) {
      initscreen(1);
      gotobot();
      gototop();
      }
   
   printf("\n");
   if (!gdata.background && !gdata.nocolor) printf("\x1b[1;33m");
   printf("Welcome to iroffer by PMG - http://iroffer.org/\n"
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
   
   printf("** iroffer is distributed under the GNU General Public License.\n"
          "**    please see the README for more information.\n");

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
   
   getconfig();
   
   mylog(CALLTYPE_NORMAL,"iroffer started v" VERSIONLONG);

   getos();
   
   if (gdata.statefile)
     {
       read_statefile();
       write_statefile();
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
   
   gdata.serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
   switchserver(-1);
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
   char salt[3], *pwout;
   
   printf("\niroffer v" VERSIONLONG " by PMG\n"
          "  Configuration File Password Generator\n"
          "\n"
          "This will take a password of your choosing and encrypt it.\n"
          "You should place the output this program generates in your config file.\n"
          "You can then use your password you enter here over irc.\n"
          "\n"
          "Your password must be between 5 and 8 characters\n");
   
   
   ok = 0;
   while ( !ok ) {
      printf("Please Enter Your Password: "); fflush(stdout);
      
      if ( (len = read(0,pw1,maxtextlengthshort-1)) < 0 )
         { fprintf(stderr,"Couldn't Read Your Password, Try Again\n"); exit(1); }
      if (pw1[len-1] == '\n') { pw1[len-1] = '\0'; len--;}
      
      if ( len < 5 || len > 8 )
         printf("Wrong Length, Try Again\n");
      else
         ok = 1;
   }
   
   printf("And Again for Verification: ");
   fflush(stdout);

   if ( (len = read(0,pw2,maxtextlengthshort-1)) < 0 )
      { fprintf(stderr,"Couldn't Read Your Password, Try Again\n"); exit(1); }
   if (pw2[len-1] == '\n') { pw2[len-1] = '\0'; len--;}
   
   if ( strcmp(pw1,pw2) )
      { fprintf(stderr,"The Password Didn't Match, Try Again\n"); exit(1); }
   
   
   srand((unsigned int)( (getpid()*5000) + (time(NULL)%5000) ));
   saltnum = (int)(4096.0*rand()/(RAND_MAX+0.0));
   salt[0] = inttosaltchar((saltnum>>6) %64);
   salt[1] = inttosaltchar( saltnum     %64);
   salt[2] = '\0';
   
   pwout = crypt(pw1,salt);
   
   if (pwout && strlen(pwout) == 13)
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
  unsigned long rtime, lastrtime;
  pqueue *pq;
  transfer *tr;
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
  
  lastrtime=0;
  
  /* if we are sending more than allowed, we need to skip the difference */
  for (i=0; i<irlist_size(&gdata.trans)-gdata.slotsmax; i++)
    {
      rtime=-1;
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          int left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          if (left > lastrtime && left < rtime)
            {
              rtime = left;
            }
          tr = irlist_get_next(tr);
        }
      if (rtime < 359999)
        {
          lastrtime=rtime;
        }
    }
  
  i=1;
  pq = irlist_get_head(&gdata.mainqueue);
  while(pq)
    {
      rtime=-1;
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          int left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          if (left > lastrtime && left < rtime)
            {
              rtime = left;
            }
          tr = irlist_get_next(tr);
        }
      if (rtime < 359999)
        {
          lastrtime=rtime;
        }
      
      notice_slow(pq->nick,"Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                  (long)(gdata.curtime-pq->queuedtime)/60/60,
                  (long)((gdata.curtime-pq->queuedtime)/60)%60,
                  pq->xpack->desc,
                  i,
                  irlist_size(&gdata.mainqueue),
                  lastrtime/60/60,
                  (lastrtime/60)%60,
                  (rtime >= 359999) ? "more" : "less");
      i++;
      pq = irlist_get_next(pq);
    }
  
}

void notifybandwidth(void)
{
  int i;
  transfer *tr;
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
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          notice_slow(tr->nick,"%s bandwidth limit: %2.1f of %2.1fKB/sec used. Your share: %2.1fKB/sec.",
                      (gdata.user_nick ? gdata.user_nick : "??"),
                      ((float)xdccsent)/XDCC_SENT_SIZE,
                      ((float)gdata.maxb)/4.0,
                      tr->lastspeed);
          tr = irlist_get_next(tr);
        }
    }
  
}

void notifybandwidthtrans(void)
{
  transfer *tr;
  
  updatecontext();
  
  if (gdata.exiting) return;
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if (!tr->nomax &&
          (tr->xpack->maxspeed > 0) &&
          (gdata.curtime-tr->connecttime > MIN_TL) &&
          (tr->lastspeed*10 > tr->xpack->maxspeed*9))
        {
          /* send if over 90% */
          notice_slow(tr->nick,"Pack bandwidth limit: %2.1f of %2.1fKB/sec used.",
                      tr->lastspeed,
                      tr->xpack->maxspeed);
        }
      tr = irlist_get_next(tr);
    }
  
}


void look_for_file_changes(xdcc *xpack)
{
  struct stat st;
  transfer *tr;
  
  if (stat(xpack->file,&st) < 0)
    {
      outerror(OUTERROR_TYPE_WARN,
               "File '%s' can no longer be accessed: %s",
               xpack->file, strerror(errno));
      if ((gdata.removelostfiles) && (errno == ENOENT))
        {
           userinput *pubplist;
           xdcc *xd;
           char *tempstr;
           int n;

           updatecontext();
           n = 0;
           xd = irlist_get_head(&gdata.xdccs);
           while(xd)
             {
               n++;
               if (xd == xpack)
                 break;
               xd = irlist_get_next(xd);
             }
           pubplist = mycalloc(sizeof(userinput));
           tempstr = mycalloc(maxtextlength);
           snprintf(tempstr,maxtextlength-1,"remove %d", n);
           u_fillwith_console(pubplist,tempstr);
           u_parseit(pubplist);
           mydelete(pubplist);
           mydelete(tempstr);
        }
      return;
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
      
      if (gdata.md5build.xpack == xpack)
        {
          outerror(OUTERROR_TYPE_WARN,"[MD5]: Canceled (file changed)");
          
          FD_CLR(gdata.md5build.file_fd, &gdata.readset);
          close(gdata.md5build.file_fd);
          gdata.md5build.file_fd = FD_UNUSED;
          gdata.md5build.xpack = NULL;
        }
      xpack->has_md5sum = 0;
      memset(xpack->md5sum,0,sizeof(MD5Digest));
      
      assert(xpack->file_fd == FD_UNUSED);
      assert(xpack->file_fd_count == 0);
#ifdef HAVE_MMAP
      assert(!irlist_size(&xpack->mmaps));
#endif
    }
  
  return;
}

void user_changed_nick(const char *oldnick, const char *newnick)
{
  transfer *tr;
  pqueue *pq;
  pqueue *old;
  int userinqueue = 0;
  
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
    {
      if (!strcasecmp(tr->nick, oldnick))
        {
          mydelete(tr->nick);
          tr->nick = mymalloc(strlen(newnick)+1);
          strcpy(tr->nick, newnick);
        }
    }
  
  for (pq = irlist_get_head(&gdata.mainqueue); pq; pq = irlist_get_next(pq))
    {
      if (!strcasecmp(pq->nick, oldnick))
        {
          mydelete(pq->nick);
          pq->nick = mymalloc(strlen(newnick)+1);
          strcpy(pq->nick, newnick);
        }
    }
  
  for (pq = irlist_get_head(&gdata.mainqueue); pq; )
    {
      if (!strcasecmp(pq->nick, newnick))
        {
          if (strcmp(pq->hostname,"man"))
            {
	      userinqueue++;
              if ( userinqueue > gdata.maxqueueditemsperperson )
                {
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

int has_joined_channels(int all)
{
  int j;
  channel_t *ch;

  j=0;
  ch = irlist_get_head(&gdata.channels);
  while(ch)
    {
       if ((ch->flags | CHAN_ONCHAN) == 0)
         {
           if (all != 0)
             return 0;
         }
       else
         j++;
       ch = irlist_get_next(ch);
     }
  return j;
}

void reverify_restrictsend(void)
{
  transfer *tr;
  pqueue *pq;
  
  if (!gdata.restrictsend ||
      (gdata.serverstatus != SERVERSTATUS_CONNECTED))
    {
      return;
    }

  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
    {
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
            }
          else if ((gdata.curtime - tr->restrictsend_bad) >= RESTRICTSEND_TIMEOUT)
            {
              t_closeconn(tr, "You are no longer on a known channel", 0);
            }
        }
    }
  
  for (pq = irlist_get_head(&gdata.mainqueue); pq;)
    {
      if (strcmp(pq->hostname,"man"))
        {
          if (isinmemberlist(pq->nick))
            {
              pq->restrictsend_bad = 0;
              pq = irlist_get_next(pq);
            }
          else if (!pq->restrictsend_bad)
            {
              pq->restrictsend_bad = gdata.curtime;
              pq = irlist_get_next(pq);
            }
          else if ((gdata.curtime - pq->restrictsend_bad) >= RESTRICTSEND_TIMEOUT)
            {
              notice(pq->nick,"** Removed From Queue: You are no longer on a known channel");
              mydelete(pq->nick);
              mydelete(pq->hostname);
              pq = irlist_delete(&gdata.mainqueue, pq);
            }
          else
            {
              pq = irlist_get_next(pq);
            }
        }
      else
        {
          pq = irlist_get_next(pq);
        }
    }
  
  return;
}



/* End of File */
