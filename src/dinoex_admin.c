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

#include <ctype.h>

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
a_respond(const userinput * const u, const char *format, ...);

void a_respond(const userinput * const u, const char *format, ...)
{
  va_list args;
  gnetwork_t *backup;

  updatecontext();
 
  va_start(args, format);
  backup = gnetwork;
  gnetwork = &(gdata.networks[u->net]);
 
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

  gnetwork = backup;
  va_end(args);
}

int hide_locked(const userinput * const u, const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;
   
  if (xd->lock == NULL)
    return 0;
   
  switch (u->method)
    {
    case method_fd:
    case method_xdl_channel:
    case method_xdl_user_privmsg:
    case method_xdl_user_notice:
      return 1;
    default:
      break;
    }
  return 0; 
}

int a_xdl_space(void)
{
   int i,s;
   xdcc *xd;

   i = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       i = max2(i,xd->gets);
       xd = irlist_get_next(xd);
     }
   s = 5;
   if (i < 10000) s = 4;
   if (i < 1000) s = 3;
   if (i < 100) s = 2;
   if (i < 10) s = 1;
   return s;
}
   
int a_xdl_left(void)
{
   int n;
   int l;

   n = irlist_size(&gdata.xdccs);
   l = 5;
   if (n < 10000) l = 4;
   if (n < 1000) l = 3;
   if (n < 100) l = 2;
   if (n < 10) l = 1;
   return l;
}

int reorder_new_groupdesc(const char *group, const char *desc)
{
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
                      xd->group_desc = mystrdup(desc);
                    }
                }
            }
        }
      xd = irlist_get_next(xd);
    }

  return k;
}

int reorder_groupdesc(const char *group)
{
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

int add_default_groupdesc(const char *group)
{
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

  firstxd->group_desc = mystrdup(group);
  return k; 
}

void strtextcpy(char *d, const char *s)
{
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

void strpathcpy(char *d, const char *s)
{
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

int invalid_group(const userinput * const u, const char *arg)
{
   if (!arg || !strlen(arg))
     {
       a_respond(u, "Try Specifying a Group");
       return 1;
     }
   return 0;
}

int invalid_dir(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Directory");
      return 1;
    }
   return 0;
}

int invalid_file(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Filename");
      return 1;
    }
   return 0;
}

int invalid_pwd(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Password");
      return 1;
    }
   return 0;
}

int invalid_nick(const userinput * const u, const char *arg)
{  
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Nick");
      return 1;
    }
   return 0;
}

int invalid_message(const userinput * const u, const char *arg)
{  
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Message");
      return 1;
    }
   return 0;
}

int invalid_announce(const userinput * const u, const char *arg)
{  
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Message (e.g. NEW)");
      return 1;
    }
   return 0;
}

int invalid_command(const userinput * const u, const char *arg)
{  
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Command");
      return 1;
    }
   return 0;
}

int invalid_channel(const userinput * const u, const char *arg)
{  
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Channel");
      return 1;
    }
   return 0;
}

int invalid_maxspeed(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg))
    {
      a_respond(u, "Try Specifying a Maxspeed");
      return 1;
    }
   return 0;
}

int invalid_pack(const userinput * const u, int num)
{
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      a_respond(u, "Try Specifying a Valid Pack Number");
      return 1;
    }
   return 0;
}

int get_network_msg(const userinput * const u, const char *arg)
{
   int net;

   net = get_network(arg);
   if (net < 0)
      a_respond(u, "Try Specifying a Valid Network");
   return net;
}

int disabled_config(const userinput * const u)
{
   if (gdata.direct_file_access == 0)
     {
       a_respond(u, "Disabled in Config");
       return 1;
     }
   return 0;
}

void a_remove_pack(const userinput * const u, xdcc *xd, int num)
{
   char *tmpdesc;
   char *tmpgroup;
   pqueue *pq;
   transfer *tr;
   gnetwork_t *backup;
   
   updatecontext();

   write_removed_xdcc(xd);

   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
           (tr->xpack == xd))
         {
           t_closeconn(tr,"Pack removed",0);
         }
       tr = irlist_get_next(tr);
     }
   
   pq = irlist_get_head(&gdata.mainqueue);
   while(pq)
     {
       if (pq->xpack == xd)
         {
           backup = gnetwork;
           gnetwork = &(gdata.networks[pq->net]);
           notice(pq->nick,"** Removed From Queue: Pack removed");
           gnetwork = backup;
           mydelete(pq->nick);
           mydelete(pq->hostname);
           pq = irlist_delete(&gdata.mainqueue, pq);
         }
       else
         {
           pq = irlist_get_next(pq);
         }
     }
   
   a_respond(u,"Removed Pack %i [%s]", num, xd->desc);
   
   if (gdata.md5build.xpack == xd)
     {
       outerror(OUTERROR_TYPE_WARN,"[MD5]: Canceled (remove)");
       
       FD_CLR(gdata.md5build.file_fd, &gdata.readset);
       close(gdata.md5build.file_fd);
       gdata.md5build.file_fd = FD_UNUSED;
       gdata.md5build.xpack = NULL;
     }
   
   assert(xd->file_fd == FD_UNUSED);
   assert(xd->file_fd_count == 0);
#ifdef HAVE_MMAP
   assert(!irlist_size(&xd->mmaps));
#endif
   
   mydelete(xd->file);
   mydelete(xd->desc);
   mydelete(xd->note);
   mydelete(xd->lock);
   mydelete(xd->dlimit_desc);
   mydelete(xd->trigger);
   /* keep group info for later work */
   tmpgroup = xd->group;
   xd->group = NULL;
   tmpdesc = xd->group_desc;
   xd->group_desc = NULL;
   irlist_delete(&gdata.xdccs, xd);
   
   if (tmpdesc != NULL)
     {
       if (tmpgroup != NULL)
         reorder_new_groupdesc(tmpgroup,tmpdesc);
       mydelete(tmpdesc);
     }
   if (tmpgroup != NULL)
     mydelete(tmpgroup);
   
   autotrigger_rebuild();
   write_statefile();
   xdccsavetext();
}

void a_remove_delayed(const userinput * const u)
{
   struct stat *st;
   xdcc *xd;
   gnetwork_t *backup;
   int n;

   updatecontext();

   backup = gnetwork;
   st = (struct stat *)(u->arg2);
   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if ((xd->st_dev == st->st_dev) &&
           (xd->st_ino == st->st_ino))
         {
           gnetwork = &(gdata.networks[u->net]);
           a_remove_pack(u, xd, n);
           /* start over, the list has changed */
           n = 0;
           xd = irlist_get_head(&gdata.xdccs);
         }
       else
         {
           xd = irlist_get_next(xd);
         }
     }
   gnetwork = backup;
}

static int a_set_group(const userinput * const u, xdcc *xd, int num, const char *group)
{
   const char *new;
   char *tmpdesc;
   char *tmpgroup;
   int rc;

   updatecontext();

   if (num == 0) num = number_of_pack(xd);
   new = "MAIN";
   if (group && strlen(group))
     new = group;

   if (xd->group != NULL)
     {
       a_respond(u, "GROUP: [Pack %i] Old: %s New: %s",
                 num, xd->group, new); 
       /* keep group info for later work */
       tmpgroup = xd->group;
       xd->group = NULL;
       tmpdesc = xd->group_desc;
       xd->group_desc = NULL;
        if (tmpdesc != NULL)
         {
           if (tmpgroup != NULL)
             reorder_new_groupdesc(tmpgroup,tmpdesc);
           mydelete(tmpdesc);
         }
       if (tmpgroup != NULL)
         mydelete(tmpgroup);
     }
   else
     {
       a_respond(u, "GROUP: [Pack %i] New: %s",
                 num, new);
     }

  if (group != new)
    return 0;
    
  xd->group = mystrdup(group);
  reorder_groupdesc(group);
  rc = add_default_groupdesc(group);
  if (rc == 1)
    a_respond(u, "New GROUPDESC: %s",group);
  return rc;
}

void a_add_delayed(const userinput * const u)
{
   userinput u2;
   xdcc *xd;
   int newgroup;
   int num;
   int rc;

   updatecontext();

   a_respond(u,"  Adding %s:", u->arg1);

   u2 = *u;
   u2.arg1e = u->arg1;
   a_add(&u2);

   if (u->arg3 == NULL)
     return;

   num = 0;
   newgroup = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       num++;
       if (!strcmp(u->arg1,xd->file))
         {
           if (xd->group != NULL)
             {
               if (strcmp(xd->group, u->arg3) == 0 )
                 break;
             }
           xd->group = mystrdup(u->arg3);
           a_respond(u, "GROUP: [Pack %i] New: %s",
                        num, u->arg3);
           break;
         }
       xd = irlist_get_next(xd);
     }

   if ((++newgroup) == 1)
     {
       rc = add_default_groupdesc(u->arg3);
       if (rc == 1)
         a_respond(u, "New GROUPDESC: %s", u->arg3);
     }
}

void a_xdlock(const userinput * const u)
{
   char *tempstr;
   int i;
   int l;
   int s;
   xdcc *xd;

   updatecontext();

   tempstr  = mycalloc(maxtextlength);

   l = a_xdl_left();
   s = a_xdl_space();
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (xd->lock != NULL)
         {
           u_xdl_pack(u,tempstr,i,l,s,xd);
           a_respond(u," \2^-\2%*sPassword: %s", s, "", xd->lock);
         }
       i++;
       xd = irlist_get_next(xd);
     }

   mydelete(tempstr);
}

void a_xdtrigger(const userinput * const u)
{
   char *tempstr;
   int i;
   int l;
   int s;
   xdcc *xd;

   updatecontext();

   tempstr  = mycalloc(maxtextlength);

   l = a_xdl_left();
   s = a_xdl_space();
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (xd->trigger != NULL)
         {
           u_xdl_pack(u,tempstr,i,l,s,xd);
           a_respond(u," \2^-\2%*sTrigger: %s", s, "", xd->trigger);
         }
       i++;
       xd = irlist_get_next(xd);
     }

   mydelete(tempstr);
}

void a_find(const userinput * const u)
{
   char *tempstr;
   const char *file;
   int i;
   int k;
   int l;
   int s;
   xdcc *xd;

   updatecontext();

   if (!u->arg1e || !strlen(u->arg1e)) {
     a_respond(u, "Try Specifying a Pattern");
     return;
   }

   tempstr  = mycalloc(maxtextlength);

   l = a_xdl_left();
   s = a_xdl_space();
   k = 0;
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       file = get_basename(xd->file);
       if (strstrnocase(file, u->arg1e) ||
           strstrnocase(xd->desc, u->arg1e) ||
           strstrnocase(xd->note, u->arg1e))
         {
           k++;
           u_xdl_pack(u, tempstr, i, l, s, xd);
           /* limit matches */
           if ((gdata.max_find != 0) && (k >= gdata.max_find))
             break;
         }
       i++;
       xd = irlist_get_next(xd);
     }

   if (!k)
      a_respond(u, "Sorry, nothing was found, try a XDCC LIST" );

   mydelete(tempstr);
}

void a_unlimited(const userinput * const u)
{
  int num = -1;
  transfer *tr;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  tr = does_tr_id_exist(num);
  if (tr == NULL)
    {
      a_respond(u,"Invalid ID number, Try \"DCL\" for a list");
      return;
    }

  tr->nomax = 1;
  tr->unlimited = 1;
}

void a_maxspeed(const userinput * const u)
{
  transfer *tr;
  float val;
  int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  if (invalid_maxspeed(u, u->arg2) != 0)
     return;

  tr = does_tr_id_exist(num);
  if (tr == NULL)
    {
      a_respond(u,"Invalid ID number, Try \"DCL\" for a list");
      return;
    }

  val = atof(u->arg2);
  a_respond(u, "CHMAXS: [Transfer %i] Old: %1.1f New: %1.1f",
            num, tr->maxspeed, val);
  tr->maxspeed = val;
}

void a_slotsmax(const userinput * const u)
{
   int val;
  
   updatecontext();
   
   if (u->arg1)
     {
       val = atoi(u->arg1);
       gdata.slotsmax = between(1,val,MAXTRANS);
     }
   a_respond(u,"SLOTSMAX now %d", gdata.slotsmax);
}

void a_queuesize(const userinput * const u)
{
   int val;
  
   updatecontext();
   
   if (u->arg1)
     {
       val = atoi(u->arg1);
       gdata.queuesize = between(0,val,1000000);
     }
   a_respond(u,"QUEUESIZE now %d", gdata.queuesize);
}

void a_requeue(const userinput * const u)
{
  int oldp = 0, newp = 0;
  pqueue *pqo;
  pqueue *pqn;

  updatecontext();

  if (u->arg1) oldp = atoi(u->arg1);
  if (u->arg2) newp = atoi(u->arg2);

  if ((oldp < 1) ||
      (oldp > irlist_size(&gdata.mainqueue)) ||
      (newp < 1) ||
      (newp > irlist_size(&gdata.mainqueue)) ||
      (newp == oldp))
    {
      a_respond(u,"Invalid Queue Entry");
      return;
    }

  a_respond(u,"** Moved Queue %i to %i", oldp, newp);

  /* get queue we are renumbering */
  pqo = irlist_get_nth(&gdata.mainqueue, oldp-1);
  irlist_remove(&gdata.mainqueue, pqo);

  if (newp == 1)
    {
      irlist_insert_head(&gdata.mainqueue, pqo);
    }
  else
    {
      pqn = irlist_get_nth(&gdata.mainqueue, newp-2);
      irlist_insert_after(&gdata.mainqueue, pqo, pqn);
    }
}

void a_removedir_sub(const userinput * const u, const char *thedir, DIR *d)
{
  struct dirent *f;
  char *tempstr;
  userinput *u2;
  int thedirlen;
  
  updatecontext();
  
  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);
  
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  while ((f = readdir(d)))
    {
      struct stat st;
      int len = strlen(f->d_name);
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          a_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
          mydelete(tempstr);
          continue;
        }
      if (S_ISDIR(st.st_mode))
        {
          if ((strcmp(f->d_name,".") == 0 ) ||
              (strcmp(f->d_name,"..") == 0))
            {
              mydelete(tempstr);
              continue;
            }
          if (gdata.include_subdirs != 0)
            a_removedir_sub(u, tempstr, NULL);
          mydelete(tempstr);
          continue;
        }
      if (!S_ISREG(st.st_mode))
        {
          mydelete(tempstr);
          continue;
        }
      
      u2 = irlist_add(&gdata.packs_delayed, sizeof(userinput));
      u2->method = u->method;
      u2->fd = u->fd;
      u2->chat = u->chat;
      u2->cmd = mystrdup( "REMOVE" );
      u2->net = gnetwork->net;
      u2->level = u->level;

      u2->arg1 = tempstr;
      tempstr = NULL;

      if (u->snick != NULL)
        {
          u2->snick = mystrdup(u->snick);
        }

      u2->arg2 = mycalloc(sizeof(struct stat));
      memcpy(u2->arg2, &st, sizeof(struct stat));
    }
  
  closedir(d);
  return;
}

void a_remove(const userinput * const u)
{
   int num1 = 0;
   int num2 = 0;
   xdcc *xd;
   
   updatecontext();

   if (u->arg1) num1 = atoi(u->arg1);
   if (invalid_pack(u, num1) != 0)
      return;

   if (u->arg2) num2 = atoi(u->arg2);
   if ( num2 < 0 || num2 > irlist_size(&gdata.xdccs) ) {
      a_respond(u, "Try Specifying a Valid Pack Number");
      return;
      }

   if (num2 == 0) {
      xd = irlist_get_nth(&gdata.xdccs, num1-1);
      a_remove_pack(u, xd, num1);
      return;
      }

   if ( num2 < num1 ) {
      a_respond(u, "Pack numbers are not in order");
      return;
      }

   for (; num2 >= num1; num2--) {
      xd = irlist_get_nth(&gdata.xdccs, num2-1);
      a_remove_pack(u, xd, num2);
      }
}

void a_removegroup(const userinput * const u)
{
   xdcc *xd;
   int n;

   updatecontext();

   if (invalid_group(u, u->arg1) != 0)
       return;

   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                a_remove_pack(u, xd, n);
                /* start over, the list has changed */
                n = 0;
                xd = irlist_get_head(&gdata.xdccs);
                continue;
             }
         }
       xd = irlist_get_next(xd);
     }
}

static int a_sort_null(const char *str1, const char *str2)
{
  if ((str1 == NULL) && (str2 == NULL))
    return 0;

  if (str1 == NULL)
    return -1;

  if (str2 == NULL)
    return 1;

  return strcasecmp(str1, str2);
}

static int a_sort_cmp(const char *k, xdcc *xd1, xdcc *xd2)
{
  xdcc *xd3 = xd1;
  xdcc *xd4 = xd2;
  int rc = 0;

  while (k != NULL) {
    switch ( *k ) { 
    case '-':
      xd3 = xd2;
      xd4 = xd1;
      k ++;
      continue;
    case '+':
      xd3 = xd1;
      xd4 = xd2;
      k ++;
      break;
    case 'D':
    case 'd':
      rc = strcasecmp(xd3->desc, xd4->desc);
      if (rc != 0)
        return rc;
      break;
    case 'P':
    case 'p':
      rc = strcasecmp(xd3->file, xd4->file);
      if (rc != 0)
        return rc;
      break;
    case 'G':
    case 'g':
      rc = a_sort_null(xd3->group, xd4->group);
      if (rc != 0)
        return rc;
      break;
    case 'B':
    case 'b':
    case 'S':
    case 's':
      if (xd3->st_size < xd4->st_size) return -1;
      if (xd3->st_size > xd4->st_size) return 1;
      break;
    case 'T':
    case 't':
      if (xd3->mtime < xd4->mtime) return -1;
      if (xd3->mtime > xd4->mtime) return 1;
      break;
    case 'N':
    case 'n':
    case 'F':
    case 'f':
      rc = strcasecmp(get_basename(xd3->file), get_basename(xd4->file));
      if (rc != 0)
        return rc;
      break;
    }
    k = strchr(k, ' ');
    if (k == NULL)
      break;
    xd3 = xd1;
    xd4 = xd2;
    k ++;
  }
  return rc;
}

void a_sort(const userinput * const u)
{
  irlist_t old_list;
  xdcc *xdo, *xdn;
  char *tmpdesc;
  int n;
  const char*k = "filename";

  updatecontext();
 
  if (irlist_size(&gdata.xdccs) == 0)
    {
      a_respond(u,"No packs to sort");
      return;
    }

  if (u->arg1e) k = u->arg1e;

  old_list = gdata.xdccs;
  /* clean start */
  gdata.xdccs.size = 0;
  gdata.xdccs.head = NULL;
  gdata.xdccs.tail = NULL;

  while (irlist_size(&old_list) > 0)
    {
      xdo = irlist_get_head(&old_list);
      irlist_remove(&old_list, xdo);

      n = 0;
      xdn = irlist_get_head(&gdata.xdccs);
      while(xdn)
        {
          n++;
          if (a_sort_cmp(k, xdo, xdn) < 0)
            break;

          xdn = irlist_get_next(xdn);
        }
      if (xdn != NULL)
        {
          irlist_insert_before(&gdata.xdccs, xdo, xdn);
        }
      else 
        {
          if (n == 0)
            irlist_insert_head(&gdata.xdccs, xdo);
          else
            irlist_insert_tail(&gdata.xdccs, xdo);
        }

      if (xdo->group != NULL)
        {
          if (xdo->group_desc != NULL)
            {
               tmpdesc = xdo->group_desc;
               xdo->group_desc = NULL;
               reorder_new_groupdesc(xdo->group, tmpdesc);
               mydelete(tmpdesc);
            }
          else
            reorder_groupdesc(xdo->group);
        }
    }

  write_statefile();
  xdccsavetext();
}

int a_open_file(char **file, int mode)
{
   int xfiledescriptor;
   int n;
   char *adir;
   char *path;

   xfiledescriptor = open(*file, mode);
   if ((xfiledescriptor >= 0) || (errno != ENOENT))
      return xfiledescriptor;

   if (errno != ENOENT)
      return xfiledescriptor;
      
   n = irlist_size(&gdata.filedir);
   if (n == 0)
      return -1;

   adir = irlist_get_head(&gdata.filedir);
   while (adir) {
      path = mymalloc(strlen(adir) + 1 + strlen(*file) + 1);
      if (adir[strlen(adir)-1] == '/')
        sprintf(path, "%s%s", adir, *file);
      else
        sprintf(path, "%s/%s", adir, *file);
      xfiledescriptor = open(path, mode);
      if ((xfiledescriptor >= 0) || (errno != ENOENT)) {
         mydelete(*file);
         *file = path;
         return xfiledescriptor;
      }
      mydelete(path);
      adir = irlist_get_next(adir);
   }
   return -1;
}

void a_add(const userinput * const u)
{
   int xfiledescriptor;
   struct stat st;
   xdcc *xd;
   char *group;
   char *file;
   char *a1;
   char *a2;
   
   updatecontext();

   if (invalid_file(u, u->arg1e) != 0)
      return;

   clean_quotes(u->arg1e);
   file = mystrdup(u->arg1e);
   convert_to_unix_slash(file);
   
   xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
   if (xfiledescriptor < 0) {
      a_respond(u,"Cant Access File: %s",strerror(errno));
      mydelete(file);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      a_respond(u,"Cant Access File Details: %s",strerror(errno));
      close(xfiledescriptor);
      mydelete(file);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      a_respond(u,"%s is not a file",file);
      mydelete(file);
      return;
     }
   
   if ( st.st_size == 0 ) {
      a_respond(u,"File has size of 0 bytes!");
      mydelete(file);
      return;
      }
   
   if ((st.st_size > gdata.max_file_size) || (st.st_size < 0)) {
      a_respond(u,"File is too large.");
      mydelete(file);
      return;
      }
   
   if (gdata.noduplicatefiles) {
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           if ((xd->st_dev == st.st_dev) &&
              (xd->st_ino == st.st_ino))
             {
               a_respond(u,"File '%s' is already added.", u->arg1e);
               return;
             }
           xd = irlist_get_next(xd);
         }
      }

   group = NULL;
   if (gdata.auto_default_group) {
      a1 = mycalloc(strlen(u->arg1e) + 1);
      strtextcpy(a1, u->arg1e);
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           a2 = mycalloc(strlen(xd->file) + 1);
           strtextcpy(a2, xd->file);
           if (!strcmp(a1, a2))
             {
               group = xd->group;
               mydelete(a2);
               break;
             }
           mydelete(a2);
           xd = irlist_get_next(xd);
         }
      mydelete(a1);
      }
   if ((gdata.auto_path_group) && (group == NULL)) {
      a1 = mycalloc(strlen(u->arg1e) + 1);
      strpathcpy(a1, u->arg1e);
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           a2 = mycalloc(strlen(xd->file) + 1);
           strpathcpy(a2, xd->file);
           if (!strcmp(a1, a2))
             {
               group = xd->group;
               mydelete(a2);
               break;
             }
           mydelete(a2);
           xd = irlist_get_next(xd);
         }
      mydelete(a1);
      }
   
   xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
   
   xd->file = file;
   
   xd->note = mystrdup("");
   
   xd->desc = mystrdup(getfilename(u->arg1e));
   
   xd->gets = 0;
   xd->minspeed = gdata.transferminspeed;
   xd->maxspeed = gdata.transfermaxspeed;
   
   xd->st_size  = st.st_size;
   xd->st_dev   = st.st_dev;
   xd->st_ino   = st.st_ino;
   xd->mtime    = st.st_mtime;
   
   xd->file_fd = FD_UNUSED;
   xd->file_fd_count = 0;
   xd->file_fd_location = 0;
   
   a_respond(u, "ADD PACK: [Pack: %i] [File: %s] Use CHDESC to change description",
             irlist_size(&gdata.xdccs),xd->file);
   
   if ((gdata.auto_default_group) && (group != NULL)) {
         xd->group = mystrdup(group);
         a_respond(u,"GROUP: [Pack: %i] New: %s",
                   irlist_size(&gdata.xdccs), xd->group);
      }
   
   write_statefile();
   xdccsavetext();
   
   if (gdata.autoaddann_short) {
      userinput *ui;
      char *tempstr;
      
      tempstr = mycalloc (maxtextlength);
      ui = mycalloc(sizeof(userinput));
      snprintf(tempstr, maxtextlength - 2, "A A A A A sannounce %i", irlist_size(&gdata.xdccs));
      u_fillwith_msg(ui,NULL,tempstr);
      ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
      ui->net = u->net;
      ui->level = u->level;
      u_parseit(ui);
      mydelete(ui);
      mydelete(tempstr);
      }

   /* iroffer-lamm: autoaddann */
   if (gdata.autoaddann) {
      userinput *ui;
      char *tempstr;
      
      tempstr = mycalloc (maxtextlength);
      ui = mycalloc(sizeof(userinput));
      snprintf(tempstr, maxtextlength - 2, "A A A A A announce %i %s",irlist_size(&gdata.xdccs),gdata.autoaddann);
      u_fillwith_msg(ui,NULL,tempstr);
      ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
      ui->net = u->net;
      ui->level = u->level;
      u_parseit(ui);
      mydelete(ui);
      mydelete(tempstr);
      } 
}

void a_adddir_sub(const userinput * const u, const char *thedir, DIR *d, int new, const char *setgroup)
{
  userinput *u2;
  struct dirent *f;
  struct stat st;
  struct stat *sta;
  char *thefile, *tempstr;
  irlist_t dirlist = {};
  int thedirlen;
  
  updatecontext();
  
  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);
  
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  while ((f = readdir(d)))
    {
      xdcc *xd;
      int len = strlen(f->d_name);
      int foundit;
      
      if (verifyshell(&gdata.adddir_exclude, f->d_name))
        continue;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          a_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
        }
      else if (S_ISDIR(st.st_mode))
        {
          if ((strcmp(f->d_name,".") == 0 ) ||
              (strcmp(f->d_name,"..") == 0))
            {
              mydelete(tempstr);
              continue;
            }
          if (gdata.include_subdirs == 0)
            {
              a_respond(u,"  Ignoring directory: %s", tempstr);
            }
          else
            {
              a_adddir_sub(u, tempstr, NULL, new, setgroup);
            }
          mydelete(tempstr);
          continue;
        }
      else if (S_ISREG(st.st_mode))
        {
          foundit = 0;
          if (new != 0)
          {
            xd = irlist_get_head(&gdata.xdccs);
            while(xd)
              {
                if ((xd->st_dev == st.st_dev) &&
                    (xd->st_ino == st.st_ino))
                  {
                    foundit = 1;
                    break;
                  }

                xd = irlist_get_next(xd);
              }
          }

          if (gdata.autoadd_delay) {
            if ((st.st_mtime + gdata.autoadd_delay) > gdata.curtime)
              foundit = 2;
          }

          if (foundit == 0)
          {
            u2 = irlist_get_head(&gdata.packs_delayed);
            while(u2)
              {
                sta = (struct stat *)(u2->arg2);
                if ((strcmp(u2->cmd,"ADD") == 0) &&
                   (sta->st_dev == st.st_dev) &&
                   (sta->st_ino == st.st_ino))
                  {
                    foundit = 1;
                    break;
                  }

                u2 = irlist_get_next(u2);
              }
          }

          if (foundit == 0)
            {
              thefile = irlist_add(&dirlist, len + thedirlen + 2);
              strcpy(thefile, tempstr);
            }
        }
      mydelete(tempstr);
    }
  
  closedir(d);
  
  if (irlist_size(&dirlist) == 0)
    return;
 
  irlist_sort(&dirlist, irlist_sort_cmpfunc_string, NULL);
  
  a_respond(u,"Adding %d files from dir %s",
            irlist_size(&dirlist), thedir);
  
  thefile = irlist_get_head(&dirlist);
  while (thefile)
    {
      u2 = irlist_add(&gdata.packs_delayed, sizeof(userinput));
      u2->method = u->method;
      u2->fd = u->fd;
      u2->chat = u->chat;
      u2->cmd = mystrdup( "ADD" );
      u2->net = gnetwork->net;
      u2->level = u->level;

      if (u2->snick != NULL)
        {
          u2->snick = mystrdup(u->snick);
        }

      u2->arg1 = mystrdup(thefile);

      if (stat(thefile,&st) == 0)
        {
          u2->arg2 = mycalloc(sizeof(struct stat));
          memcpy(u2->arg2, &st, sizeof(struct stat));
        }

      if (setgroup != NULL)
        {
          u2->arg3 = mystrdup(setgroup);
        }
      else
        {
          u2->arg3 = NULL;
        }

      thefile = irlist_delete(&dirlist, thefile);
    }
}

DIR *a_open_dir(char **dir)
{
   DIR *d;
   char *adir;
   char *path;
   int n;

   if ((*dir)[strlen(*dir)-1] == '/')
     (*dir)[strlen(*dir)-1] = 0;

   d = opendir(*dir);
   if ((d != NULL) || (errno != ENOENT))
      return d;

   if (errno != ENOENT)
      return d;

   n = irlist_size(&gdata.filedir);
   if (n == 0)
      return NULL;

   adir = irlist_get_head(&gdata.filedir);
   while (adir) {
      path = mymalloc(strlen(adir) + 1 + strlen(*dir) + 1);
      if (adir[strlen(adir)-1] == '/')
        sprintf(path, "%s%s", adir, *dir);
      else
        sprintf(path, "%s/%s", adir, *dir);
      d = opendir(path);
      if ((d != NULL) || (errno != ENOENT)) {
         mydelete(*dir);
         *dir = path;
         return d;
      }
      mydelete(path);
      adir = irlist_get_next(adir);
   }
   return NULL;
}

void a_addgroup(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_dir(u, u->arg2e) != 0)
     return;

  clean_quotes(u->arg2e);
  convert_to_unix_slash(u->arg2e);
  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2e);
  d = a_open_dir(&thedir);
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }

  a_adddir_sub(u, thedir, d, 1, u->arg1);
  mydelete(thedir);
  return;
}

static void a_newgroup_sub(const userinput * const u, const char *thedir, DIR *d, const char *group)
{
  struct dirent *f;
  struct stat st;
  char *tempstr;
  int thedirlen;
  int num;
  int foundit = 0;
  
  updatecontext();
  
  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);
  
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  while ((f = readdir(d)))
    {
      xdcc *xd;
      int len = strlen(f->d_name);
      
      if (verifyshell(&gdata.adddir_exclude, f->d_name))
        continue;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          a_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
        }
      else if (S_ISDIR(st.st_mode))
        {
          if ((strcmp(f->d_name,".") == 0 ) ||
              (strcmp(f->d_name,"..") == 0))
            {
              mydelete(tempstr);
              continue;
            }
          a_respond(u,"  Ignoring directory: %s", tempstr);
          mydelete(tempstr);
          continue;
        }
      else if (S_ISREG(st.st_mode))
        {
          num = 0;
          xd = irlist_get_head(&gdata.xdccs);
          while(xd)
            {
              num ++;
              if ((xd->st_dev == st.st_dev) &&
                  (xd->st_ino == st.st_ino))
                {
                  foundit ++;
                  a_set_group(u, xd, num, group);
                  break;
                }

              xd = irlist_get_next(xd);
            }

        }
      mydelete(tempstr);
    }
  
  closedir(d);
  if (foundit == 0)
    return;
  write_statefile();
  xdccsavetext();
}

void a_newgroup(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_dir(u, u->arg2e) != 0)
     return;

  clean_quotes(u->arg2e);
  convert_to_unix_slash(u->arg2e);
  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2e);
  d = a_open_dir(&thedir);
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }

  a_newgroup_sub(u, thedir, d, u->arg1);
  mydelete(thedir);
  return;
}

void a_chlimit(const userinput * const u)
{
   int num = 0;
   int val = 0;
   xdcc *xd;
  
   updatecontext();
  
   if (u->arg1) num = atoi(u->arg1);
   if (invalid_pack(u, num) != 0)
      return;
  
   if (!u->arg2 || !strlen(u->arg2)) {
      a_respond(u,"Try Specifying a daily Downloadlimit");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   val = atoi(u->arg2);

   a_respond(u, "CHLIMIT: [Pack %i] Old: %d New: %d",
             num,xd->dlimit_max,val);
  
   xd->dlimit_max = val;
   if (val == 0)
     xd->dlimit_used = 0;
   else
     xd->dlimit_used = xd->gets + xd->dlimit_max;
  
   write_statefile();
   xdccsavetext();
}

void a_chlimitinfo(const userinput * const u)
{
  int num = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);

  if (!u->arg2 || !strlen(u->arg2))
    {
       a_respond(u, "DLIMIT: [Pack %i] descr removed", num);
       mydelete(xd->dlimit_desc);
       xd->dlimit_desc = NULL;
    }
  else
    {
       a_respond(u, "DLIMIT: [Pack %i] descr: %s", num, u->arg2e);
       xd->dlimit_desc = mystrdup(u->arg2e);
    }

  write_statefile();
  xdccsavetext();
}

void a_chtrigger(const userinput * const u)
{
  int num = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);

  if (!u->arg2 || !strlen(u->arg2))
    {
       a_respond(u, "TRIGGER: [Pack %i] removed", num);
       mydelete(xd->trigger);
       xd->trigger = NULL;
       autotrigger_rebuild();
    }
  else
    {
       a_respond(u, "TRIGGER: [Pack %i] set: %s", num, u->arg2e);
       xd->trigger = mystrdup(u->arg2e);
       autotrigger_add(xd);
    }

  write_statefile();
  xdccsavetext();
}

void a_lock(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  if (invalid_pwd(u, u->arg2) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  a_respond(u, "LOCK: [Pack %i] Password: %s", num, u->arg2);
  xd->lock = mystrdup(u->arg2);

  write_statefile();
  xdccsavetext();
}

void a_unlock(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  a_respond(u, "UNLOCK: [Pack %i]", num);
  
  mydelete(xd->lock);
  xd->lock = NULL;
  
  write_statefile();
  xdccsavetext();
}

void a_lockgroup(const userinput * const u)
{
   xdcc *xd;
   int n;
   
   updatecontext();
   
   if (invalid_group(u, u->arg1) != 0)
      return;
 
   if (invalid_pwd(u, u->arg2) != 0)
      return;

   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                a_respond(u, "LOCK: [Pack %i] Password: %s", n, u->arg2);
                xd->lock = mystrdup(u->arg2);
             }
         }
       xd = irlist_get_next(xd);
     }
  write_statefile();
  xdccsavetext();
}

void a_unlockgroup(const userinput * const u)
{
   xdcc *xd;
   int n;
   
   updatecontext();

   if (invalid_group(u, u->arg1) != 0)
     return;

   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                a_respond(u, "UNLOCK: [Pack %i]", n);
                mydelete(xd->lock);
                xd->lock = NULL;
             }
         }
       xd = irlist_get_next(xd);
     }
  write_statefile();
  xdccsavetext();
}

void a_groupdesc(const userinput * const u)
{
  int k;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (u->arg2e && strlen(u->arg2e))
    {
      a_respond(u, "New GROUPDESC: %s",u->arg2e);
    }
  else
    {
      a_respond(u, "Removed GROUPDESC");
    }

  k = reorder_new_groupdesc(u->arg1,u->arg2e);
  if (k == 0)
    return;

  write_statefile();
  xdccsavetext();
}

void a_group(const userinput * const u)
{
  xdcc *xd;
  const char *new;
  int num = 0;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);

  new = u->arg2;
  if (!u->arg2 || !strlen(u->arg2))
    {
      if (xd->group == NULL)
      {
        a_respond(u,"Try Specifying a Group");
        return;
      }
      new = NULL;
    }
  else
    {
       if (gdata.groupsincaps)
         caps(u->arg2);
    }

  a_set_group(u, xd, num, new);
  write_statefile();
  xdccsavetext();
}

void a_movegroup(const userinput * const u)
{
  xdcc *xd;
  int num;
  int num1 = 0;
  int num2 = 0;
  
  updatecontext();
  
  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (u->arg2) num2 = atoi(u->arg2);
  if (invalid_pack(u, num2) != 0)
    return;

  if (u->arg3 && strlen(u->arg3))
    {
       if (gdata.groupsincaps)
         caps(u->arg3);
    }
  
  for (num = num1; num <= num2; num ++)
    {
       xd = irlist_get_nth(&gdata.xdccs, num-1);
       a_set_group(u, xd, num, u->arg3);
     }
  
  write_statefile();
  xdccsavetext();
}

void a_regroup(const userinput * const u)
{
  xdcc *xd;
  const char *g;
  int k;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_group(u, u->arg2) != 0)
     return;

  if (gdata.groupsincaps)
    caps(u->arg1);
   
  k = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        g = xd->group;
      else
        g = "main";
      if (strcasecmp(g,u->arg1) == 0)
        {
          k++;
          if (xd->group != NULL)
            mydelete(xd->group);
          xd->group = mystrdup(u->arg2);
        }
      xd = irlist_get_next(xd);
    }

  if (k == 0)
    return;

  a_respond(u, "GROUP: Old: %s New: %s", u->arg1, u->arg2);
  if (strcasecmp(u->arg1,"main") == 0)
    add_default_groupdesc(u->arg2);
  write_statefile();
  xdccsavetext();
}

void a_md5(const userinput * const u)
{
  int num = 0;
  xdcc *xd;

  updatecontext ();

  /* show status */
  if (gdata.md5build.xpack) {
    a_respond(u, "calculating MD5/CRC32 for pack %d",
              number_of_pack(gdata.md5build.xpack));
    return;
  }

  if (u->arg1) num = atoi(u->arg1);
  if (num == 0)
    return;

  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  a_respond(u, "Rebuilding MD5 and CRC for Pack #%i:", num);
  start_md5_hash(xd, num);
}

void a_crc(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  const char *crcmsg;

  updatecontext ();

  if (u->arg1) {
    num = atoi (u->arg1);
    if (invalid_pack(u, num) != 0)
      return;

    xd = irlist_get_nth(&gdata.xdccs, num-1);
    a_respond(u,"Validating CRC for Pack #%i:",num);
    crcmsg = validate_crc32(xd, 0);
    if (crcmsg != NULL)
      a_respond(u,"File '%s' %s.", xd->file, crcmsg);
  }
  else {
   num = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       num ++;
       crcmsg = validate_crc32(xd, 1);
       if (crcmsg != NULL)
         a_respond(u,"Pack %d, File '%s' %s.", num, xd->file, crcmsg);
       xd = irlist_get_next(xd);
     }
  }
}

static int a_newdir_check(const userinput * const u, const char *dir1, const char *dir2, xdcc *xd)
{
   struct stat st;
   transfer *tr;
   const char *off2;
   char *tempstr;
   int len1;
   int len2;
   int max;
   int xfiledescriptor;

   updatecontext();

   len1 = strlen(dir1);
   if ( strncmp(dir1, xd->file, len1) != 0 )
     return 0;

   off2 = xd->file + len1;
   len2 = strlen(off2);
   max = strlen(dir2) + len2 + 1;
   tempstr = mymalloc(max + 1);
   snprintf(tempstr, max,
            "%s%s", dir2, off2);

   xfiledescriptor=open(tempstr, O_RDONLY | ADDED_OPEN_FLAGS);

   if (xfiledescriptor < 0) {
      a_respond(u,"%s: Cant Access File: %s",tempstr,strerror(errno));
      mydelete(tempstr);
      return 0;
      }

   if (fstat(xfiledescriptor,&st) < 0)
     {
      a_respond(u,"%s: Cant Access File Details: %s",tempstr,strerror(errno));
      close(xfiledescriptor);
      mydelete(tempstr);
      return 0;
     }

   close(xfiledescriptor);

   if ( st.st_size == 0 ) {
      a_respond(u,"%s: File has size of 0 bytes!",tempstr);
      mydelete(tempstr);
      return 0;
      }

   if ((st.st_size > gdata.max_file_size) || (st.st_size < 0)) {
      a_respond(u,"%s: File is too large.",tempstr);
      mydelete(tempstr);
      return 0;
      }

   a_respond(u, "CHFILE: [Pack %i] Old: %s New: %s",
             number_of_pack(xd), xd->file, tempstr);

   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
           (tr->xpack == xd))
         {
           t_closeconn(tr,"Pack file changed",0);
         }
       tr = irlist_get_next(tr);
     }

   mydelete(xd->file);
   xd->file = tempstr;
   xd->st_size  = st.st_size;
   xd->st_dev   = st.st_dev;
   xd->st_ino   = st.st_ino;
   xd->mtime    = st.st_mtime;

   if (gdata.md5build.xpack == xd)
     {
       outerror(OUTERROR_TYPE_WARN,"[MD5]: Canceled (chfile)");

       FD_CLR(gdata.md5build.file_fd, &gdata.readset);
       close(gdata.md5build.file_fd);
       gdata.md5build.file_fd = FD_UNUSED;
       gdata.md5build.xpack = NULL;
     }
   xd->has_md5sum = 0;
   xd->has_crc32 = 0;
   memset(xd->md5sum,0,sizeof(MD5Digest));

   assert(xd->file_fd == FD_UNUSED);
   assert(xd->file_fd_count == 0);
#ifdef HAVE_MMAP
   assert(!irlist_size(&xd->mmaps));
#endif

   return 1;
}

void a_newdir(const userinput * const u)
{
   xdcc *xd;
   char *dir1;
   char *dir2;
   int found;

   updatecontext();

   if (disabled_config(u) != 0)
      return;

   if (invalid_dir(u, u->arg1) != 0)
      return;

   if (!u->arg2e || !strlen(u->arg2e)) {
      a_respond(u,"Try Specifying a new Directory");
      return;
      }

   clean_quotes(u->arg1);
   dir1 = mystrdup(u->arg1);
   convert_to_unix_slash(dir1);

   clean_quotes(u->arg2e);
   dir2 = mystrdup(u->arg2e);
   convert_to_unix_slash(dir2);

   found = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       found += a_newdir_check(u, dir1, dir2, xd);
       xd = irlist_get_next(xd);
     }
   if (found > 0)
     {
       write_statefile();
       xdccsavetext();
     }
 
   a_respond(u, "NEWDIR: %d Packs found", found);
   mydelete(dir1);
   mydelete(dir2);
}

static void a_target_file(char **file2, const char *file1)
{
   char *end;
   char *new;

   if (strchr(*file2, '/') != NULL)
     return;

   if (strrchr(file1, '/') == NULL)
     return;

   new = mymalloc(strlen(file1)+1+strlen(*file2)+1);
   strcpy(new, file1);
   end = strrchr(new, '/');
   if (end != NULL)
      strcpy(++end, *file2);
   mydelete(*file2);
   *file2 = new;
}

void a_filemove(const userinput * const u)
{
   int xfiledescriptor;
   struct stat st;
   char *file1;
   char *file2;
   
   updatecontext();

   if (disabled_config(u) != 0)
      return;

   if (invalid_file(u, u->arg1) != 0)
      return;

   if (!u->arg2e || !strlen(u->arg2e)) {
      a_respond(u,"Try Specifying a new Filename");
      return;
      }
   
   clean_quotes(u->arg1);
   file1 = mystrdup(u->arg1);
   convert_to_unix_slash(file1);
   
   xfiledescriptor = a_open_file(&file1, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0) {
      a_respond(u,"Cant Access File: %s: %s", file1, strerror(errno));
      mydelete(file1);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      a_respond(u,"Cant Access File Details: %s: %s", file1, strerror(errno));
      close(xfiledescriptor);
      mydelete(file1);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      a_respond(u,"%s is not a file", file1);
      mydelete(file1);
      return;
     }
   
   clean_quotes(u->arg2e);
   file2 = mystrdup(u->arg2e);
   convert_to_unix_slash(file2);
   
   a_target_file(&file2, file1);
   if (rename(file1,file2) < 0)
     {
       a_respond(u,"File %s could not be moved to %s: %s", file1, file2, strerror(errno));
     }
   else
     {
       a_respond(u,"File %s was moved to %s.",file1, file2);
     }
   mydelete(file1);
   mydelete(file2);
}

static int a_movefile_sub(const userinput * const u, xdcc *xd, const char *newfile)
{
   struct stat st;
   char *file1;
   char *file2;
   int xfiledescriptor;

   file1 = xd->file;

   xfiledescriptor = a_open_file(&file1, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0) {
      a_respond(u,"Cant Access File: %s: %s", file1, strerror(errno));
      return 1;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      a_respond(u,"Cant Access File Details: %s: %s", file1, strerror(errno));
      close(xfiledescriptor);
      return 1;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      a_respond(u,"%s is not a file", file1);
      return 1;
     }
   
   file2 = mystrdup(newfile);
   convert_to_unix_slash(file2);
   
   a_target_file(&file2, file1);
   if (rename(file1,file2) < 0)
     {
       a_respond(u,"File %s could not be moved to %s: %s", file1, file2, strerror(errno));
       mydelete(file2);
       return 1;
     }
   else
     {
       a_respond(u,"File %s was moved to %s.",file1, file2);
       xd->file = file2;
       mydelete(file1);
       return 0;
     }
}

void a_movefile(const userinput * const u)
{
   xdcc *xd;
   int num = 0;

   updatecontext();

   if (disabled_config(u) != 0)
      return;

   if (u->arg1) num = atoi(u->arg1);
   if (invalid_pack(u, num) != 0)
      return;

   if (!u->arg2e || !strlen(u->arg2e)) {
      a_respond(u,"Try Specifying a new Filename");
      return;
      }

   clean_quotes(u->arg2e);
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   a_movefile_sub(u, xd, u->arg2e);
}

void a_movegroupdir(const userinput * const u)
{
  DIR *d;
  xdcc *xd;
  char *thedir;
  char *tempstr;
  const char *g;
  int num;
  int foundit;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_dir(u, u->arg2e) != 0)
     return;

  clean_quotes(u->arg2e);
  convert_to_unix_slash(u->arg2e);
  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2e);
  d = a_open_dir(&thedir);
  if (!d)
    {
      a_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }

  num = 0;
  foundit = 0;
  tempstr = mycalloc(maxtextlength);
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      num ++;
      if (xd->group != NULL)
        g = xd->group;
      else
        g = "main";
      if (strcasecmp(g,u->arg1) == 0)
        {
          foundit++;
          if (u->arg2e[strlen(u->arg2e)-1] == '/')
            snprintf(tempstr, maxtextlength - 2, "%s%s", u->arg2e, get_basename(xd->file));
          else
            snprintf(tempstr, maxtextlength - 2, "%s/%s", u->arg2e, get_basename(xd->file));
          if (strcmp(tempstr, xd->file) != 0)
            {
              if (a_movefile_sub(u, xd, tempstr))
                break;
            }
        }
      xd = irlist_get_next(xd);
    }
  closedir(d);
  mydelete(tempstr);
  mydelete(thedir);
  if (foundit == 0)
    return;
  write_statefile();
  xdccsavetext();
  return;
}

static void a_filedel_disk(const userinput * const u, const char *name)
{
   int xfiledescriptor;
   struct stat st;
   char *file;
   
   updatecontext();
   
   file = mystrdup(name);
   convert_to_unix_slash(file);
   
   xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0) {
      a_respond(u,"Cant Access File: %s: %s", file, strerror(errno));
      mydelete(file);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      a_respond(u,"Cant Access File Details: %s: %s", file, strerror(errno));
      close(xfiledescriptor);
      mydelete(file);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      a_respond(u,"%s is not a file", file);
      mydelete(file);
      return;
     }
   
   if (unlink(file) < 0)
     {
       a_respond(u,"File %s could not be deleted: %s", file, strerror(errno));
     }
   else
     {
       a_respond(u,"File %s was deleted.", file);
     }
   mydelete(file);
}

void a_filedel(const userinput * const u)
{
   updatecontext();

   if (disabled_config(u) != 0)
      return;

   if (invalid_file(u, u->arg1e) != 0)
      return;

   clean_quotes(u->arg1e);
   a_filedel_disk(u, u->arg1e);
}

void a_fileremove(const userinput * const u)
{
   int num1 = 0;
   int num2 = 0;
   xdcc *xd;
   char *filename;

   updatecontext();

   if (disabled_config(u) != 0)
      return;

   if (u->arg1) num1 = atoi(u->arg1);
   if (invalid_pack(u, num1) != 0)
      return;

   if (u->arg2) num2 = atoi(u->arg2);
   if ( num2 < 0 || num2 > irlist_size(&gdata.xdccs) ) {
      a_respond(u, "Try Specifying a Valid Pack Number");
      return;
      }

   if (num2 == 0)
      num2 = num1;

   if ( num2 < num1 ) {
      a_respond(u, "Pack numbers are not in order");
      return;
      }

   for (; num2 >= num1; num2--) {
      xd = irlist_get_nth(&gdata.xdccs, num2-1);
      filename = mystrdup(xd->file);
      a_remove_pack(u, xd, num2);
      a_filedel_disk(u, filename);
      mydelete(filename);
      }
}

void a_showdir(const userinput * const u)
{
  char *thedir;
  
  updatecontext();

  if (disabled_config(u) != 0)
     return;

  if (invalid_dir(u, u->arg1e) != 0)
     return;
 
  clean_quotes(u->arg1e);
  convert_to_unix_slash(u->arg1e);
  
  if (u->arg1e[strlen(u->arg1e)-1] == '/')
    {
      u->arg1e[strlen(u->arg1e)-1] = '\0';
    }
 
  thedir = mystrdup(u->arg1e);
  u_listdir(u, thedir);
  mydelete(thedir);
  return;
}

#ifdef USE_CURL
void a_fetch(const userinput * const u)
{
  updatecontext();

  if (disabled_config(u) != 0)
     return;

  if (!gdata.uploaddir)
    {
      a_respond(u,"No uploaddir defined.");
      return;
    }
  
  if (disk_full(gdata.uploaddir) != 0)
    {
      a_respond(u,"Not enough free space on disk.");
      return;
    }

  if (invalid_file(u, u->arg1) != 0)
     return;

  if (!u->arg2e || !strlen(u->arg2e))
    {
      a_respond(u,"Try Specifying a URL");
      return;
    }

  start_fetch_url(u);
}

void a_fetchcancel(const userinput * const u)
{
  int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      a_respond(u,"Try Specifying a Valid Download Number");
      return;
    }
  fetch_cancel(num);
}
#endif /* USE_CURL */

void a_amsg(const userinput * const u)
{
  channel_t *ch;
  int ss;
  gnetwork_t *backup;
  
  updatecontext ();

  if (invalid_announce(u, u->arg1e) != 0)
    return;

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if ((ch->flags & CHAN_ONCHAN) && (ch->noannounce == 0))
          privmsg_chan(ch, u->arg1e);
        ch = irlist_get_next(ch);
        }
    }
  gnetwork = backup;
  a_respond(u,"Announced [%s]",u->arg1e);
}

channel_t *is_not_joined_channel(const userinput * const u, const char *name)
{
  channel_t *ch;

  ch = irlist_get_head(&(gnetwork->channels));
  while(ch) {
    if (strcasecmp(ch->name, name) == 0)
      return ch;

    ch = irlist_get_next(ch);
  }
  a_respond(u,"Bot not in Channel %s on %s", name, gnetwork->name);
  return NULL;
}

void a_msg_nick_or_chan(const userinput * const u, const char *name, const char *msg)
{ 
  channel_t *ch;

  if (name[0] != '#') {
    privmsg_fast(name, "%s", msg);
    return;
  }

  ch = is_not_joined_channel(u, name);
  if (ch == NULL)
    return;

  privmsg_chan(ch, "%s", msg);
}

void a_msg(const userinput * const u)
{
  gnetwork_t *backup;

  updatecontext();

  if (invalid_nick(u, u->arg1) != 0)
    return;

  if (invalid_message(u, u->arg2e) != 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
  a_msg_nick_or_chan(u, u->arg1, u->arg2e);
  gnetwork = backup;
}

void a_msgnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (invalid_nick(u, u->arg2) != 0)
    return;

  if (invalid_message(u, u->arg3e) != 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  a_msg_nick_or_chan(u, u->arg2, u->arg3e);
  gnetwork = backup;
}

void a_bann_hostmask(const userinput * const u, const char *arg)
{
   gnetwork_t *backup;
   regex_t *regexp;
   pqueue *pq;
   transfer *tr;
   char *tempstr;
   char *hostmask;
   int changed = 0;

   regexp = mycalloc(sizeof(regex_t));
   tempstr = hostmasktoregex(arg);
   if (regcomp(regexp, tempstr, REG_ICASE|REG_NOSUB)) {
     mydelete(regexp);
     mydelete(tempstr);
     return;
   }

   backup = gnetwork;

   /* XDCC REMOVE */
   pq = irlist_get_head(&gdata.mainqueue);
   while (pq) {
     hostmask = to_hostmask(pq->nick, pq->hostname);
     if (!regexec(regexp, hostmask, 0, NULL, 0)) {
       gnetwork = &(gdata.networks[pq->net]);
       notice_slow(pq->nick,
               "Removed from the queue for \"%s\"", pq->xpack->desc);
       gnetwork = backup;
       ioutput(CALLTYPE_NORMAL, OUT_L, COLOR_YELLOW,
               "Removed from the queue for \"%s\"", pq->xpack->desc);
       a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
       mydelete(pq->nick);
       mydelete(pq->hostname);
       pq = irlist_delete(&gdata.mainqueue, pq);
       changed ++;
     } else {
       pq = irlist_get_next(pq);
     }
     mydelete(hostmask);
   }
   if (changed >0)
     write_statefile();

   /* XDCC CANCEL */
   for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
     hostmask = to_hostmask(tr->nick, tr->hostname);
     if (!regexec(regexp, hostmask, 0, NULL, 0)) {
       if (tr->tr_status != TRANSFER_STATUS_DONE) {
          t_closeconn(tr, "Transfer cancelled by admin", 0);
       }
     }
     mydelete(hostmask);
   }

   mydelete(regexp);
   mydelete(tempstr);
   gnetwork = backup;
}

void a_bannnick(const userinput * const u)
{
   gnetwork_t *backup;
   pqueue *pq;
   transfer *tr;
   char *nick;
   int changed = 0;
   int net = 0;

   net = get_network_msg(u, u->arg2);
   if (net < 0)
     return;

   if (!u->arg1 || !strlen(u->arg1)) {
     a_respond(u, "Try Specifying a Nick");
     return;
   }
 
   nick = u->arg1;
   backup = gnetwork;
   gnetwork = &(gdata.networks[net]);

   /* XDCC REMOVE */
   pq = irlist_get_head(&gdata.mainqueue);
   while (pq) {
     if ((pq->net == net) && (strcasecmp(pq->nick, nick) == 0)) {
       notice_slow(pq->nick,
               "Removed from the queue for \"%s\"", pq->xpack->desc);
       ioutput(CALLTYPE_NORMAL, OUT_L, COLOR_YELLOW,
               "Removed from the queue for \"%s\"", pq->xpack->desc);
       a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
       mydelete(pq->nick);
       mydelete(pq->hostname);
       pq = irlist_delete(&gdata.mainqueue, pq);
       changed ++;
     } else {
       pq = irlist_get_next(pq);
     }
   }
   if (changed >0)
     write_statefile();

   /* XDCC CANCEL */
   for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
     if ((tr->net == net) && (strcasecmp(tr->nick, nick) == 0)) {
       if (tr->tr_status != TRANSFER_STATUS_DONE) {
          t_closeconn(tr, "Transfer cancelled by admin", 0);
       }
     }
   }

   gnetwork = backup;
}

void a_rawnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (invalid_command(u, u->arg2e) != 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  writeserver(WRITESERVER_NOW, "%s", u->arg2e);
  gnetwork = backup;
}

void a_hop(const userinput * const u)
{
   channel_t *ch;
   gnetwork_t *backup;
   int ss;

   updatecontext();

   backup = gnetwork;
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       gnetwork = &(gdata.networks[ss]);
       /* part & join channels */
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if ((!u->arg1) || (!strcasecmp(u->arg1,ch->name)))
             {
               writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
               clearmemberlist(ch);
               ch->flags &= ~CHAN_ONCHAN;
               ch->flags &= ~CHAN_KICKED;
               joinchannel(ch);
             }
           ch = irlist_get_next(ch);
         }
     }
   gnetwork = backup;
}

void a_nochannel(const userinput * const u)
{
   channel_t *ch;
   gnetwork_t *backup;
   int ss;
   int val = 0;

   updatecontext();

   if (u->arg1) val = atoi(u->arg1);

   backup = gnetwork;
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       gnetwork = &(gdata.networks[ss]);
       /* part channels */
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if ((!u->arg2) || (!strcasecmp(u->arg2,ch->name)))
             {
               writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
               clearmemberlist(ch);
               ch->flags &= ~CHAN_ONCHAN;
               ch->nextjoin = gdata.curtime + (val * 60);
             }
           ch = irlist_get_next(ch);
         }
     }
   gnetwork = backup;
}

void a_nomd5(const userinput * const u)
{
   int num = 0;

   updatecontext();

   if (u->arg1) num = atoi(u->arg1);
   gdata.nomd5_start = gdata.curtime + (60 * num) - 1;
   a_respond(u,"** MD5 and CRC checksums have been disabled for the next %i minute%s", num, num!=1 ? "s" : "");
}

void a_cleargets(const userinput * const u)
{
   xdcc *xd;

   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       xd->gets = 0;
       xd = irlist_get_next(xd);
     }

  a_respond(u,"Cleared download counter for each pack");
  write_statefile();
}

void a_identify(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (!gdata.nickserv_pass)
    {
       a_respond(u,"No nickserv_pass set!");
       return;
    }

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  identify_needed(1);
  gnetwork = backup;
  a_respond(u,"nickserv identify send.");
}

void a_holdqueue(const userinput * const u)
{
   int val;
   int i;
   
   updatecontext();
    
   if (gdata.holdqueue)
     {
       val = 0;
     }
   else
     {
       val = 1;
     }
   
   if (u->arg1) val = atoi(u->arg1);
   
   gdata.holdqueue = val;
   a_respond(u,"HOLDQUEUE now %d", val);

   if (val != 0)
     return;

   for (i=0; i<100; i++)
     {
       if (!gdata.exiting &&
           irlist_size(&gdata.mainqueue) &&
           (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
         {
           sendaqueue(0, 0);
         }
     }
}

void a_dump(const userinput * const u)
{
   dumpgdata();
   a_respond(u,"DUMP written into logfile.");
}

void a_autoadd(const userinput * const u)
{
   autoadd_all();
   a_respond(u,"AUTOADD done.");
}

/* this function imported from iroffer-lamm */
void a_queue(const userinput * const u)
{
   int num = 0;
   int alreadytrans;
   pqueue *pq;
   xdcc *xd;
   char *tempstr;
   gnetwork_t *backup;
   int net;
   
   updatecontext();

   net = get_network_msg(u, u->arg3);
   if (net < 0)
      return;

   if (invalid_nick(u, u->arg1) != 0)
      return;

   if (u->arg2) num = atoi(u->arg2);
   if (invalid_pack(u, num) != 0)
      return;

   xd = irlist_get_nth(&gdata.xdccs, num-1);

   alreadytrans = 0;
   pq = irlist_get_head(&gdata.mainqueue);
   while(pq)
     {
       if (!strcasecmp(pq->nick,u->arg1))
         {
           if (pq->xpack == xd)
             {
               alreadytrans++;
               break;
             }
         }
       pq = irlist_get_next(pq);
     }

   if (alreadytrans > 0) {
      a_respond(u,"Already Queued %s for Pack %i!", u->arg1,num);
      return;
      }
   
   a_respond(u,"Queueing %s for Pack %i", u->arg1,num);
   
   backup = gnetwork;
   gnetwork = &(gdata.networks[net]);
   tempstr = addtoqueue(u->arg1, "man", num);
   notice(u->arg1, "** %s", tempstr);
   mydelete(tempstr);
   gnetwork = backup;
   
   if (!gdata.exiting &&
       irlist_size(&gdata.mainqueue) &&
       (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
     {
       sendaqueue(0, 0);
     }
}

/* iroffer-lamm: add-ons */
void a_announce(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  channel_t *ch;
  char *tempstr;
  char *tempstr2;
  int ss;
  gnetwork_t *backup;
  
  updatecontext ();
  
  if (u->arg1) num = atoi (u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  if (invalid_announce(u, u->arg2e) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  a_respond(u,"Pack Info for Pack #%i:",num);
  
  tempstr = mycalloc(maxtextlength);
  tempstr2 = mycalloc(maxtextlength);
  snprintf(tempstr2,maxtextlength-2,"[\2%s\2] %s",u->arg2e,xd->desc);
  
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      snprintf(tempstr, maxtextlength-2, "%s - /MSG %s XDCC SEND %i",
               tempstr2, save_nick(gnetwork->user_nick), num);
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if ((ch->flags & CHAN_ONCHAN) && (ch->noannounce == 0))
          privmsg_chan(ch, tempstr);
        ch = irlist_get_next(ch);
        }
    }
  gnetwork = backup;
  a_respond(u,"Announced [%s] - %s",u->arg2e,xd->desc);
  mydelete(tempstr2);
  mydelete(tempstr);
}

void a_cannounce(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  channel_t *ch;
  char *tempstr;
  char *tempstr2;
  int ss;
  gnetwork_t *backup;
  
  updatecontext ();
  
  if (invalid_channel(u, u->arg1) != 0)
    return;

  if (u->arg2) num = atoi (u->arg2);
  if (invalid_pack(u, num) != 0)
    return;

  if (invalid_announce(u, u->arg3e) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  a_respond(u,"Pack Info for Pack #%i:",num);
  
  tempstr = mycalloc(maxtextlength);
  tempstr2 = mycalloc(maxtextlength);
  snprintf(tempstr2, maxtextlength - 2, "[\2%s\2] %s", u->arg3e, xd->desc);
  
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      snprintf(tempstr, maxtextlength-2, "%s - /MSG %s XDCC SEND %i",
               tempstr2, save_nick(gnetwork->user_nick), num);
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if ((ch->flags & CHAN_ONCHAN) && (ch->noannounce == 0)) {
          if (strcasecmp(ch->name, u->arg1) == 0)
            privmsg_chan(ch, tempstr);
        }
        ch = irlist_get_next(ch);
      }
    }
  gnetwork = backup;
  a_respond(u, "Announced [%s] - %s", u->arg3e, xd->desc);
  mydelete(tempstr2);
  mydelete(tempstr);
}

void a_sannounce(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  channel_t *ch;
  char *tempstr;
  int ss;
  gnetwork_t *backup;
 
  updatecontext ();
 
  if (u->arg1) num = atoi (u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
 
  a_respond(u,"Pack Info for Pack #%i:",num);
 
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr,maxtextlength-2,"\2%i\2 %s",num, xd->desc);
 
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if ((ch->flags & CHAN_ONCHAN) && (ch->noannounce == 0))
          privmsg_chan(ch, tempstr);
        ch = irlist_get_next(ch);
        }
    }
  gnetwork = backup;
  a_respond(u,"Announced [%s] - %s",u->arg2e,xd->desc);
  mydelete(tempstr);
}

/* iroffer-lamm: add-ons */
void a_addann(const userinput * const u)
{
  char *tempstr;
  userinput *ui;
  int i;
  
  updatecontext ();
  i = irlist_size(&gdata.xdccs);
  a_add(u);
  if (irlist_size(&gdata.xdccs) > i) {
    tempstr = mycalloc (maxtextlength);
    ui = mycalloc(sizeof(userinput));
    snprintf(tempstr, maxtextlength - 2, "A A A A A announce %i added",irlist_size(&gdata.xdccs));
    u_fillwith_msg(ui,NULL,tempstr);
    ui->method = method_out_all;       /* just OUT_S|OUT_L|OUT_D it */
    ui->net = u->net;
    ui->level = u->level;
    u_parseit(ui);
    mydelete(ui);
    mydelete(tempstr);
    }
}

/* End of File */
