/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2011-2011 Dirk Meyer
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

int ir_kqueue_fd = FD_UNUSED;
int ir_ir_kqueue_change_buffer   = 0;
int ir_kqueue_change_size = 0;
int ir_kqueue_event_max = 0;
struct kevent *ir_kqueue_change_buffer = NULL;
struct kevent *ir_kqueue_event_buffer = NULL;

/* emulate select */
fd_set ir_kqueue_readset;
fd_set ir_kqueue_writeset;

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

void ir_kqueue_exit(void)
{
   free(ir_kqueue_change_buffer);
   free(ir_kqueue_event_buffer);
}

static void ir_kqueue_update(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
  int fd;
  int max;

  max = nfds * 3;
  if (ir_kqueue_event_max > max) {
    ir_kqueue_event_max = max;
    ir_kqueue_set_buffer();
  }

  for (fd = 0; fd < nfds; ++fd) {
    if (FD_ISSET(fd, &ir_kqueue_readset)) {
      if (FD_ISSET(fd, readfds) == 0) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
      }
    } else {
      if (FD_ISSET(fd, readfds)) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_READ, EV_ADD | EV_ENABLE, NOTE_EOF, 0, 0);
      }
    }

    if (FD_ISSET(fd, &ir_kqueue_writeset)) {
      if (FD_ISSET(fd, writefds) == 0) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
      }
    } else {
      if (FD_ISSET(fd, writefds)) {
        ++ir_kqueue_change_size;
        EV_SET(&ir_kqueue_change_buffer[ir_kqueue_change_size - 1], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, NOTE_EOF, 0, 0);
      }
    }
  }
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  FD_ZERO(exceptfds);
}

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
      continue;
    }

    if (ir_kqueue_event_buffer[i].flags & EV_ERROR) {
      /* ignore for now */
      continue;
    }

    if (ir_kqueue_event_buffer[i].filter == EVFILT_READ) {
      FD_SET(fd, readfds);
    }
    if (ir_kqueue_event_buffer[i].filter == EVFILT_WRITE) {
      FD_SET(fd, writefds);
    }
    if (ir_kqueue_event_buffer[i].filter == EVFILT_WRITE) {
      FD_SET(fd, exceptfds);
    }
  }

  if (res == ir_kqueue_event_max) {
    ir_kqueue_event_max <<= 1;
    ir_kqueue_set_buffer();
  }
  return res;
}

#else

int ir_kqueue_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
    struct timeval timestruct;
  
    timestruct.tv_sec = 0;
    timestruct.tv_usec = 250*1000;
    return select(nfds, readfds, writefds, exceptfds, &timestruct);
}

#endif /* USE_KQUEUE */

/* End of File */
