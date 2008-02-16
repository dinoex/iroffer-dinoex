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
#include "dinoex_irc.h"
#include "dinoex_badip.h"
#include "dinoex_misc.h"
#include "dinoex_http.h"

#include <ctype.h>

#ifndef WITHOUT_HTTP

#define MAX_WEBLIST_SIZE	(2 * 1024 * 1024)

typedef struct {
  const char *hg_group;
  const char *hg_desc;
  int hg_packs;
  int hg_agets;
  float hg_rgets;
  off_t hg_sizes;
  off_t hg_traffic;
} html_group_t;

static int http_listen[MAX_VHOSTS];
static int http_family[MAX_VHOSTS];

static const char *http_header_status =
"HTTP/1.1 %d OK\r\n"
"Date: %s\r\n"
"Last-Modified: %s\r\n"
"Server: iroffer-dinoex/" VERSIONLONG "\r\n"
"Content-Type: %s\r\n"
"Connection: close\r\n"
"Content-Length: %" LLPRINTFMT "u\r\n"
"\r\n";

static const char *http_header_notfound =
"HTTP/1.1 404 Not Found\r\n"
"Date: %s\r\n"
"Server: iroffer-dinoex/" VERSIONLONG "\r\n"
"Content-Type: text/plain\r\n"
"Connection: close\r\n"
"Content-Length: 13\r\n"
"\r\n"
"Not Found\r\n"
"\r\n";

static const char *http_header_admin =
"HTTP/1.1 401 Unauthorized\r\n"
"Date: %s\r\n"
"WWW-Authenticate: Basic realm= \"iroffer admin\"\r\n"
"Content-Type: text/plain\r\n"
"Connection: close\r\n"
"Content-Length: 26\r\n"
"\r\n"
"Authorization Required\r\n"
"\r\n";

static const char *htpp_auth_key = "Basic ";

typedef struct {
  const char *m_ext;
  const char *m_mime;
} http_magic_const_t;

static const http_magic_const_t http_magic[] = {
  { "txt", "text/plain" },
  { "html", "text/html" },
  { "htm", "text/html" },
  { "ico", "image/x-icon" },
  { "png", "image/png" },
  { "jpg", "image/jpeg" },
  { "jpeg", "image/jpeg" },
  { "gif", "image/gif" },
  { "css", "text/css" },
  { "js", "application/x-javascript" },
  { NULL, "application/octet-stream" }
};

typedef struct {
  char s_ch;
  const char *s_html;
} http_special_t;

static const http_special_t http_special[] = {
  { '&', "&amp;" },
  { '<', "&lt;" },
  { '>', "&gt;" },
  { '"', "&quot;" },
  { '\'', "&#39;" },
  { 0, NULL },
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
b64decode_quartet(unsigned char *decoded, const unsigned char *coded )
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
b64decode_string(const char *coded)
{
  char *dest;
  char *result;
  size_t len;

  len = strlen(coded);
  result = mycalloc(len);
  dest = result;
  while (len >= 4) {
    b64decode_quartet((unsigned char *)dest, (const unsigned char *)coded);
    dest += 3;
    coded += 4;
    len -= 4;
  }
  *(dest++) = 0;
  return result;
}

static const char *html_mime(const char *file)
{
  const char *ext;
  http_magic_t *mime;
  int i;

  ext = strchr(file, '.');
  if (ext == NULL)
    ext = file;
  else
    ext++;

  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_delete(&gdata.mime_type, mime)) {
    if (strcasecmp(mime->m_ext, ext) != 0)
      return mime->m_mime;
  }

  for (i=0; http_magic[i].m_ext; i++) {
    if (strcasecmp(http_magic[i].m_ext, ext) == 0)
      break;
  }
  return http_magic[i].m_mime;
}

static ssize_t html_encode(char *buffer, ssize_t max, const char *src)
{
  char *dest = buffer;
  ssize_t len;
  int i;
  char ch;

  max--;
  for (;;) {
    if (max <= 0)
      break;
    ch = *(src++);
    if (ch == 0)
      break;
    for (i=0; http_special[i].s_ch; i++) {
      if (ch != http_special[i].s_ch)
        continue;
      len = strlen(http_special[i].s_html);
      if (len > max) {
        max = 0;
        break;
      }
      strncpy(dest, http_special[i].s_html, len);
      dest += len;
      max -= len;
      break;
    }
    if (http_special[i].s_ch != 0)
      continue;
    switch (ch) {
    case 0x03: /* color */
      if (!isdigit(*src)) break;
      src++;
      if (isdigit(*src)) src++;
      if ((*src) != ',') break;
      src++;
      if (isdigit(*src)) src++;
      if (isdigit(*src)) src++;
      break;
    case 0x02: /* bold */
    case 0x0F: /* end formatting */
    case 0x16: /* inverse */
    case 0x1F: /* underline */
      break;
    default:
      *(dest++) = ch;
    }
  }
  *dest = 0;
  len = dest - buffer;
  return len;
}

static ssize_t html_decode(char *buffer, ssize_t max, const char *src)
{
  char *dest = buffer;
  const char *code;
  ssize_t len;
  int i;
  int hex;
  char ch;

  max--;
  for (;;) {
    if (max <= 0)
      break;
    code = src;
    ch = *(src++);
    if (ch == 0)
      break;
    for (i=0; http_special[i].s_ch; i++) {
      len = strlen(http_special[i].s_html);
      if (strncmp(code, http_special[i].s_html, len) != 0)
        continue;
      *(dest++) = http_special[i].s_ch;
      src += len - 1;
      break;
    }
    if (http_special[i].s_ch != 0)
      continue;


    switch (ch) {
    case '+':
    case '"':
      *(dest++) = ' ';
      break;
    case '%': /* html */
      hex = 32;
      sscanf(src, "%2x", &hex);
      *(dest++) = hex;
      src++;
      src++;
      break;
    default:
      *(dest++) = ch;
    }
  }
  *dest = 0;
  len = dest - buffer;
  return len;
}

static ssize_t pattern_decode(char *buffer, ssize_t max, const char *src)
{
  char *dest = buffer;
  ssize_t len;
  char ch;

  max--;
  max--;
  max--;
  *(dest++) = '*';
  for (;;) {
    if (max <= 0)
      break;
    ch = *(src++);
    if (ch == 0)
      break;

    switch (ch) {
    case ' ':
      *(dest++) = '*';
      break;
    default:
      *(dest++) = ch;
    }
  }
  *(dest++) = '*';
  *dest = 0;
  len = dest - buffer;
  return len;
}

static char *html_str_split(char *buffer, char delimiter)
{
  char *data;

  data = strchr(buffer, delimiter);
  if (data != NULL)
    *(data++) = 0;

  return data;
}

void h_close_listen(void)
{
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (http_listen[i] != FD_UNUSED) {
      FD_CLR(http_listen[i], &gdata.readset);
      close(http_listen[i]);
      http_listen[i] = FD_UNUSED;
    }
  }
}

static int h_open_listen(int i)
{
  char *vhost = NULL;
  char *msg;
  int rc;
  ir_sockaddr_union_t listenaddr;

  updatecontext();

  vhost = irlist_get_nth(&gdata.http_vhost, i);
  if (vhost == NULL)
    return 1;

  rc = open_listen(0, &listenaddr, &(http_listen[i]), gdata.http_port, 1, 0, vhost);
  if (rc != 0)
    return 1;

  http_family[i] = listenaddr.sa.sa_family;
  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &listenaddr.sa, 0);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "HTTP SERVER waiting for connection on %s",  msg);
  mydelete(msg);
  return 0;
}

int h_setup_listen(void)
{
  int i;
  int rc = 0;

  updatecontext();

  for (i=0; i<MAX_VHOSTS; i++) {
    http_listen[i] = FD_UNUSED;
    http_family[i] = 0;
  }

  if (gdata.http_port == 0)
    return 1;

  for (i=0; i<MAX_VHOSTS; i++) {
    rc += h_open_listen(i);
  }
  return rc;
}

void h_reash_listen(void)
{
  h_close_listen();
  if (gdata.http_port == 0)
    return;
  h_setup_listen();
}

int h_select_fdset(int highests)
{
  http *h;
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (http_listen[i] != FD_UNUSED) {
      FD_SET(http_listen[i], &gdata.readset);
      highests = max2(highests, http_listen[i]);
    }
  }

  for (h = irlist_get_head(&gdata.https);
       h;
       h = irlist_get_next(h)) {
    if (h->status == HTTP_STATUS_GETTING) {
      FD_SET(h->con.clientsocket, &gdata.readset);
      highests = max2(highests, h->con.clientsocket);
    }
    if (h->status == HTTP_STATUS_POST) {
      FD_SET(h->con.clientsocket, &gdata.readset);
      highests = max2(highests, h->con.clientsocket);
    }
    if (h->status == HTTP_STATUS_SENDING) {
      FD_SET(h->con.clientsocket, &gdata.writeset);
      highests = max2(highests, h->con.clientsocket);
    }
  }
  return highests;
}

/* connections */

static void h_closeconn(http * const h, const char *msg, int errno1)
{
  updatecontext();

  if (errno1) {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "HTTP: Connection closed: %s (%s)", msg, strerror(errno1));
  } else {
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "HTTP: Connection closed: %s", msg);
  }

  if (gdata.debug > 0) {
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "clientsock = %d", h->con.clientsocket);
  }

  if (h->con.clientsocket != FD_UNUSED && h->con.clientsocket > 2) {
    FD_CLR(h->con.clientsocket, &gdata.writeset);
    FD_CLR(h->con.clientsocket, &gdata.readset);
    shutdown_close(h->con.clientsocket);
    h->con.clientsocket = FD_UNUSED;
  }

  if (h->filedescriptor != FD_UNUSED && h->filedescriptor > 2) {
    close(h->filedescriptor);
    h->filedescriptor = FD_UNUSED;
  }

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
  h->status = HTTP_STATUS_DONE;
}

static void h_accept(int i)
{
  SIGNEDSOCK int addrlen;
  ir_sockaddr_union_t remoteaddr;
  char *msg;
  http *h;
  int clientsocket;

  updatecontext();

  if (http_family[i] != AF_INET) {
    addrlen = sizeof (struct sockaddr_in6);
    clientsocket = accept(http_listen[i], &remoteaddr.sa, &addrlen);
  } else {
    addrlen = sizeof (struct sockaddr_in);
    clientsocket = accept(http_listen[i], &remoteaddr.sa, &addrlen);
  }
  if (clientsocket < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    return;
  }

  if (set_socket_nonblocking(clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  h = irlist_add(&gdata.https, sizeof(http));
  h->filedescriptor = FD_UNUSED;
  h->con.clientsocket = clientsocket;
  h->con.remote = remoteaddr;
  if (http_family[i] != AF_INET) {
    h->con.family = remoteaddr.sin6.sin6_family;
    h->con.remoteport = ntohs(remoteaddr.sin6.sin6_port);
  } else {
    h->con.family = remoteaddr.sin.sin_family;
    h->con.remoteport = ntohs(remoteaddr.sin.sin_port);
  }
  h->con.connecttime = gdata.curtime;
  h->con.lastcontact = gdata.curtime;
  h->status = HTTP_STATUS_GETTING;

  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &remoteaddr.sa, addrlen);
  h->con.remoteaddr = mystrdup(msg);
  mydelete(msg);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "HTTP connection received from %s",  h->con.remoteaddr);

  if (is_in_badip(&(h->con.remote)))
    h_closeconn(h, "HTTP connection ignored", 0);
}

static void h_write_header(http * const h, const char *header)
{
  char *tempstr;
  char *date;
  struct tm *localt;
  size_t len;

  localt = gmtime(&gdata.curtime);
  tempstr = mycalloc(maxtextlength);
  date = mycalloc(maxtextlengthshort);
  strftime(date, maxtextlengthshort - 1, "%a, %d %b %Y %T %Z", localt);
  len = snprintf(tempstr, maxtextlength-1, header, date);
  mydelete(date);
  write(h->con.clientsocket, tempstr, len);
  mydelete(tempstr);
}

static void h_write_status(http * const h, const char *mime, time_t *now)
{
  char *tempstr;
  char *date;
  char *last = NULL;
  struct tm *localt;
  size_t len;
  int http_status = 200;

  tempstr = mycalloc(maxtextlength);
  localt = gmtime(&gdata.curtime);
  date = mycalloc(maxtextlengthshort);
  strftime(date, maxtextlengthshort - 1, "%a, %d %b %Y %T %Z", localt);
  if (now) {
    last = mycalloc(maxtextlengthshort);
    localt = gmtime(now);
    strftime(last, maxtextlengthshort - 1, "%a, %d %b %Y %T %Z", localt);
    if (h->modified) {
      if (strcmp(last, h->modified) == 0) {
        http_status = 304;
        h->head = 1;
      }
    }
  }
  len = snprintf(tempstr, maxtextlength-1, http_header_status, http_status, date, last ? last : date, html_mime(mime), h->totalsize);
  mydelete(last);
  mydelete(date);
  write(h->con.clientsocket, tempstr, len);
  mydelete(tempstr);
  if (h->head)
    h->totalsize = 0;
}

static void h_error(http * const h, const char *header)
{
  updatecontext();

  h->totalsize = 0;
  h_write_header(h, header);
  h->status = HTTP_STATUS_SENDING;
}

static void h_readfile(http * const h, const char *file)
{
  struct stat st;

  updatecontext();

  h->bytessent = 0;
  h->file = mystrdup(file);
  h->filedescriptor = open(h->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (h->filedescriptor < 0) {
    if (gdata.debug > 1)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "File not found: '%s'", file);
    h->filedescriptor = FD_UNUSED;
    h_error(h, http_header_notfound);
    return;
  }
  if (fstat(h->filedescriptor, &st) < 0) {
    if (gdata.debug > 1)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "Unable to stat file '%s': %s",
              file, strerror(errno));
    close(h->filedescriptor);
    h->filedescriptor = FD_UNUSED;
    h_error(h, http_header_notfound);
    return;
  }

  h->bytessent = 0;
  h->filepos = 0;
  h->totalsize = st.st_size;
  h_write_status(h, h->file, &st.st_mtime);
  h->status = HTTP_STATUS_SENDING;
}

static void h_readbuffer(http * const h)
{
  updatecontext();

  h->bytessent = 0;
  h->totalsize = strlen(h->buffer_out);
  h_write_status(h, "html", NULL);
  h->status = HTTP_STATUS_SENDING;
  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP response %ld bytes", (long)(h->totalsize));
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

static off_t h_stat(const char *path)
{
  struct stat st;

  if (stat(path, &st) < 0)
    return 0;

  return st.st_size;
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
              "File not found: '%s'", file);
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
  close(fd);
  h->end += howmuch;
  h->left -= howmuch;
}

typedef int (*cmpfunc_t)(const void *a, const void *b);

static int html_order_group(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return strcmp(hg1->hg_group, hg2->hg_group);
}

static int html_order_pack(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return hg2->hg_packs - hg1->hg_packs;
}

static int html_order_gets(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return hg2->hg_agets - hg1->hg_agets;
}

static int html_order_rget(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return hg2->hg_rgets - hg1->hg_rgets;
}

static int html_cmp_offset(off_t diff)
{
  if (diff < 0)
    return -1;
  if (diff > 0)
    return 1;
  return 0;
}

static int html_order_size(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return html_cmp_offset(hg2->hg_sizes - hg1->hg_sizes);
}

static int html_order_tvol(const void *a, const void *b)
{
  const html_group_t *hg1 = (const html_group_t *)a;
  const html_group_t *hg2 = (const html_group_t *)b;

  return html_cmp_offset(hg2->hg_traffic - hg1->hg_traffic);
}

static cmpfunc_t html_order_func(const char *order)
{
  if (order == NULL)
    return html_order_group;

  if (strcmp(order, "pack") == 0)
    return html_order_pack;

  if (strcmp(order, "gets") == 0)
    return html_order_gets;

  if (strcmp(order, "rget") == 0)
    return html_order_rget;

  if (strcmp(order, "size") == 0)
    return html_order_size;

  if (strcmp(order, "tvol") == 0)
    return html_order_tvol;

  return html_order_group;
}

static int html_link_start;

static int html_link_option(char *str, size_t size, const char *option, const char *val)
{
  char *tempstr;

  size_t len = 0;
  if (html_link_start++ > 0) {
    len = snprintf(str, size, "&amp;");
  }
  tempstr = mycalloc(maxtextlength);
  html_encode(tempstr, maxtextlength - 1, val);
  len += snprintf(str + len, size - len, "%s=%s", option, tempstr);
  mydelete(tempstr);
  return len;
}

static char *html_link_build(const char *class, const char *caption, const char *text,
				const char *group, int traffic, const char *order)
{
  char *tempstr;
  size_t len;

  tempstr = mycalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength - 1, "<a%s title=\"%s\" href=\"/?", class, caption);
  html_link_start = 0;
  if (group)
    len += html_link_option(tempstr + len, maxtextlength -1 - len, "group", group);
  if (traffic)
    len += html_link_option(tempstr + len, maxtextlength -1 - len, "traffic", "1");
  if (order)
    len += html_link_option(tempstr + len, maxtextlength -1 - len, "order", order);
  len += snprintf(tempstr + len, maxtextlength -1 - len, "\">%s</a>", text);
  return tempstr;
}

static char *h_html_link_more(http * const h, const char *caption, const char *text)
{
  int traffic;

  traffic = (h->traffic) ? 0 : 1; /* Toggle Traffic */
  return html_link_build("", caption, text, h->group, traffic, h->order);
}

static char *h_html_link_group(http * const h, const char *caption, const char *text, const char *group)
{
  return html_link_build("", caption, text, group, h->traffic, h->order);
}

static char *h_html_link_order(http * const h, const char *caption, const char *text, const char *order)
{
  if (order == NULL) {
    if (h->order == NULL) {
      return mystrdup(text);
    }
  } else {
    if (h->order != NULL) {
      if (strcasecmp(order, h->order) == 0) {
        return mystrdup(text);
      }
    }
  }
  return html_link_build(" class=\"head\"", caption, text, h->group, h->traffic, order);
}

static float gets_per_pack(int agets, int packs)
{
  float result = 0.0;

  if (packs > 0) {
    result = (float)agets;
    result /= packs;
  }
  return result;
}

static int h_html_filter_main(http * const h, xdcc *xd, const char *group)
{
  if (strcmp(group, ".") == 0) {
    if (xd->group != NULL)
      return 1;
  } else {
    if (xd->group == NULL)
      return 1;
    if (strcasecmp(group, xd->group) != 0)
      return 1;
  }
  return 0;
}

static int h_html_filter_group(http * const h, xdcc *xd)
{
  if (h->group != NULL) {
    if (strcmp(h->group, "*") != 0) {
      if (strcmp(h->group, ".") == 0) {
        if (xd->group != NULL)
          return 1;
      } else {
        if (xd->group == NULL)
          return 1;
        if (strcasecmp(h->group, xd->group) != 0)
          return 1;
      }
    }
  }
  if (h->pattern) {
    if (fnmatch(h->pattern, xd->desc, FNM_CASEFOLD) != 0)
      return 1;
  }
  return 0;
}

static void h_html_search(http * const h)
{
  if (!gdata.http_search)
    return;

  h_respond(h, "<form action=\"\" method=\"post\">\n");
  h_respond(h, "%s&nbsp;\n", "Search");
  h_respond(h, "<input type=\"text\" name=\"search\" value=\"%s\" size=30>&nbsp;\n", (h->search) ? h->search : "");
  h_respond(h, "<input type=\"submit\" name=\"submit\" value=\" %s \">\n", "Search");
  h_respond(h, "</form>\n" );
}

static void h_html_main(http * const h)
{
  xdcc *xd;
  irlist_t grplist = {0, 0, 0};
  char *tempstr;
  char *tlink;
  char *savegroup;
  char *savedesc;
  html_group_t *hg;
  cmpfunc_t order_func;
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
    sizes += xd->st_size;
    traffic += xd->gets * xd->st_size;
    if (xd->group == NULL) {
      if (nogroup == 0) {
        nogroup ++;
        groups ++;
        snprintf(tempstr, maxtextlength-1,
                 "%s %s", ".", "no group");
        hg = irlist_add(&grplist, sizeof(html_group_t));
        hg->hg_group = ".";
        hg->hg_desc = "no group";
      }
      continue;
    }
    if (xd->group_desc == NULL)
      continue;
    groups ++;

    snprintf(tempstr, maxtextlength-1,
             "%s %s", xd->group, xd->group_desc);
    hg = irlist_add(&grplist, sizeof(html_group_t));
    hg->hg_group = xd->group;
    hg->hg_desc = xd->group_desc;
  }
  mydelete(tempstr);
  for (hg = irlist_get_head(&grplist);
       hg;
       hg = irlist_get_next(hg)) {
    hg->hg_sizes = 0;
    hg->hg_traffic = 0;
    hg->hg_packs = 0;
    hg->hg_agets = 0;
    hg->hg_rgets = 0;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (hide_pack(xd))
        continue;

      if (h_html_filter_main(h, xd, hg->hg_group))
        continue;

      hg->hg_packs ++;
      hg->hg_agets += xd->gets;
      hg->hg_sizes += xd->st_size;
      if (h->traffic)
        hg->hg_traffic += xd->gets * xd->st_size;
    }
    if (h->traffic)
      hg->hg_rgets = gets_per_pack(hg->hg_agets, hg->hg_packs);
  }
  order_func = html_order_func(h->order);
  irlist_sort(&grplist, order_func);

  h_respond(h, "<h1>%s %s</h1>\n", h->nick, "Group list" );
  h_html_search(h);
  if (gdata.http_search)
    h_respond(h, "<br>\n");
  h_respond(h, "<table cellpadding=\"2\" cellspacing=\"0\" summary=\"list\">\n<thead>\n<tr>\n");
  tempstr = h_html_link_order(h, "sort by pack-Nr.", "PACKs", "pack");
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  tempstr = h_html_link_order(h, "sort by downloads", "DLs", "gets");
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  if (h->traffic) {
    tempstr = h_html_link_order(h, "sort by downloads per file", "DLs/Pack", "rget");
    h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
    mydelete(tempstr);
  }
  tempstr = h_html_link_order(h, "sort by size of file", "Size", "size");
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  if (h->traffic) {
    tempstr = h_html_link_order(h, "sort by traffic", "Traffic", "tvol");
    h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
    mydelete(tempstr);
  }
  tempstr = h_html_link_order(h, "sort by group", "GROUP", NULL);
  h_respond(h, "<th class=\"head\">%s</th>\n", tempstr);
  mydelete(tempstr);
  if (h->traffic)
    tlink = h_html_link_more(h, "hide traffic", "(less)");
  else
    tlink = h_html_link_more(h, "show traffic", "(more)");
  h_respond(h, "<th class=\"head\">%s&nbsp;%s</th>\n", "DESCRIPTION", tlink);
  mydelete(tlink);
  h_respond(h, "</tr>\n</thead>\n<tfoot>\n<tr>\n");

  h_respond(h, "<th class=\"right\">%d</th>\n", packs);
  h_respond(h, "<th class=\"right\">%d</th>\n", agets);
  if (h->traffic)
    h_respond(h, "<th class=\"right\">%.1f</th>\n", gets_per_pack(agets, packs));
  tempstr = sizestr(0, sizes);
  h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
  mydelete(tempstr);
  if (h->traffic) {
    tempstr = sizestr(0, traffic);
    h_respond(h, "<th class=\"right\">%s</th>\n", tempstr);
    mydelete(tempstr);
  }
  h_respond(h, "<th class=\"head\">%d</th>\n", groups);
  tlink = h_html_link_group(h, "show all packs in one list", "all packs", "*");
  tempstr = sizestr(0, traffic);
  h_respond(h, "<th class=\"head\">%s [%s]&nbsp;%s</th>\n", tlink, tempstr, "complete downloaded" );
  mydelete(tempstr);
  mydelete(tlink);
  h_respond(h, "</tr>\n</tfoot>\n<tbody>\n");

  updatecontext();
  hg = irlist_get_head(&grplist);
  while (hg) {
    h_respond(h, "<tr>\n");
    h_respond(h, "<td class=\"right\">%d</td>\n", hg->hg_packs);
    h_respond(h, "<td class=\"right\">%d</td>\n", hg->hg_agets);
    if (h->traffic)
      h_respond(h, "<td class=\"right\">%.1f</td>\n", hg->hg_rgets);
    tempstr = sizestr(0, hg->hg_sizes);
    h_respond(h, "<td class=\"right\">%s</td>\n", tempstr);
    mydelete(tempstr);
    if (h->traffic) {
      tempstr = sizestr(0, hg->hg_traffic);
      h_respond(h, "<td class=\"right\">%s</td>\n", tempstr);
      mydelete(tempstr);
    }
    savegroup = mycalloc(maxtextlength);
    html_encode(savegroup, maxtextlength, hg->hg_group);
    h_respond(h, "<td class=\"content\">%s</td>\n", savegroup);
    mydelete(savegroup);
    savedesc = mycalloc(maxtextlength);
    html_encode(savedesc, maxtextlength, hg->hg_desc);
    tlink = h_html_link_group(h, "show list of packs", savedesc, hg->hg_group);
    mydelete(savedesc);
    h_respond(h, "<td class=\"content\">%s</td>\n", tlink);
    mydelete(tlink);
    h_respond(h, "</tr>\n");
    hg = irlist_delete(&grplist, hg);
  }
}

static void h_html_file(http * const h)
{
  xdcc *xd;
  char *tempstr;
  char *javalink;
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
    if (hide_pack(xd))
      continue;

    if (h_html_filter_group(h, xd))
      continue;

    packs ++;
    agets += xd->gets;
    sizes += xd->st_size;
    traffic += xd->gets * xd->st_size;
  }

  h_respond(h, "<h1>%s %s</h1>\n", h->nick, "File list" );
  h_html_search(h);
  h_respond(h, "<p>%s<span class=\"cmd\">/msg %s xdcc send nummer</span></p>\n",
            "Download in IRC with: ", h->nick);
  h_respond(h, "<table cellpadding=\"2\" cellspacing=\"0\" summary=\"list\">\n<thead>\n<tr>\n");
  h_respond(h, "<th class=\"head\">%s</th>\n", "PACKs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "DLs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "Size");
  tempstr = h_html_link_group(h, "back", "(back)", NULL);
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
    if (hide_pack(xd))
      continue;

    if (h_html_filter_group(h, xd))
      continue;

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
    html_encode(tempstr, maxtextlength, xd->desc);
    len = strlen(tempstr);
    if (xd->lock)
      len += snprintf(tempstr + len, maxtextlength - 1 - len, " (%s)", "locked");
    if (xd->note[0]) {
      len += snprintf(tempstr + len, maxtextlength - 1 - len, "<br>");
      len += html_encode(tempstr + len, maxtextlength - 1 - len, xd->note);
    }
    javalink = mycalloc(maxtextlength);
    len = snprintf(javalink, maxtextlength - 1,
                   "<a href=\"javascript:ToClipboard('/msg %s xdcc send %d');\">%s</a>",
                   h->nick, num, tempstr);
    mydelete(tempstr);
    h_respond(h, "<td class=\"content\"  title=\"%s\">%s</td>\n", tlabel, javalink);
    mydelete(javalink);
    mydelete(tlabel);
    h_respond(h, "</tr>\n");
  }
}

static void h_html_weblist_info(http * const h, char *key, char *text)
{
  char *tempstr = NULL;

  updatecontext();

  while (text != NULL) {
    if (strcmp(key, "uptime") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      tempstr = getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
      break;
    }
    if (strcmp(key, "minspeed") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.transferminspeed);
      break;
    }
    if (strcmp(key, "maxspeed") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.transfermaxspeed);
      break;
    }
    if (strcmp(key, "cap") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort - 1, "%i.0KB/s", gdata.maxb / 4);
      break;
    }
    if (strcmp(key, "record") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.record);
      break;
    }
    if (strcmp(key, "send") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort - 1, "%1.1fKB/s", gdata.sentrecord);
      break;
    }
    if (strcmp(key, "daily") == 0) {
      tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
      break;
    }
    if (strcmp(key, "weekly") == 0) {
      tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
      break;
    }
    if (strcmp(key, "monthly") == 0) {
      tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
      break;
    }
    outerror(OUTERROR_TYPE_WARN, "Unknown weblist_info: %s", key);
    return;
  }
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", text);
  h_respond(h, "<td>%s</td>\n", tempstr);
  h_respond(h, "</tr>\n");
  mydelete(tempstr);
}

static char* html_getdatestr(char* str, time_t Tp, int len)
{
  const char *format;
  struct tm *localt = NULL;
  ssize_t llen;

  localt = localtime(&Tp);
  format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";
  llen = strftime(str, len, format, localt);
  if ((llen == 0) || (llen == len))
    str[0] = '\0';
  return str;
}

static int h_html_index(http * const h)
{
  char *info;
  char *buffer;
  char *text;
  char *tempstr;
  char *clean;
  char *tlabel;
  ssize_t len;
  int slots;

  updatecontext();

  h->nick = save_nick(gdata.config_nick);
  if (gdata.support_groups == 0) {
    mydelete(h->group);
    h->group = mystrdup("*");
  }
  if (h->search) {
    if (gdata.http_search) {
      mydelete(h->group);
      h->group = mystrdup("*");
      len = strlen(h->search) + 1;
      clean = mycalloc(len);
      html_decode(clean, len, h->search);
      mydelete(h->search);
      len = strlen(clean) + 3;
      h->pattern = mycalloc(len);
      pattern_decode(h->pattern, len, clean);
      h->search = mycalloc(maxtextlength);
      html_encode(h->search, maxtextlength - 1, clean);
      mydelete(clean);
    } else {
      mydelete(h->search);
    }
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
  html_getdatestr(tempstr, gdata.curtime, maxtextlength-1);
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

  tempstr = get_current_bandwidth();
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "Current Bandwidth");
  h_respond(h, "<td>%s</td>\n", tempstr);
  h_respond(h, "</tr>\n");
  mydelete(tempstr);

  for (info = irlist_get_head(&gdata.weblist_info);
       info;
       info = irlist_get_next(info)) {

    buffer = mystrdup(info);
    text = strchr(buffer, ' ');
    if (text == NULL) {
      mydelete(buffer);
      continue;
    }
    *(text++) = 0;
    clean_quotes(text);
    h_html_weblist_info(h, buffer, text);
    mydelete(buffer);
  }

  h_respond(h, "</tbody>\n</table>\n<br>\n");
  h_respond(h, "<a class=\"credits\" href=\"http://iroffer.dinoex.net/\">%s</a>\n", "Sourcecode" );
  h_include(h, "footer.html");
  return 0;
}

static char *get_url_param(const char *url, const char *key)
{
  char *result;
  const char *found;

  found = strcasestr(url, key);
  if (found == NULL)
    return NULL;

  result = mystrdup(found + strlen(key));
  html_str_split(result, ' ');
  html_str_split(result, '&');
  return result;
}

static int get_url_number(const char *url, const char *key)
{
  char *result;
  int val;

  result = get_url_param(url, key);
  if (result == NULL)
    return 0;

  val = atoi(result);
  mydelete(result);
  return val;
}

static void h_webliste(http * const h, const char *body)
{
  size_t guess;

  updatecontext();

  if (body)
    h->search = get_url_param(body, "search=");
  h->group = get_url_param(h->url, "group=");
  h->order = get_url_param(h->url, "order=");
  h->traffic = get_url_number(h->url, "traffic=");
  guess = 2048;
  guess += irlist_size(&gdata.xdccs) * 300;
  guess += h_stat("header.html");
  guess += h_stat("footer.html");
  h->buffer_out = mycalloc(guess);
  h->buffer_out[ 0 ] = 0;
  h->end = h->buffer_out;
  h->left = guess - 1;
  h_html_index(h);
  mydelete(h->group);
  mydelete(h->order);
  mydelete(h->search);
  mydelete(h->pattern);
  h_readbuffer(h);
}

static void h_admin(http * const h, int level)
{
  updatecontext();

  if (strcasecmp(h->url, "") == 0) {
    /* send standtus */
    h_readfile(h, "help-admin-en.txt");
    return;
  }

  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP not found: '%s'", h->url);
  h_error(h, http_header_notfound);
}

static char *h_bad_request(http * const h)
{
  char *url;
  url = (char *)gdata.sendbuff;

  h->modified = NULL;
  h->post = 0;
  h->head = 0;
  if (strncasecmp(url, "GET ", 4 ) == 0) {
    url += 4;
    return url;
  }
  if (strncasecmp(url, "POST ", 5 ) == 0) {
    h->post = 1;
    url += 5;
    return url;
  }
  if (strncasecmp(url, "HEAD ", 5 ) == 0) {
    h->head = 1;
    url += 5;
    return url;
  }
  return NULL;
}

static char *h_read_http(http * const h)
{
  int howmuch, howmuch2;
  int i;

  updatecontext();

  h->bytesgot = 0;
  gdata.sendbuff[0] = 0;
  howmuch2 = BUFFERSIZE;
  for (i=0; i<MAXTXPERLOOP; i++) {
    if (h->bytesgot >= BUFFERSIZE - 1)
      break;
    if (!is_fd_readable(h->con.clientsocket))
      continue;
    howmuch = read(h->con.clientsocket, gdata.sendbuff + h->bytesgot, howmuch2);
    if (howmuch < 0) {
      h_closeconn(h, "Connection Lost", errno);
      return NULL;
    }
    if (howmuch < 1) {
      h_closeconn(h, "Connection Lost", 0);
      return NULL;
    }
    h->con.lastcontact = gdata.curtime;
    h->bytesgot += howmuch;
    howmuch2 -= howmuch;
    gdata.sendbuff[h->bytesgot] = 0;
    if (gdata.debug > 3) {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "HTTP data received data=\n%s", gdata.sendbuff);
    }
  }
  return (char *)gdata.sendbuff;
}

static void html_str_prefix(char **str, int len)
{
  char *new;

  new = mystrdup(*str + len);
  mydelete(*str);
  *str = new;
}

static void h_parse(http * const h, char *body)
{
  const char *auth;
  char *end;
  char *tempstr;
  char *passwd;
  size_t len;

  updatecontext();

  if (strncasecmp(h->url, "/?", 2) == 0) {
    /* send standtus */
    h_webliste(h, body);
    return;
  }

  if (strcmp(h->url, "/") == 0) {
    /* send standtus */
    h_readfile(h, gdata.xdcclistfile);
    return;
  }

  if ((gdata.http_admin) && (strncasecmp(h->url, "/admin/", 7) == 0)) {
    html_str_prefix(&(h->url), 0);
    auth = strcasestr(body, htpp_auth_key);
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
          h_admin(h, gdata.hadminlevel);
          return;
        }

        if (verifypass2(gdata.adminpass, passwd) ) {
          mydelete(tempstr);
          h_admin(h, gdata.adminlevel);
          return;
        }
      }

      mydelete(tempstr);
    }
    count_badip(&(h->con.remote));
    h_error(h, http_header_admin);
    return;
  }

  if (gdata.http_dir) {
    tempstr = mycalloc(maxtextlength);
    len = snprintf(tempstr, maxtextlength-1, "%s%s", gdata.http_dir, h->url);
    h_readfile(h, tempstr);
    mydelete(tempstr);
    return;
  }

  if (gdata.debug > 1)
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "HTTP not found: '%s'", h->url);
  h_error(h, http_header_notfound);
}

static void h_get(http * const h)
{
  char *data;
  char *hval;

  updatecontext();

  if (h->filedescriptor != FD_UNUSED)
    return;

  data = h_read_http(h);
  if (data == NULL)
    return;

  h->url = h_bad_request(h);
  if (h->url == NULL) {
    h_closeconn(h, "Bad request", 0);
    return;
  }

  /* parse header */
  data = html_str_split(h->url, ' ');
  for (data = strtok(data, "\n");
       data;
       data = strtok(NULL, "\n")) {
    html_str_split(data, '\r');
    if (strlen(data) == 0) {
      data = strtok(NULL, "\n");
      break;
    }
    hval = html_str_split(data, ':');
    if (!hval)
      continue;

    while (hval[0] == ' ')
      hval++;

    if (strcmp(data, "If-Modified-Since") == 0) {
      h->modified = mystrdup(hval);
      continue;
    }
    if (strcmp(data, "Authorization") == 0) {
      h->authorization = mystrdup(hval);
      continue;
    }
  }

  hval = mystrdup(h->url);
  h->url = hval;
  if ((h->post) && (!data)) {
    h->status = HTTP_STATUS_POST;
    return;
  }

  h_parse(h, data);
}

static void h_post(http * const h)
{
  char *data;

  if (h->filedescriptor != FD_UNUSED)
    return;

  data = h_read_http(h);
  if (data == NULL)
    return;

  if (h->bytesgot == 0)
    return;

  h_parse(h, data);
}

static void h_send(http * const h)
{
  off_t offset;
  char *data;
  size_t attempt;
  ssize_t howmuch, howmuch2;
  long bucket;
  int errno2;

  updatecontext();

  if (h->filedescriptor == FD_UNUSED) {
    if (h->buffer_out == NULL) {
      h_closeconn(h, "Complete", 0);
      return;
    }
  }

  bucket = TXSIZE * MAXTXPERLOOP;
  do {
    attempt = min2(bucket - (bucket % TXSIZE), BUFFERSIZE);
    if (h->filedescriptor == FD_UNUSED) {
      howmuch = h->totalsize - h->bytessent;
      data = h->buffer_out + h->bytessent;
    } else {
      if (h->filepos != h->bytessent) {
        offset = lseek(h->filedescriptor, h->bytessent, SEEK_SET);
        if (offset != h->bytessent) {
          errno2 = errno;
          outerror(OUTERROR_TYPE_WARN, "Can't seek location in file '%s': %s",
                   h->file, strerror(errno));
          h_closeconn(h, "Unable to locate data in file", errno2);
          return;
        }
        h->filepos = h->bytessent;
      }
      howmuch = read(h->filedescriptor, gdata.sendbuff, attempt);
      data = (char *)gdata.sendbuff;
    }
    if (howmuch < 0 && errno != EAGAIN) {
      errno2 = errno;
      outerror(OUTERROR_TYPE_WARN, "Can't read data from file '%s': %s",
              h->file, strerror(errno));
      h_closeconn(h, "Unable to read data from file", errno2);
      return;
    }
    if (howmuch <= 0)
      break;

    h->filepos += howmuch;
    howmuch2 = write(h->con.clientsocket, data, howmuch);
    if (howmuch2 < 0 && errno != EAGAIN) {
      h_closeconn(h, "Connection Lost", errno);
      return;
    }

    howmuch2 = max2(0, howmuch2);
    if (howmuch2 > 0) {
      h->con.lastcontact = gdata.curtime;
    }

    h->bytessent += howmuch2;
    bucket -= howmuch2;
    if (gdata.debug > 4)
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_BLUE, "File %ld Write %ld", (long)howmuch, (long)howmuch2);

  } while ((bucket >= TXSIZE) && (howmuch2 > 0));

  if (h->bytessent >= h->totalsize) {
    if (h->filedescriptor == FD_UNUSED) {
      mydelete(h->buffer_out);
    } else {
      close(h->filedescriptor);
      h->filedescriptor = FD_UNUSED;
    }
  }
}

static void h_istimeout(http * const h)
{
  updatecontext();

  if ((gdata.curtime - h->con.lastcontact) > 5)
    {
      h_closeconn(h, "HTTP Timeout (5 Sec Timeout)", 0);
    }
}

void h_perform(int changesec)
{
  http *h;
  int i;

  for (i=0; i<MAX_VHOSTS; i++) {
    if (http_listen[i] != FD_UNUSED) {
      if (FD_ISSET(http_listen[i], &gdata.readset)) {
        h_accept(i);
      }
    }
  }
  h = irlist_get_head(&gdata.https);
  while (h) {
    if (h->status == HTTP_STATUS_GETTING) {
      if (FD_ISSET(h->con.clientsocket, &gdata.readset)) {
        h_get(h);
      }
    }
    if (h->status == HTTP_STATUS_POST) {
      if (FD_ISSET(h->con.clientsocket, &gdata.readset)) {
        h_post(h);
      }
    }
    if (h->status == HTTP_STATUS_SENDING) {
      if (FD_ISSET(h->con.clientsocket, &gdata.writeset)) {
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
}

#endif

/* End of File */
