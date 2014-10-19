/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2014 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
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
#include "dinoex_http.h"
#include "dinoex_config.h"
#include "dinoex_main.h"
#include "dinoex_kqueue.h"
#include "dinoex_misc.h"

#define GET_NEXT_DATA(x)	{ ++argv; --argc; x = *argv; \
				if (x == NULL) usage(); }

gnetwork_t *gnetwork;
gdata_t gdata;

static const char *ir_basename;

static void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
/* tell the admin what options we support on the commandline */
usage(void)
{
  printf("\n"
"iroffer-dinoex " VERSIONLONG ", see " "http://iroffer.dinoex.net/" "\n"
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
" [-w dir]"
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
"        -w dir    Chdir to dir as working directory.\n"
"        -i file   Import and old style mybot.xdcc file.\n"
"\n", ir_basename);
  exit(64);
}

static unsigned int add_config_file(unsigned int fc, const char *cptr)
{
  char *newptr;

  if (cptr == NULL)
    return fc;

  if (fc >= MAXCONFIG)
    usage();

  newptr = mystrdup(cptr);
  convert_to_unix_slash(newptr);
  gdata.configfile[ fc++ ] = newptr;
  return fc;
}

/* parse command line options */
void command_options(int argc, char *const *argv)
{
  const char *cptr;
  char ch;
  unsigned int fc = 0;

  gdata.argv = argv;
  ir_basename = getfilename(argv[0]);
  while (--argc > 0) {
    cptr = *(++argv);
    if (*cptr == '-') {
      ch = *(++cptr);
      switch (ch) {
      case 'b': /* go background */
        gdata.background = 1;
        break;
      case 'c': /* create password */
        add_config_file(fc, argv[1]);
        createpassword();
        exit(0);
      case 'd': /* increase debug */
        ++(gdata.debug);
        break;
      case 'i': /* import xdccfile */
        GET_NEXT_DATA(cptr);
        gdata.import = cptr;
        break;
      case 'k': /* adjust core */
        ++(gdata.adjustcore);
        break;
      case 'n': /* no color */
        ++(gdata.nocolor);
        break;
      case 's': /* no screen */
        ++(gdata.noscreen);
        break;
#if !defined(NO_CHROOT)
      case 't': /* chroot dir */
        GET_NEXT_DATA(cptr);
        gdata.chrootdir = cptr;
        break;
#endif
#if !defined(NO_SETUID)
      case 'u':
        GET_NEXT_DATA(cptr);
        gdata.runasuser = cptr;
        break;
#endif
      case 'v': /* show version */
        printf("iroffer-dinoex " VERSIONLONG FEATURES ", see " "http://iroffer.dinoex.net/" "\n");
        exit(0);
      case 'w': /* workdir */
        GET_NEXT_DATA(cptr);
        gdata.workdir = cptr;
        break;
      default:
        usage();
      }
      continue;
    }
    fc = add_config_file(fc, cptr);
  }
  if (fc == 0) {
    fprintf(stderr, "%s: no configuration file specified\n", argv[0]);
    usage();
  }
}

/* add the adminpassword to the configfile */
int add_password(const char *hash)
{
  char *line;
  size_t len;
  int fd;

  if (gdata.configfile[0] == NULL)
    return 1;

  fd = open_append(gdata.configfile[0], "Configfile");
  if (fd < 0)
    return 1;

  line = mymalloc(maxtextlength);
  len = add_snprintf(line, maxtextlength, "\n"
                     "%s %s\n" "\n", "adminpass", hash); /* NOTRANSLATE */
  write(fd, line, len);
  mydelete(line)
  close(fd);
  return 0;
}

static void outerror_sysname(const char *configured, const char *sysname)
{
  if (strcmp(sysname, configured))
    outerror(OUTERROR_TYPE_WARN_LOUD, "Configured for %s but running on %s?!?", configured, sysname);
  printf(", Good\n");
}

/* report and check operation system name */
void check_osname(const char *sysname)
{
   /* verify we are who we were configured for, and set config */
#if defined(_OS_Linux)
   outerror_sysname("Linux", sysname); /* NOTRANSLATE */
#elif defined(_OS_FreeBSD)
   outerror_sysname("FreeBSD", sysname); /* NOTRANSLATE */
#elif defined(_OS_OpenBSD)
   outerror_sysname("OpenBSD", sysname); /* NOTRANSLATE */
#elif defined(_OS_NetBSD)
   outerror_sysname("NetBSD", sysname); /* NOTRANSLATE */
#elif defined(_OS_BSDI)
   outerror_sysname("BSD/OS", sysname); /* NOTRANSLATE */
#elif defined(_OS_BSD_OS)
   outerror_sysname("BSD/OS", sysname); /* NOTRANSLATE */
#elif defined(_OS_SunOS)
   outerror_sysname("SunOS", sysname); /* NOTRANSLATE */
#elif defined(_OS_HPUX)
   outerror_sysname("HP-UX", sysname); /* NOTRANSLATE */
#elif defined(_OS_IRIX)
   outerror_sysname("IRIX", sysname); /* NOTRANSLATE */
#elif defined(_OS_IRIX64)
   outerror_sysname("IRIX64", sysname); /* NOTRANSLATE */
#elif defined(_OS_OSF1)
   outerror_sysname("OSF1", sysname); /* NOTRANSLATE */
#elif defined(_OS_Rhapsody)
   outerror_sysname("Rhapsody", sysname); /* NOTRANSLATE */
#elif defined(_OS_Darwin)
   outerror_sysname("Darwin", sysname); /* NOTRANSLATE */
#elif defined(_OS_AIX)
   outerror_sysname("AIX", sysname); /* NOTRANSLATE */
#elif defined(_OS_CYGWIN)
   if (strncmp(sysname, "CYGWIN", 6)) /* NOTRANSLATE */
     outerror(OUTERROR_TYPE_WARN_LOUD, "Configured for %s but running on %s?!?", "CYGWIN", sysname);
   printf(", Good\n");
#else
   printf(", I don't know of that!\n");
#endif
}

#ifdef DEBUG

static void free_state(void)
{
  xdcc *xd;
  channel_t *ch;
  transfer *tr;
  upload *up;
  ir_pqueue *pq;
  igninfo *i;
  msglog_t *ml;
  xlistqueue_t *user;
  dcc_options_t *dcc_options;
  http *h;
  http_magic_t *mime;
  autoadd_group_t *ag;
  unsigned int ss;

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

  for (ss = gdata.networks_online; ss--; ) {
    gnetwork = &(gdata.networks[ss]);
    mydelete(gnetwork->curserveractualname);
    mydelete(gnetwork->user_nick);
    mydelete(gnetwork->caps_nick);
    mydelete(gnetwork->name);
    mydelete(gnetwork->curserver.hostname);
    mydelete(gnetwork->curserver.password);
    mydelete(gnetwork->connectionmethod.host);
    mydelete(gnetwork->connectionmethod.password);
    mydelete(gnetwork->connectionmethod.vhost);

    for (user = irlist_get_head(&(gnetwork->xlistqueue));
         user;
         user = irlist_delete(&(gnetwork->xlistqueue), user)) {
      mydelete(user->nick);
      mydelete(user->msg);
    }

    for (dcc_options = irlist_get_head(&(gnetwork->dcc_options));
         dcc_options;
         dcc_options = irlist_delete(&(gnetwork->dcc_options), dcc_options)) {
      mydelete(dcc_options->nick);
    }

    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_delete(&(gnetwork->channels), ch)) {
      clearmemberlist(ch);
      free_channel_data(ch);
    }
  }
  gnetwork = NULL;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_delete(&gdata.trans, tr)) {
    mydelete(tr->nick);
    mydelete(tr->caps_nick);
    mydelete(tr->hostname);
    mydelete(tr->con.localaddr);
    mydelete(tr->con.remoteaddr);
    mydelete(tr->country);
  }

  for (up = irlist_get_head(&gdata.uploads);
       up;
       up = irlist_delete(&gdata.uploads, up)) {
    mydelete(up->nick);
    mydelete(up->hostname);
    mydelete(up->file);
    mydelete(tr->con.remoteaddr);
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
  free_delayed();
  for (i = irlist_get_head(&gdata.ignorelist);
       i;
       i = irlist_delete(&gdata.ignorelist, i)) {
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
    mydelete(h->url);
    mydelete(h->authorization);
    mydelete(h->group);
    mydelete(h->order);
    mydelete(h->search);
    mydelete(h->pattern);
    mydelete(h->modified);
    mydelete(h->buffer_out);
    mydelete(h->con.remoteaddr);
  }
  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_delete(&gdata.mime_type, mime)) {
    mydelete(mime->m_ext);
    mydelete(mime->m_mime);
  }
  for (ag = irlist_get_head(&gdata.autoadd_group_match);
       ag;
       ag = irlist_delete(&gdata.autoadd_group_match, ag)) {
    mydelete(ag->a_group);
    mydelete(ag->a_pattern);
  }
  irlist_delete_all(&gdata.autotrigger);
  irlist_delete_all(&gdata.console_history);
  irlist_delete_all(&gdata.jobs_delayed);
  irlist_delete_all(&gdata.http_bad_ip4);
  irlist_delete_all(&gdata.http_bad_ip6);
  mydelete(gdata.sendbuff);
  mydelete(gdata.console_input_line);
  mydelete(gdata.osstring);

  mydelete(xdcc_statefile.desc);
  mydelete(xdcc_listfile.desc);
}

static void free_config(void)
{
#if 0
  channel_t *ch;
#endif
  unsigned int si;

  updatecontext();

  mydelete(gdata.nosendmsg);
  mydelete(gdata.r_pidfile);

  for (si=0; si<MAX_NETWORKS; ++si) {
    mydelete(gdata.networks[si].curserver.hostname);
    mydelete(gdata.networks[si].curserver.password);
    mydelete(gdata.networks[si].curserveractualname);
#if 0
    mydelete(gdata.networks[si].r_config_nick);
    mydelete(gdata.networks[si].r_local_vhost);

    for (ch = irlist_get_head(&(gnetwork->r_channels));
         ch;
         ch = irlist_delete(&(gnetwork->r_channels), ch)) {
      clearmemberlist(ch);
      free_channel_data(ch);
    }
#endif
  } /* networks */
  for (si=0; si<MAXCONFIG; ++si)
    mydelete(gdata.configfile[si]);
}

static void debug_memory(void)
{
  meminfo_t *mi;
  unsigned char *ut;
  unsigned int j;
  unsigned int leak = 0;

  ++(gdata.crashing); /* stop traceback */
  signal(SIGSEGV, SIG_DFL);
  free_state();
  free_config();
  config_reset();

  for (j=1; j<(MEMINFOHASHSIZE * gdata.meminfo_depth); ++j) {
    mi = &(gdata.meminfo[j]);
    ut = mi->ptr;
    if (ut != NULL) {
      ++leak;
      outerror(OUTERROR_TYPE_WARN_LOUD, "Pointer 0x%8.8lX not free", (long)ut);
      outerror(OUTERROR_TYPE_WARN_LOUD, "alloctime = %ld", (long)(mi->alloctime));
      outerror(OUTERROR_TYPE_WARN_LOUD, "size      = %ld", (long)(mi->size));
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_func  = %s", mi->src_func);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_file  = %s", mi->src_file);
      outerror(OUTERROR_TYPE_WARN_LOUD, "src_line  = %u", mi->src_line);
      hexdump(OUT_S|OUT_L|OUT_D, COLOR_RED|COLOR_BOLD, "WARNING:" , ut, mi->size);
    }
  }
  if (leak == 0)
    return;

  *((volatile int*)(0)) = 0;
  free(gdata.meminfo);
  gdata.meminfo = NULL;
  return;
}
#endif /* DEBUG */

void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
/* cleanup all stuff before exit or restart */
exit_iroffer(unsigned int gotsignal)
{
  if (gotsignal == 0) {
    updatecontext();
    shutdown_dinoex();
    updatecontext();
  }
  tostdout_disable_buffering();
  uninitscreen();
  if (gdata.pidfile)
    unlink(gdata.pidfile);

#ifdef DEBUG
  if (gotsignal == 0) {
    updatecontext();
    debug_memory();
    updatecontext();
  }
#endif /* DEBUG */

#ifdef USE_KQUEUE
  ir_kqueue_exit();
#endif /* USE_KQUEUE */

  if (gdata.needrestart)
    execvp(gdata.argv[0], gdata.argv);
  exit(0);
}

/* End of File */
