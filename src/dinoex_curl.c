/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2013 Dirk Meyer
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
#include "dinoex_admin.h"
#include "dinoex_ruby.h"
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_curl.h"

#ifdef USE_CURL
#include <curl/curl.h>
#include <ctype.h>

typedef struct
{
  userinput u;
  unsigned int id;
  unsigned int net;
  char *name;
  char *url;
  char *uploaddir;
  char *fullname;
  char *contentname;
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

/* free all transfer ressouces */
static fetch_curl_t *clean_fetch(fetch_curl_t *ft)
{
  updatecontext();
  if (ft->curlhandle != 0 )
    curl_easy_cleanup(ft->curlhandle);
  if (ft->writefd != NULL)
    fclose(ft->writefd);
  mydelete(ft->errorbuf);
  if (ft->contentname != NULL)
    mydelete(ft->contentname);
  mydelete(ft->uploaddir);
  mydelete(ft->fullname);
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
    a_respond(&(ft->u), "fetch '%s' canceled", ft->name);
    cms = curl_multi_remove_handle(cm, ft->curlhandle);
    if ( cms != 0 ) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "curl_multi_remove_handle() = %d", cms);
    }
    --fetch_started;
    ft = clean_fetch(ft);
    start_qupload();
    return 0;
  }
  return 1;
}

/* set the modification time of a file */
static void fetch_set_time(const char *filename, long seconds)
{
  struct timeval tv[2];
  int rc;

  if (seconds <= 0)
    return;

  tv[0].tv_sec = tv[1].tv_sec = seconds;
  tv[0].tv_usec = tv[1].tv_usec = 0;
  rc = utimes(filename, tv);
  if (rc != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "setting date of file failed with %d : %s",
             rc, strerror(errno));
  }
}

/* get the best filename for the uploaded file */
static char *fetch_get_filename(fetch_curl_t *ft, const char *effective_url)
{
  if (ft->contentname != NULL)
    return getsendname(ft->contentname);

  if (effective_url != NULL)
    return getsendname(effective_url);

  return getsendname(ft->url);
}

/* rename file after upload */
static void fetch_rename(fetch_curl_t *ft, const char *effective_url)
{
  char *name;
  char *destname;
  int rc;
  int fd;

  if (strncasecmp(ft->name, "AUTO", 4) != 0) /* NOTRANSLATE */
    return;

  name = fetch_get_filename(ft, effective_url);
  if (name == NULL)
    return;

  destname = mystrjoin(ft->uploaddir, name, '/');
  fd = open(destname, O_RDONLY | ADDED_OPEN_FLAGS);
  if (fd >= 0) {
    close(fd);
    outerror(OUTERROR_TYPE_WARN_LOUD, "File %s could not be moved to %s: %s",
             ft->name, name, strerror(EEXIST));
  } else {
    rc = rename(ft->fullname, destname);
    if (rc < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "File %s could not be moved to %s: %s",
               ft->name, name, strerror(errno));
    } else {
      a_respond(&(ft->u), "fetched: '%s'", destname);
    }
  }
  mydelete(destname);
  mydelete(name);
}

/* process all running connections */
void fetch_perform(void)
{
  CURLMcode cms;
  CURLMsg *msg;
  CURL *ch;
  fetch_curl_t *ft;
  gnetwork_t *backup;
  char *effective_url;
  int running;
  int msgs_in_queue;
  unsigned int seen = 0;
  long filetime = 0;

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
          outerror(OUTERROR_TYPE_WARN_LOUD, "fetch '%s' failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
        if (msg->data.result != 0 ) {
          a_respond(&(ft->u), "fetch '%s' failed with %d: %s", ft->name, msg->data.result, ft->errorbuf);
        } else {
          a_respond(&(ft->u), "fetch '%s' completed", ft->name);
          ioutput(OUT_L, COLOR_NO_COLOR, "fetch '%s' completed", ft->name);
          curl_easy_getinfo(ft->curlhandle, CURLINFO_EFFECTIVE_URL, &effective_url);
          a_respond(&(ft->u), "fetched effective url: '%s'", effective_url);
          ioutput(OUT_L, COLOR_NO_COLOR, "fetched effective url: '%s'", effective_url);
          curl_easy_getinfo(ft->curlhandle, CURLINFO_FILETIME, &filetime);
          a_respond(&(ft->u), "fetched remote time: %ld", filetime);
          ioutput(OUT_L, COLOR_NO_COLOR, "fetched remote time: %ld", filetime);
          if (ft->contentname != NULL) {
            a_respond(&(ft->u), "fetched content name: '%s'", ft->contentname);
            ioutput(OUT_L, COLOR_NO_COLOR, "fetched content name: '%s'", ft->contentname);
          }
          fclose(ft->writefd); /* sync all data to disk */
          ft->writefd = NULL;
          fetch_set_time(ft->fullname, filetime);
          fetch_rename(ft, effective_url);
#ifdef USE_RUBY
          do_myruby_upload_done( ft->fullname );
#endif /* USE_RUBY */
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
  start_qupload();
}

/* callback for CURLOPT_HEADERFUNCTION */
/* from curl tool_cb_hdr.c, Copyright (C) 1998 - 2011, Daniel Stenberg */
static size_t fetch_header_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  const size_t cb = size * nmemb;
  const size_t failure = (cb) ? 0 : 1;
  fetch_curl_t *ft = userdata;
  char *temp;
  char *work;
  char *end;
  size_t len;

  updatecontext();
  if (ft == NULL)
    return cb; /* ignore */

#ifdef DEBUG
  if(size * nmemb > (size_t)CURL_MAX_HTTP_HEADER) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Header data = %ld exceeds single call write limit!", size);
    return failure;
  }
#endif
  temp = mystrdup(ptr);
  len = cb;
  if (temp[len - 1] == '\n') {
    --(len);
    if (temp[len - 1] == '\r') {
      --(len);
    }
  }
  temp[len] = 0;
  if ((gdata.debug > 0) && (cb > 2)) {
    a_respond(&(ft->u), "FETCH header '%s'", temp);
  }
  if (cb >= 20) {
    if (strncasecmp("Content-disposition:", temp, 20) == 0) { /* NOTRANSLATE */
      /* look for the 'filename=' parameter
         (encoded filenames (*=) are not supported) */
      work = strstr(temp + 20, "filename="); /* NOTRANSLATE */
      if (work != NULL) {
        work += 9;
        /* stop at first ; */
        end = strchr(work, ';');
        if (end != NULL)
          *end = 0;

        ft->contentname = mystrdup(work);
        clean_quotes(ft->contentname);
        a_respond(&(ft->u), "FETCH filename '%s'", ft->contentname);
      }
    }
  }

  mydelete(temp);
  return cb;
}

static void curl_respond(const userinput *const u, const char *text, CURLcode ces)
{
  a_respond(u, "curl_easy_setopt %s failed with %d", text, ces);
}

static unsigned int curl_fetch(const userinput *const u, fetch_curl_t *ft)
{
  char *vhost;
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

  vhost = get_local_vhost();
  if (vhost) {
    ft->vhosttext = mystrdup(vhost);
    ces = curl_easy_setopt(ch, CURLOPT_INTERFACE, ft->vhosttext);
    if (ces != 0) {
      a_respond(u, "curl_easy_setopt INTERFACE for %s failed with %d", ft->vhosttext, ces);
      return 1;
    }
  }

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

  ces = curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
  if (ces != 0) {
    curl_respond( u, "SSL_VERIFYPEER", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_REFERER, ft->url);
  if (ces != 0) {
    curl_respond( u, "REFERER", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
  if (ces != 0) {
    curl_respond( u, "FOLLOWLOCATION", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_AUTOREFERER, 1);
  if (ces != 0) {
    curl_respond( u, "AUTOREFERER", ces); /* NOTRANSLATE */
    return 1;
  }

  ces = curl_easy_setopt(ch, CURLOPT_FILETIME, 1);
  if (ces != 0) {
    curl_respond( u, "FILETIME", ces); /* NOTRANSLATE */
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

  ces = curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, fetch_header_cb);
  if (ces != 0) {
    curl_respond( u, "HEADERFUNCTION", ces); /* NOTRANSLATE */
    return 1;
  }
  ces = curl_easy_setopt(ch, CURLOPT_HEADERDATA, ft);
  if (ces != 0) {
    curl_respond( u, "HEADERDATA", ces); /* NOTRANSLATE */
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

/* start a transfer later */
static void fetch_later(const userinput *const u, const char *uploaddir, char *name, char *url)
{
  fetch_queue_t *fq;

  updatecontext();

  fq = irlist_add(&gdata.fetch_queue, sizeof(fetch_queue_t));
  fq->u.method = u->method;
  if (u->snick != NULL) {
    fq->u.snick = mystrdup(u->snick);
  }
  fq->u.fd = u->fd;
  fq->u.chat = u->chat;
  fq->net = gnetwork->net;
  fq->name = mystrdup(name);
  fq->url = mystrdup(url);
  fq->uploaddir = mystrdup(uploaddir);
}

/* start a transfer now */
static void fetch_now(const userinput *const u, const char *uploaddir, char *name, char *url)
{
  off_t resumesize;
  fetch_curl_t *ft;
  char *fullfile;
  FILE *writefd;
  struct stat s;
  int retval;

  resumesize = 0;
  fullfile = mystrjoin(uploaddir, name, '/');
  writefd = fopen(fullfile, "w+x"); /* NOTRANSLATE */
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
    writefd = fopen(fullfile, "a+"); /* NOTRANSLATE */
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
  ft->id = ++fetch_id;
  ft->net = gnetwork->net;
  ft->name = mystrdup(name);
  ft->url = mystrdup(url);
  ft->uploaddir = mystrdup(uploaddir);
  ft->fullname = fullfile;
  fullfile = NULL;
  ft->writefd = writefd;
  ft->resumesize = resumesize;
  ft->errorbuf = mymalloc(CURL_ERROR_SIZE);
  ft->errorbuf[0] = 0;
  ft->starttime = gdata.curtime;

  if (curl_fetch(u, ft)) {
    clean_fetch(ft);
    return;
  }

  a_respond(u, "fetch '%s' started", ft->name);
  ++fetch_started;
}

/* try to start a transfer */
void start_fetch_url(const userinput *const u, const char *uploaddir)
{
  char *name;
  char *url;

  name = u->arg1;
  url = u->arg2e;
  while (*url == ' ') ++url;
  if (max_uploads_reached() != 0) {
    fetch_later(u, uploaddir, name, url);
  } else {
    fetch_now(u, uploaddir, name, url);
  }
}

/* start next transfer */
void fetch_next(void)
{
  gnetwork_t *backup;
  fetch_queue_t *fq;

  updatecontext();

  if (irlist_size(&gdata.fetch_queue) == 0)
    return;

  fq = irlist_get_head(&gdata.fetch_queue);
  if (fq == NULL)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[fq->net]);
  fetch_now(&(fq->u), fq->uploaddir, fq->name, fq->url);
  mydelete(fq->u.snick);
  mydelete(fq->name);
  mydelete(fq->url);
  mydelete(fq->uploaddir);
  irlist_delete(&gdata.fetch_queue, fq);
  gnetwork = backup;
}

/* show running transfers */
void dinoex_dcl(const userinput *const u)
{
  fetch_curl_t *ft;
  fetch_queue_t *fq;
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

  updatecontext();
  progress = 0;
  for (fq = irlist_get_head(&gdata.fetch_queue); fq; fq = irlist_get_next(fq)) {
    a_respond(u, "   %2i  fetch       %-32s   Waiting", ++progress, fq->name);
  }
}

/* show running transfers in detail */
void dinoex_dcld(const userinput *const u)
{
  fetch_curl_t *ft;
  fetch_queue_t *fq;
  char *effective_url;
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

    effective_url = NULL;
    curl_easy_getinfo(ft->curlhandle, CURLINFO_EFFECTIVE_URL, &effective_url);

    started = min2(359999, gdata.curtime - ft->starttime);
    left = min2(359999, (dl_total - dl_size) /
                        ((int)(max2(dl_speed, 1))));
    progress = ((dl_size + 50) * 100) / max2(dl_total, 1);
    a_respond(u, "   %2i  fetch       %-32s   Receiving %d%%", ft->id, ft->name, progress);
    a_respond(u, "                   %s", effective_url ? effective_url : ft->url);
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

  updatecontext();
  progress = 0;
  for (fq = irlist_get_head(&gdata.fetch_queue); fq; fq = irlist_get_next(fq)) {
    a_respond(u, "   %2i  fetch       %-32s   Waiting", ++progress, fq->name);
    a_respond(u, "                   %s", fq->url);
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

/* check if a file is already in transfer */
unsigned int fetch_running(void)
{
  return irlist_size(&fetch_trans);
}

#endif /* USE_CURL */

/* End of File */
