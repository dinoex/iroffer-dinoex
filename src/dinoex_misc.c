/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2005 Dirk Meyer
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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
#include "iroffer_dinoex.h"

#include <ctype.h>
#include <fnmatch.h>

#ifdef USE_GEOIP
#include <GeoIP.h>
#endif /* USE_GEOIP */

#ifdef USE_CURL
#include <curl/curl.h>
#endif /* USE_CURL */

extern const ir_uint32 crctable[256];

/* local functions */
#ifdef USE_CURL
static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
a_respond(const userinput * const u, const char *format, ...);

static void a_respond(const userinput * const u, const char *format, ...)
{
  va_list args;

  updatecontext();
 
  va_start(args, format);
 
  switch (u->method)
    {
    case method_console:
      vioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR, format, args);
      break;
    case method_dcc:
      vwritedccchat(u->chat, 1, format, args);
      break;
    case method_out_all:
      vioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, format, args);
      break;
    case method_fd:
      {
        ssize_t retval;
        char tempstr[maxtextlength];
        int llen;

        llen = vsnprintf(tempstr,maxtextlength-3,format,args);
        if ((llen < 0) || (llen >= maxtextlength-3))
          {
            outerror(OUTERROR_TYPE_WARN,"string too long!");
            tempstr[0] = '\0';
            llen = 0;
          }

        if (!gdata.xdcclistfileraw)
          {
            removenonprintablectrl(tempstr);
          }

#if defined(_OS_CYGWIN)
        tempstr[llen++] = '\r';
#endif
        tempstr[llen++] = '\n';
        tempstr[llen] = '\0';

        retval = write(u->fd, tempstr, strlen(tempstr));
        if (retval < 0)
          {
            outerror(OUTERROR_TYPE_WARN_LOUD,"Write failed: %s", strerror(errno));
          }
      }
      break;
    case method_msg:
      vprivmsg(u->snick, format, args);
      break;
    default:
      break;
    }

  va_end(args);
}
#endif /* USE_CURL */

int verifyshell(irlist_t *list, const char *file)
{
  char *pattern;

  updatecontext();

  pattern = irlist_get_head(list);
  while (pattern)
    {
    if (fnmatch(pattern,file,FNM_CASEFOLD) == 0)
      {
        return 1;
      }
    pattern = irlist_get_next(pattern);
    }

  return 0;
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

void admin_jobs(const char *job) {
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


int check_lock(const char* lockstr, const char* pwd)
{
  if (lockstr == NULL)
    return 0; /* no lock */
  if (pwd == NULL)
    return 1; /* locked */
  return strcmp(lockstr, pwd);
}

int reorder_new_groupdesc(const char *group, const char *desc) {
  xdcc *xd;
  int k;

  updatecontext();

  k = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        {
          if (strcasecmp(xd->group,group) == 0)
            {
              k++;
              /* delete all matching entires */
              if (xd->group_desc != NULL)
                mydelete(xd->group_desc);
              /* write only the first entry */
              if (k == 1)
                {
                  if (desc && strlen(desc))
                    {
                      xd->group_desc = mycalloc(strlen(desc)+1);
                      strcpy(xd->group_desc,desc);
                    }
                }
            }
        }
      xd = irlist_get_next(xd);
    }

  return k;
}

int reorder_groupdesc(const char *group) {
  xdcc *xd; 
  xdcc *firstxd;
  xdcc *descxd;
  int k;

  updatecontext();

  k = 0;
  firstxd = NULL;
  descxd = NULL;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        {
          if (strcasecmp(xd->group,group) == 0)
            {
              k++;
              if (xd->group_desc != NULL)
                {
                   if (descxd == NULL)
                     {
                       descxd = xd;
                     }
                   else
                     {
                       /* more than one desc */
                       mydelete(xd->group_desc);
                     }
                }
              /* check only the first entry */
              if (k == 1)
                {
                  firstxd = xd;
                }
            }
        }
      xd = irlist_get_next(xd);
    }

  if (k == 0)
    return k;

  if (descxd == NULL)
    return k;

  if (descxd == firstxd)
    return k;

  firstxd->group_desc = descxd->group_desc;
  descxd->group_desc = NULL;
  return k;
}

int add_default_groupdesc(const char *group) {
  xdcc *xd; 
  xdcc *firstxd;
  int k;

  updatecontext();

  k = 0;
  firstxd = NULL;
  xd = irlist_get_head(&gdata.xdccs); 
  while(xd) 
    {
      if (xd->group != NULL)
        {
          if (strcasecmp(xd->group,group) == 0)
            {
              k++;
              if (xd->group_desc != NULL) 
                return 0;
              
              /* check only the first entry */
              if (k == 1)
                {
                  firstxd = xd;
                }
            }
        }
      xd = irlist_get_next(xd);
    }

  if (k != 1)
    return k;

  firstxd->group_desc = mycalloc(strlen(group)+1);
  strcpy(firstxd->group_desc,group); 
  return k; 
}

void strtextcpy(char *d, const char *s) {
   const char *x;
   char *w;
   char ch;
   size_t l;
   
   if (d == NULL)
      return;
   if (s == NULL)
      return;
   
   /* ignore path */
   x = strrchr(s, '/');
   if (x != NULL)
      x ++;
   else
      x = s;
   
   strcpy(d,x);
   /* ignore extension */
   w = strrchr(d, '.');
   if (w != NULL)
      *w = 0;
   
   l = strlen(d);
   if ( l < 8 )
      return;

   w = d + l - 1;
   ch = *w;
   switch (ch) {
   case '}':
      w = strrchr(d, '{');
      if (w != NULL)
         *w = 0;
      break;
   case ')':
      w = strrchr(d, '(');
      if (w != NULL)
         *w = 0;
      break;
   case ']':
      w = strrchr(d, '[');
      if (w != NULL)
         *w = 0;
      break;
   }

   /* strip numbers */
   x = d;
   w = d;
   for (;;) {
      ch = *(x++);
      *w = ch;
      if (ch == 0)
         break;
      if (isalpha(ch))
         w++;
   }
}

void strpathcpy(char *d, const char *s) {
   char *w;
   
   if (d == NULL)
      return;
   if (s == NULL)
      return;
   
   strcpy(d,s);
   
   /* ignore file */
   w = strrchr(d, '/');
   if (w != NULL)
      w[0] = 0;
}

void update_natip (const char *var)
{
  struct hostent *hp;
  struct in_addr old;
  struct in_addr in;
  long oldip;
  char *oldtxt;

  if (var == NULL)
    return;

  gdata.usenatip = 1;
  if (gdata.r_ourip != 0)
    return;

  bzero((char *)&in, sizeof(in));
  if (inet_aton(var, &in) == 0)
    {
      hp = gethostbyname(var);
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
  if (old.s_addr == in.s_addr)
    return;

  oldip = gdata.ourip;
  gdata.ourip = ntohl(in.s_addr);
  if (oldip != 0 )
    {
      oldtxt = strdup(inet_ntoa(old));
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "DCC IP changed from %s to %s", oldtxt, inet_ntoa(in));
      free(oldtxt);
    }
 
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

void privmsg_chan(const channel_t *ch, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg_chan(ch, format, args);
  va_end(args);
}

void vprivmsg_chan(const channel_t *ch, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  int len;
  
  if (!ch) return;
  if (!ch->name) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"PRVMSG-CHAN: Output too large, ignoring!");
      return;
    }
  
  writeserver_channel(ch->delay, ch->name, "PRIVMSG %s :%s", ch->name, tempstr);
}

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
writeserver_channel (int delay, const char *chan, const char *format, ... )
{
  va_list args;
  va_start(args, format);
  vwriteserver_channel(delay, chan, format, args);
  va_end(args);
}

void vwriteserver_channel(int delay, const char *chan, const char *format, va_list ap)
{
  char *msg;
  channel_announce_t *item;
  int len;
  
  msg = mycalloc(maxtextlength+1);
  
  len = vsnprintf(msg,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"WRITESERVER: Output too large, ignoring!");
      mydelete(msg);
      return;
    }
  
#ifdef MULTINET
  if (gdata.exiting || (gnetwork->serverstatus != SERVERSTATUS_CONNECTED))
#else /* MULTINET */
  if (gdata.exiting || (gdata.serverstatus != SERVERSTATUS_CONNECTED))
#endif /* MULTINET */
    {
      mydelete(msg);
      return;
    }
  
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
  
#ifdef MULTINET
  if (irlist_size(&(gnetwork->serverq_channel)) < MAXSENDQ)
#else /* MULTINET */
  if (irlist_size(&gdata.serverq_channel) < MAXSENDQ)
#endif /* MULTINET */
    {
#ifdef MULTINET
      item = irlist_add(&(gnetwork->serverq_channel), sizeof(channel_announce_t));
#else /* MULTINET */
      item = irlist_add(&gdata.serverq_channel, sizeof(channel_announce_t));
#endif /* MULTINET */
      item->delay = delay;
      item->chan = mycalloc(strlen(chan)+1);
      strcpy(item->chan, chan);
      item->msg = mycalloc(len + 1);
      strcpy(item->msg, msg);
    }
  else
    {
      outerror(OUTERROR_TYPE_WARN,"Server queue is very large. Dropping additional output.");
    }
  
  mydelete(msg);
  return;
}

void sendannounce(void)
{
  channel_announce_t *item;
  
#ifdef MULTINET
  item = irlist_get_head(&(gnetwork->serverq_channel));
#else /* MULTINET */
  item = irlist_get_head(&gdata.serverq_channel);
#endif /* MULTINET */
  if (!item)
    return;

  if ( --(item->delay) > 0 )
    return;

  writeserver(WRITESERVER_SLOW, "%s", item->msg);
  mydelete(item->chan);
  mydelete(item->msg);
#ifdef MULTINET
  irlist_delete(&(gnetwork->serverq_channel), item);
#else /* MULTINET */
  irlist_delete(&gdata.serverq_channel, item);
#endif /* MULTINET */
}

void stoplist(const char *nick)
{
  char *item;
  char *copy;
  char *end;
  char *inick;
  int stopped = 0;
  
  ioutput(CALLTYPE_MULTI_FIRST,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC STOP from %s", nick);
#ifdef MULTINET
  item = irlist_get_head(&(gnetwork->xlistqueue));
#else /* MULTINET */
  item = irlist_get_head(&gdata.xlistqueue);
#endif /* MULTINET */
  while (item)
    {
      if (strcasecmp(item,nick) == 0)
        {
           stopped ++;
#ifdef MULTINET
           item = irlist_delete(&(gnetwork->xlistqueue), item);
#else /* MULTINET */
           item = irlist_delete(&gdata.xlistqueue, item);
#endif /* MULTINET */
           continue;
         }
      item = irlist_get_next(item);
    }
  
#ifdef MULTINET
  item = irlist_get_head(&(gnetwork->serverq_slow));
#else /* MULTINET */
  item = irlist_get_head(&gdata.serverq_slow);
#endif /* MULTINET */
  while (item)
    {
      inick = NULL;
      copy = mymalloc(strlen(item)+1);
      strcpy(copy, item);
      inick = strchr(copy, ' ');
      if (inick != NULL)
        {
          *(inick++) = 0;
          end = strchr(inick, ' ');
          if (end != NULL)
            {
              *(end++) = 0;
              if (strcasecmp(inick,nick) == 0)
                {
                   if ( (strcmp(copy,"PRIVMSG") == 0) || (strcmp(copy,"NOTICE") == 0) )
                     {
                       stopped ++;
#ifdef MULTINET
                       item = irlist_delete(&(gnetwork->serverq_slow), item);
#else /* MULTINET */
                       item = irlist_delete(&gdata.serverq_slow, item);
#endif /* MULTINET */
                       continue;
                     }
                }
            }
        }
      item = irlist_get_next(item);
    }
  ioutput(CALLTYPE_MULTI_END,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," (stopped %d)", stopped);
}

void notifyqueued_nick(const char *nick)
{
  int i;
  unsigned long rtime, lastrtime;
  pqueue *pq;
  transfer *tr;
  
  updatecontext();
  
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
      
      if (!strcasecmp(pq->nick, nick))
        {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D,COLOR_YELLOW,
                  "Notifying Queued status to %s",
                  nick);
          notice_slow(pq->nick,"Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                  (long)(gdata.curtime-pq->queuedtime)/60/60,
                  (long)((gdata.curtime-pq->queuedtime)/60)%60,
                  pq->xpack->desc,
                  i,
                  irlist_size(&gdata.mainqueue),
                  lastrtime/60/60,
                  (lastrtime/60)%60,
                  (rtime >= 359999) ? "more" : "less");
        }
      i++;
      pq = irlist_get_next(pq);
    }
  
}

void look_for_file_remove(void)
{

  xdcc *xd;
  userinput *pubplist;
  char *tempstr;
  int r;
  int n;
  
  updatecontext();

  /* look to see if any files changed */
  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
       n ++;
       r = look_for_file_changes(xd);
       if (r == 0)
         {
            xd = irlist_get_next(xd);
            continue;
         }
       
       pubplist = mycalloc(sizeof(userinput));
       tempstr = mycalloc(maxtextlength);
       snprintf(tempstr,maxtextlength-1,"remove %d", n);
       u_fillwith_console(pubplist,tempstr);
       u_parseit(pubplist);
       mydelete(pubplist);
       mydelete(tempstr);
       
       /* start over */
       xd = irlist_get_head(&gdata.xdccs);
       n = 0;
    }
     
  return;
}

#ifdef MULTINET
int has_closed_servers(void)
{
  int ss;

  for (ss=0; ss<gdata.networks_online; ss++)
    {
      if (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED)
        return 0;
    }
  return 1;
}
#endif /* MULTINET */

int has_joined_channels(int all)
{
  int j;
#ifdef MULTINET
  int n;
  int ss;
#endif /* MULTINET */
  channel_t *ch;

  j=0;
#ifdef MULTINET
  for (ss=0; ss<gdata.networks_online; ss++) {
    ch = irlist_get_head(&gdata.networks[ss].channels);
    while(ch)
      {
         if ((ch->flags & CHAN_ONCHAN) == 0)
           {
             if (all != 0)
               return 0;
           }
         else
           {
             j++;
             n++;
           }
         ch = irlist_get_next(ch);
       }
    }
#else /* MULTINET */
  ch = irlist_get_head(&gdata.channels);
  while(ch)
    {
       if ((ch->flags & CHAN_ONCHAN) == 0)
         {
           if (all != 0)
             return 0;
         }
       else
         j++;
       ch = irlist_get_next(ch);
     }
#endif /* MULTINET */
  return j;
}

void reset_download_limits(void)
{
  int num;
  int new;
  xdcc *xd;

  num = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      num++;
      if (xd->dlimit_max != 0)
        {
          new = xd->gets + xd->dlimit_max;
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                  "Resetting download limit of pack %d, used %d",
                  num, new - xd->dlimit_used);
          xd->dlimit_used = new;
        }
      xd = irlist_get_next(xd);
    }
}

#ifdef USE_GEOIP

#define GEOIP_FLAGS GEOIP_MEMORY_CACHE

GeoIP *gi = NULL;

char *check_geoip(transfer *const t);
char *check_geoip(transfer *const t)
{
  static char hostname[20];
  static char code[20];
  const char *result;

  if (gi == NULL)
    gi = GeoIP_new(GEOIP_FLAGS);

  snprintf(hostname, sizeof(hostname), "%ld.%ld.%ld.%ld",
            t->remoteip>>24, (t->remoteip>>16) & 0xFF, (t->remoteip>>8) & 0xFF, t->remoteip & 0xFF );
  result = GeoIP_country_code_by_addr(gi, hostname);
  if (result == NULL)
    {
      code[0] = 0;
      return code;
    }

  code[0] = tolower(result[0]);
  code[1] = tolower(result[1]);
  code[2] = 0;
  return code;
}
#endif /* USE_GEOIP */

void check_new_connection(transfer *const tr)
{
#ifdef USE_GEOIP
  const char *country;
  char *msg;
#endif /* USE_GEOIP */
  
#ifdef USE_GEOIP
  country = check_geoip(tr);
  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"GeoIP [%s]: Info %ld.%ld.%ld.%ld -> %s)",
            tr->nick,
            tr->remoteip>>24, (tr->remoteip>>16) & 0xFF,
            (tr->remoteip>>8) & 0xFF, tr->remoteip & 0xFF,
            country);
   
   if (irlist_size(&gdata.geoipcountry))
     {
       if (!verifyshell(&gdata.geoipcountry, country))
         {
           if (!verifyshell(&gdata.geoipexcludenick, tr->nick))
             {
               msg = mycalloc(maxtextlength);
               if (country == NULL)
                  country = "error";
               snprintf(msg, maxtextlength - 1, "Sorry, no downloads to your country = %s", country);
               t_closeconn(tr, msg, 0);
               ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                        "IP from other country (%s) detected", country);
               mydelete(msg);
               return;
             }
         }
     }
#endif /* USE_GEOIP */
  
  if ((gdata.ignoreduplicateip) && (gdata.maxtransfersperperson > 0))
    {
      check_duplicateip(tr);
    }
}

void check_duplicateip(transfer *const newtr)
{
  igninfo *ignore;
  char *bhostmask;
  transfer *tr;
  int found;
  int num;

  num = 24 * 60; /* 1 day */
  found = 0;
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if ((tr->tr_status == TRANSFER_STATUS_SENDING) &&
         (tr->remoteip == newtr->remoteip))
        {
          if (strcmp(tr->hostname,"man"))
            found ++;
        }
      tr = irlist_get_next(tr);
    }

  if (found <= gdata.maxtransfersperperson)
    return;

  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if ((tr->tr_status == TRANSFER_STATUS_SENDING) &&
         (tr->remoteip == newtr->remoteip))
        {
          if (strcmp(tr->hostname,"man"))
            {
              t_closeconn(tr, "You are being punished for pararell downloads", 0);
              bhostmask = mymalloc(strlen(tr->hostname)+5);
              sprintf(bhostmask, "*!*@%s", tr->hostname);
              ignore = irlist_get_head(&gdata.ignorelist);
              while(ignore)
                {
                  if (ignore->regexp && !regexec(ignore->regexp,bhostmask,0,NULL,0))
                    {
                      break;
                    }
                  ignore = irlist_get_next(ignore);
                }
              
              if (!ignore)
                {
                  char *tempstr;
                  
                  ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
                  ignore->regexp = mycalloc(sizeof(regex_t));
                  
                  ignore->hostmask = mymalloc(strlen(bhostmask)+1);
                  strcpy(ignore->hostmask,bhostmask);
                  
                  tempstr = hostmasktoregex(bhostmask);
                  if (regcomp(ignore->regexp,tempstr,REG_ICASE|REG_NOSUB))
                    {
                      ignore->regexp = NULL;
                    }
                  
                  ignore->flags |= IGN_IGNORING;
                  ignore->lastcontact = gdata.curtime;
                  
                  mydelete(tempstr);
                }
              
              ignore->flags |= IGN_MANUAL;
              ignore->bucket = (num*60)/gdata.autoignore_threshold;
              
              ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
                      "same IP detected, Ignore activated for %s which will last %i min",
                      bhostmask,num);
            }
        }
      tr = irlist_get_next(tr);
    }

  write_statefile();
}

#ifdef USE_CURL
static CURLM *cm;
static irlist_t fetch_trans;
static int fetch_started;
#endif /* USE_CURL */

static xdcc xdcc_statefile;
static int dinoex_lasthour;

void startup_dinoex(void)
{
#ifdef USE_CURL
  CURLcode cs;
#endif /* USE_CURL */

  bzero((char *)&xdcc_statefile, sizeof(xdcc_statefile));
  xdcc_statefile.note = mymalloc(1);
  strcpy(xdcc_statefile.note,"");
  xdcc_statefile.file_fd = FD_UNUSED;
  xdcc_statefile.has_md5sum = 2;
  xdcc_statefile.has_crc32 = 2;
  dinoex_lasthour = -1;
#ifdef USE_CURL
  bzero((char *)&fetch_trans, sizeof(fetch_trans));
  fetch_started = 0;

  cs = curl_global_init(CURL_GLOBAL_ALL);
  if (cs != 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
              "curl_global_init failed with %d", cs);
    }
  cm = curl_multi_init();
  if (cm == NULL)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
              "curl_multi_init failed");
    }
#endif /* USE_CURL */
}

void shutdown_dinoex(void)
{
#ifdef USE_GEOIP
  if (gi != NULL)
    {
      GeoIP_delete(gi);
      gi = NULL;
    }
#endif
#ifdef USE_CURL
  curl_global_cleanup();
#endif /* USE_CURL */
}

void rehash_dinoex(void)
{
#ifdef USE_GEOIP
  if (gi != NULL)
    {
      GeoIP_delete(gi);
      gi = NULL;
    }
#endif
}

static int send_statefile(void)
{
  xdcc *xd;
  transfer *tr;
  char *nick;
  const char *hostname;
  char *sendnamestr;
  struct stat st;
  int xfiledescriptor;

  updatecontext();

  nick = gdata.send_statefile;
  hostname = "man";
  xd = &xdcc_statefile;
  xd->file = gdata.statefile;
  xd->desc = gdata.statefile;
  xd->minspeed = gdata.transferminspeed;
  xd->maxspeed = gdata.transfermaxspeed;
  xfiledescriptor = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (xfiledescriptor < 0)
    return 1;
  
  if (fstat(xfiledescriptor,&st) < 0)
    {
      close(xfiledescriptor);
      return 1;
    }

  xd->st_size  = st.st_size;
  xd->st_dev   = st.st_dev;
  xd->st_ino   = st.st_ino;
  xd->mtime    = st.st_mtime;
  close(xfiledescriptor);

  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
          "Send: %s to %s bytes=%" LLPRINTFMT "u",
          gdata.statefile, nick, (unsigned long long)xd->st_size);
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
#ifdef MULTINET
  tr->net = gnetwork->net;
#endif /* MULTINET */
  t_setuplisten(tr);
   
  if (tr->tr_status != TRANSFER_STATUS_LISTENING)
    return 1;

  sendnamestr = getsendname(tr->xpack->file);
  privmsg_fast(nick,"\1DCC SEND %s %lu %i %" LLPRINTFMT "u\1",
               sendnamestr,
               gdata.ourip,
               tr->listenport,
               (unsigned long long)tr->xpack->st_size);
  mydelete(sendnamestr);
  return 0;
}

void update_hour_dinoex(int hour, int minute)
{
  xdcc *xd;
  transfer *tr;
  int sendrunning;
  int lastminute;
#ifdef MULTINET
  gnetwork_t *backup;
  int net = 0;
#endif /* MULTINET */

  if (!gdata.send_statefile)
    return;

  updatecontext();

  if (dinoex_lasthour == hour)
    return;

  minute %= 60;
  lastminute = gdata.send_statefile_minute + 10;
  if (lastminute < 60)
    {
      if (minute < gdata.send_statefile_minute)
        return;
      if (minute >= lastminute)
        return;
    }
  else
    {
      lastminute = lastminute % 60;
      if ((minute < gdata.send_statefile_minute) && (minute > lastminute))
        return;
    }

#ifdef MULTINET
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
     return;

 /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gnetwork->lastservercontact >= 150)
     return;

#endif /* MULTINET */
#ifndef MULTINET
  if (gdata.serverstatus != SERVERSTATUS_CONNECTED)
     return;

  /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gdata.lastservercontact >= 150)
     return;

#endif /* not MULTINET */
  if (has_joined_channels(0) < 1)
     return;

  xd = &xdcc_statefile;
  sendrunning = 0;
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if (xd == tr->xpack)
        {
          sendrunning++;
        }
      tr = irlist_get_next(tr);
    } 
  if (sendrunning == 0)
    {
       if (send_statefile() == 0)
         dinoex_lasthour = hour;
    } 
    
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
}

/* iroffer-lamm: @find and long !list */
int noticeresults(const char *nick, const char *match)
{
  int             i, j, k, len;
  char           *tempstr = mycalloc(maxtextlength);
  char           *sizestrstr; 
  char           *tempr;
  regex_t        *regexp = mycalloc(sizeof(regex_t));
  xdcc           *xd;
  
  len = k = 0;
  
  tempr = hostmasktoregex(match);

  if (!regcomp(regexp, tempr, REG_ICASE | REG_NOSUB)) {
    i = 1;
    xd = irlist_get_head(&gdata.xdccs);
    while (xd) {
      if (!regexec(regexp, xd->file, 0, NULL, 0) || !regexec(regexp, xd->desc, 0, NULL, 0) || !regexec(regexp, xd->note, 0, NULL, 0)) {
        if (!k) {
          if (gdata.slotsmax - irlist_size(&gdata.trans) < 0)
            j = irlist_size(&gdata.trans);
          else
            j = gdata.slotsmax;
          snprintf(tempstr, maxtextlength - 1, "XDCC SERVER - Slot%s:[%i/%i]", j != 1 ? "s" : "", j - irlist_size(&gdata.trans), j);
          len = strlen(tempstr);
           if (gdata.slotsmax <= irlist_size(&gdata.trans)) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Queue:[%i/%i]", irlist_size(&gdata.mainqueue), gdata.queuesize);
            len = strlen(tempstr);
          }
          if (gdata.transferminspeed > 0) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Min:%1.1fKB/s", gdata.transferminspeed);
            len = strlen(tempstr);
          }
          if (gdata.transfermaxspeed > 0) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Max:%1.1fKB/s", gdata.transfermaxspeed);
            len = strlen(tempstr);
          }
          if (gdata.maxb) {
            snprintf(tempstr + len, maxtextlength - 1 - len, ", Cap:%i.0KB/s", gdata.maxb / 4);
            len = strlen(tempstr);
          }
#ifdef MULTINET
          snprintf(tempstr + len, maxtextlength - 1 - len, " - /msg %s xdcc send x -",
                   (gnetwork->user_nick ? gnetwork->user_nick : "??"));
#else
          snprintf(tempstr + len, maxtextlength - 1 - len, " - /msg %s xdcc send x -",
                   (gdata.user_nick ? gdata.user_nick : "??"));
#endif /* MULTINET */
          len = strlen(tempstr);
          if (!strcmp(match, "*"))
            snprintf(tempstr + len, maxtextlength - 1 - len, " Packs:");
          else
            snprintf(tempstr + len, maxtextlength - 1 - len, " Found:");
          len = strlen(tempstr);
        }
        sizestrstr = sizestr(0, xd->st_size);
        snprintf(tempstr + len, maxtextlength - 1 - len, " #%i:%s,%s", i, xd->desc, sizestrstr);
        if (strlen(tempstr) > 400) {
          snprintf(tempstr + len, maxtextlength - 1 - len, " [...]");
          notice_slow(nick, tempstr);
          snprintf(tempstr, maxtextlength - 2, "[...] #%i:%s,%s", i, xd->desc, sizestrstr);
        }
        len = strlen(tempstr);
        mydelete(sizestrstr);
        k++;
      }
      i++;
      xd = irlist_get_next(xd);
    }
  }
  
  if (k)
    notice_slow(nick, tempstr);
  mydelete(tempr);
  mydelete(regexp);
  mydelete(tempstr);
  return k;
}

static const char *badcrc = "badcrc";

const char *validate_crc32(xdcc *xd, int quiet)
{
   char *newcrc;
   char *line;
   const char *x;
   char *w;
   regex_t *regexp;
  
   if (xd->has_crc32 == 0) {
     if (quiet)
       return NULL;
     else
       return "no CRC32 calculated";
   }

   newcrc = mycalloc(10);
   snprintf(newcrc,10,"%.8lX", xd->crc32);
   line = mycalloc(strlen(xd->file)+1);

   /* ignore path */
   x = strrchr(xd->file, '/');
   if (x != NULL) 
      x ++; 
   else 
      x = xd->file;

   strcpy(line, x);
   /* ignore extension */
   w = strrchr(line, '.');
   if (w != NULL) 
      *w = 0;

   caps(line);
   if (strstr(line, newcrc) != NULL) {
     if (quiet)
       x = NULL;
     else
       x = "CRC32 verified OK";
     /* unlock pack */
     if ((quiet == 2) && (xd->lock != NULL)) {
       if (strcmp(xd->lock,badcrc)) {
         mydelete(xd->lock);
         xd->lock = NULL;
       }
     }
   }
   else {
     x = "CRC32 not found";
     regexp = mycalloc(sizeof(regex_t));
     if (!regcomp(regexp, "[0-9A-F]{8,}", REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
       if (!regexec(regexp, line, 0, NULL, 0)) {
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"crc expected %s, failed %s", newcrc, line);
         x = "CRC32 failed";
         if (quiet == 2) {
           xd->lock = mycalloc(strlen(badcrc)+1);
           strcpy(xd->lock,badcrc);
         }
       }
     }
     mydelete(regexp);
   }
   mydelete(line)
   mydelete(newcrc)
   return x;
}

void autoadd_scan(const char *dir, const char *group)
{
   userinput *uxdl;
   char *line;

   if (dir == NULL)
      return;

   line = mycalloc(maxtextlength);
   if (group != NULL)
     snprintf(line,maxtextlength -1,"A A A A A ADDGROUP %s %s", group, dir);
   else
     snprintf(line,maxtextlength -1,"A A A A A ADDNEW %s", dir);

   uxdl = mycalloc(sizeof(userinput));
   u_fillwith_msg(uxdl,NULL,line);
   uxdl->method = method_out_all;
   u_parseit(uxdl);
   mydelete(line);
}

void autoadd_all(void)
{
  char *dir;

  updatecontext();

  dir = irlist_get_head(&gdata.autoadd_dirs);
  while (dir)
    {
      autoadd_scan(dir, gdata.autoadd_group);
      dir = irlist_get_next(dir);
    }
}

#ifdef USE_CURL

typedef struct
{
  userinput u;
#ifdef MULTINET
  int net;
#endif /* MULTINET */
  char *name;
  char *url;
  char *vhosttext;
  FILE *writefd;
  off_t resumesize;
  char *errorbuf;
  CURL *curlhandle;
} fetch_curl_t;

void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd)
{
  CURLMcode cs;

  cs = curl_multi_fdset(cm, read_fd_set, write_fd_set, exc_fd_set, max_fd);
}

static fetch_curl_t *clean_fetch(fetch_curl_t *ft);
static fetch_curl_t *clean_fetch(fetch_curl_t *ft)
{
  fclose(ft->writefd);
  mydelete(ft->errorbuf);
  mydelete(ft->name);
  mydelete(ft->url);
  if (ft->u.snick != NULL)
    mydelete(ft->u.snick);
  if (ft->vhosttext != NULL)
    mydelete(ft->vhosttext);
  return irlist_delete(&fetch_trans, ft);
}

void fetch_perform(void)
{
  CURLMcode cms;
  CURLMsg *msg;
  CURL *ch;
  int running;
  int msgs_in_queue;
  int seen = 0;
  fetch_curl_t *ft;
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */

  do {
    cms = curl_multi_perform(cm, &running);
  } while (cms == CURLM_CALL_MULTI_PERFORM);

  if (running == fetch_started)
    return;

  updatecontext();
#ifdef MULTINET
  backup = gnetwork;
#endif /* MULTINET */
  do {
     msg = curl_multi_info_read(cm, &msgs_in_queue);
     if (msg == NULL)
       break;

     ch = msg->easy_handle;
     ft = irlist_get_head(&fetch_trans);
     while(ft)
       {
         if (ft->curlhandle == ch)
           {
#ifdef MULTINET
             gnetwork = &(gdata.networks[ft->net]);
#endif /* MULTINET */
             if (ft->errorbuf[0] != 0)
               outerror(OUTERROR_TYPE_WARN_LOUD,"fetch: %s",ft->errorbuf);
             if (msg->data.result != 0 )
               {
                 a_respond(&(ft->u),"fetch %s failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
               }
              else
               {
                 a_respond(&(ft->u),"fetch %s completed", ft->name);
               }
             curl_easy_cleanup(ft->curlhandle);
             seen ++;
             fetch_started --;
             ft = clean_fetch(ft);
             continue;
           }
         ft = irlist_get_next(ft);
       }
  } while (msgs_in_queue > 0);
  if (seen == 0)
    outerror(OUTERROR_TYPE_WARN_LOUD,"curlhandle not found ");
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
}

void start_fetch_url(const userinput *const u)
{
  off_t resumesize;
  fetch_curl_t *ft;
  char *fullfile;
  unsigned char *name;
  unsigned char *url;
  FILE *writefd;
  struct stat s;
  int retval;
  CURL *ch;
  CURLcode ces;
  CURLcode cms;

  name = u->arg1;
  url = u->arg2e;

  resumesize = 0;
  fullfile = mymalloc(strlen(gdata.uploaddir) + strlen(name) + 2);
  sprintf(fullfile, "%s/%s", gdata.uploaddir, name);
  writefd = fopen(fullfile, "w+");
  if ((writefd < 0) && (errno == EEXIST))
    {
      retval = stat(fullfile, &s);
      if (retval < 0)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Stat Upload File '%s': %s",
                   fullfile,strerror(errno));
          a_respond(u,"File Error, File couldn't be opened for writing");
          mydelete(fullfile);
          return;
        }
      writefd = fopen(fullfile, "a");
      if (writefd >= 0)
        {
          resumesize = s.st_size;
        }
    }
  if (writefd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Access Upload File '%s': %s",
               fullfile,strerror(errno));
      a_respond(u,"File Error, File couldn't be opened for writing");
      mydelete(fullfile);
      return;
    }
  
  updatecontext();
  ft = irlist_add(&fetch_trans, sizeof(fetch_curl_t));
  ft->u.method = u->method;
  if (u->snick != NULL)
    {
       ft->u.snick = mycalloc(strlen(u->snick)+1);;
       strcpy(ft->u.snick, u->snick);
    }
  ft->u.fd = u->fd;
  ft->u.chat = u->chat;
#ifdef MULTINET
  ft->net = gnetwork->net;
#endif /* MULTINET */
  ft->name = mycalloc(strlen(name)+1);
  strcpy((ft->name),name);
  ft->url = mycalloc(strlen(url)+1);
  strcpy((ft->url),url);
  ft->writefd = writefd;
  ft->resumesize = resumesize;
  ft->errorbuf = mycalloc(CURL_ERROR_SIZE);

  updatecontext();
  ch = curl_easy_init();
  if (ch == NULL)
    {
      a_respond(u,"Curl not ready");
      clean_fetch(ft);
      return;
    }

  ft->curlhandle = ch;

  ces = curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, ft->errorbuf);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt ERRORBUFFER failed with %d", ces);
      clean_fetch(ft);
      return;
    }

#if 0
  if (gdata.local_vhost)
    {
      char *vhosttext;

      ft->vhosttext = mycalloc(40);
      snprintf(ft->vhosttext,40,"%ld.%ld.%ld.%ld",
               gdata.local_vhost>>24,
               (gdata.local_vhost>>16) & 0xFF,
               (gdata.local_vhost>>8) & 0xFF,
               gdata.local_vhost & 0xFF);
      ces = curl_easy_setopt(ch, CURLOPT_INTERFACE, ft->vhosttext);
      if (ces != 0)
        {
          a_respond(u,"curl_easy_setopt INTERFACE for %s failed with %d", ft->vhosttext, ces);
          clean_fetch(ft);
          return;
        }
    }
#endif

  ces = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt NOPROGRESS failed with %d", ces);
      return;
    }

  ces = curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt NOSIGNAL failed with %d", ces);
      return;
    }

  ces = curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt FAILONERROR failed with %d", ces);
      return;
    }

  ces = curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt SSL_VERIFYPEER failed with %d", ces);
      return;
    }

  ces = curl_easy_setopt(ch, CURLOPT_URL, ft->url);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt URL failed with %d", ces);
      return;
    }

  ces = curl_easy_setopt(ch, CURLOPT_WRITEDATA, ft->writefd);
  if (ces != 0)
    {
      a_respond(u,"curl_easy_setopt WRITEDATA failed with %d", ces);
      return;
    }

  if (resumesize > 0L)
    {
      ces = curl_easy_setopt(ch, CURLOPT_RESUME_FROM_LARGE, ft->resumesize);
      if (ces != 0)
        {
          a_respond(u,"curl_easy_setopt RESUME_FROM failed with %d", ces);
          return;
        }
    }

  cms = curl_multi_add_handle(cm, ch);
  if (cms != 0)
    {
      a_respond(u,"curl_multi_add_handle failed with %d", cms);
      return;
    }

    a_respond(u,"fetch %s started", ft->name);
    fetch_started ++;
}
#endif /* USE_CURL */

char* getpart_eol(const char *line, int howmany)
{
  char *part;
  int li;
  size_t plen;
  int hi;
  
  li=0;
  
  for (hi = 1; hi < howmany; hi++)
    {
      while (line[li] != ' ')
        {
          if (line[li] == '\0')
            {
              return NULL;
            }
          else
            {
              li++;
            }
        }
      li++;
    }
  
  if (line[li] == '\0')
    {
      return NULL;
    }
  
  plen = strlen(line+li);
  part = mycalloc(plen+1);
  memcpy(part, line+li, plen);
  part[plen] = '\0';
  
  return part;
}

#ifdef MULTINET
int get_network(const char *arg1)
{
  int net;
  
  /* default */
  if (arg1 == NULL)
    return 0;
  
  /* numeric */
  net = atoi(arg1);
  if ((net > 0) && (net <= gdata.networks_online))
    return --net;
  
  /* text */
  for (net=0; net<gdata.networks_online; net++)
    {
      if (gdata.networks[net].name == NULL)
        continue;
      if (strcasecmp(gdata.networks[net].name,arg1) == 0)
        return net;
    }
  
  /* unknown */
  return -1;
}
#endif /* MULTINET */

void crc32_init(void)
{
  gdata.crc32build.crc = ~0;
  gdata.crc32build.crc_total = ~0;
}

void crc32_update(char *buf, unsigned long len)
{
  char *p;
  ir_uint32 crc = gdata.crc32build.crc;
  ir_uint32 crc_total = gdata.crc32build.crc_total;

  for (p = buf; len--; ++p) {
    crc = (crc >> 8) ^ crctable[(crc ^ *p) & 0xff];
    crc_total = (crc >> 8) ^ crctable[(crc_total ^ *p) & 0xff];
  }
  gdata.crc32build.crc = crc;
  gdata.crc32build.crc_total = crc_total;
}

void crc32_final(xdcc *xd)
{
  xd->crc32 = ~gdata.crc32build.crc;
  xd->has_crc32 = 1;
}

int disk_full(const char *path)
{
#ifndef NO_STATVFS
  struct statvfs stf;
#else
#ifndef NO_STATFS
  struct statfs stf;
#endif
#endif
  off_t freebytes;

  freebytes = 0L;
#ifndef NO_STATVFS
  if (statvfs(path, &stf) < 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "Unable to determine device sizes: %s",
              strerror(errno));
    }
  else
    {
      freebytes = (off_t)stf.f_bavail * (off_t)stf.f_frsize;
    }
#else
#ifndef NO_STATFS
  if (statfs(dpath, &stf) < 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "Unable to determine device sizes: %s",
              strerror(errno));
    }
  else
    {
      freebytes = (off_t)stf.f_bavail * (off_t)stf.f_bsize;
    }
#endif
#endif

  if (gdata.debug > 0) 
    ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
           "disk_free= %" LLPRINTFMT "d, required= %" LLPRINTFMT "d",
           (long long)freebytes, (long long)gdata.uploadminspace);

  if (freebytes >= gdata.uploadminspace)
    return 0;

  return 1;
}

/* End of File */
