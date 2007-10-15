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
#include "dinoex_http.h"
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

void cleanannounce(void)
{
  channel_announce_t *item;

  for (item = irlist_get_head(&(gnetwork->serverq_channel));
       item;
       item = irlist_delete(&(gnetwork->serverq_channel), item)) {
     mydelete(item->chan);
     mydelete(item->msg);
  }
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

static float guess_maxspeed(xdcc *xd)
{
  int nolimit = 1;
  float speed = 0.0;

  if (gdata.overallmaxspeed > 0) {
    speed = gdata.overallmaxspeed;
    nolimit = 0;
  }
  if (gdata.transfermaxspeed > 0) {
    if (nolimit != 0) {
      speed = gdata.transfermaxspeed;
      nolimit = 0;
    } else {
      speed = min2(speed, gdata.transfermaxspeed);
    }
  }
  if (xd->maxspeed > 0)
    speed = xd->maxspeed;
  if (speed <= 0.0)
    speed = 1000.0;
  return speed;
}

typedef struct {
  transfer *tr;
  int left;
} remaining_transfer_time;

static remaining_transfer_time *next_remaining_transfer(remaining_transfer_time *rm)
{
  if (rm == NULL)
    return rm;
  return irlist_get_next(rm);
}

void notifyqueued_nick(const char *nick)
{
  gnetwork_t *backup;
  pqueue *pq;
  transfer *tr;
  irlist_t list;
  irlist_t list2;
  remaining_transfer_time *remain;
  remaining_transfer_time *rm;
  unsigned long rtime;
  unsigned long lastrtime;
  float speed;
  int left;
  int i;

  updatecontext();

  /* make sortd list of all transfers */
  memset(&list, 0, sizeof(irlist_t));
  memset(&list2, 0, sizeof(irlist_t));
  for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
    left = min2(359999, (tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed, 0.001)*1024)));
    remain = irlist_add(&list2, sizeof(remaining_transfer_time));
    remain->tr = tr;
    remain->left = left;
    irlist_remove(&list2, remain);

    rm = irlist_get_head(&list);
    while(rm) {
      if (remain->left < rm->left)
        break;
      rm = irlist_get_next(rm);
    }
    if (rm != NULL) {
      irlist_insert_before(&list, remain, rm);
    } else {
      irlist_insert_tail(&list, remain);
    }
  }

  lastrtime = 0;
  remain = irlist_get_head(&list);
  if (remain != NULL)
    lastrtime = remain->left;

  /* if we are sending more than allowed, we need to skip the difference */
  for (i=0; i<irlist_size(&gdata.trans) - gdata.slotsmax; i++) {
    remain = next_remaining_transfer(remain);
    if (remain == NULL)
      break;
    lastrtime = remain->left;
  }

  updatecontext();
  i = 0;
  for (pq = irlist_get_head(&gdata.mainqueue); pq; pq = irlist_get_next(pq)) {
    i ++;
    rtime = lastrtime;
    remain = next_remaining_transfer(remain);
    if (remain != NULL)
      lastrtime = remain->left;
    speed = guess_maxspeed(pq->xpack);
    left = min2(359999, (pq->xpack->st_size)/((int)(max2(speed, 0.001)*1024)));
    lastrtime += left;

    if (nick != NULL) {
      if (pq->net != gnetwork->net)
        continue;

      if (strcasecmp(pq->nick, nick) != 0)
        continue;
    } else {
      gnetwork = &(gdata.networks[pq->net]);
    }

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D, COLOR_YELLOW,
            "Notifying Queued status to %s",
            pq->nick);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(&gdata.mainqueue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999) ? "more" : "less");
  }

  backup = gnetwork;
  i = 0;
  for (pq = irlist_get_head(&gdata.idlequeue); pq; pq = irlist_get_next(pq)) {
    i ++;
    rtime = lastrtime;
    remain = next_remaining_transfer(remain);
    if (remain != NULL)
      lastrtime = remain->left;
    speed = guess_maxspeed(pq->xpack);
    left = min2(359999, (pq->xpack->st_size)/((int)(max2(speed, 0.001)*1024)));
    lastrtime += left;

    if (nick != NULL) {
      if (pq->net != gnetwork->net)
        continue;

      if (strcasecmp(pq->nick, nick) != 0)
        continue;
    } else {
      gnetwork = &(gdata.networks[pq->net]);
    }

    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_D,COLOR_YELLOW,
            "Notifying Queued status to %s",
            pq->nick);
    notice_slow(pq->nick, "Queued %lih%lim for \"%s\", in position %i of %i. %lih%lim or %s remaining.",
                (long)(gdata.curtime-pq->queuedtime)/60/60,
                (long)((gdata.curtime-pq->queuedtime)/60)%60,
                pq->xpack->desc,
                i,
                irlist_size(&gdata.idlequeue),
                rtime/60/60,
                (rtime/60)%60,
                (rtime >= 359999) ? "more" : "less");
  }

  irlist_delete_all(&list);
  gnetwork = backup;
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

  if (tr->family != AF_INET)
    return;

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

  if (newtr->family != AF_INET)
    return;

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
              bhostmask = to_hostmask( "*", tr->hostname);
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
              mydelete(bhostmask);
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

void config_dinoex(void) {
  h_setup_listen();
}

void shutdown_dinoex(void)
{
  h_close_listen();
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
  h_reash_listen();
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
  tr->maxspeed = xd->maxspeed;
  tr->net = gnetwork->net;
  t_setuplisten(tr);
   
  if (tr->tr_status != TRANSFER_STATUS_LISTENING)
    return 1;

  t_setup_dcc(tr, nick);
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
   x = get_basename(xd->file);

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
  char *name;
  char *url;
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
  if (strstr(line, "Nickname is registered to someone else") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "This nickname has been registered") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "This nickname is registered and protected") != NULL)
    {
      identify_needed(0);
    }
  if (strstr(line, "please choose a different nick") != NULL)
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
  len = snprintf(line, maxtextlength -1, "xx_size %" LLPRINTFMT "i\n", xd->st_size);
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

static int check_manual_send(const char* hostname, int *man)
{
  if (man == NULL)
    return 0;

  if (!strcmp(hostname, "man")) {
    *man = 1;
  } else {
    *man = 0;
  }
  return *man;
}

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text)
{
  updatecontext();

  if (check_manual_send(hostname, man) == 0) {
    if (!verifyhost(&gdata.downloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (verifyhost(&gdata.nodownloadhost, hostmask)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (host denied): ");
      notice(nick, "** XDCC %s denied, I don't send transfers to %s", text, hostmask);
      return NULL;
    }
    if (gdata.restrictsend && !isinmemberlist(nick)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (restricted): ");
      if ((gdata.need_voice != 0) || (gdata.need_level != 0))
        notice(nick, "** XDCC %s denied, you must have voice on a known channel to request a pack", text);
      else
        notice(nick, "** XDCC %s denied, you must be on a known channel to request a pack", text);
      return NULL;
    }
    if (gdata.enable_nick && !isinmemberlist(gdata.enable_nick)) {
      ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (offline): ");
      notice(nick, "** XDCC %s denied, owner of this bot is not online", text);
      return NULL;
    }
  }

  if ((pack > irlist_size(&gdata.xdccs)) || (pack < 1)) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " (Bad Pack Number): ");
    notice(nick, "** Invalid Pack Number, Try Again");
    return NULL;
  }
  return irlist_get_nth(&gdata.xdccs, pack-1);
}

void autotrigger_add(xdcc *xd)
{
  autotrigger_t *at;

  at = irlist_add(&gdata.autotrigger, sizeof(autotrigger_t));
  at->pack = xd;
  at->word = xd->trigger;
}

void autotrigger_rebuild(void)
{
  xdcc *xd;

  irlist_delete_all(&gdata.autotrigger);
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    if (xd->trigger != NULL)
      autotrigger_add(xd);
    xd = irlist_get_next(xd);
  }
}

void start_md5_hash(xdcc *xd, int packnum)
{
  if (!gdata.attop) { gototop(); }
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "[MD5]: Calculating pack %d",packnum);

  gdata.md5build.file_fd = open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (gdata.md5build.file_fd >= 0) {
    gdata.md5build.xpack = xd;
    if (!gdata.nocrc32)
      crc32_init();
    MD5Init(&gdata.md5build.md5sum);
    if (set_socket_nonblocking(gdata.md5build.file_fd, 1) < 0) {
      outerror(OUTERROR_TYPE_WARN, "[MD5]: Couldn't Set Non-Blocking");
    }
  } else {
    outerror(OUTERROR_TYPE_WARN,
             "[MD5]: Pack %d: Cant Access Offered File '%s': %s",
             packnum, xd->file, strerror(errno));
             gdata.md5build.file_fd = FD_UNUSED;
             check_for_file_remove(packnum);
  }
}

void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch)
{
  char *line;
  userinput_method_e method;

  method = manplist->method;
  if (ch->pgroup) {
    line = mycalloc(maxtextlength);
    snprintf(line, maxtextlength -1, "A A A A A xdlgroup %s", ch->pgroup);
    u_fillwith_msg(manplist, name, line);
    mydelete(line);
  } else {
    if ((gdata.xdcclist_grouponly) || (method != method_xdl_channel)) {
      u_fillwith_msg(manplist, name, "A A A A A xdl");
    } else {
      u_fillwith_msg(manplist, name, "A A A A A xdlfull");
    }
  }
  manplist->method = method;
}
 
void close_server(void)
{
  FD_CLR(gnetwork->ircserver, &gdata.readset);
  shutdown_close(gnetwork->ircserver);
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
}

void queue_update_nick(irlist_t *list, const char *oldnick, const char *newnick)
{
  pqueue *pq;

  for (pq = irlist_get_head(list); pq; pq = irlist_get_next(pq)) {
    if (!strcasecmp(pq->nick, oldnick)) {
      mydelete(pq->nick);
      pq->nick = mystrdup(newnick);
    }
  }
}

void queue_reverify_restrictsend(irlist_t *list)
{
  gnetwork_t *backup;
  pqueue *pq;

  backup = gnetwork;
  for (pq = irlist_get_head(list); pq;) {
    gnetwork = &(gdata.networks[pq->net]);
    if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcmp(pq->hostname, "man") == 0) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (isinmemberlist(pq->nick)) {
      pq->restrictsend_bad = 0;
      pq = irlist_get_next(pq);
      continue;
    }
    if (!pq->restrictsend_bad) {
      pq->restrictsend_bad = gdata.curtime;
      if (gdata.restrictsend_warning) {
        notice(pq->nick, "You are no longer on a known channel");
      }
      pq = irlist_get_next(pq);
      continue;
    }
    if ((gdata.curtime - pq->restrictsend_bad) < gdata.restrictsend_timeout) {
      pq = irlist_get_next(pq);
      continue;
    }

    notice(pq->nick, "** Removed From Queue: You are no longer on a known channel");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Removed From Queue: %s on %s not in known Channel.",
            pq->nick, gdata.networks[ pq->net ].name);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
  gnetwork = backup;
}

void queue_pack_limit(irlist_t *list, xdcc *xd)
{
  gnetwork_t *backup;
  pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->xpack != xd) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick, "** Sorry, This Pack is over download limit for today.  Try again tomorrow.");
    notice_slow(pq->nick, xd->dlimit_desc);
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

void queue_punishslowusers(irlist_t *list, int network, const char *nick)
{
  gnetwork_t *backup;
  pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(nick, "** Removed From Queue: You are being punished for your slowness");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

int queue_xdcc_remove(irlist_t *list, int network, const char *nick)
{
  gnetwork_t *backup;
  pqueue *pq;
  int changed = 0;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(nick,
           "Removed you from the queue for \"%s\", you waited %li minute%s.",
           pq->xpack->desc,
           (long)(gdata.curtime-pq->queuedtime)/60,
           ((gdata.curtime-pq->queuedtime)/60) != 1 ? "s" : "");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    changed ++;
  }
  return changed;
}

void queue_pack_remove(irlist_t *list, xdcc *xd)
{
  gnetwork_t *backup;
  pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->xpack != xd) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(pq->nick, "** Removed From Queue: Pack removed");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

void queue_all_remove(irlist_t *list, const char *message)
{
  gnetwork_t *backup;
  pqueue *pq;

  backup = gnetwork;
  for (pq = irlist_get_head(list); pq; pq = irlist_delete(list, pq)) {
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick, message);
    mydelete(pq->nick);
    mydelete(pq->hostname);
  }
  gnetwork = backup;
}

char* addtoidlequeue(char *tempstr, const char* nick, const char* hostname, xdcc *xd, int pack, int inq)
{
  pqueue *tempq;

  updatecontext();
  if (gdata.idlequeuesize == 0)
    return NULL;

  if (inq >= gdata.maxidlequeuedperperson) {
    ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW," Denied (user/idle): ");
    snprintf(tempstr, maxtextlength,
             "Denied, You already have %i items queued, Try Again Later",
             inq);
    return tempstr;
  }
 
  if (irlist_size(&gdata.idlequeue) >= gdata.idlequeuesize) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (slot/idle): ");
    snprintf(tempstr, maxtextlength,
             "Idle queue of size %d is Full, Try Again Later",
             gdata.idlequeuesize);
    return tempstr;
  }

  ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Queued (idle slot): ");
  tempq = irlist_add(&gdata.idlequeue, sizeof(pqueue));
  tempq->queuedtime = gdata.curtime;
  tempq->nick = mystrdup(nick);
  tempq->hostname = mystrdup(hostname);
  tempq->xpack = xd;
  tempq->net = gnetwork->net;

  snprintf(tempstr, maxtextlength,
           "Added you to the idle queue for pack %d (\"%s\") in position %d. To Remove yourself at "
           "a later time type \"/MSG %s XDCC REMOVE\".",
           pack, xd->desc,
           irlist_size(&gdata.idlequeue),
           save_nick(gnetwork->user_nick));
  return tempstr;
}

void check_idle_queue(void)
{
  pqueue *pq;
  pqueue *mq;
  pqueue *tempq;
  transfer *tr;
  int usertrans;
  int pass;

  updatecontext();
  if (gdata.exiting)
    return;

  if (gdata.holdqueue)
    return;

  if (gdata.restrictlist && (has_joined_channels(0) == 0))
    return;

  if (irlist_size(&gdata.idlequeue) == 0)
    return;

  if (irlist_size(&gdata.mainqueue) >= gdata.queuesize)
    return;

  for (pass = 0; pass < 2; pass++) {
    for (pq = irlist_get_head(&gdata.idlequeue); pq; pq = irlist_get_next(pq)) {
      if (gdata.networks[pq->net].serverstatus != SERVERSTATUS_CONNECTED)
        continue;

      /* timeout for restart must be less then Transfer Timeout 180s */
      if (gdata.curtime - gdata.networks[pq->net].lastservercontact > 150)
        continue;

      usertrans=0;
      for (mq = irlist_get_head(&gdata.mainqueue); mq; mq = irlist_get_next(mq)) {
        if ((!strcmp(mq->hostname, pq->hostname)) || (!strcasecmp(mq->nick, pq->nick))) {
          usertrans++;
        }
      }

      if (usertrans >= gdata.maxqueueditemsperperson)
        continue;

      if (pass == 0)
        continue;

      usertrans=0;
      for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
        if ((!strcmp(tr->hostname, pq->hostname)) || (!strcasecmp(tr->nick, pq->nick))) {
          usertrans++;
        }
      }
      if (usertrans >= gdata.maxtransfersperperson)
        continue;

      break; /* found the person that will get the send */
    }
  }
  if (pq == NULL)
    return;

  tempq = irlist_add(&gdata.mainqueue, sizeof(pqueue));
  *tempq = *pq;
  irlist_delete(&gdata.idlequeue, pq);

  if (irlist_size(&gdata.mainqueue) &&
      (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax))) {
    sendaqueue(0, 0, NULL);
  }
}

int open_listen(int ipv6, ir_sockaddr_union_t *listenaddr, int *listen_socket, int port, int reuse, int search)
{ 
  int family;
  int rc;
  int tempc;

  updatecontext();

  if (ipv6 == 0)
    family = AF_INET;
  else
    family = AF_INET6;
  *listen_socket = socket(family, SOCK_STREAM, 0);
  if (*listen < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Could Not Create Socket, Aborting: %s", strerror(errno));
    *listen_socket = FD_UNUSED;
    return 1;
  }

  if (reuse) {
    tempc = 1;
    setsockopt(*listen_socket, SOL_SOCKET, SO_REUSEADDR, &tempc, sizeof(int));
  }

  bzero((char *) listenaddr, sizeof(ir_sockaddr_union_t));
  if (ipv6 == 0) {
    listenaddr->sa.sa_len = sizeof(struct sockaddr_in);
    listenaddr->sin.sin_family = AF_INET;
    listenaddr->sin.sin_addr.s_addr = INADDR_ANY;
    listenaddr->sin.sin_port = htons(port);
  } else {
    listenaddr->sa.sa_len = sizeof(struct sockaddr_in6);
    listenaddr->sin6.sin6_family = AF_INET6;
    listenaddr->sin6.sin6_port = htons(port);
  }

  if (search) {
    rc = ir_bind_listen_socket(*listen_socket, listenaddr);
  } else {
    rc = bind(*listen_socket, &(listenaddr->sa), listenaddr->sa.sa_len);
  }

  if (rc < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Couldn't Bind to Socket, Aborting: %s", strerror(errno));
    shutdown_close(*listen_socket);
    *listen_socket = FD_UNUSED;
    return 1; 
  } 

  if (listen(*listen_socket, 1) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't Listen, Aborting: %s", strerror(errno));
    shutdown_close(*listen_socket);
    *listen_socket = FD_UNUSED;
    return 1;
  }

  return 0;
} 

char *setup_dcc_local(ir_sockaddr_union_t *listenaddr)
{
   char *msg;

   if (listenaddr->sa.sa_family == AF_INET)
     listenaddr->sin.sin_addr = gnetwork->myip.sin.sin_addr;
   else
     listenaddr->sin6.sin6_addr = gnetwork->myip.sin6.sin6_addr;
   msg = mycalloc(maxtextlength);
   my_dcc_ip_port(msg, maxtextlength -1,
                  listenaddr, listenaddr->sa.sa_len);
   return msg;
}

int l_setup_file(upload * const l, struct stat *stp)
{
  char *fullfile;
  int retval;

  updatecontext();

  if (gdata.uploaddir == NULL) {
    l_closeconn(l, "No upload hosts or no uploaddir defined.", 0);
    return 1;
  }

  /* local file already exists? */
  fullfile = mymalloc(strlen(gdata.uploaddir) + strlen(l->file) + 2);
  sprintf(fullfile, "%s/%s", gdata.uploaddir, l->file);
 
  l->filedescriptor = open(fullfile,
                           O_WRONLY | O_CREAT | O_EXCL | ADDED_OPEN_FLAGS,
                           CREAT_PERMISSIONS );
 
  if ((l->filedescriptor < 0) && (errno == EEXIST)) {
    retval = stat(fullfile, stp);
    if (retval < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Stat Upload File '%s': %s",
               fullfile, strerror(errno));
      l_closeconn(l, "File Error, File couldn't be opened for writing", errno);
      mydelete(fullfile);
      return 1;
    }
    if (!S_ISREG(stp->st_mode) || (stp->st_size >= l->totalsize)) {
      l_closeconn(l,"File Error, That filename already exists",0);
      mydelete(fullfile);
      return 1;
    }
    l->filedescriptor = open(fullfile, O_WRONLY | O_APPEND | ADDED_OPEN_FLAGS);
    if (l->filedescriptor >= 0) {
      l->resumesize = l->bytesgot = stp->st_size;
      if (l->resumed <= 0) {
        close(l->filedescriptor);
        mydelete(fullfile);
        return 2; /* RESUME */
      }
    }
  }
  if (l->filedescriptor < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access Upload File '%s': %s",
             fullfile, strerror(errno));
    l_closeconn(l, "File Error, File couldn't be opened for writing", errno);
    mydelete(fullfile);
    return 1;
  }
  mydelete(fullfile);
  return 0;
}

int l_setup_listen(upload * const l)
{
  char *msg;
  ir_sockaddr_union_t listenaddr;
  int rc;
  int listenport;

  updatecontext();

  rc = open_listen(l->family, &listenaddr, &(l->clientsocket), 0, gdata.tcprangestart, 1);
  if (rc != 0) {
    l_closeconn(l, "Connection Lost", 0);
    return 1;
  }
 
  listenport = get_port(&listenaddr);
  msg = setup_dcc_local(&listenaddr);
  privmsg_fast(l->nick, "\1DCC SEND %s %s %" LLPRINTFMT "i %d\1",
               l->file, msg, (long long)(l->totalsize), l->token);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND sent to %s on %s, waiting for connection on %s",
          l->nick, gnetwork->name, msg);
  mydelete(msg);

  l->localport = listenport;
  l->connecttime = gdata.curtime;
  l->lastcontact = gdata.curtime;
  l->ul_status = UPLOAD_STATUS_LISTENING;
  return 0;
}

int l_setup_passive(upload * const l, char *token)
{
  int retval;
  struct stat s;

  updatecontext();

  /* strip T */
  if (token[strlen(token)-1] == 'T')
    token[strlen(token)-1] = '\0';
  l->token = atoi(token);

  retval = l_setup_file(l, &s);
  if (retval == 2)
    {
      privmsg_fast(l->nick, "\1DCC RESUME %s %i %" LLPRINTFMT "u %d\1",
                   l->file, l->remoteport, (unsigned long long)s.st_size, l->token);
      l->connecttime = gdata.curtime;
      l->lastcontact = gdata.curtime;
      l->ul_status = UPLOAD_STATUS_RESUME;
      return 0;
    }
  if (retval != 0)
    {
      return 1;
    }

  return l_setup_listen(l);
}

void l_setup_accept(upload * const l)
{
  SIGNEDSOCK int addrlen;
  ir_sockaddr_union_t remoteaddr;
  int listen_fd;
  char *msg;
 
  updatecontext();

  listen_fd = l->clientsocket;
  addrlen = sizeof(remoteaddr);
  if ((l->clientsocket = accept(listen_fd, &remoteaddr.sa, &addrlen)) < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    FD_CLR(listen_fd, &gdata.readset);
    close(listen_fd);
    l->clientsocket = FD_UNUSED;
    l_closeconn(l, "Connection Lost", 0);
    return;
  }

  ir_listen_port_connected(l->localport);

  FD_CLR(listen_fd, &gdata.readset);
  close(listen_fd);
 
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND connection received");
 
  if (set_socket_nonblocking(l->clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  notice(l->nick, "DCC Send Accepted, Connecting...");

  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(remoteaddr.sa), addrlen);
  l->remoteaddr = mystrdup(msg);
  mydelete(msg);
  l->remoteport = get_port(&remoteaddr);
  l->connecttime = gdata.curtime;
  l->lastcontact = gdata.curtime;
  l->ul_status = UPLOAD_STATUS_GETTING;
}

void child_resolver(void)
{
#if !defined(NO_GETADDRINFO)
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *res;
#else
  struct hostent *remotehost;
#endif
  struct sockaddr_in *remoteaddr;
  res_addrinfo_t rbuffer;
  ssize_t bytes;
  int i;
#if !defined(NO_GETADDRINFO)
  int status;
  int found;
  char portname[16];
#endif

  for (i=3; i<FD_SETSIZE; i++) {
    /* include [0], but not [1] */
    if (i != gnetwork->serv_resolv.sp_fd[1]) {
      close(i);
    }
  }

  /* enable logfile */
  gdata.logfd = FD_UNUSED;

#if !defined(NO_GETADDRINFO)
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  snprintf(portname, sizeof(portname), "%d",
           gnetwork->serv_resolv.to_port);
  status = getaddrinfo(gnetwork->serv_resolv.to_ip, portname,
                       &hints, &results);
  if ((status) || results == NULL) {
#else
  remotehost = gethostbyname(gnetwork->serv_resolv.to_ip);
  if (remotehost == NULL) {
#endif
#ifdef NO_HOSTCODES
    exit(10);
#else
    switch (h_errno) {
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

  remoteaddr = (struct sockaddr_in *)(&(rbuffer.ai_addr));
  rbuffer.ai_reset = 0;
#if !defined(NO_GETADDRINFO)
  found = -1;
  for (res = results; res; res = res->ai_next) {
    found ++;
    if (found < gnetwork->serv_resolv.next)
      continue;
    if (res->ai_next == NULL)
      rbuffer.ai_reset = 1;
    break;
  }
  if (res == NULL) {
    res = results;
    rbuffer.ai_reset = 1;
  }

  rbuffer.ai_family = res->ai_family;
  rbuffer.ai_socktype = res->ai_socktype;
  rbuffer.ai_protocol = res->ai_protocol;
  rbuffer.ai_addrlen = res->ai_addrlen;
  rbuffer.ai_addr = *(res->ai_addr);
  memcpy(remoteaddr, res->ai_addr, res->ai_addrlen);
#else
  rbuffer.ai_family = AF_INET;
  rbuffer.ai_socktype = SOCK_STREAM;
  rbuffer.ai_protocol = 0;
  rbuffer.ai_addrlen = remotehost->h_length;
  rbuffer.ai_addr.sa_family = remotehost->h_addrtype;
  rbuffer.ai_addr.sa_len = remotehost->h_length;
  remoteaddr->sin_port = htons(gnetwork->serv_resolv.to_port);
  memcpy(&(remoteaddr->sin_addr), remotehost->h_addr_list[0], sizeof(struct in_addr));
#endif
  bytes = write(gnetwork->serv_resolv.sp_fd[1],
                &rbuffer,
                sizeof(res_addrinfo_t));
#if !defined(NO_GETADDRINFO)
  freeaddrinfo(results);
#endif
  if (bytes != sizeof(res_addrinfo_t)) {
     exit(11);
  }
}

int my_getnameinfo(char *buffer, size_t len, const struct sockaddr *sa, socklen_t salen)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

  if (salen == 0)
    salen = sa->sa_len;
  if (getnameinfo(sa, salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "host=%s port=%s", hbuf, sbuf);
#else
  const struct sockaddr_in *remoteaddr = (const struct sockaddr_in *)sa;
  unsigned long to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  return snprintf(buffer, len, "%lu.%lu.%lu.%lu:%d",
                  (to_ip >> 24) & 0xFF,
                  (to_ip >> 16) & 0xFF,
                  (to_ip >>  8) & 0xFF,
                  (to_ip      ) & 0xFF,
                  ntohs(remoteaddr->sin_port));
#endif
}

int my_dcc_ip_port(char *buffer, size_t len, ir_sockaddr_union_t *sa, socklen_t salen)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
#endif
  long ip;

#if !defined(NO_GETADDRINFO)
  if (sa->sa.sa_family == AF_INET) {
    ip = ntohl(sa->sin.sin_addr.s_addr);
    return snprintf(buffer, len, "%lu %d",
                    ip, ntohs(sa->sin.sin_port));
  }
  if (salen == 0)
    salen = sa->sa.sa_len;
  if (getnameinfo(&(sa->sa), salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "%s %s", hbuf, sbuf);
#else
  ip = ntohl(sa->sin.sin_addr.s_addr);
  return snprintf(buffer, len, "%lu %d",
                  ip, ntohs(sa->sin.sin_port));
#endif
}

int connectirc2(res_addrinfo_t *remote)
{
  struct sockaddr_in *remoteaddr;
  struct sockaddr_in localaddr;
  int retval;
  int family;

  if (remote->ai_reset)
    gnetwork->serv_resolv.next = 0;

  family = remote->ai_addr.sa_family;
  remoteaddr = (struct sockaddr_in *)(&(remote->ai_addr));
  gnetwork->ircserver = socket(family, remote->ai_socktype, remote->ai_protocol);
  if (gnetwork->ircserver < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,"Socket Error");
    return 1;
  }

  if (gdata.debug > 0) {
    char *msg;
    msg = mycalloc(maxtextlength);
    my_getnameinfo(msg, maxtextlength -1, &(remote->ai_addr), remote->ai_addrlen);
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "Connecting to %s", msg);
    mydelete(msg);
  }

  if (gdata.local_vhost) {
    bzero((char*)&localaddr, sizeof(struct sockaddr_in));
    localaddr.sin_family = family;
    localaddr.sin_port = 0;
    localaddr.sin_addr.s_addr = htonl(gdata.local_vhost);

    if (bind(gnetwork->ircserver, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Bind To Virtual Host");
      close(gnetwork->ircserver);
      return 1;
    }
  }

  if (set_socket_nonblocking(gnetwork->ircserver,1) < 0 )
    outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");

  alarm(CTIMEOUT);
  retval = connect(gnetwork->ircserver, &(remote->ai_addr), remote->ai_addrlen);
  if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) ) {
    outerror(OUTERROR_TYPE_WARN_LOUD,"Connection to Server Failed");
    alarm(0);
    close(gnetwork->ircserver);
    return 1;
  }
  alarm(0);

  if (gdata.debug > 0) {
    ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"ircserver socket = %d",gnetwork->ircserver);
  }

  gnetwork->lastservercontact=gdata.curtime;

  /* good */
  gnetwork->serverstatus = SERVERSTATUS_TRYING;

  return 0;
}

void t_setup_dcc(transfer *tr, const char *nick)
{
  char *dccdata;
  char *sendnamestr;

  sendnamestr = getsendname(tr->xpack->file);
  dccdata = setup_dcc_local(&tr->serveraddress);
  privmsg_fast(nick,"\1DCC SEND %s %s %" LLPRINTFMT "u\1",
               sendnamestr, dccdata,
               (unsigned long long)tr->xpack->st_size);

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "listen on port %d for %s (%s on %s)",
          tr->listenport, nick, tr->hostname, gnetwork->name);

  mydelete(dccdata);
  mydelete(sendnamestr);
}

#ifdef DEBUG

static void free_state(void)
{
  xdcc *xd;
  channel_t *ch;
  transfer *tr;
  upload *up;
  pqueue *pq;
  userinput *u;
  igninfo *i;
  msglog_t *ml;
  gnetwork_t *backup;
  http *h;
  int ss;

  updatecontext();

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_delete(&gdata.xdccs, xd)) {
     mydelete(xd->file);
     mydelete(xd->desc);
     mydelete(xd->note);
     mydelete(xd->group);
     mydelete(xd->group_desc);
     mydelete(xd->lock);
     mydelete(xd->dlimit_desc);
     mydelete(xd->trigger);
  }

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    mydelete(gnetwork->curserveractualname);
    mydelete(gnetwork->user_nick);
    mydelete(gnetwork->caps_nick);
    mydelete(gnetwork->name);
    mydelete(gnetwork->curserver.hostname);
    mydelete(gnetwork->curserver.password);
    irlist_delete_all(&(gnetwork->xlistqueue));

    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_delete(&(gnetwork->channels), ch)) {
       clearmemberlist(ch);
       mydelete(ch->name);
       mydelete(ch->key);
       mydelete(ch->headline);
       mydelete(ch->pgroup);
    }
  }

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_delete(&gdata.trans, tr)) {
     mydelete(tr->nick);
     mydelete(tr->caps_nick);
     mydelete(tr->hostname);
     mydelete(tr->localaddr);
     mydelete(tr->remoteaddr);
  }

  for (up = irlist_get_head(&gdata.uploads);
       up;
       up = irlist_delete(&gdata.uploads, up)) {
     mydelete(up->nick);
     mydelete(up->hostname);
     mydelete(up->file);
     mydelete(tr->remoteaddr);
  }

  for (pq = irlist_get_head(&gdata.mainqueue);
       pq;
       pq = irlist_delete(&gdata.mainqueue, pq)) {
     mydelete(pq->nick);
     mydelete(pq->hostname);
  }
  for (pq = irlist_get_head(&gdata.idlequeue);
       pq;
       pq = irlist_delete(&gdata.idlequeue, pq)) {
     mydelete(pq->nick);
     mydelete(pq->hostname);
  }
  for (u = irlist_get_head(&gdata.packs_delayed);
       u;
       u = irlist_delete(&gdata.packs_delayed, u)) {
     mydelete(u->cmd);
     mydelete(u->arg1);
     mydelete(u->arg2);
  }
  for (i = irlist_get_head(&gdata.ignorelist);
       i;
       i = irlist_delete(&gdata.ignorelist, i)) {
     mydelete(i->regexp);
     mydelete(i->hostmask);
  }
  for (ml = irlist_get_head(&gdata.msglog);
       ml;
       ml = irlist_delete(&gdata.msglog, ml)) {
     mydelete(ml->hostmask);
     mydelete(ml->message);
  }
  for (h = irlist_get_head(&gdata.https);
       h;
       h = irlist_delete(&gdata.https, h)) {
     mydelete(h->file);
     mydelete(h->group);
     mydelete(h->buffer);
  }
  irlist_delete_all(&gdata.autotrigger);
  irlist_delete_all(&gdata.console_history);
  irlist_delete_all(&gdata.jobs_delayed);
  irlist_delete_all(&gdata.http_bad_ip4);
  irlist_delete_all(&gdata.http_bad_ip6);
  mydelete(gdata.connectionmethod.host);
  mydelete(gdata.connectionmethod.password);
  mydelete(gdata.connectionmethod.vhost);
  mydelete(gdata.sendbuff);
  mydelete(gdata.console_input_line);
  mydelete(gdata.osstring);


  mydelete(xdcc_statefile.note);
}

static void free_config(void)
{
  autoqueue_t *aq;
  regex_t *rh;
  int si;

  updatecontext();
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
  mydelete(gdata.r_pidfile);
  mydelete(gdata.pidfile);
  mydelete(gdata.r_config_nick);
  mydelete(gdata.config_nick);
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
    irlist_delete_all(&gdata.networks[si].r_channels);
    irlist_delete_all(&gdata.networks[si].server_join_raw);
    irlist_delete_all(&gdata.networks[si].server_connected_raw);
    irlist_delete_all(&gdata.networks[si].channel_join_raw);
  } /* networks */
  mydelete(gdata.logfile);
  mydelete(gdata.user_realname);
  mydelete(gdata.user_modes);
  irlist_delete_all(&gdata.proxyinfo);
  irlist_delete_all(&gdata.adddir_exclude);
  irlist_delete_all(&gdata.geoipcountry);
  irlist_delete_all(&gdata.geoipexcludenick);
  irlist_delete_all(&gdata.autoadd_dirs);
  irlist_delete_all(&gdata.autocrc_exclude);
  irlist_delete_all(&gdata.filedir);
  mydelete(gdata.enable_nick);
  mydelete(gdata.owner_nick);
  mydelete(gdata.geoipdatabase);
  mydelete(gdata.respondtochannellistmsg);
  mydelete(gdata.admin_job_file);
  mydelete(gdata.autoaddann);
  mydelete(gdata.autoadd_group);
  mydelete(gdata.send_statefile);
  mydelete(gdata.xdccremovefile);
  mydelete(gdata.autoadd_sort);
  mydelete(gdata.creditline);
  mydelete(gdata.headline);
  mydelete(gdata.nickserv_pass);
  mydelete(gdata.periodicmsg_nick);
  mydelete(gdata.periodicmsg_msg);
  mydelete(gdata.uploaddir);
  mydelete(gdata.restrictprivlistmsg);
  mydelete(gdata.loginname);
  mydelete(gdata.statefile);
  mydelete(gdata.xdcclistfile);
  mydelete(gdata.adminpass);
  mydelete(gdata.hadminpass);
  mydelete(gdata.http_admin);
  mydelete(gdata.http_dir);
}

#endif

void exit_iroffer(void)
{
#ifdef DEBUG
  meminfo_t *mi;
  unsigned char *ut;
  int j;
  int i;
  int leak = 0;

  signal(SIGSEGV, SIG_DFL);
  free_config();
  free_state();
  updatecontext();

  for (j=1; j<(MEMINFOHASHSIZE * gdata.meminfo_depth); j++) {
    mi = &(gdata.meminfo[j]);
    ut = mi->ptr;
    if (ut != NULL) {
      leak ++;
      outerror(OUTERROR_TYPE_WARN_LOUD, "Pointer 0x%8.8lX not free", (long)ut);
      outerror(OUTERROR_TYPE_WARN_LOUD, "alloctime = %ld", (long)(mi->alloctime));
      outerror(OUTERROR_TYPE_WARN_LOUD, "size      = %ld", (long)(mi->size));
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_func  = %s", mi->src_func);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_file  = %s", mi->src_file);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_line  = %d", mi->src_line);
      for(i=0; i<(12*12); i+=12) {
        outerror(OUTERROR_TYPE_WARN_LOUD," : %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X = \"%c%c%c%c%c%c%c%c%c%c%c%c\"",
               ut[i+0], ut[i+1], ut[i+2], ut[i+3], ut[i+4], ut[i+5], ut[i+6], ut[i+7], ut[i+8], ut[i+9], ut[i+10], ut[i+11],
               onlyprintable(ut[i+0]), onlyprintable(ut[i+1]),
               onlyprintable(ut[i+2]), onlyprintable(ut[i+3]),
               onlyprintable(ut[i+4]), onlyprintable(ut[i+5]),
               onlyprintable(ut[i+6]), onlyprintable(ut[i+7]),
               onlyprintable(ut[i+8]), onlyprintable(ut[i+9]),
               onlyprintable(ut[i+10]), onlyprintable(ut[i+11]));
      }
    }
  }
  if (leak == 0)
    exit(0);

  *((int*)(0)) = 0;
#else
  exit(0);
#endif
}

/* End of File */
