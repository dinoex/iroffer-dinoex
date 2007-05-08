/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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
#include "dinoex_utilities.h"
#include "dinoex_admin.h"
#include "dinoex_misc.h"
#include "dinoex_config.h"

#include <ctype.h>

#ifdef USE_GEOIP
#include <GeoIP.h>
#endif /* USE_GEOIP */

#ifdef USE_CURL
#include <curl/curl.h>
#endif /* USE_CURL */

extern const ir_uint32 crctable[256];

static void admin_line(int fd, const char *line) {
   userinput *uxdl;
   char *full;
   
   if (line == NULL)
      return;
   
   uxdl = mycalloc(sizeof(userinput));
   full = mycalloc(maxtextlength);

   snprintf(full,maxtextlength -1,"A A A A A %s", line);
   u_fillwith_msg(uxdl,NULL,full);
   uxdl->method = method_fd;
   uxdl->fd = fd;
   uxdl->net = 0;
   uxdl->level = ADMIN_LEVEL_CONSOLE;
   u_parseit(uxdl);
   
   mydelete(uxdl);
   mydelete(full);
}

static void admin_run(const char *cmd) {
   int fd;
   const char *job;
   char *done;
   
   job = gdata.admin_job_file;
   if (job == NULL)
      return;

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
      admin_line(fd, cmd);
      close(fd);
    }
   mydelete(done)
}

void admin_jobs(void) {
   FILE *fin;
   const char *job;
   char *line;
   char *l;
   char *r;
   char *new;
   
   job = gdata.admin_job_file;
   if (job == NULL)
      return;

   fin = fopen(job, "ra" );
   if (fin == NULL)
      return;

   line = mycalloc(maxtextlength);
   while (!feof(fin)) {
      r = fgets(line, maxtextlength - 1, fin);
      if (r == NULL )
         break;
      l = line + strlen(line) - 1;
      while (( *l == '\r' ) || ( *l == '\n' ))
         *(l--) = 0;
      new = irlist_add(&gdata.jobs_delayed, strlen(line) + 1);
      strcpy(new, line);
   }
   mydelete(line)
   fclose(fin);
   unlink(job);
}


int hide_pack(const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;
  
  if (xd->lock == NULL)
    return 0;
  
  return 1;
}

int check_lock(const char* lockstr, const char* pwd)
{
  if (lockstr == NULL)
    return 0; /* no lock */
  if (pwd == NULL)
    return 1; /* locked */
  return strcmp(lockstr, pwd);
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
      oldtxt = mystrdup(inet_ntoa(old));
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "DCC IP changed from %s to %s", oldtxt, inet_ntoa(in));
      mydelete(oldtxt);
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
  
  if (gdata.exiting || (gnetwork->serverstatus != SERVERSTATUS_CONNECTED))
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
  
  if (irlist_size(&(gnetwork->serverq_channel)) < MAXSENDQ)
    {
      item = irlist_add(&(gnetwork->serverq_channel), sizeof(channel_announce_t));
      item->delay = delay;
      item->chan = mystrdup(chan);
      item->msg = mystrdup(msg);
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
  
  item = irlist_get_head(&(gnetwork->serverq_channel));
  if (!item)
    return;

  if ( --(item->delay) > 0 )
    return;

  writeserver(WRITESERVER_SLOW, "%s", item->msg);
  mydelete(item->chan);
  mydelete(item->msg);
  irlist_delete(&(gnetwork->serverq_channel), item);
}

void stoplist(const char *nick)
{
  char *item;
  char *copy;
  char *end;
  char *inick;
  int stopped = 0;
  
  ioutput(CALLTYPE_MULTI_FIRST, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC STOP from (%s on %s)",
          nick, gnetwork->name);
  item = irlist_get_head(&(gnetwork->xlistqueue));
  while (item)
    {
      if (strcasecmp(item,nick) == 0)
        {
           stopped ++;
           item = irlist_delete(&(gnetwork->xlistqueue), item);
           continue;
         }
      item = irlist_get_next(item);
    }
  
  item = irlist_get_head(&(gnetwork->serverq_slow));
  while (item)
    {
      inick = NULL;
      copy = mystrdup(item);
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
                       mydelete(copy);
                       item = irlist_delete(&(gnetwork->serverq_slow), item);
                       continue;
                     }
                }
            }
        }
      mydelete(copy);
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

int check_for_file_remove(int n)
{
  xdcc *xd;
  userinput *pubplist;
  char *tempstr;
  
  updatecontext();

  xd = irlist_get_nth(&gdata.xdccs, n-1);
  if (look_for_file_changes(xd) == 0)
    return 0;

  pubplist = mycalloc(sizeof(userinput));
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr,maxtextlength-1,"remove %d", n);
  u_fillwith_console(pubplist,tempstr);
  u_parseit(pubplist);
  mydelete(pubplist);
  mydelete(tempstr);
  return 1;
}

static int last_look_for_file_remove = -1;

void look_for_file_remove(void)
{
  int i;
  int p;
  int m;

  updatecontext();

  p = irlist_size(&gdata.xdccs);
  m = min2(gdata.monitor_files, p);
  for (i=0; i<m; i++) {
    last_look_for_file_remove ++;
    if (last_look_for_file_remove < 0 || last_look_for_file_remove >= p)
      last_look_for_file_remove = 0;

    if (check_for_file_remove(last_look_for_file_remove + 1))
      return;
  }
  return;
}

void set_default_network_name(void)
{
  char *var;

  var = mymalloc(10);
  snprintf(var, 10, "%d", gdata.networks_online + 1);

  if (gdata.networks[gdata.networks_online].name == NULL)
    {
      gdata.networks[gdata.networks_online].name = var;
      return;
    }

  if (strlen(gdata.networks[gdata.networks_online].name) == 0)
    {
      mydelete(gdata.networks[gdata.networks_online].name);
      gdata.networks[gdata.networks_online].name = var;
      return;
    }

  mydelete(var);
  return;
}

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

int has_joined_channels(int all)
{
  int j;
  int n;
  int ss;
  channel_t *ch;

  j=0;
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
    {
      if (gdata.geoipdatabase != NULL)
        gi = GeoIP_open(gdata.geoipdatabase, GEOIP_FLAGS);
      else
        gi = GeoIP_new(GEOIP_FLAGS);
    }

  if (gi == NULL)
    {
      code[0] = 0;
      return code;
    }

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
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "GeoIP [%s on %s]: Info %ld.%ld.%ld.%ld -> %s)",
            tr->nick,
            gdata.networks[ tr->net ].name,
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
               snprintf(msg, maxtextlength - 1, "Sorry, no downloads to your country = \"%s\", ask owner.", country);
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
                  
                  ignore->hostmask = mystrdup(bhostmask);
                  
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
int fetch_started;
#endif /* USE_CURL */

static xdcc xdcc_statefile;
static int dinoex_lasthour;

void startup_dinoex(void)
{
#ifdef USE_CURL
  CURLcode cs;
#endif /* USE_CURL */

  config_startup();
  bzero((char *)&xdcc_statefile, sizeof(xdcc_statefile));
  xdcc_statefile.note = mystrdup("");
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
  tr->nick = mystrdup(nick);
  tr->caps_nick = mystrdup(nick);
  caps(tr->caps_nick);
  tr->hostname = mystrdup(hostname);
  tr->xpack = xd;
  tr->net = gnetwork->net;
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
  int net = 0;

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

  gnetwork = &(gdata.networks[net]);
  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
     return;

 /* timeout for restart must be less then Transfer Timeout 180s */
  if (gdata.curtime - gnetwork->lastservercontact >= 150)
     return;

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
      if (hide_pack(xd) == 0) {
        if (!regexec(regexp, xd->file, 0, NULL, 0) ||
            !regexec(regexp, xd->desc, 0, NULL, 0) ||
            !regexec(regexp, xd->note, 0, NULL, 0)) {
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
            snprintf(tempstr + len, maxtextlength - 1 - len, " - /MSG %s XDCC SEND x -",
                     save_nick(gnetwork->user_nick));
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
        /* limit matches */
        if ((gdata.max_find != 0) && (k >= gdata.max_find))
          break;
        }
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

   if (verifyshell(&gdata.autocrc_exclude, xd->file)) {
     if (quiet)
       return NULL;
     else
       return "skipped CRC32";
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
       if (strcmp(xd->lock,badcrc) == 0) {
         ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"unlock Pack %d, File %s",
                 number_of_pack(xd), line);
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
           ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"lock Pack %d, File %s",
                   number_of_pack(xd), line);
           xd->lock = mystrdup(badcrc);
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
   int net = 0;

   if (dir == NULL)
      return;

   updatecontext();

   gnetwork = &(gdata.networks[net]);
   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"autoadd scan %s", dir);
   line = mycalloc(maxtextlength);
   if (group != NULL)
     snprintf(line,maxtextlength -1,"A A A A A ADDGROUP %s %s", group, dir);
   else
     snprintf(line,maxtextlength -1,"A A A A A ADDNEW %s", dir);

   uxdl = mycalloc(sizeof(userinput));
   u_fillwith_msg(uxdl,NULL,line);
   uxdl->method = method_out_all;
   uxdl->net = 0;
   uxdl->level = ADMIN_LEVEL_AUTO;
   u_parseit(uxdl);
   mydelete(uxdl);
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
  int net;
  char *name;
  char *url;
  char *vhosttext;
  FILE *writefd;
  off_t resumesize;
  char *errorbuf;
  CURL *curlhandle;
  long starttime;
} fetch_curl_t;

void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd)
{
  CURLMcode cs;

  cs = curl_multi_fdset(cm, read_fd_set, write_fd_set, exc_fd_set, max_fd);
}

static fetch_curl_t *clean_fetch(fetch_curl_t *ft)
{
  updatecontext();
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

void fetch_cancel(int num)
{
  fetch_curl_t *ft;
  int i;

  updatecontext();

  i = 0;
  ft = irlist_get_head(&fetch_trans);
  while(ft)
    {
      i++;
      if (++i == num)
        {
          ft = irlist_get_next(ft);
          continue;
        }
      a_respond(&(ft->u),"fetch %s canceled", ft->name);
      curl_multi_remove_handle(cm, ft->curlhandle);
      curl_easy_cleanup(ft->curlhandle);
      fetch_started --;
      ft = clean_fetch(ft);
    }
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
  gnetwork_t *backup;

  do {
    cms = curl_multi_perform(cm, &running);
  } while (cms == CURLM_CALL_MULTI_PERFORM);

  if (running == fetch_started)
    return;

  updatecontext();
  backup = gnetwork;
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
             gnetwork = &(gdata.networks[ft->net]);
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
             updatecontext();
             curl_easy_cleanup(ft->curlhandle);
             seen ++;
             fetch_started --;
             ft = clean_fetch(ft);
             continue;
           }
         ft = irlist_get_next(ft);
       }
  } while (msgs_in_queue > 0);
  updatecontext();
  if (seen == 0)
    outerror(OUTERROR_TYPE_WARN_LOUD,"curlhandle not found ");
  gnetwork = backup;
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
  if ((writefd == NULL) && (errno == EEXIST))
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
      resumesize = s.st_size;
      writefd = fopen(fullfile, "a");
    }
  if (writefd == NULL)
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
       ft->u.snick = mystrdup(u->snick);
    }
  ft->u.fd = u->fd;
  ft->u.chat = u->chat;
  ft->net = gnetwork->net;
  ft->name = mystrdup(name);
  ft->url = mystrdup(url);
  ft->writefd = writefd;
  ft->resumesize = resumesize;
  ft->errorbuf = mycalloc(CURL_ERROR_SIZE);
  ft->starttime = gdata.curtime;

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

void dinoex_dcl(const userinput *const u)
{
  fetch_curl_t *ft;
  double dl_total;
  double dl_size;
  int progress;
  int i = 0;

  updatecontext();
  ft = irlist_get_head(&fetch_trans);
  while(ft)
    { 
      i ++;
      dl_size = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_SIZE_DOWNLOAD, &dl_size);

      dl_total = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_total);

      progress = 0;
      progress = ((dl_size + 50) * 100) / max2(dl_total, 1);
      a_respond(u,"   %2i  fetch       %-32s   Receiving %d%%", i, ft->name, progress);
      ft = irlist_get_next(ft);
    }
}

void dinoex_dcld(const userinput *const u)
{
  fetch_curl_t *ft;
  double dl_total;
  double dl_size;
  double dl_speed;
  double dl_time;
  int progress;
  int started;
  int left;
  int i = 0;

  updatecontext();
  ft = irlist_get_head(&fetch_trans);
  while(ft)
    {
      i ++;
      dl_size = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_SIZE_DOWNLOAD, &dl_size);

      dl_total = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_total);

      dl_speed = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_SPEED_DOWNLOAD, &dl_speed);

      dl_time = 0.0;
      curl_easy_getinfo(ft->curlhandle, CURLINFO_TOTAL_TIME, &dl_time);
      
      started = min2(359999, gdata.curtime - ft->starttime);
      left = min2(359999, (dl_total - dl_size) /
                          ((int)(max2(dl_speed, 1))));
      progress = ((dl_size + 50) * 100) / max2(dl_total, 1);
      a_respond(u,"   %2i  fetch       %-32s   Receiving %d%%", i, ft->name, progress);
      a_respond(u,"                   %s", ft->url);
      a_respond(u,"  ^- %5.1fK/s    %6" LLPRINTFMT "iK/%6" LLPRINTFMT "iK  %2i%c%02i%c/%2i%c%02i%c",
                  (float)(dl_speed/1024),
                  (long long)(dl_size/1024),
                  (long long)(dl_total/1024),
                  started < 3600 ? started/60 : started/60/60 ,
                  started < 3600 ? 'm' : 'h',
                  started < 3600 ? started%60 : (started/60)%60 ,
                  started < 3600 ? 's' : 'm',
                  left < 3600 ? left/60 : left/60/60 ,
                  left < 3600 ? 'm' : 'h',
                  left < 3600 ? left%60 : (left/60)%60 ,
                  left < 3600 ? 's' : 'm');

      ft = irlist_get_next(ft);
    }
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


void identify_needed(int force)
{
  if (force == 0)
    {
      if ((gnetwork->next_identify > 0) && (gnetwork->next_identify >= gdata.curtime))
        return;
    }
  /* wait 1 sec before idetify again */
  gnetwork->next_identify = gdata.curtime + 1;
  privmsg("nickserv", "IDENTIFY %s", gdata.nickserv_pass);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "IDENTIFY send to nickserv on %s.", gnetwork->name);
}

void identify_check(const char *line)
{
  if (strstr(line, "Nickname is registered to someone else.") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "This nickname has been registered") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "This nickname is registered and protected.") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "please choose a different nick.") != NULL)
    {
      identify_needed(0);
    }
}

void changesec_dinoex(void)
{
  userinput *u;
  char *job;

  u = irlist_get_head(&gdata.packs_delayed);
  while (u)
    {
      if (strcmp(u->cmd,"REMOVE") == 0)
        {
          a_remove_delayed(u);
          mydelete(u->cmd);
          mydelete(u->arg1);
          mydelete(u->arg2);
          u = irlist_delete(&gdata.packs_delayed, u);
          /* process only one file */
          return;
        }
      if (strcmp(u->cmd,"ADD") == 0)
        {
          a_add_delayed(u);
          mydelete(u->cmd);
          mydelete(u->arg1);
          mydelete(u->arg2);
          u = irlist_delete(&gdata.packs_delayed, u);
          /* process only one file */
          return;
        }
      /* ignore */
      outerror(OUTERROR_TYPE_WARN, "Unknown cmd %s in packs_delayed", u->cmd);
      u = irlist_delete(&gdata.packs_delayed, u);
    }

  job = irlist_get_head(&gdata.jobs_delayed);
  while (job)
    {
       admin_run(job);
       job = irlist_delete(&gdata.jobs_delayed, job);
       return;
    }
}

static void admin_msg_line(const char *nick, char *line, int line_len, int level)
{
  userinput ui;

  if (line[line_len-1] == '\n') {
    line[line_len-1] = '\0';
    line_len--;
  }
  u_fillwith_msg(&ui, nick, line);
  ui.net = gnetwork->net;
  ui.level = level;
  u_parseit(&ui);
}

int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line, int line_len)
{
  int err = 0;

  if ( verifyhost(&gdata.adminhost, hostmask) ) {
    if ( verifypass2(gdata.adminpass, passwd) ) {
      admin_msg_line(nick, line, line_len, gdata.adminlevel);
      return 1;
    } else {
      err ++;
    }
  }
  if ( verifyhost(&gdata.hadminhost, hostmask) ) {
    if ( verifypass2(gdata.hadminpass, passwd) ) {
      admin_msg_line(nick, line, line_len, gdata.hadminlevel);
      return 1;
    } else {
      err ++;
    }
  }
  if (err == 0) {
    notice(nick, "ADMIN: %s is not allowed to issue admin commands", hostmask);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Hostname (%s on %s)",
            hostmask, gnetwork->name);
  }
  if (err > 0) {
    notice(nick, "ADMIN: Incorrect Password");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "Incorrect ADMIN Password (%s on %s)",
            hostmask, gnetwork->name);
  }
  return 0;
}

static const char *find_groupdesc(const char *group)
{
  xdcc *xd;
  int k;

  if (group == NULL)
    return "";

  k = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        {
          if (strcasecmp(xd->group,group) == 0)
            {
              return xd->group_desc;
            }
        }
      xd = irlist_get_next(xd);
    }

  return "";
}

void write_removed_xdcc(xdcc *xd)
{
  char *line;
  int len;
  int fd;

  if (gdata.xdccremovefile == NULL)
    return;

  fd = open(gdata.xdccremovefile,
            O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);

  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Create XDCC Remove File '%s': %s",
             gdata.xdccremovefile, strerror(errno));
    return;
  }

  line = mycalloc(maxtextlength);
  len = snprintf(line, maxtextlength -1, "\n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_file %s\n", xd->file);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_desc %s\n", xd->desc);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_note %s\n", xd->note);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_gets %d\n", xd->gets);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength -1, "xx_mins \n");
  else
    len = snprintf(line, maxtextlength -1, "xx_mins %f\n", xd->minspeed);
  write(fd, line, len);
  if (gdata.transferminspeed == xd->minspeed)
    len = snprintf(line, maxtextlength -1, "xx_maxs \n");
  else
    len = snprintf(line, maxtextlength -1, "xx_maxs %f\n", xd->maxspeed);
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_data %s\n", xd->group ? xd->group : "");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_trig \n");
  write(fd, line, len);
  len = snprintf(line, maxtextlength -1, "xx_trno %s\n", find_groupdesc(xd->group));
  write(fd, line, len);
  mydelete(line)

  close(fd);
}

/* End of File */
