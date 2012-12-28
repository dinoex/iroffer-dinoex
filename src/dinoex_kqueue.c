/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2011-2012 Dirk Meyer
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
#include "dinoex_kqueue.h"

#ifdef USE_KQUEUE

#include <sys/event.h>

#ifndef NOTE_EOF
#define NOTE_EOF 0
#endif

static int ir_kqueue_fd = FD_UNUSED;
static int ir_kqueue_change_size = 0;
static int ir_kqueue_event_max = 0;
static struct kevent *ir_kqueue_change_buffer = NULL;
static struct kevent *ir_kqueue_event_buffer = NULL;

/* emulate select */
static fd_set ir_kqueue_readset;
static fd_set ir_kqueue_writeset;

static void ir_kqueue_set_buffer(void)
{
  if (ir_kqueue_change_size > 0) {
    /* keep old data */
    ir_kqueue_change_buffer = realloc(ir_kqueue_change_buffer, sizeof (struct kevent) * ir_kqueue_event_max);
  } else {
    if (ir_kqueue_change_buffer != NULL)
      free(ir_kqueue_change_buffer);
    ir_kqueue_change_buffer = malloc( sizeof (struct kevent) * ir_kqueue_event_max);
  }
  if (ir_kqueue_event_buffer != NULL)
    free(ir_kqueue_event_buffer);
  ir_kqueue_event_buffer = malloc( sizeof (struct kevent) * ir_kqueue_event_max);
}

/* setup buffers for kqueue */
void ir_kqueue_init(void)
{
  ir_kqueue_change_size = 0;
  ir_kqueue_event_max = 64;
  FD_ZERO(&ir_kqueue_readset);
  FD_ZERO(&ir_kqueue_writeset);

  ir_kqueue_fd = kqueue ();
  if (ir_kqueue_fd < 0) {
    fprintf(stderr, "Failed to start kqueue()\n");
    ir_kqueue_fd = FD_UNUSED;
  } else {
    fcntl(ir_kqueue_fd, F_SETFD, FD_CLOEXEC);
    ir_kqueue_set_buffer();
  }
}

/* release buffers for kqueue */
void ir_kqueue_exit(void)
{
   free(ir_kqueue_change_buffer);
   free(ir_kqueue_event_buffer);
}

static void ir_kqueue_update(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
  int fd;
  int max;
  int reading;
  int writing;

  max = nfds * 3;
  if (ir_kqueue_event_max > max) {
    ir_kqueue_event_max = max;
    ir_kqueue_set_buffer();
  }

  for (fd = 0; fd < nfds; ++fd) {
    reading = FD_ISSET(fd, &ir_kqueue_readset);
    writing = FD_ISSET(fd, &ir_kqueue_writeset);
    if (reading) {
      if (FD_ISSET(fd, readfds) == 0) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
        FD_CLR(fd, &ir_kqueue_readset);
        if ( gdata.debug > 80 )
          ioutput(OUT_S|OUT_D, COLOR_YELLOW, "kqueue del read %d", fd); /* NOTRANSLATE */
      }
    }
    if (writing) {
      if (FD_ISSET(fd, writefds) == 0) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        FD_CLR(fd, &ir_kqueue_writeset);
        if ( gdata.debug > 80 )
          ioutput(OUT_S|OUT_D, COLOR_YELLOW, "kqueue del write %d", fd); /* NOTRANSLATE */
      }
    }

    if (reading == 0) {
      if (FD_ISSET(fd, readfds)) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_READ, EV_ADD | EV_ENABLE, NOTE_EOF, 0, 0);
        FD_SET(fd, &ir_kqueue_readset);
        if ( gdata.debug > 80 )
          ioutput(OUT_S|OUT_D, COLOR_YELLOW, "kqueue add read %d", fd); /* NOTRANSLATE */
      }
    }
    if (writing == 0) {
      if (FD_ISSET(fd, writefds)) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, NOTE_EOF, 0, 0);
        FD_SET(fd, &ir_kqueue_writeset);
        if ( gdata.debug > 80 )
          ioutput(OUT_S|OUT_D, COLOR_YELLOW, "kqueue add write %d", fd); /* NOTRANSLATE */
      }
    }
  }
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  FD_ZERO(exceptfds);
}

/* poll kqueue status */
int ir_kqueue_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
  struct timespec ts;
  int res;
  int i;

  ir_kqueue_update(nfds, readfds, writefds, exceptfds);

  /* need to resize so there is enough space for errors */
  if (ir_kqueue_change_size > ir_kqueue_event_max) {
    ir_kqueue_event_max = ir_kqueue_change_size;
    ir_kqueue_set_buffer();

  }

  ts.tv_sec = 0;
  ts.tv_nsec = 250*1000;
  if ( gdata.debug > 80 ) {
    if (ir_kqueue_change_size > 0)
      ioutput(OUT_S|OUT_D, COLOR_YELLOW, "kevent %d %d", ir_kqueue_change_size, ir_kqueue_event_max); /* NOTRANSLATE */
  }
  res = kevent(ir_kqueue_fd, ir_kqueue_change_buffer, ir_kqueue_change_size, ir_kqueue_event_buffer, ir_kqueue_event_max, &ts);
  if (res < 0) {
    if (errno == EINTR) {
      return 0; /* again  with all changes */
    }
    return res;
  }

  ir_kqueue_change_size = 0;
  for (i = 0; i < res; ++i) {
    int fd = ir_kqueue_event_buffer[i].ident;

    if (fd >= gdata.max_fds_from_rlimit) {
      /* ignore for now */
      outerror(OUTERROR_TYPE_WARN, "kqueue error on fd %d: %s", fd, "FDMAX"); /* NOTRANSLATE */
      continue;
    }

    if (ir_kqueue_event_buffer[i].flags & EV_ERROR) {
      int err = ir_kqueue_event_buffer[i].data;
      outerror(OUTERROR_TYPE_WARN, "kqueue error on fd %d: %s", fd, strerror(err)); /* NOTRANSLATE */
      /* report to caller */
      FD_SET(fd, readfds);
      FD_SET(fd, writefds);
      continue;
    }

    if (ir_kqueue_event_buffer[i].filter == EVFILT_READ) {
      FD_SET(fd, readfds);
    }
    if (ir_kqueue_event_buffer[i].filter == EVFILT_WRITE) {
      FD_SET(fd, writefds);
    }
  }

  if (res == ir_kqueue_event_max) {
    ir_kqueue_event_max <<= 1;
    ir_kqueue_set_buffer();
  }
  return res;
}

static void ir_kqueue_close(int fd)
{
   FD_CLR(fd, &ir_kqueue_readset);
   FD_CLR(fd, &ir_kqueue_writeset);
}

#else

/* poll select status */
int ir_kqueue_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
    struct timeval timestruct;

    timestruct.tv_sec = 0;
    timestruct.tv_usec = 250*1000;
    return select(nfds, readfds, writefds, exceptfds, &timestruct);
}

#endif /* USE_KQUEUE */

/* close and drop events */
void event_close(int handle)
{
  FD_CLR(handle, &gdata.readset);
  FD_CLR(handle, &gdata.writeset);
#ifdef USE_KQUEUE
  ir_kqueue_close(handle);
#endif /* USE_KQUEUE */
  close(handle);
}

/* close an TCP connection safely */
void shutdown_close(int handle)
{
  /*
   * cygwin close() is broke, if outstanding data is present
   * it will block until the TCP connection is dead, sometimes
   * upto 10-20 minutes, calling shutdown() first seems to help
   */
  shutdown(handle, SHUT_RDWR);
  event_close(handle);
}

/* End of File */
