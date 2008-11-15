/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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
#if 0
#include "dinoex_config.h"
#include "dinoex_transfer.h"
#include "dinoex_geoip.h"
#include "dinoex_ssl.h"
#include "dinoex_curl.h"
#include "dinoex_irc.h"
#include "dinoex_telnet.h"
#include "dinoex_queue.h"
#include "dinoex_ruby.h"
#include "dinoex_jobs.h"
#endif
#include "dinoex_main.h"
#include "dinoex_misc.h"

#define GET_NEXT_DATA(x)	{ argv++; argc--; x = *argv; \
				if (x == NULL) usage(); }

static const char *ir_basename;

static void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
usage(void)
{
  printf("\n"
"iroffer-dinoex " VERSIONLONG ", see http://iroffer.dinoex.net/\n"
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
"        -i file   Import and old style mybot.xdcc file.\n"
"\n", ir_basename);
  exit(64);
}

void command_options(int argc, const char *const *argv)
{
  const char *cptr;
  char ch;
  int fc = 0;

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
        createpassword();
        exit(0);
      case 'd': /* increase debug */
        gdata.debug++;
        break;
      case 'i': /* import xdccfile */
        GET_NEXT_DATA(cptr);
        gdata.import = cptr;
        break;
      case 'k': /* adjust core */
        gdata.adjustcore = 1;
        break;
      case 'n': /* no color */
        gdata.nocolor = 1;
        break;
      case 's': /* no screen */
        gdata.noscreen = 1;
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
        printf("iroffer-dinoex " VERSIONLONG ", see http://iroffer.dinoex.net/\n");
        exit(0);
      default:
        usage();
      }
      continue;
    }
    if (fc >= MAXCONFIG)
      usage();
    gdata.configfile[ fc++ ] = mystrdup(cptr);
  }
  if (fc == 0) {
    fprintf(stderr, "%s: no configuration file specified\n", argv[0]);
    usage();
  }
}

#ifdef DEBUG

static void free_state(void)
{
  xdcc *xd;
  channel_t *ch;
  transfer *tr;
  upload *up;
  ir_pqueue *pq;
  userinput *u;
  igninfo *i;
  msglog_t *ml;
  gnetwork_t *backup;
  xlistqueue_t *user;
  http *h;
  http_magic_t *mime;
  autoadd_group_t *ag;
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
    mydelete(gnetwork->connectionmethod.host);
    mydelete(gnetwork->connectionmethod.password);
    mydelete(gnetwork->connectionmethod.vhost);
    irlist_delete_all(&(gnetwork->xlistqueue));

    for (user = irlist_get_head(&(gnetwork->xlistqueue));
         user;
         user = irlist_delete(&(gnetwork->xlistqueue), user)) {
       mydelete(user->nick);
       mydelete(user->msg);
    }

    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_delete(&(gnetwork->channels), ch)) {
       clearmemberlist(ch);
       free_channel_data(ch);
    }
  }

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_delete(&gdata.trans, tr)) {
     mydelete(tr->nick);
     mydelete(tr->caps_nick);
     mydelete(tr->hostname);
     mydelete(tr->con.localaddr);
     mydelete(tr->con.remoteaddr);
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
  }
  for (ag = irlist_get_head(&gdata.autoadd_group_match);
       ag;
       ag = irlist_delete(&gdata.autoadd_group_match, ag)) {
     mydelete(ag->a_group);
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
  int si;

  updatecontext();

  mydelete(gdata.nosendmsg);
  mydelete(gdata.r_pidfile);
  for (si=0; si<MAX_NETWORKS; si++) {
    mydelete(gdata.networks[si].curserver.hostname);
    mydelete(gdata.networks[si].curserver.password);
    mydelete(gdata.networks[si].curserveractualname);
  } /* networks */
  for (si=0; si<MAXCONFIG; si++)
    mydelete(gdata.configfile[si]);
}

static void debug_memory(void)
{
  meminfo_t *mi;
  unsigned char *ut;
  int j;
  int i;
  int leak = 0;

  signal(SIGSEGV, SIG_DFL);
  free_state();
  free_config();
  reinit_config_vars();
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
        if (i >= mi->size)
          break;
        outerror(OUTERROR_TYPE_WARN_LOUD, " : %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X = \"%c%c%c%c%c%c%c%c%c%c%c%c\"",
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
    return;

  *((int*)(0)) = 0;
  free(gdata.meminfo);
  gdata.meminfo = NULL;
  return;
}
#endif /* DEBUG */

void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
exit_iroffer(void)
{
  shutdown_dinoex();
  tostdout_disable_buffering();
  uninitscreen();
  if (gdata.pidfile)
    unlink(gdata.pidfile);

#ifdef DEBUG
  debug_memory();
#endif /* DEBUG */
  exit(0);
}

/* End of File */
