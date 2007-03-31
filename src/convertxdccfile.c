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
 * @(#) convertxdccfile.c 1.4@(#)
 * pmg@wellington.i202.centerclick.org|src/convertxdccfile.c|20050116225130|56710
 * 
 */

/* include the headers */
#define GEX 
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"

/* local functions */

static void usage(const char *progname)
{
  fprintf(stderr, "Usage: %s [-v] [-g] -x mybot.xdcc -s mybot.state\n", progname);
  exit(1);
}

int splizzer;

struct list_s {
  struct list_s *l_next;
  char *l_data;
} *seen_list;

static struct list_s *seen_init(const char *data)
{
   struct list_s *new;

   new = malloc(sizeof(struct list_s));
   if (new == NULL)
      return NULL;
   new->l_next = NULL;
   new->l_data = (data != NULL) ? strdup(data) : NULL;
   return new;
}

static int seen(const char *data)
{
   struct list_s *old;
   struct list_s *new;

   if (seen_list == NULL) {
      new = seen_init(data);
      seen_list = new;
      return 0;
      }
   old = seen_list;
   while (old->l_next != NULL) {
      if (old->l_data != NULL) {
         if (strcasecmp(old->l_data,data) == 0)
            return 1;
         }
      old = old->l_next;
   }

   if (old->l_data != NULL) {
      if (strcasecmp(old->l_data,data) == 0)
         return 1;
      }
   new = seen_init(data);
   old->l_next = new;
   return 0;
}

static void getxdccconfig(const char *filename) {
   char *templine1 = mycalloc(maxtextlength);
   char *templine2 = mycalloc(maxtextlength);
   char *templine3 = mycalloc(maxtextlength);
   char *templine4 = mycalloc(maxtextlength);
   char *templine5 = mycalloc(maxtextlength);
   char *templine6 = mycalloc(maxtextlength);
   char *templine7 = mycalloc(maxtextlength);
   char *templine8 = mycalloc(maxtextlength);
   char *templine9 = mycalloc(maxtextlength);
   char *msg;
   int ok,i;
   int filedescriptor,xfiledescriptor;
   struct stat st;
   xdcc *xd;
   
   updatecontext();

   printf("** Converting %s ... \n",filename);
   
   filedescriptor=open(filename, O_RDONLY | O_CREAT | ADDED_OPEN_FLAGS, CREAT_PERMISSIONS);
   if (filedescriptor < 0)
      outerror(OUTERROR_TYPE_CRASH,"Cant Access XDCC File '%s': %s",filename,strerror(errno));
   
   if (getfline(templine1,maxtextlength,filedescriptor,0) != NULL) {            
      ok = 0;
      
      msg = getpart(templine1,6);
      if (msg) {
         gdata.record=atof(msg);
         mydelete(msg);
         }
      else
         ok++;
         
      msg = getpart(templine1,7);
      if (msg) {
         gdata.sentrecord=atof(msg);
         mydelete(msg);
         }
      else
         ok++;
      
      gdata.totalsent=0;
      msg = getpart(templine1,8);
      if (msg) {
         gdata.totalsent=atoull(msg);
         mydelete(msg);
         }
      else
         ok++;
      
      gdata.totaluptime=0;
      msg = getpart(templine1,9);
      if (msg) {
         gdata.totaluptime=atol(msg);
         mydelete(msg);
         }
      else
         ok++;
      
      
      if (ok) {
         outerror(OUTERROR_TYPE_WARN_LOUD,"Invalid First Line, Reverting Values to 0");
         gdata.record = gdata.sentrecord = 0.0;
         gdata.totalsent = 0;
	 gdata.totaluptime = 0;
         }
      
      /* old format */
      msg = getpart(templine1,10);
      if (msg) {
         gdata.totaluptime=atol(msg);
         mydelete(msg);
         
         msg = getpart(templine1,9);
         if (msg) {
            gdata.totalsent=atoull(msg);
            mydelete(msg);
            }
      
         }
      
      }
   else {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Empty XDCC File, Starting With No Packs Offered");
   
      close(filedescriptor);

      mydelete(templine1);
      mydelete(templine2);
      mydelete(templine3);
      mydelete(templine4);
      mydelete(templine5);
      mydelete(templine6);
      return;
      }

   ok=0;
   
   while (getfline(templine1,maxtextlength,filedescriptor,1) != NULL) {
   
      if (getfline(templine1,maxtextlength,filedescriptor,0) == NULL)  ok++;
      if (getfline(templine2,maxtextlength,filedescriptor,0) == NULL)  ok++;
      if (getfline(templine3,maxtextlength,filedescriptor,0) == NULL)  ok++;
      if (getfline(templine4,maxtextlength,filedescriptor,0) == NULL)  ok++;
      if (getfline(templine5,maxtextlength,filedescriptor,0) == NULL)  ok++;
      if (getfline(templine6,maxtextlength,filedescriptor,0) == NULL)  ok++;
      
      if (splizzer != 0) {
         if (getfline(templine7,maxtextlength,filedescriptor,0) == NULL)  ok++;
         if (getfline(templine8,maxtextlength,filedescriptor,0) == NULL)  ok++;
         if (getfline(templine9,maxtextlength,filedescriptor,0) == NULL)  ok++;
         }

      if (ok) {
         printf("** ERROR: to read a XDCC file with group definitions use '-g'\n");
         outerror(OUTERROR_TYPE_CRASH,"XDCC file syntax error (missing/extra line)");
         }
      
      for (i=strlen(templine1)-1; i>7 && templine1[i] == ' '; i--)
         templine1[i] = '\0';
      for (i=strlen(templine2)-1; i>7 && templine2[i] == ' '; i--)
         templine2[i] = '\0';
      for (i=strlen(templine3)-1; i>7 && templine3[i] == ' '; i--)
         templine3[i] = '\0';
      for (i=strlen(templine4)-1; i>7 && templine4[i] == ' '; i--)
         templine4[i] = '\0';
      for (i=strlen(templine5)-1; i>7 && templine5[i] == ' '; i--)
         templine5[i] = '\0';
      for (i=strlen(templine6)-1; i>7 && templine6[i] == ' '; i--)
         templine6[i] = '\0';
      for (i=strlen(templine7)-1; i>7 && templine7[i] == ' '; i--)
         templine7[i] = '\0';
      for (i=strlen(templine8)-1; i>7 && templine8[i] == ' '; i--)
         templine8[i] = '\0';
      for (i=strlen(templine9)-1; i>7 && templine9[i] == ' '; i--)
         templine9[i] = '\0';

      if ((strlen(templine1) < 8) || strncmp(templine1+3,"file ",5)) ok++;
      if ((strlen(templine2) < 8) || strncmp(templine2+3,"desc ",5)) ok++;
      if ((strlen(templine3) < 8) || strncmp(templine3+3,"note ",5)) ok++;
      if ((strlen(templine4) < 8) || strncmp(templine4+3,"gets ",5)) ok++;
      if ((strlen(templine5) < 8) || strncmp(templine5+3,"mins ",5)) ok++;
      if ((strlen(templine6) < 8) || strncmp(templine6+3,"maxs ",5)) ok++;
      if (splizzer != 0) {
         if ((strlen(templine7) < 8) || strncmp(templine7+3,"data ",5)) ok++;
         if ((strlen(templine8) < 8) || strncmp(templine8+3,"trig ",5)) ok++;
         if ((strlen(templine9) < 8) || strncmp(templine9+3,"trno ",5)) ok++;
         }
      if (ok)
         printf("error=%d, near file=%s\n", ok, templine1);

      if (ok)
         outerror(OUTERROR_TYPE_CRASH,"XDCC file syntax error (incorrect order?)");
      
      xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
      
      xd->file = mystrdup(templine1+8);
      convert_to_unix_slash(xd->file);

      xd->desc = mystrdup(templine2+8);
      
      xd->note = mystrdup(templine3+8);
      
      xd->gets     = atoi(templine4+8);
      
      xd->group = NULL;
      xd->group_desc = NULL;
      if (splizzer != 0) {
         xd->group = mystrdup(templine7+8);
         if (seen(templine9+8) == 0) {
            xd->group_desc = mystrdup(templine9+8);
            }
         }

      xd->minspeed = gdata.transferminspeed;
      if ( atof(templine5+8) > 0)
         xd->minspeed = atof(templine5+8);
            
      xd->maxspeed = gdata.transfermaxspeed;
      if ( atof(templine6+8) )
         xd->maxspeed = atof(templine6+8);
      
      xfiledescriptor=open(xd->file, O_RDONLY | ADDED_OPEN_FLAGS);
      if (xfiledescriptor < 0)
        {
          outerror(OUTERROR_TYPE_WARN,"Cant Access Offered File '%s': %s",
                   xd->file,strerror(errno));
          memset(&st,0,sizeof(st));
        }
      else if (fstat(xfiledescriptor,&st) < 0)
        {
          outerror(OUTERROR_TYPE_WARN,"Cant Access Offered File Details '%s': %s",
                   xd->file,strerror(errno));
          memset(&st,0,sizeof(st));
        }
      
      xd->st_size  = st.st_size;
      xd->st_dev   = st.st_dev;
      xd->st_ino   = st.st_ino;
      xd->mtime    = st.st_mtime;
      
      if ( xd->st_size == 0 )
        {
          outerror(OUTERROR_TYPE_WARN,"The file \"%s\" has size of 0 bytes!",
                   xd->file);
        }

      if ( xd->st_size > gdata.max_file_size )
        {
          outerror(OUTERROR_TYPE_CRASH,"The file \"%s\" is too large!",
                   xd->file);
        }

      if (xfiledescriptor >= 0)
        {
          close(xfiledescriptor);
        }
      }
   
   if (!gdata.totalsent)
     {     
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           gdata.totalsent += ((unsigned long long)xd->gets)*((unsigned long long)xd->st_size);
           xd = irlist_get_next(xd);
         }
     }
   
   close(filedescriptor);
   
   mydelete(templine1);
   mydelete(templine2);
   mydelete(templine3);
   mydelete(templine4);
   mydelete(templine5);
   mydelete(templine6);
   mydelete(templine6);
   mydelete(templine7);
   mydelete(templine8);
   mydelete(templine9);

   }


/* main */
int main(int argc, char *argv[])
{
  const char *xdccfile;
  int callval_i;
  
  memset(&gdata, 0, sizeof(gdata_t));
  xdccfile = NULL;
  gdata.nocolor = 1;
  gdata.noscreen = 1;
  gdata.debug = 1;
  splizzer = 0;
  seen_list = NULL;
  
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
  
  while ((callval_i = getopt(argc, argv, "vgx:s:")) >= 0)
    {
      switch (callval_i)
        {
        case 'v':
          gdata.debug++;
          break;

        case 'g':
          splizzer++;
          break;
          
        case 'x':
          xdccfile = optarg;
          break;
          
        case 's':
          gdata.statefile = optarg;
          break;
          
        case ':':
        case '?':
        default:
          usage(argv[0]);
        }
    }
  
  if ((optind != argc) ||
      !xdccfile ||
      !gdata.statefile)
    {
      usage(argv[0]);
    }
  
  getxdccconfig(xdccfile);
  
  write_statefile();
  
  return(0);
}



/* stubs */
void tostdout_disable_buffering(int flush) { return; }
void uninitscreen(void) { return; }
void writeserver(writeserver_type_e type, const char *format, ... ) { return; }
void writedccchat(dccchat_t *chat, int add_return, const char *format, ...) { return; }
void gototop(void) { return; }

void tostdout(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}


/* End of File */







   
