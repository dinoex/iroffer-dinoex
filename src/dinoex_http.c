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

#define MAX_WEBLIST_SIZE	(2 * 1024 * 1024)

static int http_listen = FD_UNUSED;

static const char *http_header_status =
"HTTP/1.0 200 OK\r\n"
"Server: iroffer " VERSIONLONG "\r\n"
"Content-Type: %s\r\n"
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

typedef struct {
  const char *m_ext;
  const char *m_mime;
} http_magic_t;

static const http_magic_t http_magic[] = {
  { ".txt", "text/plain" },
  { ".html", "text/html" },
  { ".htm", "text/html" },
  { ".ico", "image/x-icon" },
  { ".png", "image/png" },
  { ".jpg", "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".gif", "image/gif" },
  { ".css", "text/css" },
  { NULL, "application/octet-stream" }
};

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

  mydelete(h->buffer);
  h->status = HTTP_STATUS_DONE;
}

static void h_error(http * const h, const char *header)
{
  updatecontext();

  h->totalsize = 0;
  write(h->clientsocket, header, strlen(header));
  h->status = HTTP_STATUS_SENDING;
}

static void h_readfile(http * const h, const char *header, const char *file)
{
  char *tempstr;
  const char *ext;
  size_t len;
  struct stat st;
  int i;

  updatecontext();

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
    h_closeconn(h, "Unable to stat file", errno);
    close(h->filedescriptor);
    return;
  }

  ext = strchr(h->file, '.');
  if (ext == NULL)
    ext = ".bin";
  for (i=0; http_magic[i].m_ext; i++) {
    if (strcasecmp(http_magic[i].m_ext, ext) == 0)
      break;
  }
  h->bytessent = 0;
  h->filepos = 0;
  h->totalsize = st.st_size;
  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength-1, header, http_magic[i].m_mime, h->totalsize);
  write(h->clientsocket, tempstr, len);
  mydelete(tempstr);
  h->status = HTTP_STATUS_SENDING;
}

static void h_readbuffer(http * const h, const char *header)
{
  char *tempstr;
  size_t len;

  updatecontext();

  h->bytessent = 0;
  h->totalsize = strlen(h->buffer);
  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength-1, header, http_magic[1].m_mime, h->totalsize);
  write(h->clientsocket, tempstr, len);
  mydelete(tempstr);
  h->status = HTTP_STATUS_SENDING;
  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP response=%ld", (long)(h->totalsize));
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
h_respond(http * const h, const char *format, ...);

static void h_respond(http * const h, const char *format, ...)
{
  va_list args;
  ssize_t len;

  va_start(args, format);
  if (h->left > 0 ) {
    len = vsnprintf(h->end, h->left, format, args);
    if (len < 0) {
      h->end[0] = 0;
      len = 0;
    } else {
      h->end += len;
      h->left -= len;
    }
  }
  va_end(args);
}

static void h_include(http * const h, const char *file)
{
  ssize_t len;
  ssize_t howmuch;
  struct stat st;
  int fd;

  fd = open(file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (fd < 0) {
    if (gdata.debug > 1)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "file not found=%s", file);
    return;
  }
  if (fstat(fd, &st) < 0) {
    outerror(OUTERROR_TYPE_WARN, "Unable to stat file '%s': %s",
             file, strerror(errno));
    return;
  }

  len = st.st_size;
  if (len > h->left) {
    len = h->left;
    outerror(OUTERROR_TYPE_WARN, "File to big '%s'", file);
  }
  howmuch = read(fd, h->end, len);
  if (howmuch < 0) {
    outerror(OUTERROR_TYPE_WARN, "Can't read data from file '%s': %s",
             file, strerror(errno));
    return;
  }
  h->end += howmuch;
  h->left -= howmuch;
}

static int html_hide_locked(const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;

  if (xd->lock == NULL)
    return 0;

  return 1;
}

static char *html_link(const char *caption, const char *url, const char *text)
{
  char *tempstr;

  if (text == NULL)
    text = caption;
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr, maxtextlength-1, "<a title=\"%s\" href=\"%s\">%s</a>", caption, url, text);
  return tempstr;
}

static void h_html_main(http * const h)
{
  xdcc *xd;
  irlist_t grplist = {};
  char *tempstr;
  char *tlink;
  char *tref;
  char *inlist;
  char *desc;
  off_t sizes = 0;
  off_t traffic = 0;
  int groups = 0;
  int packs = 0;
  int agets = 0;
  int nogroup = 0;

  updatecontext();

  tempstr = mycalloc(maxtextlength);
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    packs ++;
    agets += xd->gets;
    traffic += xd->gets * xd->st_size;
    if (xd->group == NULL) {
      if (nogroup == 0) {
        nogroup ++;
        groups ++;
        snprintf(tempstr, maxtextlength-1,
                 "%s %s", ".", "no group");
                inlist = irlist_add(&grplist, strlen(tempstr) + 1);
        strcpy(inlist, tempstr);
      }
      continue;
    }
    if (xd->group_desc == NULL)
      continue;
    groups ++;
    snprintf(tempstr, maxtextlength-1,
             "%s %s", xd->group, xd->group_desc);
            inlist = irlist_add(&grplist, strlen(tempstr) + 1);
    strcpy(inlist, tempstr);
  }
  mydelete(tempstr);
  irlist_sort(&grplist, irlist_sort_cmpfunc_string, NULL);

  h_respond(h, "<h1>%s %s</h1>\n", h->nick, "Group list" );
  h_respond(h, "<table cellpadding=\"2\" cellspacing=\"0\" summary=\"list\">\n<thead>\n<tr>\n");
  h_respond(h, "<th class=\"head\">%s</th>\n", "PACKs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "DLs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "Size");
  h_respond(h, "<th class=\"head\">%s</th>\n", "GROUP");
  h_respond(h, "<th class=\"head\">%s</th>\n", "DESCRIPTION");
  h_respond(h, "</tr>\n</thead>\n<tfoot>\n<tr>\n");

  h_respond(h, "<th class=\"right\">%d</th>\n", packs);
  h_respond(h, "<th class=\"right\">%d</th>\n", agets);
  tempstr = sizestr(0, sizes);
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  h_respond(h, "<th class=\"head\">%d</th>\n", groups);
  tlink = html_link("show all packs in one list", "/?group=*", "all packs");
  tempstr = sizestr(0, traffic);
  h_respond(h, "<th class=\"head\">%s [%s]&nbsp;%s</th>\n", tlink, tempstr, "complete downloaded" );
  mydelete(tempstr);
  mydelete(tlink);
  h_respond(h, "</tr>\n</tfoot>\n<tbody>\n");

  updatecontext();
  inlist = irlist_get_head(&grplist);
  while (inlist) {
    desc = strchr(inlist, ' ');
    if (desc != NULL)
      *(desc++) = 0;
    sizes = 0;
    traffic = 0;
    groups = 0;
    packs = 0;
    agets = 0;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (html_hide_locked(xd))
        continue;
      if (strcmp(inlist, ".") == 0) {
        if (xd->group != NULL)
          continue;
      } else {
        if (xd->group == NULL)
          continue;
        if (strcasecmp(inlist, xd->group) != 0)
          continue;
      }
      packs ++;
      agets += xd->gets;
      sizes += xd->st_size;
      traffic += xd->gets * xd->st_size;
    }

    h_respond(h, "<tr>\n");
    h_respond(h, "<td class=\"right\">%d</td>\n", packs);
    h_respond(h, "<td class=\"right\">%d</td>\n", agets);
    tempstr = sizestr(0, sizes);
    h_respond(h, "<td class=\"right\">%s</td>\n", tempstr);
    mydelete(tempstr);
/* XXXXX escape & in GROUP */
    h_respond(h, "<td class=\"content\">%s</td>\n", inlist);
    tref = mycalloc(maxtextlength);
    snprintf(tref, maxtextlength-1, "/?group=%s", inlist);
/* XXXXX escape & in DESC */
    tlink = html_link("show list of packs", tref, desc);
    mydelete(tref);
    h_respond(h, "<td class=\"content\">%s</td>\n", tlink);
    mydelete(tlink);
    h_respond(h, "</tr>\n");
    inlist = irlist_delete(&grplist, inlist);
  }
}

static void h_html_file(http * const h)
{
  xdcc *xd;
  char *tempstr;
  char *tlabel;
  off_t sizes = 0;
  off_t traffic = 0;
  ssize_t len;
  int packs = 0;
  int agets = 0;
  int num;

  updatecontext();

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (html_hide_locked(xd))
      continue;
    if (strcmp(h->group, "*") != 0) {
      if (strcmp(h->group, ".") == 0) {
        if (xd->group != NULL)
          continue;
      } else {
        if (xd->group == NULL)
          continue;
        if (strcasecmp(h->group, xd->group) != 0)
          continue;
      }
    }
    packs ++;
    agets += xd->gets;
    sizes += xd->st_size;
    traffic += xd->gets * xd->st_size;
  }

  h_respond(h, "<h1>%s %s</h1>\n", h->nick, "File list" );
  h_respond(h, "%s<span class=\"cmd\">/msg %s xdcc send nummer</span></p>\n",
            "Download in IRC with", h->nick);
  h_respond(h, "<table cellpadding=\"2\" cellspacing=\"0\" summary=\"list\">\n<thead>\n<tr>\n");
  h_respond(h, "<th class=\"head\">%s</th>\n", "PACKs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "DLs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "Size");
  tempstr = html_link("back", "/?", "(back)");
  h_respond(h, "<th class=\"head\">%s&nbsp;%s</th>\n", "DESCRIPTION", tempstr);
  mydelete(tempstr);
  h_respond(h, "</tr>\n</thead>\n<tfoot>\n<tr>\n");
  h_respond(h, "<th class=\"right\">%d</th>\n", packs);
  h_respond(h, "<th class=\"right\">%d</th>\n", agets);
  tempstr = sizestr(0, sizes);
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  tempstr = sizestr(0, traffic);
  h_respond(h, "<th class=\"head\">[%s]&nbsp;%s</th>\n", tempstr, "complete downloaded" );
  mydelete(tempstr);
  h_respond(h, "</tr>\n</tfoot>\n<tbody>\n");

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (html_hide_locked(xd))
      continue;
    if (strcmp(h->group, "*") != 0) {
      if (strcmp(h->group, ".") == 0) {
        if (xd->group != NULL)
          continue;
      } else {
        if (xd->group == NULL)
          continue;
        if (strcasecmp(h->group, xd->group) != 0)
          continue;
      }
    }
    num = number_of_pack(xd);
    h_respond(h, "<tr>\n");
    h_respond(h, "<td class=\"right\">#%d</td>\n", num);
    h_respond(h, "<td class=\"right\">%d</td>\n", xd->gets);
    tempstr = sizestr(0, xd->st_size);
    h_respond(h, "<td class=\"right\">%s</td>\n", tempstr);
    mydelete(tempstr);
    tlabel = mycalloc(maxtextlength);
    len = snprintf(tlabel, maxtextlength-1, "%s\n/msg %s xdcc send %d",
                   "Download with:", h->nick, num);
    if (xd->has_md5sum)
      len += snprintf(tlabel + len, maxtextlength - 1 - len, "\nmd5: " MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
    if (xd->has_crc32)
      len += snprintf(tlabel + len, maxtextlength - 1 - len, "\ncrc32: %.8lX", xd->crc32);
    tempstr = mycalloc(maxtextlength);
    len = snprintf(tempstr, maxtextlength-1, "%s", xd->desc);
    if (xd->lock)
      len += snprintf(tempstr + len, maxtextlength - 1 - len, " (%s)", "locked");
    if (xd->note[0])
      len += snprintf(tempstr + len, maxtextlength - 1 - len, "<br>%s", xd->note);
/* XXXXX escape & in DESC */
/* XXXXX escape & in NOTE */
    h_respond(h, "<td class=\"content\"  title=\"%s\">%s</td>\n", tlabel, tempstr);
    mydelete(tempstr);
    mydelete(tlabel);
    h_respond(h, "</tr>\n");
  }
}

static int h_html_index(http * const h)
{
  char *tempstr;
  char *tlabel;
  xdcc *xd;
  ssize_t len;
  ir_uint64 xdccsent;
  int slots;
  int i;

  updatecontext();

  h->support_groups = 0;
  h->nick = gdata.config_nick ? gdata.config_nick : "??";
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL) {
      h->support_groups = 1;
      break;
    }
  }

  if (h->support_groups == 0) {
    mydelete(h->group);
    h->group = mystrdup("*");
  }

  h_include(h, "header.html");
  if (h->group) {
    h_html_file(h);
  } else {
    h_html_main(h);
  }

  h_respond(h, "</tbody>\n</table>\n<table class=\"status\">\n<tbody>\n<tr><td>Version</td>\n");

  tlabel = mycalloc(maxtextlength);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  len  = snprintf(tlabel,       maxtextlength - 1,       "%6s %s\n", tempstr, "Traffic today");
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  len += snprintf(tlabel + len, maxtextlength - 1 - len, "%6s %s\n", tempstr, "Traffic this week");
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  len += snprintf(tlabel + len, maxtextlength - 1 - len, "%6s %s\n", tempstr, "Traffic this month");
  mydelete(tempstr);
  h_respond(h, "<td title=\"%s\">%s</td>\n", tlabel, VERSIONLONG);
  mydelete(tlabel);

  tempstr = mycalloc(maxtextlength);
  getdatestr(tempstr, gdata.curtime, maxtextlength-1);
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "last update");
  h_respond(h, "<td>%s</td>\n", tempstr);
  h_respond(h, "</tr>\n");
  mydelete(tempstr);

  slots = gdata.slotsmax - irlist_size(&gdata.trans);
  if (slots < 0)
    slots = 0;
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "slots open");
  h_respond(h, "<td>%d</td>\n", slots);
  h_respond(h, "</tr>\n");

  for (i=0,xdccsent=0; i<XDCC_SENT_SIZE; i++) {
    xdccsent += (ir_uint64)gdata.xdccsent[i];
  }
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr, maxtextlength - 1, "%1.1fKB/s",
           ((float)xdccsent) / XDCC_SENT_SIZE / 1024.0);
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "Current Bandwidth");
  h_respond(h, "<td>%s</td>\n", tempstr);
  h_respond(h, "</tr>\n");
  mydelete(tempstr);

  h_respond(h, "</tbody>\n</table>\n<br>\n");
  h_respond(h, "<a class=\"credits\" href=\"http://iroffer.dinoex.net/\">%s</a>", "Sourcecode" );
  h_include(h, "footer.html");
  return 0;
}

static char *get_url_param(const char *url, const char *key)
{
  char *result;
  char *end;
  const char *found;

  found = strcasestr(url, key);
  if (found == NULL)
    return NULL;

  result = mystrdup(found + strlen(key));
  end = strchr(result, ' ');
  if (end != NULL)
    *(end++) = 0;
  end = strchr(result, '&');
  if (end != NULL)
    *(end++) = 0;
  return result;
}

static void h_webliste(http * const h, const char *header, const char *url)
{
  size_t guess;

  updatecontext();

  h->group = get_url_param(url, "group=");
  guess = MAX_WEBLIST_SIZE;
  h->buffer = mycalloc(guess);
  h->buffer[ 0 ] = 0;
  h->end = h->buffer;
  h->left = guess - 1;
  h_html_index(h);
  mydelete(h->group);
  h_readbuffer(h, http_header_status);
}

static void h_admin(http * const h, int level, char *url)
{
  updatecontext();

  if (strcasecmp(url, "/") == 0) {
    /* send standtus */
    h_readfile(h, http_header_status, "help-admin-en.txt");
    return;
  }

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
  size_t len;
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

  if (strncasecmp(url, "/?", 2) == 0) {
    /* send standtus */
    h_webliste(h, http_header_status, url);
    return;
  }

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

  if (gdata.http_dir) {
    tempstr = mycalloc(maxtextlength);
    len = snprintf(tempstr, maxtextlength-1, "%s%s", gdata.http_dir, url);
    h_readfile(h, http_header_status, tempstr);
    mydelete(tempstr);
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
  char *data;
  size_t attempt;
  ssize_t howmuch, howmuch2;
  long bucket;

  updatecontext();

  if (h->filedescriptor == FD_UNUSED) {
    if (h->buffer == NULL) {
      h_closeconn(h, "Complete", 0);
      return;
    }
  }

  bucket = TXSIZE * MAXTXPERLOOP;
  do {
    attempt = min2(bucket - (bucket % TXSIZE), BUFFERSIZE);
    if (h->filedescriptor == FD_UNUSED) {
      howmuch = h->totalsize - h->bytessent;
      data = h->buffer + h->bytessent;
    } else {
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
      howmuch = read(h->filedescriptor, gdata.sendbuff, attempt);
      data = gdata.sendbuff;
    }
    if (howmuch < 0 && errno != EAGAIN) {
      outerror(OUTERROR_TYPE_WARN, "Can't read data from file '%s': %s",
              h->file, strerror(errno));
      h_closeconn(h, "Unable to read data from file", errno);
      return;
    }
    if (howmuch <= 0)
      break;

    h->filepos += howmuch;
    howmuch2 = write(h->clientsocket, data, howmuch);
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
    if (h->filedescriptor == FD_UNUSED) {
      mydelete(h->buffer);
    } else {
      close(h->filedescriptor);
      h->filedescriptor = FD_UNUSED;
    }
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
