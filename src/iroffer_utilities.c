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
 * @(#) iroffer_utilities.c 1.205@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_utilities.c|20051123201144|35346
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_jobs.h"
#include "dinoex_http.h"
#include "dinoex_badip.h"
#include "dinoex_irc.h"
#include "dinoex_config.h"
#include "dinoex_main.h"
#include "dinoex_misc.h"


static void irlist_insert_tail(irlist_t *list, void *item);

void getos (void) {

   struct utsname u1;

   updatecontext();

   if ( uname(&u1) < 0)
      outerror(OUTERROR_TYPE_CRASH,"Couldn't Get Your OS Type");
   
   printf("** You Are Running %s %s on a %s",u1.sysname,u1.release,u1.machine);
   mylog("You Are Running %s %s on a %s", u1.sysname, u1.release, u1.machine);
   
   gdata.osstring = mymalloc(strlen(u1.sysname) + strlen(u1.release) + 2);
   
   sprintf(gdata.osstring, "%s %s",
           u1.sysname, u1.release);
   
   /* verify we are who we were configured for, and set config */
#if defined(_OS_CYGWIN)
   {
     int count,v1=0,v2=0,v3=0;
     count = sscanf(u1.release,"%d.%d.%d",&v1,&v2,&v3);
     if (
	 (v1 < 1) ||
	 ((v1 == 1) && (v2 < 5)) ||
	 ((v1 == 1) && (v2 == 5) && (v3 < 3))
	 )
       {
	 printf(", Too Old\n");
	 outerror(OUTERROR_TYPE_CRASH,"CYGWIN too old (1.5.3 or greater required)");
       }
   }
#endif
   check_osname(u1.sysname);
   }

void outerror (outerror_type_e type, const char *format, ...) {
   char tempstr[maxtextlength];
   va_list args;
   int ioutput_options = OUT_S|OUT_L|OUT_D;
   int len;

   /* can't log an error if the error was due to logging */
   if (type & OUTERROR_TYPE_NOLOG)
     {
       ioutput_options = OUT_S|OUT_D;
     }
   type &= ~OUTERROR_TYPE_NOLOG;
   
   updatecontext();

   va_start(args, format);
   len = vsnprintf(tempstr, maxtextlength, format, args);
   va_end(args);
   
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      snprintf(tempstr, maxtextlength, "OUTERROR-INT: Output too large, ignoring!");
    }

   if ( type == OUTERROR_TYPE_CRASH ) {
      
      ioutput(ioutput_options, COLOR_RED|COLOR_BOLD, "ERROR: %s", tempstr);
      
      tostdout_disable_buffering();
      
      uninitscreen();
      
      if (gdata.background)
        {
          printf("** ERROR: %s\n\n",tempstr);
        }
         
      mylog("iroffer exited (Error)\n\n");
      if (gdata.pidfile) unlink(gdata.pidfile);

      exit(1);
      }
   else if (type == OUTERROR_TYPE_WARN_LOUD ) {
      ioutput(ioutput_options, COLOR_RED|COLOR_BOLD, "WARNING: %s", tempstr);
      }
   else if (type == OUTERROR_TYPE_WARN ) {
      ioutput(ioutput_options, COLOR_RED, "WARNING: %s", tempstr);
      }
   
   }

char* getdatestr(char* str, time_t Tp, size_t len)
{
  struct tm *localt = NULL;
  size_t llen;
    
  if (Tp == 0)
    {
      Tp = gdata.curtime;
    }
  localt = localtime(&Tp);
  
  llen = strftime (str, len, "%Y-%m-%d-%H:%M:%S", localt);
  if ((llen == 0) || (llen == len))
    {
      str[0] = '\0';
    }
  
  return str;
}

char* getuptime(char *str, unsigned int type, time_t fromwhen, size_t len)
{
  int days, hours, mins;
  long temp;
  
  updatecontext();
  
  temp  = (gdata.curtime-fromwhen)/60;
  days  = temp/60/24;
  hours = temp/60 - (24*days);
  mins  = temp - (60*hours) - (24*60*days);
  
  if (type)
    {
      add_snprintf(str, len, "%dD %dH %dM",
                   days, hours, mins);
    }
  else
    {
      add_snprintf(str, len, "%d Days %d Hrs and %d Min",
                   days, hours, mins);
    }
  
  return str;
}


void mylog(const char *format, ...)
{
  char tempstr[maxtextlength];
  va_list args;
  size_t len;
  
  if (gdata.logfile == NULL)
    {
      return;
    }
  
  va_start(args, format);
  len = vsnprintf(tempstr, maxtextlength, format, args);
  va_end(args);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD | OUTERROR_TYPE_NOLOG,
               "MYLOG-INT: Output too large, ignoring!");
      return;
    }
  
  logfile_add(gdata.logfile, tempstr);
  return;
}


unsigned long atoul (const char *str) {
   unsigned long num,temp;
   int i,j;
   if (str == NULL) return 0;
   
   num = 0;
   
   for (i=strlen(str)-1; i>=0; i--) {
      temp = (str[i]-'0');
      for (j=strlen(str)-1; j>i; j--)
         temp *= 10;
      num += temp;
      }
   return num;
   }

unsigned long long atoull (const char *str) {
   unsigned long long num,temp;
   int i,j;
   if (str == NULL) return 0;
   
   num = 0;
   
   for (i=strlen(str)-1; i>=0; i--) {
      temp = (str[i]-'0');
      for (j=strlen(str)-1; j>i; j--)
         temp *= 10;
      num += temp;
      }
   return num;
   }

void ioutput(int dest, unsigned int color_flags, const char *format, ...) {
   va_list args;
   va_start(args, format);
   vioutput(dest, color_flags, format, args);
   va_end(args);
}

void vioutput(int dest, unsigned int color_flags, const char *format, va_list ap) {
   char tempstr[maxtextlength];
   int len;
   dccchat_t *chat;
   
   len = vsnprintf(tempstr,maxtextlength,format,ap);
   
   if (len < 0)
     {
       snprintf(tempstr, maxtextlength, "IOUTPUT-INT: Output too large, ignoring!");
     }
   if (len >= (int)maxtextlength)
     {
       tempstr[maxtextlength-1] = 0;
     }

   /* screen */
   if (!gdata.background && (dest & OUT_S)) {
      
      if (!gdata.attop) gototop();
      
          if (!gdata.nocolor && (color_flags != COLOR_NO_COLOR))
            {
              tostdout(IRVT_COLOR_SET1,
                       (color_flags & COLOR_BOLD) ? 1 : 0,
                       (color_flags & ~COLOR_BOLD));
            }
          if (gdata.timestampconsole)
            {
              char tempstr2[maxtextlength];
              user_getdatestr(tempstr2, 0, maxtextlength);
              tostdout("** %s: ",tempstr2);
            }
          else
            {
              tostdout("** ");
            }
      
      tostdout("%s",tempstr);
      
          if (!gdata.nocolor && (color_flags != COLOR_NO_COLOR))
            {
              tostdout(IRVT_COLOR_RESET "\n");
            }
          else
            {
              tostdout("\n");
            }
      }
   
   /* log */
   if (dest & OUT_L)
      mylog("%s", tempstr);
   
   /* dcc chat */
   if (dest & OUT_D)
     {
       for (chat = irlist_get_head(&gdata.dccchats);
            chat;
            chat = irlist_get_next(chat))
         {
           if (chat->status == DCCCHAT_CONNECTED)
             {
               writedccchat(chat, 0, "--> %s\n", tempstr);
             }
         }
      }
   
   /* httpd */
   if (dest & OUT_H)
     {
        if (gdata.logfile_httpd)
           {
             logfile_add(gdata.logfile_httpd, tempstr);
             return;
           }
        /* no log defined, use global log */
        if (dest & OUT_L)
           return;
        
        mylog("%s", tempstr);
     }
   
   }

void privmsg_slow(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg_slow(nick, format, args);
  va_end(args);
}

void vprivmsg_slow(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"PRVMSG-SLOW: Output too large, ignoring!");
      return;
    }
  
  writeserver_privmsg(WRITESERVER_SLOW, nick, tempstr, len);
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vprivmsg_fast(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"PRVMSG-FAST: Output too large, ignoring!");
      return;
    }
  
  writeserver_privmsg(WRITESERVER_FAST, nick, tempstr, len);
}

void privmsg_fast(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg_fast(nick, format, args);
  va_end(args);
}

void vprivmsg(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"PRVMSG: Output too large, ignoring!");
      return;
    }
  
  writeserver_privmsg(WRITESERVER_NORMAL, nick, tempstr, len);
}

void privmsg(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprivmsg(nick, format, args);
  va_end(args);
}

void notice_slow(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vnotice_slow(nick, format, args);
  va_end(args);
}

void vnotice_slow(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"NOTICE-SLOW: Output too large, ignoring!");
      return;
    }
  
  writeserver_notice(WRITESERVER_SLOW, nick, tempstr, len);
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vnotice_fast(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"NOTICE-FAST: Output too large, ignoring!");
      return;
    }
  
  writeserver_notice(WRITESERVER_FAST, nick, tempstr, len);
}

void notice_fast(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vnotice_fast(nick, format, args);
  va_end(args);
}

void notice(const char *nick, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vnotice(nick, format, args);
  va_end(args);
}

void vnotice(const char *nick, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  ssize_t len;
  
  if (!nick) return;
  
  len = vsnprintf(tempstr,maxtextlength,format,ap);
  
  if ((len < 0) || (len >= maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN,"NOTICE: Output too large, ignoring!");
      return;
    }
  
  writeserver_notice(WRITESERVER_NORMAL, nick, tempstr, len);
}

size_t sstrlen (const char *p) {
   if (!p) return 0;
   return (strlen(p));
   }


#ifndef WITHOUT_MEMSAVE

static unsigned long mycalloc_hash(void *ptr)
{
  unsigned long retval;
  
  retval = 0xAA;
  retval ^= ((unsigned long)ptr >>  0) & 0xFF;
  retval ^= ((unsigned long)ptr >>  8) & 0xFF;
  retval ^= ((unsigned long)ptr >> 16) & 0xFF;
  retval ^= ((unsigned long)ptr >> 24) & 0xFF;
  
  return retval & (MEMINFOHASHSIZE-1);
}

static void meminfo_grow(int grow)
{
  meminfo_t *newmeminfo;
  unsigned int cc;
  unsigned int dd;
  unsigned int meminfo_depth_grow;
  size_t len;
  unsigned long i;
  unsigned long start;

  meminfo_depth_grow = gdata.meminfo_depth;
  meminfo_depth_grow += grow;
  len = MEMINFOHASHSIZE * sizeof(meminfo_t) * (meminfo_depth_grow);
  newmeminfo = calloc(len,1);
  
  /* replace zero entry */
  if (gdata.meminfo)
    {
      gdata.meminfo[0].ptr       = NULL;
      gdata.meminfo[0].alloctime = 0;
      gdata.meminfo[0].size      = 0;
      gdata.meminfo[0].src_func  = NULL;
      gdata.meminfo[0].src_file  = NULL;
      gdata.meminfo[0].src_line  = 0;
    }
  else
    {
      /* first time, count item #0 */
      gdata.meminfo_count++;
    }
  
  newmeminfo[0].ptr          = newmeminfo;
  newmeminfo[0].alloctime    = gdata.curtime;
  newmeminfo[0].size         = len;
  newmeminfo[0].src_func     = __FUNCTION__;
  newmeminfo[0].src_file     = __FILE__;
  newmeminfo[0].src_line     = __LINE__;
  
  for (cc=0; cc<MEMINFOHASHSIZE; cc++)
    {
      for (dd=0; dd<gdata.meminfo_depth; dd++)
        {
          if (gdata.meminfo[(cc*(gdata.meminfo_depth)) + dd].ptr)
            {
              /* find new location */
              start = mycalloc_hash(gdata.meminfo[(cc*(gdata.meminfo_depth)) + dd].ptr) * (meminfo_depth_grow);
              
              for (i=0; newmeminfo[(i+start)%(MEMINFOHASHSIZE * (meminfo_depth_grow))].ptr; i++) ;
              
              i = (i+start)%(MEMINFOHASHSIZE * (meminfo_depth_grow));
              
              newmeminfo[i] = gdata.meminfo[(cc*(gdata.meminfo_depth)) + dd];
            }
        }
    }
  
  if (gdata.meminfo)
    {
      /* second or later time */
      free(gdata.meminfo);
    }
  
  gdata.meminfo = newmeminfo;
  gdata.meminfo_depth = meminfo_depth_grow;
  
  if (gdata.debug > 0)
    {
      if (gdata.crashing == 0)
      ioutput(OUT_S, COLOR_NO_COLOR, "growing meminfo from %u to %u",
              gdata.meminfo_depth-grow, gdata.meminfo_depth);
    }

  return;
}

void* mymalloc2(size_t len, int zero,
                const char *src_function, const char *src_file, unsigned int src_line) {
   void *t = NULL;
   unsigned int i;
   unsigned long start;
   
   updatecontext();

   t = zero ? calloc(len,1) : malloc(len);
   
   if (t == NULL)
     {
       outerror(OUTERROR_TYPE_CRASH,"Couldn't Allocate Memory!!");
     }
   
   if (gdata.meminfo_count >= ((MEMINFOHASHSIZE * gdata.meminfo_depth) / 2))
     {
       meminfo_grow((int)(gdata.meminfo_depth + 1));
     }
   
   start = mycalloc_hash(t) * gdata.meminfo_depth;
   
   for (i=0; gdata.meminfo[(i+start)%(MEMINFOHASHSIZE * gdata.meminfo_depth)].ptr; i++) ;
   
   i = (i+start)%(MEMINFOHASHSIZE * gdata.meminfo_depth);
   
   gdata.meminfo[i].ptr       = t;
   gdata.meminfo[i].alloctime = gdata.curtime;
   gdata.meminfo[i].size      = len;
   gdata.meminfo[i].src_func  = src_function;
   gdata.meminfo[i].src_file  = src_file;
   gdata.meminfo[i].src_line  = src_line;
   
   gdata.meminfo_count++;
   
   return t;
   }

void mydelete2(void *t) {
   unsigned char *ut = (unsigned char *)t;
   unsigned long i;
   unsigned long start;
   
   updatecontext();

   if (t == NULL) return;
   
   start = mycalloc_hash(t) * gdata.meminfo_depth;
   
   for (i=0; (i<(MEMINFOHASHSIZE * gdata.meminfo_depth) && (gdata.meminfo[(i+start)%(MEMINFOHASHSIZE * gdata.meminfo_depth)].ptr != t)); i++) ;
   
   if (i == (MEMINFOHASHSIZE * gdata.meminfo_depth)) {
      gdata.crashing = 1; /* stop trackback here */
      outerror(OUTERROR_TYPE_WARN_LOUD,"Pointer 0x%8.8lX not found in meminfo database while trying to free!!",(long)t);
      outerror(OUTERROR_TYPE_WARN_LOUD, "Please report this error to Dinoex dinoex@dinoex.net");
      hexdump(OUT_S|OUT_L|OUT_D, COLOR_RED|COLOR_BOLD, "WARNING:" , ut, 8*16);
      outerror(OUTERROR_TYPE_WARN_LOUD,"Aborting Program! (core file should be generated)");
      abort(); /* getting a core file will help greatly */
      }
   else {
      free(t);
      
      i = (i+start)%(MEMINFOHASHSIZE * gdata.meminfo_depth);
      
      gdata.meminfo[i].ptr       = NULL;
      gdata.meminfo[i].alloctime = 0;
      gdata.meminfo[i].size      = 0;
      gdata.meminfo[i].src_func  = NULL;
      gdata.meminfo[i].src_file  = NULL;
      gdata.meminfo[i].src_line  = 0;
      
      gdata.meminfo_count--;
      }
   
   /* we are crashing, do not shrink meminfo */
   if (gdata.crashing)
     return;
   
   if ((gdata.meminfo_depth > 1) &&
       (gdata.meminfo_count < ((MEMINFOHASHSIZE * gdata.meminfo_depth) / 8)))
     {
       meminfo_grow(-((int)((gdata.meminfo_depth+1)/2)));
     }
   
   return;
   }

#endif /* WITHOUT_MEMSAVE */

void updatecontext_f(const char *file, const char *func, unsigned int line)
{
  context_t *c;
  
  if (gdata.crashing)
    {
      return;
    }
  
  gdata.context_cur_ptr++;
  if (gdata.context_cur_ptr >= MAXCONTEXTS)
    gdata.context_cur_ptr %= MAXCONTEXTS;
  
  c = &gdata.context_log[gdata.context_cur_ptr];
  
  c->file = file;
  c->func = func;
  c->line = line;
  
  if (gdata.debug > 0)
    {
      gettimeofday(&c->tv, NULL);
    }
  else
    {
      c->tv.tv_sec  = 0;
      c->tv.tv_usec = 0;
    }
  
}

void dumpcontext(void)
{
  unsigned int i;
  context_t *c;
  
  for (i=0; i<MAXCONTEXTS; i++)
    {
      c = &gdata.context_log[(gdata.context_cur_ptr + 1 + i) % MAXCONTEXTS];
      if (c->file == NULL)
        continue;
      
      ioutput(OUT_S|OUT_L, COLOR_NO_COLOR,
              "Trace %3u  %-20s %-16s:%5u  %lu.%06lu",
              i-MAXCONTEXTS+1,
              c->func ? c->func : "UNKNOWN",
              c->file ? c->file : "UNKNOWN",
              c->line,
              (unsigned long)c->tv.tv_sec,
              (unsigned long)c->tv.tv_usec);
    }
  return;
}


static void dump_config_string4(const char *name, const char *val)
{
  dump_line("GDATA * " "%s: %s", name, val ? val : "<undef>" );
}

static void dump_config_int4(const char *name, unsigned int val)
{
  dump_line("GDATA * " "%s: %u", name, val);
}

#define gdata_string(x) ((x) ? (x) : "<undef>")

#define gdata_print_number(format,name) \
    dump_line("GDATA * " #name ": " format, gdata. name);

#define gdata_print_number_cast(format,name,type) \
    dump_line("GDATA * " #name ": " format, (type) gdata. name);

#define gdata_print_string(name) \
    dump_config_string4(#name, gdata. name);

#define gdata_print_int(name) \
    dump_config_int4(#name, gdata. name);

#define gdata_print_uint(name)  gdata_print_number("%u", name)
#define gdata_print_long(name)  gdata_print_number("%ld", name)
#define gdata_print_float(name) gdata_print_number("%.5f", name)
#define gdata_print_time(name)  gdata_print_number("%" TTPRINTFMT, name)


#define gdata_print_number_array(format,name) \
    { if (gdata. name [ii]) { dump_line("GDATA * " #name "[%d]: " format, ii, gdata. name [ii]); } }

#define gdata_print_string_array(name) \
    { if (gdata. name [ii]) { dump_line("GDATA * " #name "[%d]: %s", ii, gdata_string(gdata. name [ii])); } }

#define gdata_print_number_array_item(format,name,item) \
    { if (gdata. name [ii] . item) { dump_line("GDATA * " #name "[%d]: " #item "=" format, ii, gdata. name [ii] . item); } }

#define gdata_print_number_array_item_cast(format,name,item,type) \
    { if (gdata. name [ii] . item) { dump_line("GDATA * " #name "[%d]: " #item "=" format, ii, (type) gdata. name [ii] . item); } }

#define gdata_print_string_array_item(name,item) \
    { if (gdata. name [ii] . item) { dump_line("GDATA * " #name "[%d]: " #item "=%s", ii, gdata_string(gdata. name [ii] . item)); } }

#define gdata_print_int_array(name)   gdata_print_number_array("%d", name)
#define gdata_print_ulong_array(name) gdata_print_number_array("%lu", name)


#define gdata_irlist_iter_start(name, type) \
    { type *iter; dump_line("GDATA * " #name ":"); for(iter=irlist_get_head(&gdata. name); iter; iter=irlist_get_next(iter)) { 

#define gdata_irlist_iter_end } }

#define gdata_iter_as_print_string \
    dump_line("  : %s", gdata_string(iter));


#define gdata_iter_print_number(format,name) \
    dump_line("  " #name ": " format, iter-> name);

#define gdata_iter_print_number_cast(format,name,type) \
    dump_line("  " #name ": " format, (type) iter-> name);

#define gdata_iter_print_string(name) \
    dump_line("  " #name ": %s", gdata_string(iter-> name));

#define gdata_iter_print_int(name)   gdata_iter_print_number("%d", name)
#define gdata_iter_print_uint(name)  gdata_iter_print_number("%u", name)
#define gdata_iter_print_long(name)  gdata_iter_print_number("%ld", name)
#define gdata_iter_print_time(name)  gdata_iter_print_number("%" TTPRINTFMT, name)



void dumpgdata(void)
{
  unsigned int ii;
  unsigned int ss;
  char ip6[maxtextlengthshort];
  char *text;
  
  text = print_config_key("features"); /* NOTRANSLATE */
  dump_line("%s", text);
  mydelete(text);

  config_dump();
  dump_line("GDATA DUMP BEGIN");
  
  gdata_print_int(transfermethod);
  
  for (ii=0; ii<MAXCONFIG; ii++)
    {
      gdata_print_string_array(configfile);
      dump_line("  : lastmodified=%ld", (long)gdata.configtime[ii]);
    }
  gdata_print_string(osstring);
  gdata_print_time(startuptime);
  gdata_print_int(background);
  gdata_print_number_cast("%d",last_logrotate,int);
  gdata_print_number_cast("%d", last_update, int);

  gdata_print_int(maxb);
  
  for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
    {
      gdata_print_number_array_item("%" LLPRINTFMT "u",transferlimits,limit);
      gdata_print_number_array_item("%" LLPRINTFMT "u",transferlimits,used);
      gdata_print_number_array_item_cast("%ld",transferlimits,ends,long int);
    }
  gdata_print_int(transferlimits_over);
  
  gdata_irlist_iter_start(autotrigger, autotrigger_t);
  dump_line("GDATA * " "pack" ": " "%d", number_of_pack(iter->pack));
  gdata_iter_print_string(word);
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(packs_delayed, userinput);
  gdata_iter_print_uint(method);
  gdata_iter_print_string(snick);
  gdata_iter_print_string(cmd);
  gdata_iter_print_string(arg1);
  /* ppointer stat arg2 */
  gdata_iter_print_string(arg3);
  gdata_iter_print_int(fd);
  gdata_iter_print_int(net);
  /* ppointer chat */
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(jobs_delayed, char);
  gdata_iter_as_print_string;
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(https, http);
  dump_line("  : clientsocket=%d filedescriptor=%d",
            iter->con.clientsocket, iter->filedescriptor);
  dump_line("  : bytesgot=%" LLPRINTFMT "d bytessent=%" LLPRINTFMT "d",
            iter->bytesgot, iter->bytessent);
  dump_line("  : filepos=%" LLPRINTFMT "d totalsize=%" LLPRINTFMT "d",
            iter->filepos, iter->totalsize);
  dump_line("  : lastcontact=%ld connecttime=%ld",
            (long)iter->con.lastcontact,
            (long)iter->con.connecttime);
  dump_line( "  : remote%s",
            iter->con.remoteaddr);
  gdata_iter_print_uint(status);
  gdata_iter_print_uint(support_groups);
  gdata_iter_print_string(file);
  gdata_iter_print_string(buffer_out);
  gdata_iter_print_string(end);
  gdata_iter_print_string(nick);
  gdata_iter_print_string(group);
  gdata_iter_print_string(order);
  gdata_iter_print_string(search);
  gdata_iter_print_string(pattern);
  gdata_iter_print_string(modified);
  gdata_iter_print_int(traffic);
  dump_line("  : left=%ld",
            (long)iter->left);
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(http_bad_ip4, badip4);
  dump_line("  : remoteip=0x%.8X",
            iter->remoteip);
  dump_line("  : lastcontact=%ld connecttime=%ld",
            (long)iter->lastcontact,
            (long)iter->connecttime);
  gdata_iter_print_long(count);
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(http_bad_ip6, badip6);
  inet_ntop(AF_INET6, &(iter->remoteip), ip6, maxtextlengthshort);
  dump_line("  : remoteip=%s", ip6);
  dump_line("  : lastcontact=%ld connecttime=%ld",
            (long)iter->lastcontact,
            (long)iter->connecttime);
  gdata_iter_print_long(count);
  gdata_irlist_iter_end;

  gdata_print_string(nosendmsg);

  gdata_print_number_cast("%ld", nomd5_start, long);
  gdata_print_number_cast("%ld", noannounce_start, long);
  gdata_print_number_cast("%ld", noautoadd, long);
  
  gdata_irlist_iter_start(tuploadhost, tupload_t);
  gdata_iter_print_string(u_host);
  gdata_iter_print_long(u_time);
  gdata_irlist_iter_end;
  gdata_irlist_iter_start(quploadhost, qupload_t);
  gdata_iter_print_string(q_host);
  gdata_iter_print_string(q_nick);
  gdata_iter_print_string(q_pack);
  gdata_iter_print_int(q_net);
  gdata_iter_print_long(q_time);
  gdata_irlist_iter_end;
  gdata_irlist_iter_start(fetch_queue, fetch_queue_t);
  gdata_iter_print_int(net);
  gdata_iter_print_string(name);
  gdata_iter_print_string(url);
  gdata_iter_print_string(uploaddir);
  gdata_irlist_iter_end;
  
  gdata_print_int(networks_online);
  for (ss=0; ss<gdata.networks_online; ss++)
    {
      gdata_print_int(networks[ss].net);
      gdata_print_string(networks[ss].name);
      gdata_print_string(networks[ss].user_nick);
      gdata_print_string(networks[ss].caps_nick);
      /* r_config_nick */
      gdata_print_number("0x%.8X", networks[ss].ourip);
      /* r_ourip */
      gdata_print_string(networks[ss].curserver.hostname);
      gdata_print_uint(networks[ss].curserver.port);
      gdata_print_string(networks[ss].curserver.password);
      gdata_print_string(networks[ss].curserveractualname);
      gdata_print_int(networks[ss].nocon);
      gdata_print_int(networks[ss].servertime);
      gdata_print_number_cast("%d", networks[ss].serverstatus, int);
      gdata_print_number_cast("%d", networks[ss].botstatus, int);
      gdata_print_number_cast("%d", networks[ss].lag_method, int);
      gdata_print_time(networks[ss].connecttime);
      gdata_print_time(networks[ss].lastservercontact);
      gdata_print_time(networks[ss].lastnotify);
      gdata_print_time(networks[ss].lastping);
      gdata_print_time(networks[ss].lastslow);
      gdata_print_time(networks[ss].lastnormal);
      gdata_print_time(networks[ss].lastfast);
      gdata_print_time(networks[ss].lastsend);
      gdata_print_long(networks[ss].lag);
      gdata_print_time(networks[ss].next_identify);
      gdata_print_time(networks[ss].next_restrict);
  
      gdata_irlist_iter_start(networks[ss].serverq_fast, char);
      gdata_iter_as_print_string;
      gdata_irlist_iter_end;
  
      gdata_irlist_iter_start(networks[ss].serverq_normal, char);
      gdata_iter_as_print_string;
      gdata_irlist_iter_end;
  
      gdata_irlist_iter_start(networks[ss].serverq_slow, char);
      gdata_iter_as_print_string;
      gdata_irlist_iter_end;
  
      gdata_irlist_iter_start(networks[ss].serverq_channel, channel_announce_t);
      gdata_iter_print_string(channel);
      gdata_iter_print_string(msg);
      gdata_iter_print_long(c_time);
      gdata_irlist_iter_end;
  
      gdata_print_int(networks[ss].serverbucket);
      gdata_print_int(networks[ss].ircserver);
      gdata_print_int(networks[ss].serverconnectbackoff);
      
      for (ii=0; ii<MAX_PREFIX && gdata.networks[ss].prefixes[ii].p_mode; ii++)
        {
          gdata_print_number_array_item("%c", networks[ss].prefixes, p_mode);
          gdata_print_number_array_item("%c", networks[ss].prefixes, p_symbol);
        }
      
      for (ii=0; ii<MAX_CHANMODES && gdata.networks[ss].chanmodes[ii]; ii++)
        {
          gdata_print_number_array("%c", networks[ss].chanmodes);
        }
      
      for (ii=0; ii<INAMNT_SIZE; ii++)
        {
          gdata_print_int_array(networks[ss].inamnt);
        }
    } /* networks */
   
  /* r_channel t_channel r_pidfile */
  /* r_transferminspeed r_transfermaxspeed */
  
  gdata_print_int(adjustcore);

  gdata_print_int(attop);
  gdata_print_int(needsclear);
  gdata_print_int(termcols);
  gdata_print_int(termlines);
  gdata_print_int(nocolor);
  gdata_print_int(noscreen);
  gdata_print_int(curcol);
  gdata_print_int(console_history_offset);
  gdata_print_string(console_input_line);

  gdata_irlist_iter_start(console_history, char);
  gdata_iter_as_print_string;
  gdata_irlist_iter_end;
  
  /* startup_tio */
     
  /* stdout_buffer_init stdout_buffer */
  
  for (ss=0; ss<gdata.networks_online; ss++)
    {
      
      gdata_irlist_iter_start(networks[ss].channels, channel_t);
      dump_line("  : name=%s flags=%x", iter->name, iter->flags);
      gdata_iter_print_time(nextann);
      gdata_iter_print_time(nextmsg);
      gdata_iter_print_time(nextjoin);
      gdata_iter_print_time(lastjoin);
  /* members */
  gdata_irlist_iter_end;
  
      gdata_print_int(networks[ss].recentsent);
      gdata_print_int(networks[ss].nick_number);
  
      gdata_irlist_iter_start(networks[ss].xlistqueue, xlistqueue_t);
      gdata_iter_print_string(nick);
      gdata_iter_print_string(msg);
      gdata_irlist_iter_end;
      
      gdata_irlist_iter_start(networks[ss].dcc_options, dcc_options_t);
      gdata_iter_print_string(nick);
      gdata_iter_print_time(last_seen);
      gdata_iter_print_int(options);
      gdata_irlist_iter_end;
      
    }
  
  gdata_irlist_iter_start(msglog, msglog_t);
  dump_line(
          "  : when=%ld hostmask=%s message=%s",
          (long)iter->when,
          gdata_string(iter->hostmask),
          gdata_string(iter->message));
  gdata_irlist_iter_end;
  

  gdata_irlist_iter_start(dccchats, dccchat_t);
  dump_line(
          "  : status=%d fd=%d",
          iter->status,
          iter->con.clientsocket);
  dump_line(
          "  : lastcontact=%ld connecttime=%ld",
          (long)iter->con.lastcontact,
          (long)iter->con.connecttime);
  dump_line("  : localport=%d local%s remote%s",
            iter->con.localport,
            iter->con.localaddr,
            iter->con.remoteaddr);
  gdata_iter_print_string(nick);
  gdata_irlist_iter_end;
  gdata_print_int(num_dccchats);

  gdata_print_number_cast("%d",curtime,int);
  
  /* readset writeset */
  
  gdata_print_float(record);
  gdata_print_float(sentrecord);
  gdata_print_number("%" LLPRINTFMT "d", totalsent);
  gdata_print_long(totaluptime);
  gdata_print_int(exiting);
  gdata_print_int(crashing);
  
  for (ii=0; ii<XDCC_SENT_SIZE; ii++)
    {
      gdata_print_ulong_array(xdccsent);
    }
  for (ii=0; ii<XDCC_SENT_SIZE; ii++)
    {
      gdata_print_ulong_array(xdccrecv);
    }
  for (ii=0; ii<XDCC_SENT_SIZE; ii++)
    {
      gdata_print_ulong_array(xdccsum);
    }
  
  gdata_print_int(ignore);
  gdata_print_number_cast("%ld", noautosave, long);
  gdata_print_number_cast("%ld", nonewcons, long);
  gdata_print_number_cast("%ld", nolisting, long);
  gdata_print_int(needsrehash);
  gdata_print_int(needsshutdown);
  gdata_print_int(needsswitch);
  gdata_print_int(needsreap);
  gdata_print_int(delayedshutdown);
  gdata_print_int(cursendptr);
  gdata_print_int(next_tr_id);
  gdata_print_number("%" LLPRINTFMT "d", max_file_size);

  gdata_print_uint(maxtrans);
  gdata_print_uint(max_fds_from_rlimit);

  gdata_print_int(logfd);

  /* sendbuff context_log context_cur_ptr */

  gdata_irlist_iter_start(ignorelist, igninfo);
  dump_line("  : hostmask=%s flags=%d bucket=%ld lastcontact=%ld",
            iter->hostmask,
            iter->flags,
            iter->bucket,
            (long)iter->lastcontact);
  gdata_irlist_iter_end;
  
  gdata_irlist_iter_start(xdccs, xdcc);
  gdata_iter_print_string(file);
  gdata_iter_print_string(desc);
  gdata_iter_print_string(note);
  gdata_iter_print_string(trigger);
  gdata_iter_print_number_cast("%d", mtime, int);
  gdata_iter_print_number_cast("%d", st_size, int);
  gdata_iter_print_number_cast("%d", st_dev, int);
  gdata_iter_print_number_cast("%d", st_ino, int);
  gdata_iter_print_number_cast("%d", xtime, int);
  dump_line("  : ptr=%p gets=%d minspeed=%.1f maxspeed=%.1f st_size=%" LLPRINTFMT "d",
          iter,
          iter->gets,
          iter->minspeed,
          iter->maxspeed,
          iter->st_size);
  /* st_dev st_ino */
  dump_line(
          "  : fd=%d fd_count=%d fd_loc=%" LLPRINTFMT "d",
          iter->file_fd,
          iter->file_fd_count,
          iter->file_fd_location);
  dump_line(
          "  : has_md5=%d md5sum=" MD5_PRINT_FMT,
          iter->has_md5sum, MD5_PRINT_DATA(iter->md5sum));
  dump_line("  : crc32=" CRC32_PRINT_FMT, iter->crc32);
  gdata_iter_print_uint(announce);
  gdata_iter_print_string(group);
  gdata_iter_print_string(group_desc);
  gdata_iter_print_string(lock);
  dump_line(
          "  : dlimit max=%d used=%d",
          iter->dlimit_max, iter->dlimit_used);
  gdata_iter_print_string(dlimit_desc);
#ifdef HAVE_MMAP
  {
    mmap_info_t *iter2;
    dump_line("  : mmaps:");
    for(iter2 = irlist_get_head(&iter->mmaps);
        iter2;
        iter2 = irlist_get_next(iter2))
      {
        dump_line(
                "  : ptr=%p ref_count=%d mmap_ptr=%p mmap_offset=0x%.8" LLPRINTFMT "X mmap_size=0x%.8" LLPRINTFMT "X",
                iter2,
                iter2->ref_count,
                iter2->mmap_ptr,
                (ir_uint64)(iter2->mmap_offset),
                (ir_uint64)(iter2->mmap_size));
      }
  }
#endif
  gdata_irlist_iter_end;
  
  gdata_irlist_iter_start(mainqueue, ir_pqueue);
  gdata_iter_print_string(nick);
  gdata_iter_print_string(hostname);
  dump_line(
          "  : xpack=%p queuedtime=%ld",
          iter->xpack,
          (long)iter->queuedtime);
  dump_line("  : restrictsend_bad=%ld" , (long)iter->restrictsend_bad );
  dump_line("  : net=%d", iter->net + 1 );
  gdata_irlist_iter_end;
  
  gdata_irlist_iter_start(idlequeue, ir_pqueue);
  gdata_iter_print_string(nick);
  gdata_iter_print_string(hostname);
  dump_line(
          "  : xpack=%p queuedtime=%ld",
          iter->xpack,
          (long)iter->queuedtime);
  dump_line("  : restrictsend_bad=%ld" , (long)iter->restrictsend_bad );
  dump_line("  : net=%d", iter->net + 1 );
  gdata_irlist_iter_end;
  
  gdata_irlist_iter_start(trans, transfer);
  dump_line(
          "  : listen=%d client=%d id=%d",
          iter->con.listensocket,
          iter->con.clientsocket,
          iter->id);
  dump_line("  : net=%d", iter->net + 1 );
  dump_line(
          "  : sent=%" LLPRINTFMT "d got=%" LLPRINTFMT "d lastack=%" LLPRINTFMT "d curack=%" LLPRINTFMT "d resume=%" LLPRINTFMT "d speedamt=%" LLPRINTFMT "d tx_bucket=%li",
          iter->bytessent,
          iter->bytesgot,
          iter->lastack,
          iter->curack,
          iter->startresume,
          iter->lastspeedamt,
          iter->tx_bucket);
  dump_line(
          "  : lastcontact=%ld connecttime=%ld lastspeed=%.1f pack=%p",
          (long)iter->con.lastcontact,
          (long)iter->con.connecttime,
          iter->lastspeed,
          iter->xpack);
  dump_line(
          "  : listenport=%d local%s remote%s",
          iter->con.localport,
          iter->con.localaddr,
          iter->con.remoteaddr);
  dump_line("  : restrictsend_bad=%ld" , (long)iter->restrictsend_bad );
#ifdef HAVE_MMAP
  dump_line("  : mmap_info=%p", iter->mmap_info);
#endif
  /* severaddress */
  gdata_iter_print_string(nick);
  gdata_iter_print_string(caps_nick);
  gdata_iter_print_string(hostname);
  dump_line(
          "  : nomin=%d nomax=%d reminded=%d overlimit=%d unlimited=%d tr_status=%d",
          iter->nomin,
          iter->nomax,
          iter->reminded,
          iter->overlimit,
          iter->unlimited,
          iter->tr_status);
  gdata_irlist_iter_end;
  
  gdata_irlist_iter_start(uploads, upload);
  dump_line(
          "  : client=%d file=%d ul_status=%d",
          iter->con.clientsocket,
          iter->filedescriptor,
          iter->ul_status);
  dump_line(
          "  : got=%" LLPRINTFMT "d totalsize=%" LLPRINTFMT "d resume=%" LLPRINTFMT "d speedamt=%" LLPRINTFMT "d",
          iter->bytesgot,
          iter->totalsize,
          iter->resumesize,
          iter->lastspeedamt);
  dump_line(
          "  : lastcontact=%ld connecttime=%ld lastspeed=%.1f",
          (long)iter->con.lastcontact,
          (long)iter->con.connecttime,
          iter->lastspeed);
  dump_line(
          "  : localport=%d remoteport=%d remote=%s",
          iter->con.localport,
          iter->con.remoteport,
          iter->con.remoteaddr);
  dump_line("  : net=%d", iter->net + 1 );
  dump_line("  : token=%d", iter->token);
  gdata_iter_print_string(nick);
  gdata_iter_print_string(hostname);
  gdata_iter_print_string(file);
  gdata_irlist_iter_end;

  gdata_irlist_iter_start(listen_ports, ir_listen_port_item_t);
  gdata_iter_print_number_cast("%hu",port,unsigned short int);
  gdata_iter_print_number_cast("%u",listen_time,unsigned int);
  gdata_irlist_iter_end;
  
  gdata_print_number("%p", md5build.xpack);
  gdata_print_int(md5build.file_fd);
  
  /* meminfo */
  
#if !defined(NO_CHROOT)
  gdata_print_string(chrootdir);
#endif
  
#if !defined(NO_SETUID)
  gdata_print_string(runasuser);
#endif

  dump_line("GDATA DUMP END");
  
}


void clearmemberlist(channel_t *c)
{
  member_t *member;
  
  /* clear members list */
  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "clearing %s",c->name);
    }
  
  for (member = irlist_get_head(&c->members);
       member;
       member = irlist_delete(&c->members, member))
    {
      mydelete(member->nick);
    }
  return;
}


int isinmemberlist(const char *nick)
{
  member_t *member;
  channel_t *ch;
  
  updatecontext();

  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "checking for %s",nick);
    }
  
  ch = irlist_get_head(&(gnetwork->channels));
  while(ch)
    {
      member = irlist_get_head(&ch->members);
      while(member)
        {
          if (!strcasecmp(caps(member->nick),nick))
            {
              if (check_level( member->prefixes[0] ) != 0)
                return 1;
            }
          member = irlist_get_next(member);
        }
      ch = irlist_get_next(ch);
    }
  
  return 0;
}


void addtomemberlist(channel_t *c, const char *nick)
{
  member_t *member;
  char prefixes[MAX_PREFIX];
  
  updatecontext();

  bzero(prefixes, sizeof(prefixes));
  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "adding %s to %s",nick,c->name);
    }
  
  /* find any prefixes */
 more:
  if (*nick)
    {
      unsigned int pi;
      for (pi = 0; ((pi < MAX_PREFIX) && gnetwork->prefixes[pi].p_symbol); pi++)
        {
          if (*nick == gnetwork->prefixes[pi].p_symbol)
            {
              for (pi = 0;
                   ((pi < MAX_PREFIX) && prefixes[pi] && (prefixes[pi] != *nick));
                   pi++) ;
              if (pi < MAX_PREFIX)
                {
                  prefixes[pi] = *nick;
                }
              nick++;
              goto more;
            }
        }
    }
  
  /* is in list for this channel already? */
  member = irlist_get_head(&c->members);
  while(member)
    {
      if (!strcasecmp(member->nick,nick))
        {
          break;
        }
      member = irlist_get_next(member);
    }
  
  if (!member)
    {
      /* add it */
      member = irlist_add(&c->members, sizeof(member_t));
      member->nick = mystrdup(nick);
      memcpy(member->prefixes, prefixes, sizeof(member->prefixes));
    }
  
  return;
}

void removefrommemberlist(channel_t *c, const char *nick)
{
  member_t *member;
  
  updatecontext();
  
  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "removing %s from %s",nick,c->name);
      ioutput(OUT_L, COLOR_NO_COLOR,
              "removing %s from %s", nick, c->name);
    }
  
  /* is in list for this channel? */
  member = irlist_get_head(&c->members);
  while(member)
    {
      if (!strcasecmp(member->nick,nick))
        {
          mydelete(member->nick);
          irlist_delete(&c->members, member);
          return;
        }
      member = irlist_get_next(member);
    }
  
  /* not found */
  return;
}


void changeinmemberlist_mode(channel_t *c, const char *nick, int mode, unsigned int add)
{
  member_t *member;
  unsigned int pi;
  
  updatecontext();
  
  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "%s prefix %c on %s in %s",
              add ? "adding" : "removing",
              mode, nick, c->name);
    }
  
  /* is in list for this channel? */
  member = irlist_get_head(&c->members);
  while(member)
    {
      if (!strcasecmp(member->nick,nick))
        {
          break;
        }
      member = irlist_get_next(member);
    }
  
  if (member)
    {
      if (add)
        {
          for (pi = 0;
               (pi < MAX_PREFIX && member->prefixes[pi] && member->prefixes[pi] != mode);
               pi++) ;
          if (pi < MAX_PREFIX)
            {
              member->prefixes[pi] = mode;
            }
        }
      else
        {
          for (pi = 0; (pi < MAX_PREFIX && member->prefixes[pi]); pi++)
            {
              if (member->prefixes[pi] == mode)
                {
                  for (pi++; pi < MAX_PREFIX; pi++)
                    {
                      member->prefixes[pi-1] = member->prefixes[pi];
                    }
                  member->prefixes[MAX_PREFIX-1] = '\0';
                  break;
                }
            }
        }
      
    }
  
  return;
}


void changeinmemberlist_nick(channel_t *c, const char *oldnick, const char *newnick)
{
  member_t *oldmember;
  
  updatecontext();
  
  if (gdata.debug > 10)
    {
      ioutput(OUT_S, COLOR_NO_COLOR,
              "changing %s to %s in %s",oldnick,newnick,c->name);
    }
  
  /* find old in list for this channel  */
  oldmember = irlist_get_head(&c->members);
  while(oldmember)
    {
      if (!strcasecmp(oldmember->nick,oldnick))
        {
          mydelete(oldmember->nick);
          oldmember->nick = mystrdup(newnick);
          return;
        }
      oldmember = irlist_get_next(oldmember);
    }
}

int set_socket_nonblocking (int s, int nonblock)
{
  long current;
  
  current = fcntl(s, F_GETFL, 0);
  
  if (current == -1)
    {
      return -1;
    }
  
  if (nonblock)
    return fcntl(s, F_SETFL, current | O_NONBLOCK);
  else
    return fcntl(s, F_SETFL, current & ~O_NONBLOCK);
}

void set_loginname(void)
{
  struct passwd *p;
  
  p = getpwuid(geteuid());
  if (p == NULL || p->pw_name == NULL)
    {
#if !defined(NO_SETUID)
      if (gdata.runasuser)
        {
          gdata.loginname = mystrdup(gdata.runasuser);
        }
      else
#endif
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Get username, specify loginname in config file");
          gdata.loginname = mystrdup("UNKNOWN");
        }
    }
  else
    {
      gdata.loginname = mystrdup(p->pw_name);
    }
  
}


int is_fd_readable(int fd)
{
  int ret;
  fd_set readfds;
  struct timeval timeout;
  
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);
  
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  
  ret = select(fd+1, &readfds, NULL, NULL, &timeout);
  
  if (ret < 0)
    {
      return 0;
    }
  
  if (FD_ISSET(fd, &readfds))
    {
      return 1;
    }
  
  return 0;
}

int is_fd_writeable(int fd)
{
  int ret;
  fd_set writefds;
  struct timeval timeout;
  
  FD_ZERO(&writefds);
  FD_SET(fd, &writefds);
  
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  
  ret = select(fd+1, NULL, &writefds, NULL, &timeout);
  
  if (ret < 0)
    {
      return 0;
    }
  
  if (FD_ISSET(fd, &writefds))
    {
      return 1;
    }
  
  return 0;
}



#ifdef NO_SNPRINTF

int snprintf(char *str, size_t n, const char *format, ... )
{
  va_list args;
  int actlen;
  char mysnprintf_buff[1024*10];   
  
  va_start(args, format);
  actlen = vsprintf(mysnprintf_buff,format,args);
  va_end(args);
  
  strncpy(str,mysnprintf_buff,min2(n-1,actlen));
  str[min2(n-1,actlen)] = '\0';
  
  return min2(n-1,actlen);
}

int vsnprintf(char *str, size_t n, const char *format, va_list ap )
{
  int actlen;
  char mysnprintf_buff[1024*10];   
  
  actlen = vsprintf(mysnprintf_buff,format,ap);
  
  strncpy(str,mysnprintf_buff,min2(n-1,actlen));
  str[min2(n-1,actlen)] = '\0';
  
  return min2(n-1,actlen);
}

#endif

#ifdef NO_STRCASECMP
#include <ctype.h>
int strcasecmp(const char *s1, const char *s2)
{
  while (*s1 && (tolower(*s1) == tolower(*s2)))
    {
      s1++;
      s2++;
    }
  
  return tolower(*s1) - tolower(*s2);
}
#endif

#ifdef NO_STRSIGNAL
const char *strsignal(int sig)
{
  switch (sig)
    {
    case SIGBUS:
      return "sigbus";
    case SIGABRT:
      return "sigabrt";
    case SIGILL:
      return "sigill";
    case SIGFPE:
      return "sigfpe";
    case SIGSEGV:
      return "sigsegv";
    case SIGTERM:
      return "sigterm";
    case SIGINT:
      return "sigint";
    case SIGUSR1:
      return "sigusr1";
    case SIGUSR2:
      return "sigusr2";
    default:
      return "unknown";
    }
}
#endif


char* convert_to_unix_slash(char *ss)
{
  int ii;
  
  for (ii=0; ; ii++)
    {
      if (ss[ii] == '\\')
        {
          ss[ii] = '/';
        }
      else if (ss[ii] == '\0')
        {
          return ss;
        }
    }
}

#define IRLIST_INT_TO_EXT(p) ((irlist_item_t *)p + 1)
#define IRLIST_INT_TO_EXT_CONST(p) ((const irlist_item_t *)p + 1)
#define IRLIST_EXT_TO_INT(p) ((irlist_item_t *)p - 1)
#define IRLIST_EXT_TO_INT_CONST(p) ((const irlist_item_t *)p - 1)

#ifndef WITHOUT_MEMSAVE
void* irlist_add2(irlist_t *list, size_t size,
                  const char *src_function, const char *src_file, unsigned int src_line)
#else /* WITHOUT_MEMSAVE */
void* irlist_add(irlist_t *list, size_t size)
#endif /* WITHOUT_MEMSAVE */
{
  irlist_item_t *iitem;
  
  updatecontext();
  
#ifndef WITHOUT_MEMSAVE
  iitem = mymalloc2(sizeof(irlist_item_t) + size, 1,
                    src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
  iitem = mycalloc(sizeof(irlist_item_t) + size);
#endif /* WITHOUT_MEMSAVE */
  
  irlist_insert_tail(list, IRLIST_INT_TO_EXT(iitem));
  
  return IRLIST_INT_TO_EXT(iitem);
}

void irlist_insert_head(irlist_t *list, void *item)
{
  irlist_item_t *iitem = IRLIST_EXT_TO_INT(item);
  
  updatecontext();
  
  assert(!iitem->next);
  
  if (!list->size)
    {
      assert(!list->head);
      assert(!list->tail);
      
      list->tail = iitem;
    }
  else
    {
      assert(list->head);
      assert(list->tail);
      assert(!list->tail->next);
    }
  
  iitem->next = list->head;
  list->head = iitem;
  
  list->size++;
  
  return;
}

static void *irlist_loop_match(const irlist_t *list, irlist_item_t *match) 
{
  irlist_item_t *tail;
  irlist_item_t *test;

  tail = NULL;
  test = list->head;
  while (test != NULL) {
    if (test == match)
      return tail;
    tail = test;
    test = tail->next;
  }
  return NULL;
}

void irlist_insert_tail(irlist_t *list, void *item)
{
  irlist_item_t *iitem = IRLIST_EXT_TO_INT(item);
  
  updatecontext();
  
  assert(iitem);
  assert(!iitem->next);
  
  if (!list->size)
    {
      assert(!list->head);
      assert(!list->tail);
      
      list->head = iitem;
    }
  else
    {
      assert(list->head);
      assert(list->tail);
      assert(!list->tail->next);
      list->tail->next = iitem;
    }
  
  iitem->next = NULL;
  list->tail = iitem;
  
  list->size++;
  
  return;
}

void irlist_insert_after(irlist_t *list, void *item, void *after_this)
{
  irlist_item_t *iitem = IRLIST_EXT_TO_INT(item);
  irlist_item_t *iafter = IRLIST_EXT_TO_INT(after_this);
  
  updatecontext();
  
  assert(list->size > 0);
  assert(!iitem->next);
  
  if (iafter->next)
    {
      iitem->next = iafter->next;
    }
  else
    {
      assert(list->tail == iafter);
      list->tail = iitem;
    }
  
  iafter->next = iitem;
  
  list->size++;
  
  return;
}


void* irlist_delete(irlist_t *list, void *item)
{
  irlist_item_t *iitem = IRLIST_EXT_TO_INT(item);
  void *retval;
  
  updatecontext();
  
  retval = irlist_remove(list, item);
  
  mydelete(iitem);
  
  return retval;
}


void* irlist_remove(irlist_t *list, void *item)
{
  irlist_item_t *iitem = IRLIST_EXT_TO_INT(item);
  irlist_item_t *next;
  irlist_item_t *tail;
  
  updatecontext();
  
  assert(list->size > 0);
  assert(list->head);
  assert(list->tail);
  
  next = iitem->next;
  
  if (list->head == iitem)
    {
      list->head = next;
      if (list->tail == iitem)
        {
          assert(!next);
          list->tail = NULL;
        }
    }
  else
    {
      tail = irlist_loop_match(list, iitem);
      assert(tail);
      assert(tail->next == iitem);
      tail->next = next;
      if (list->tail == iitem)
        {
          assert(!next);
          list->tail = tail;
        }
    }
  
  iitem->next = NULL;
  
  list->size--;
  assert(list->size >= 0);
  
  if (next)
    {
      return IRLIST_INT_TO_EXT(next);
    }
  else
    {
      return NULL;
    }
}


void irlist_delete_all(irlist_t *list)
{
  void *cur;
  
  updatecontext();
  
  for (cur = irlist_get_head(list); cur; cur = irlist_delete(list, cur));
  
  assert(list->size == 0);
  assert(!list->head);
  assert(!list->tail);
  
  return;
}


void* irlist_get_head(const irlist_t *list)
{
  updatecontext();
  
  if (list->head)
    {
      assert(list->size > 0);
      assert(list->tail);
      return IRLIST_INT_TO_EXT(list->head);
    }
  else
    {
      assert(list->size == 0);
      assert(!list->tail);
      return NULL;
    }
}

void* irlist_get_tail(const irlist_t *list)
{
  updatecontext();
  
  if (list->tail)
    {
      assert(list->size > 0);
      assert(list->head);
      return IRLIST_INT_TO_EXT(list->tail);
    }
  else
    {
      assert(list->size == 0);
      assert(!list->head);
      return NULL;
    }
}


void* irlist_get_next(const void *cur)
{
  const irlist_item_t *iitem = IRLIST_EXT_TO_INT_CONST(cur);
  
  if (iitem->next)
    {
      return IRLIST_INT_TO_EXT(iitem->next);
    }
  else
    {
      return NULL;
    }
}


unsigned int irlist_size(const irlist_t *list)
{
  updatecontext();
  
  assert(list->size >= 0);
  
  return list->size;
}

void* irlist_get_nth(irlist_t *list, unsigned int nth)
{
  irlist_item_t *iitem;
  
  updatecontext();
  
  for (iitem = list->head; (iitem && nth--); iitem = iitem->next) ;
  
  if (iitem)
    {
      return IRLIST_INT_TO_EXT(iitem);
    }
  else
    {
      return NULL;
    }
}

int irlist_sort_cmpfunc_string(const void *a, const void *b)
{
  return strcmp((const char *)a, (const char *)b);
}

int irlist_sort_cmpfunc_off_t(const void *a, const void *b)
{
  off_t ai, bi;
  ai = *(const off_t*)a;
  bi = *(const off_t*)b;
  return ai - bi;
}

transfer* does_tr_id_exist(unsigned int tr_id)
{
  transfer *tr;
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      if (tr->id == tr_id)
        {
          return tr;
        }
      tr = irlist_get_next(tr);
    }
  
  return NULL;
}

unsigned int get_next_tr_id(void)
{
  transfer *tr;
  
  while(1)
    {
      ++(gdata.next_tr_id);
      gdata.next_tr_id %= max2((gdata.maxtrans * 3) / 2, 1000);
      gdata.next_tr_id = max2(1,gdata.next_tr_id);
      tr = does_tr_id_exist(gdata.next_tr_id);
      if (!tr)
        {
          return gdata.next_tr_id;
        }
    }
}

void ir_listen_port_connected(ir_uint16 port)
{
  ir_listen_port_item_t *lp;
  
  lp = irlist_get_head(&gdata.listen_ports);
  while(lp)
    {
      if (lp->port == port)
        {
          if (gdata.debug > 0)
            {
              ioutput(OUT_S, COLOR_YELLOW,
                      "listen complete port %d", port);
            }
          lp = irlist_delete(&gdata.listen_ports, lp);
        }
      else
        {
          lp = irlist_get_next(lp);
        }
    }
  return;
}

static int ir_listen_port_is_in_list(ir_uint16 port)
{
  int retval = 0;
  ir_listen_port_item_t *lp;
  
  lp = irlist_get_head(&gdata.listen_ports);
  while(lp)
    {
      if ((lp->listen_time + LISTEN_PORT_REUSE_TIME) < gdata.curtime)
        {
          if (gdata.debug > 0)
            {
              ioutput(OUT_S, COLOR_YELLOW,
                      "listen expire port %d", lp->port);
            }
          lp = irlist_delete(&gdata.listen_ports, lp);
        }
      else
        {
          if (lp->port == port)
            {
              retval = 1;
            }
          lp = irlist_get_next(lp);
        }
    }
  return retval;
}

int ir_bind_listen_socket(int fd, ir_sockaddr_union_t *sa)
{
  ir_listen_port_item_t *lp;
  int retry;
  int max;
  ir_uint16 port;
  SIGNEDSOCK int addrlen;
  
  max = (gdata.maxtrans + irlist_size(&gdata.listen_ports));
  addrlen = (sa->sa.sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  
  for (retry = 0; retry < max; retry++)
    {
      if (gdata.tcprangestart)
        {
          port = gdata.tcprangestart + retry;
          if ((unsigned int)port > gdata.tcprangelimit)
            {
              /* give up */
              retry = max;
              break;
            }

          if (ir_listen_port_is_in_list(port))
            {
              continue;
            }
          
          sa->sin.sin_port = htons(port);
        }
      else
        {
          sa->sin.sin_port = htons(0);
        }
      
      if (bind(fd, &(sa->sa), addrlen) < 0)
        {
          if (!gdata.tcprangestart)
            {
              /* give up */
              retry = max;
              break;
            }
        }
      else
        {
          break;
        }
    }

  if (retry == max)
    {
      return -1;
    }
  
  if ((getsockname(fd, &(sa->sa), &addrlen)) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't get Port Number, Aborting");
      return -1;
    }
  
  port = get_port(sa);
  if (gdata.debug > 0)
    {
      ioutput(OUT_S, COLOR_YELLOW,
              "listen got port %d", port);
    }
  
  lp = irlist_add(&gdata.listen_ports,sizeof(ir_listen_port_item_t));
  lp->port = port;
  lp->listen_time = gdata.curtime;
  
  return 0;
}


int ir_boutput_write(ir_boutput_t *bout, const void *buffer, int buffer_len)
{
  ir_boutput_segment_t *segment;
  int cur;
  int len;
  int all;
  const unsigned char *buffer_c = (const unsigned char*)buffer;
  
  cur = 0;
  
 begin_again:
  
  segment = irlist_get_tail(&bout->segments);
  
  if (!segment)
    {
      /* first */
      segment = irlist_add(&bout->segments, sizeof(ir_boutput_segment_t));
    }
  
  while (cur < buffer_len)
    {
      assert(segment->begin <= segment->end);
      assert(segment->begin <= IR_BOUTPUT_SEGMENT_SIZE);
      assert(segment->end <= IR_BOUTPUT_SEGMENT_SIZE);
      
      if (segment->end == IR_BOUTPUT_SEGMENT_SIZE)
        {
          /* segment is full */
          if ((bout->flags & BOUTPUT_NO_LIMIT) ||
              (irlist_size(&bout->segments) < IR_BOUTPUT_MAX_SEGMENTS))
            {
              segment = irlist_add(&bout->segments, sizeof(ir_boutput_segment_t));
            }
          else
            {
              /* too much data in buffer, attempt flush */
              if (ir_boutput_attempt_flush(bout) > 0)
                {
                  /* flush wrote something so start over fresh */
                  goto begin_again;
                }
              
              /* unable to flush, drop characters */
              bout->count_dropped += buffer_len - cur;
              return cur;
            }
        }
      
      all = IR_BOUTPUT_SEGMENT_SIZE - segment->end;
      len = min2(all, buffer_len - cur);
      
      memcpy(segment->buffer+segment->end, buffer_c+cur, len);
      
      cur += len;
      segment->end += len;
      bout->count_written += len;
    }
  
  ir_boutput_attempt_flush(bout);
  return buffer_len;
}

int ir_boutput_attempt_flush(ir_boutput_t *bout)
{
  ir_boutput_segment_t *segment;
  int count = 0;
  
  while ((segment = irlist_get_head(&bout->segments)))
    {
      int retval;
      
      assert(segment->begin <= segment->end);
      assert(segment->begin <= IR_BOUTPUT_SEGMENT_SIZE);
      assert(segment->end <= IR_BOUTPUT_SEGMENT_SIZE);
      
      retval = write(bout->fd,
                     segment->buffer + segment->begin,
                     segment->end - segment->begin);
      
      if ((retval < 0) && (errno != EAGAIN))
        {
          /* write failure */
          count = -1;
          break;
        }
      else if (retval < 0)
        {
          /* EAGAIN, that's all for now */
          break;
        }
      else
        {
          segment->begin += retval;
          count += retval;
          bout->count_flushed += retval;
        }
      
      assert(segment->begin <= segment->end);
      if (segment->begin == segment->end)
        {
          irlist_delete(&bout->segments, segment);
        }
    }
  
  return count;
}

void ir_boutput_init(ir_boutput_t *bout, int fd, unsigned int flags)
{
  memset(bout, 0, sizeof(*bout));
  bout->fd = fd;
  bout->flags = flags;
  return;
}

void ir_boutput_delete(ir_boutput_t *bout)
{
  irlist_delete_all(&bout->segments);
  memset(bout, 0, sizeof(*bout));
  return;
}

const char *transferlimit_type_to_string(transferlimit_type_e type)
{
  switch (type)
    {
      case TRANSFERLIMIT_DAILY:
        return "daily";
      case TRANSFERLIMIT_WEEKLY:
        return "weekly";
      case TRANSFERLIMIT_MONTHLY:
        return "monthly";
      case NUMBER_TRANSFERLIMITS:
      default:
        return "unknown";
    }
}

/* End of File */
