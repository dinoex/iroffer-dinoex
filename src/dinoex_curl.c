/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2011 Dirk Meyer
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
#include "dinoex_curl.h"
#include "dinoex_admin.h"

#ifdef USE_CURL
#include <curl/curl.h>

typedef struct
{
  userinput u;
  unsigned int id;
  unsigned int net;
  char *name;
  char *url;
  char *vhosttext;
  FILE *writefd;
  off_t resumesize;
  char *errorbuf;
  CURL *curlhandle;
  time_t starttime;
} fetch_curl_t;

static CURLM *cm;
static irlist_t fetch_trans;
static unsigned int fetch_id;
int fetch_started;

/* setup the curl lib */
void curl_startup(void)
{
  CURLcode cs;

  bzero((char *)&fetch_trans, sizeof(fetch_trans));
  fetch_started = 0;
  fetch_id = 0;

  cs = curl_global_init(CURL_GLOBAL_ALL);
  if (cs != 0) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "curl_global_init failed with %d", cs);
  }
  cm = curl_multi_init();
  if (cm == NULL) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "curl_multi_init failed");
  }
}

/* close the curl lib */
void curl_shutdown(void)
{
  if (cm == NULL) {
    curl_multi_cleanup(cm);
    cm = NULL;
  }
  curl_global_cleanup();
}

/* register active connections for select() */
void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd)
{
  CURLMcode cms;

  cms = curl_multi_fdset(cm, read_fd_set, write_fd_set, exc_fd_set, max_fd);
  if (cms != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "curl_multi_fdset() = %d", cms);
  }
}

static fetch_curl_t *clean_fetch(fetch_curl_t *ft)
{
  updatecontext();
  if (ft->curlhandle != 0 )
    curl_easy_cleanup(ft->curlhandle);
  fclose(ft->writefd);
  mydelete(ft->errorbuf);
  mydelete(ft->name);
  mydelete(ft->url);
  if (ft->u.snick != NULL)
    mydelete(ft->u.snick);
  if (ft->vhosttext != NULL)
    mydelete(ft->vhosttext);
  if (fetch_started == 0)
    fetch_id = 0;
  return irlist_delete(&fetch_trans, ft);
}

/* cancel a running fetch command */
unsigned int fetch_cancel(unsigned int num)
{
  fetch_curl_t *ft;
  CURLMcode cms;

  updatecontext();

  ft = irlist_get_head(&fetch_trans);
  while (ft) {
    if (num > 0) {
      if (ft->id != num) {
        ft = irlist_get_next(ft);
        continue;
      }
    }
    a_respond(&(ft->u), "fetch %s canceled", ft->name);
    cms = curl_multi_remove_handle(cm, ft->curlhandle);
    if ( cms != 0 ) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "curl_multi_remove_handle() = %d", cms);
    }
    --fetch_started;
    ft = clean_fetch(ft);
    return 0;
  }
  return 1;
}

/* process all running connections */
void fetch_perform(void)
{
  CURLMcode cms;
  CURLMsg *msg;
  CURL *ch;
  int running;
  int msgs_in_queue;
  unsigned int seen = 0;
  fetch_curl_t *ft;
  gnetwork_t *backup;

  do {
    cms = curl_multi_perform(cm, &running);
  } while (cms == CURLM_CALL_MULTI_PERFORM);
  if ( cms != 0 ) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "curl_multi_perform() = %d", cms);
  }

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
    while(ft) {
      if (ft->curlhandle == ch) {
        gnetwork = &(gdata.networks[ft->net]);
        if (ft->errorbuf[0] != 0)
          outerror(OUTERROR_TYPE_WARN_LOUD, "fetch %s failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
        if (msg->data.result != 0 ) {
          a_respond(&(ft->u), "fetch %s failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
        } else {
          a_respond(&(ft->u), "fetch %s completed", ft->name);
        }
        updatecontext();
        ++seen;
        --fetch_started;
        ft = clean_fetch(ft);
        continue;
      }
      ft = irlist_get_next(ft);
    }
  } while (msgs_in_queue > 0);
  updatecontext();
  if (seen == 0)
    outerror(OUTERROR_TYPE_WARN_LOUD, "curlhandle not found %d/%d", running, fetch_started);
  fetch_started = running;
  gnetwork = backup;
}

static void curl_respond(const userinput *const u, const char *text, CURLcode ces)
{
  a_respond(u, "curl_easy_setopt %s failed with %d", text, ces);
}

static unsigned int curl_fetch(const userinput *const u, fetch_curl_t *ft)
{
  CURL *ch;
  CURLcode ces;
  CURLcode cms;

  updatecontext();

  ch = curl_easy_init();
  if (ch == NULL) {
    a_respond(u, "Curl not ready");
    return 1;
  }
  ft->curlhandle = ch;

  ces = curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, ft->errorbuf);
  if (ces != 0) {
    curl_respond( u, "ERRORBUFFER", ces); /* NOTRANSLATE */
    return 1;
  }

#if 0
  {
    char *vhost;

    vhost = get_local_vhost();
    if (vhost) {
      ft->vhosttext = mystrdup(vhost);
      ces = curl_easy_setopt(ch, CURLOPT_INTERFACE, ft->vhosttext);
      if (ces != 0) {
        a_respond(u, "curl_easy_setopt INTERFACE for %s failed with %d", ft->vhosttext, ces);
        return 1;
      }
    }
  }
#endif

  ces = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
  if (ces != 0) {
    curl_respond( u, "NOPROGRESS", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
  if (ces != 0) {
    curl_respond( u, "NOSIGNAL", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
  if (ces != 0) {
    curl_respond( u, "FAILONERROR", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0);
  if (ces != 0) {
    curl_respond( u, "SSL_VERIFYHOST", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
  if (ces != 0) {
    curl_respond( u, "FOLLOWLOCATION", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
  if (ces != 0) {
    curl_respond( u, "SSL_VERIFYPEER", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_URL, ft->url);
  if (ces != 0) {
    curl_respond( u, "URL", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_WRITEDATA, ft->writefd);
  if (ces != 0) {
    curl_respond( u, "WRITEDATA", ces); /* NOTRANSLATE */
    return 1;
  }

  if (ft->resumesize > 0L) {
#if LIBCURL_VERSION_NUM <= 0x70b01
    ces = curl_easy_setopt(ch, CURLOPT_RESUME_FROM, ft->resumesize);
#else
    ces = curl_easy_setopt(ch, CURLOPT_RESUME_FROM_LARGE, ft->resumesize);
#endif
    if (ces != 0) {
      curl_respond( u, "RESUME_FROM", ces); /* NOTRANSLATE */
      return 1;
    }
  }

  cms = curl_multi_add_handle(cm, ch);
  if (cms != 0) {
    a_respond(u, "curl_multi_add_handle failed with %d", cms);
    return 1;
  }

  return 0;
}

/* start a transfer */
void start_fetch_url(const userinput *const u, const char *uploaddir)
{
  off_t resumesize;
  fetch_curl_t *ft;
  char *fullfile;
  char *name;
  char *url;
  FILE *writefd;
  struct stat s;
  int retval;

  name = u->arg1;
  url = u->arg2e;
  while (*url == ' ') ++url;

  resumesize = 0;
  fullfile = mystrjoin(uploaddir, name, '/');
  writefd = fopen(fullfile, "w+"); /* NOTRANSLATE */
  if ((writefd == NULL) && (errno == EEXIST)) {
    retval = stat(fullfile, &s);
    if (retval < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Stat Upload File '%s': %s",
               fullfile, strerror(errno));
      a_respond(u, "File Error, File couldn't be opened for writing");
      mydelete(fullfile);
      return;
    }
    resumesize = s.st_size;
    writefd = fopen(fullfile, "a"); /* NOTRANSLATE */
  }
  if (writefd == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access Upload File '%s': %s",
             fullfile, strerror(errno));
    a_respond(u, "File Error, File couldn't be opened for writing");
    mydelete(fullfile);
    return;
  }

  updatecontext();
  mydelete(fullfile);
  ft = irlist_add(&fetch_trans, sizeof(fetch_curl_t));
  ft->u.method = u->method;
  if (u->snick != NULL) {
    ft->u.snick = mystrdup(u->snick);
  }
  ft->u.fd = u->fd;
  ft->u.chat = u->chat;
  ft->id = ++fetch_id;
  ft->net = gnetwork->net;
  ft->name = mystrdup(name);
  ft->url = mystrdup(url);
  ft->writefd = writefd;
  ft->resumesize = resumesize;
  ft->errorbuf = mymalloc(CURL_ERROR_SIZE);
  ft->errorbuf[0] = 0;
  ft->starttime = gdata.curtime;

  if (curl_fetch(u, ft)) {
    clean_fetch(ft);
    return;
  }

  a_respond(u, "fetch %s started", ft->name);
  ++fetch_started;
}

/* show running transfers */
void dinoex_dcl(const userinput *const u)
{
  fetch_curl_t *ft;
  double dl_total;
  double dl_size;
  int progress;

  updatecontext();
  for (ft = irlist_get_head(&fetch_trans); ft; ft = irlist_get_next(ft)) {
    dl_size = 0.0;
    curl_easy_getinfo(ft->curlhandle, CURLINFO_SIZE_DOWNLOAD, &dl_size);

    dl_total = 0.0;
    curl_easy_getinfo(ft->curlhandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_total);

    progress = 0;
    progress = ((dl_size + 50) * 100) / max2(dl_total, 1);
    a_respond(u, "   %2i  fetch       %-32s   Receiving %d%%", ft->id, ft->name, progress);
  }
}

/* show running transfers in detail */
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

  updatecontext();
  for (ft = irlist_get_head(&fetch_trans); ft; ft = irlist_get_next(ft)) {
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
    a_respond(u, "   %2i  fetch       %-32s   Receiving %d%%", ft->id, ft->name, progress);
    a_respond(u, "                   %s", ft->url);
    a_respond(u, "  ^- %5.1fK/s    %6" LLPRINTFMT "dK/%6" LLPRINTFMT "dK  %2i%c%02i%c/%2i%c%02i%c",
                (float)(dl_speed/1024),
                (ir_int64)(dl_size/1024),
                (ir_int64)(dl_total/1024),
                started < 3600 ? started/60 : started/60/60 ,
                started < 3600 ? 'm' : 'h',
                started < 3600 ? started%60 : (started/60)%60 ,
                started < 3600 ? 's' : 'm',
                left < 3600 ? left/60 : left/60/60 ,
                left < 3600 ? 'm' : 'h',
                left < 3600 ? left%60 : (left/60)%60 ,
                left < 3600 ? 's' : 'm');
  }
}

/* check if a file is already in transfer */
unsigned int fetch_is_running(const char *file)
{
  fetch_curl_t *ft;

  updatecontext();
  for (ft = irlist_get_head(&fetch_trans); ft; ft = irlist_get_next(ft)) {
    if (strcmp(ft->name, file) == 0)
      return 1;
  }
  return 0;
}

#endif /* USE_CURL */

/* End of File */
