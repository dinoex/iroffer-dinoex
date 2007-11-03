/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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
#endif /* USE_CURL */

#ifdef USE_CURL
static CURLM *cm;
static irlist_t fetch_trans;
int fetch_started;
#endif /* USE_CURL */

void curl_startup(void)
{
#ifdef USE_CURL
  CURLcode cs;
#endif /* USE_CURL */

#ifdef USE_CURL
  bzero((char *)&fetch_trans, sizeof(fetch_trans));
  fetch_started = 0;

  cs = curl_global_init(CURL_GLOBAL_ALL);
  if (cs != 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "curl_global_init failed with %d", cs);
  }
  cm = curl_multi_init();
  if (cm == NULL) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "curl_multi_init failed");
  }
#endif /* USE_CURL */
}

void curl_shutdown(void)
{
#ifdef USE_CURL
  curl_global_cleanup();
#endif /* USE_CURL */
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
  while(ft) {
    i++;
    if (++i == num) {
      ft = irlist_get_next(ft);
      continue;
    }
    a_respond(&(ft->u), "fetch %s canceled", ft->name);
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
    while(ft) {
      if (ft->curlhandle == ch) {
        gnetwork = &(gdata.networks[ft->net]);
        if (ft->errorbuf[0] != 0)
          outerror(OUTERROR_TYPE_WARN_LOUD, "fetch: %s", ft->errorbuf);
        if (msg->data.result != 0 ) {
          a_respond(&(ft->u), "fetch %s failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
        }
              else {
          a_respond(&(ft->u), "fetch %s completed", ft->name);
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
    outerror(OUTERROR_TYPE_WARN_LOUD, "curlhandle not found ");
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
    writefd = fopen(fullfile, "a");
  }
  if (writefd == NULL) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access Upload File '%s': %s",
             fullfile, strerror(errno));
    a_respond(u, "File Error, File couldn't be opened for writing");
    mydelete(fullfile);
    return;
  }

  updatecontext();
  ft = irlist_add(&fetch_trans, sizeof(fetch_curl_t));
  ft->u.method = u->method;
  if (u->snick != NULL) {
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
  if (ch == NULL) {
    a_respond(u, "Curl not ready");
    clean_fetch(ft);
    return;
  }

  ft->curlhandle = ch;

  ces = curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, ft->errorbuf);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt ERRORBUFFER failed with %d", ces);
    clean_fetch(ft);
    return;
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
        clean_fetch(ft);
        return;
      }
    }
  }
#endif

  ces = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt NOPROGRESS failed with %d", ces);
    return;
  }

  ces = curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt NOSIGNAL failed with %d", ces);
    return;
  }

  ces = curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt FAILONERROR failed with %d", ces);
    return;
  }

  ces = curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt SSL_VERIFYPEER failed with %d", ces);
    return;
  }

  ces = curl_easy_setopt(ch, CURLOPT_URL, ft->url);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt URL failed with %d", ces);
    return;
  }

  ces = curl_easy_setopt(ch, CURLOPT_WRITEDATA, ft->writefd);
  if (ces != 0) {
    a_respond(u, "curl_easy_setopt WRITEDATA failed with %d", ces);
    return;
  }

  if (resumesize > 0L) {
    ces = curl_easy_setopt(ch, CURLOPT_RESUME_FROM_LARGE, ft->resumesize);
    if (ces != 0) {
      a_respond(u, "curl_easy_setopt RESUME_FROM failed with %d", ces);
      return;
    }
  }

  cms = curl_multi_add_handle(cm, ch);
  if (cms != 0) {
    a_respond(u, "curl_multi_add_handle failed with %d", cms);
    return;
  }

  a_respond(u, "fetch %s started", ft->name);
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
  for (ft = irlist_get_head(&fetch_trans); ft; ft = irlist_get_next(ft)) {
    i ++;
    dl_size = 0.0;
    curl_easy_getinfo(ft->curlhandle, CURLINFO_SIZE_DOWNLOAD, &dl_size);

    dl_total = 0.0;
    curl_easy_getinfo(ft->curlhandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_total);

    progress = 0;
    progress = ((dl_size + 50) * 100) / max2(dl_total, 1);
    a_respond(u, "   %2i  fetch       %-32s   Receiving %d%%", i, ft->name, progress);
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
  for (ft = irlist_get_head(&fetch_trans); ft; ft = irlist_get_next(ft)) {
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
    a_respond(u, "   %2i  fetch       %-32s   Receiving %d%%", i, ft->name, progress);
    a_respond(u, "                   %s", ft->url);
    a_respond(u, "  ^- %5.1fK/s    %6" LLPRINTFMT "uK/%6" LLPRINTFMT "uK  %2i%c%02i%c/%2i%c%02i%c",
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
  }
}

#endif /* USE_CURL */

/* End of File */
