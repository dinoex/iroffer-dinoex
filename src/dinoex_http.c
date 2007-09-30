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

static int http_listen = FD_UNUSED;

static const char *http_header_status =
"HTTP/1.0 200 OK\r\n"
"Server: iroffer " VERSIONLONG "\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: " LLPRINTFMT "\r\n"
"\r\n";

static const char *http_header_notfound =
"HTTP/1.0 404 Not Found\r\n"
"Server: iroffer " VERSIONLONG "\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 13\r\n"
"\r\n"
"Not Found\r\n"
"\r\n";

static const char *http_header_admin =
"HTTP/1.0 401 Unauthorized\r\n"
"WWW-Authenticate: Basic realm= \"iroffer admin\"\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 26\r\n"
"\r\n"
"Authorization Required\r\n"
"\r\n";

static const char *htpp_auth_key = "Authorization: Basic ";

/*
	BASE 64

        | b64  | b64   | b64   |  b64 |
        | octect1 | octect2 | octect3 |
*/

static unsigned char base64decode[ 256 ] = {
/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,62, 0, 0, 0,63,
  52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0,
   0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25, 0, 0, 0, 0, 0,
   0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void
b64decode_quartet( unsigned char *decoded, const unsigned char *coded )
{
  unsigned char ch;

  ch = base64decode[ coded[ 0 ] ];
  decoded[ 0 ]  = (unsigned char)( ch << 2 );
  ch = base64decode[ coded[ 1 ] ];
  decoded[ 0 ] |= ch >> 4;
  ch &= 0x0F;
  decoded[ 1 ]  = (unsigned char)( ch << 4 );
  ch = base64decode[ coded[ 2 ] ];
  decoded[ 1 ] |= ch >> 2;
  ch &= 0x03;
  decoded[ 2 ]  = (unsigned char)( ch << 6 );
  ch = base64decode[ coded[ 3 ] ];
  decoded[ 2 ] |= ch;
}

static char *
b64decode_string(const unsigned char *coded)
{
  char *dest;
  char *result;
  size_t len;

  len = strlen(coded);
  result = mycalloc(len);
  dest = result;
  while (len >= 4) {
    b64decode_quartet(dest, coded);
    dest += 3;
    coded += 4;
    len -= 4;
  }
  *(dest++) = 0;
  return result;
}

void h_close_listen(void)
{
  if (http_listen == FD_UNUSED)
    return;

  FD_CLR(http_listen, &gdata.readset);
  close(http_listen);
}

static int is_in_badip(long remoteip)
{
  badip *b;

  for (b = irlist_get_head(&gdata.http_ips);
       b;
       b = irlist_get_next(b)) {
    if (b->remoteip == remoteip) {
      b->lastcontact = gdata.curtime;
      if (b->count > 10)
        return 1;
      break;
    }
  }
  return 0;
}

static void count_badip(long remoteip)
{
  badip *b;

  for (b = irlist_get_head(&gdata.http_ips);
       b;
       b = irlist_get_next(b)) {
    if (b->remoteip == remoteip) {
      b->count ++;
      b->lastcontact = gdata.curtime;
      return;
    }
  }

  b = irlist_add(&gdata.http_ips, sizeof(badip));
  b->remoteip = remoteip;
  b->connecttime = gdata.curtime;
  b->lastcontact = gdata.curtime;
  b->count = 1;
}

static void expire_badip(void)
{
  badip *b;

  b = irlist_get_head(&gdata.http_ips);
  while (b) {
    if ((gdata.curtime - b->lastcontact) > 3600) {
      b = irlist_delete(&gdata.http_ips, b);
    } else {
      b = irlist_get_next(b);
    }
  }
}

int h_setup_listen(void)
{
  int tempc;
  int listenport;
  struct sockaddr_in listenaddr;

  updatecontext();

  if (gdata.http_port == 0)
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
  listenaddr.sin_port = htons(gdata.http_port);

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

void h_reash_listen(void)
{
  if (gdata.http_port == 0) {
    h_close_listen();
    return;
  }
  if (http_listen == FD_UNUSED) {
    h_setup_listen();
  }
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
    if (h->status == HTTP_STATUS_GETTING) {
      FD_SET(h->clientsocket, &gdata.readset);
      highests = max2(highests, h->clientsocket);
    }
    if (h->status == HTTP_STATUS_SENDING) {
      FD_SET(h->clientsocket, &gdata.writeset);
      highests = max2(highests, h->clientsocket);
    }
  }
  return highests;
}

/* connections */

static void h_accept(void)
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
  h->status = HTTP_STATUS_GETTING;

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "HTTP connection received from %lu.%lu.%lu.%lu:%d",
          (h->remoteip >> 24) & 0xFF,
          (h->remoteip >> 16) & 0xFF,
          (h->remoteip >>  8) & 0xFF,
          (h->remoteip      ) & 0xFF,
          h->remoteport);
}

static void h_closeconn(http * const h, const char *msg, int errno1)
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
    h->clientsocket = FD_UNUSED;
  }

  if (h->filedescriptor != FD_UNUSED && h->filedescriptor > 2) {
    close(h->filedescriptor);
    h->filedescriptor = FD_UNUSED;
  }

  h->status = HTTP_STATUS_DONE;
}

static void h_error(http * const h, const char *header)
{
  h->totalsize = 0;
  write(h->clientsocket, header, strlen(header));
  h->status = HTTP_STATUS_SENDING;
}

static void h_readfile(http * const h, const char *header, const char *file)
{
  char *tempstr;
  size_t len;
  struct stat st;

  h->bytessent = 0;
  h->file = mystrdup(file);
  h->filedescriptor = open(h->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (h->filedescriptor < 0) {
    if (gdata.debug > 1)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "file not found=%s", file);
    h_error(h, http_header_notfound);
    return;
  }
  if (fstat(h->filedescriptor, &st) < 0) {
    h_closeconn(h, "Uanble to stat file", errno);
    close(h->filedescriptor);
    return;
  }
  h->bytessent = 0;
  h->filepos = 0;
  h->totalsize = st.st_size;
  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength-1, header, h->totalsize);
  write(h->clientsocket, tempstr, len);
  h->status = HTTP_STATUS_SENDING;
  mydelete(tempstr);
}

static void h_admin(http * const h, int level, char *url)
{
  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP not found=%s", url);
  h_error(h, http_header_notfound);
}

static void h_get(http * const h)
{
  char *data;
  char *url;
  char *auth;
  char *end;
  char *tempstr;
  char *passwd;
  int howmuch, howmuch2;
  int i;

  updatecontext();
  if (h->filedescriptor != FD_UNUSED)
    return;

  howmuch2 = BUFFERSIZE;
  for (i=0; i<MAXTXPERLOOP; i++) {
    if (h->bytesgot >= BUFFERSIZE - 1)
      break;
    if (is_fd_readable(h->clientsocket)) {
      howmuch = read(h->clientsocket, gdata.sendbuff + h->bytesgot, howmuch2);
      if (howmuch < 0) {
        h_closeconn(h, "Connection Lost", errno);
        return;
      }
      if (howmuch < 1) {
        h_closeconn(h, "Connection Lost", 0);
        return;
      }
      h->lastcontact = gdata.curtime;
      h->bytesgot += howmuch;
      howmuch2 -= howmuch;
      gdata.sendbuff[h->bytesgot] = 0;
      if (gdata.debug > 3) {
        ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                "HTTP data received data=\n%s", gdata.sendbuff);
      }
    }
  }

  if (strncasecmp(gdata.sendbuff, "GET ", 4 ) != 0) {
    h_closeconn(h, "Bad request", 0);
    return;
  }
  if (is_in_badip(h->remoteip)) {
    h_error(h, http_header_notfound);
    return;
  }
  url = gdata.sendbuff + 4;
  data = strchr(url, ' ');
  if (data != NULL)
    *(data++) = 0;

  if (strcasecmp(url, "/") == 0) {
    /* send standtus */
    h_readfile(h, http_header_status, gdata.xdcclistfile);
    return;
  }

  if ((gdata.http_admin) && (strncasecmp(url, "/admin/", 7) == 0)) {
    auth = strcasestr(data, htpp_auth_key);
    if (auth != NULL) {
      auth += strlen(htpp_auth_key);
      end = strchr(auth, ' ');
      if (end != NULL)
        *(end++) = 0;
      tempstr = b64decode_string(auth);
      passwd = strchr(tempstr, ':');
      if (passwd != NULL)
        *(passwd++) = 0;

      if (strcasecmp(tempstr, gdata.http_admin) == 0) {
        if (verifypass2(gdata.hadminpass, passwd) ) {
          mydelete(tempstr);
          h_admin(h, gdata.hadminlevel, url + 7);
          return;
        }

        if (verifypass2(gdata.adminpass, passwd) ) {
          mydelete(tempstr);
          h_admin(h, gdata.adminlevel, url + 7);
          return;
        }
      }

      mydelete(tempstr);
    }
    count_badip(h->remoteip);
    h_error(h, http_header_admin);
    return;
  }

  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP not found=%s", url);
  h_error(h, http_header_notfound);
}

static void h_send(http * const h)
{
  off_t offset;
  size_t attempt;
  ssize_t howmuch, howmuch2;
  long bucket;

  updatecontext();

  if (h->filedescriptor == FD_UNUSED) {
    h_closeconn(h, "Complete", 0);
    return;
  }

  bucket = TXSIZE * MAXTXPERLOOP;
  do {
    if (h->filepos != h->bytessent) {
      offset = lseek(h->filedescriptor, h->bytessent, SEEK_SET);
      if (offset != h->bytessent) {
        outerror(OUTERROR_TYPE_WARN,"Can't seek location in file '%s': %s",
                 h->file, strerror(errno));
        h_closeconn(h, "Unable to locate data in file", errno);
        return;
      }
      h->filepos = h->bytessent;
    }
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

    h->filepos += howmuch;
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

static void h_istimeout(http * const h)
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
    if (h->status == HTTP_STATUS_GETTING) {
      if (FD_ISSET(h->clientsocket, &gdata.readset)) {
        h_get(h);
      }
    }
    if (h->status == HTTP_STATUS_SENDING) {
      if (FD_ISSET(h->clientsocket, &gdata.writeset)) {
        h_send(h);
      }
    }
    if (h->status == HTTP_STATUS_DONE) {
      mydelete(h->file);
      h = irlist_delete(&gdata.https, h);
    } else {
      if (changesec)
        h_istimeout(h);
      h = irlist_get_next(h);
    }
  }

  if (changesec)
   expire_badip();
}

/* End of File */
