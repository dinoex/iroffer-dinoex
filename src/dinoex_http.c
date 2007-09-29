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
#include "dinoex_config.h"
#include "dinoex_http.h"

int http_port = 8813;
int http_listen = FD_UNUSED;


const char *http_header = 
"HTTP/1.0 200 OK\r\n"
"Server: iroffer " VERSIONLONG "\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: " LLPRINTFMT "\r\n"
"\r\n";

#if 0
HTTP/1.1 200 OK
Server: Apache/1.3.29 (Unix) PHP/4.3.4
Content-Length: (Größe von infotext.html in Byte)
Content-Language: de (nach ISO 639 und ISO 3166)
Content-Type: text/html
Connection: close
#endif


void h_close_listen(void)
{
  if (http_listen == FD_UNUSED)
    return;

  FD_CLR(http_listen, &gdata.readset);
  close(http_listen);
}

int h_setup_listen(void)
{
  int tempc;
  int listenport;
  struct sockaddr_in listenaddr;

  updatecontext();

  if (http_port == 0)
    return 1;

  if ((http_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Could Not Create Socket, Aborting: %s", strerror(errno));
    http_listen = FD_UNUSED;
    return 1;
  }

  tempc = 1;
  setsockopt(http_listen, SOL_SOCKET, SO_REUSEADDR, &tempc, sizeof(int));

  bzero ((char *) &listenaddr, sizeof (struct sockaddr_in));
  listenaddr.sin_family = AF_INET;
  listenaddr.sin_addr.s_addr = INADDR_ANY;
  listenaddr.sin_port = htons(http_port);

  if (bind(http_listen, (struct sockaddr *)&listenaddr, sizeof(struct sockaddr_in)) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Couldn't Bind to Socket, Aborting: %s", strerror(errno));
    http_listen = FD_UNUSED;
    return 1;
  }

  listenport = ntohs (listenaddr.sin_port);
  if (listen (http_listen, 1) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't Listen, Aborting: %s", strerror(errno));
    http_listen = FD_UNUSED;
    return 1;
  }

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "HTTP SERVER waiting for connection on %lu.%lu.%lu.%lu:%d",
          (gdata.ourip >> 24) & 0xFF,
          (gdata.ourip >> 16) & 0xFF,
          (gdata.ourip >>  8) & 0xFF,
          (gdata.ourip      ) & 0xFF,
          listenport);
  return 0;
}

int h_listen(int highests)
{
  http *h;

  if (http_listen != FD_UNUSED) {
    FD_SET(http_listen, &gdata.readset);
    highests = max2(highests, http_listen);
  }

  for (h = irlist_get_head(&gdata.https);
       h;
       h = irlist_get_next(h)) {
    if (h->ul_status == HTTP_STATUS_GETTING) {
      FD_SET(h->clientsocket, &gdata.readset);
      highests = max2(highests, h->clientsocket);
    }
    if (h->ul_status == HTTP_STATUS_WAITING) {
      FD_SET(h->clientsocket, &gdata.readset);
      highests = max2(highests, h->clientsocket);
      FD_SET(h->clientsocket, &gdata.writeset);
      highests = max2(highests, h->clientsocket);
    }
  }
  return highests;
}

/* connections */

void h_accept(void)
{
  SIGNEDSOCK int addrlen;
  struct sockaddr_in remoteaddr;
  int clientsocket;
  http *h;

  updatecontext();

  addrlen = sizeof (struct sockaddr_in);
  if ((clientsocket = accept(http_listen, (struct sockaddr *) &remoteaddr, &addrlen)) < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    return;
  }

  if (set_socket_nonblocking(clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  h = irlist_add(&gdata.https, sizeof(http));
  h->filedescriptor = FD_UNUSED;
  h->clientsocket = clientsocket;
  h->remoteip   = ntohl(remoteaddr.sin_addr.s_addr);
  h->remoteport = ntohs(remoteaddr.sin_port);
  h->connecttime = gdata.curtime;
  h->lastcontact = gdata.curtime;
  h->ul_status = HTTP_STATUS_GETTING;

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "HTTP connection received from %lu.%lu.%lu.%lu:%d",
          (h->remoteip >> 24) & 0xFF,
          (h->remoteip >> 16) & 0xFF,
          (h->remoteip >>  8) & 0xFF,
          (h->remoteip      ) & 0xFF,
          h->remoteport);
}

void h_closeconn(http * const h, const char *msg, int errno1)
{
  updatecontext();

  if (errno1) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
              "HTTP: Connection closed: %s (%s)", msg, strerror(errno1));
  } else {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
              "HTTP: Connection closed: %s", msg);
  }

  if (gdata.debug > 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S,COLOR_YELLOW, "clientsock = %d", h->clientsocket);
  }

  if (h->clientsocket != FD_UNUSED && h->clientsocket > 2) {
    FD_CLR(h->clientsocket, &gdata.writeset);
    FD_CLR(h->clientsocket, &gdata.readset);
    shutdown_close(h->clientsocket);
  }

  if (h->filedescriptor != FD_UNUSED && h->filedescriptor > 2) {
    close(h->filedescriptor);
  }

  h->ul_status = HTTP_STATUS_DONE;
}

void h_get(http * const h) {
  int i, howmuch, howmuch2;
  char *tempstr;
  size_t len;
  struct stat st;

  updatecontext();
  if (h->filedescriptor == FD_UNUSED) {
    if (gdata.debug > 4)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "HTTP data received");
  } else {
    if (gdata.debug > 4)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "HTTP data ignored");
    return;
  }

  howmuch = BUFFERSIZE;
  howmuch2 = 1;
  for (i=0; i<MAXTXPERLOOP; i++) {
    if ((howmuch == BUFFERSIZE && howmuch2 > 0) && is_fd_readable(h->clientsocket)) {
      howmuch = read(h->clientsocket, gdata.sendbuff, BUFFERSIZE);
      if (howmuch < 0) {
        h_closeconn(h, "Connection Lost", errno);
        return;
      }
      if (howmuch < 1) {
        h_closeconn(h, "Connection Lost", 0);
        return;
      }
	howmuch2 = howmuch;
        if (gdata.debug > 3) {
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "HTTP data received bytes=%d", howmuch);
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "HTTP data received data=\n%s", gdata.sendbuff);
        }

        if (howmuch > 0) h->lastcontact = gdata.curtime;

        h->bytesgot += howmuch2;
    }
  }

  h->file = mystrdup(gdata.xdcclistfile);
  h->filedescriptor = open(gdata.xdcclistfile, O_RDONLY | ADDED_OPEN_FLAGS);
  if (h->filedescriptor < 0) {
    h_closeconn(h, "Uanble to open xdcclistfile", errno);
    return;
  }

  if (fstat(h->filedescriptor, &st) < 0) {
    h_closeconn(h, "Uanble to stat xdcclistfile", errno);
    return;
  }

  h->bytessent = 0;
  h->totalsize = st.st_size;
  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength-1, http_header, h->totalsize);
  write(h->clientsocket, tempstr, len);
  h->ul_status = HTTP_STATUS_WAITING;
  mydelete(tempstr);
}

void h_send(http * const h)
{
  size_t attempt;
  ssize_t howmuch, howmuch2;
  long bucket;

  updatecontext();
  if (gdata.debug > 4)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP data send %" LLPRINTFMT "i of %" LLPRINTFMT "i", h->bytessent, h->totalsize);

  if (h->filedescriptor == FD_UNUSED) {
    h_closeconn(h, "Complete", 0);
    return;
  }
 
  bucket = TXSIZE * MAXTXPERLOOP;
  do {
    attempt = min2(bucket - (bucket % TXSIZE), BUFFERSIZE);
    howmuch = read(h->filedescriptor, gdata.sendbuff, attempt);
    if (howmuch < 0 && errno != EAGAIN) {
      outerror(OUTERROR_TYPE_WARN, "Can't read data from file '%s': %s",
              h->file, strerror(errno));
      h_closeconn(h, "Unable to read data from file", errno);
      return;
    }
    if (howmuch <= 0)
      break;
    
    howmuch2 = write(h->clientsocket, gdata.sendbuff, howmuch);
    if (howmuch2 < 0 && errno != EAGAIN) {
      h_closeconn(h, "Connection Lost", errno);
      return;
    }

    howmuch2 = max2(0, howmuch2);
    if (howmuch2 > 0) {
      h->lastcontact = gdata.curtime;
    }

    h->bytessent += howmuch2;
    bucket -= howmuch2;
    if (gdata.debug > 4)
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_BLUE, "File %ld Write %ld", (long)howmuch, (long)howmuch2);

  } while ((bucket >= TXSIZE) && (howmuch2 > 0));

  if (h->bytessent >= h->totalsize) {
    close(h->filedescriptor);
    h->filedescriptor = FD_UNUSED;
  }
}

void h_istimeout(http * const h)
{
  updatecontext();

  if ((gdata.curtime - h->lastcontact) > 5)
    {
      h_closeconn(h, "HTTP Timeout (5 Sec Timeout)", 0);
    }
}

void h_done_select(int changesec)
{
  http *h;

  if (FD_ISSET(http_listen, &gdata.readset)) {
    h_accept();
  }
  h = irlist_get_head(&gdata.https);
  while (h) {
    if (h->ul_status == HTTP_STATUS_GETTING) {
      if (FD_ISSET(h->clientsocket, &gdata.readset)) {
        h_get(h);
      }
    }
    if (h->ul_status == HTTP_STATUS_WAITING) {
      if (FD_ISSET(h->clientsocket, &gdata.readset)) {
        h_get(h);
      }
      if (FD_ISSET(h->clientsocket, &gdata.writeset)) {
        h_send(h);
      }
    }
    if (h->ul_status != HTTP_STATUS_DONE) {
    }
    if (h->ul_status == HTTP_STATUS_DONE) {
      mydelete(h->file);
      h = irlist_delete(&gdata.https, h);
    } else {
      if (changesec)
        h_istimeout(h);
      h = irlist_get_next(h);
    }
  }
}

/* End of File */
