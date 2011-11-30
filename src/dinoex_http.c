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
#include "dinoex_irc.h"
#include "dinoex_badip.h"
#include "dinoex_misc.h"
#ifdef USE_RUBY
#include "dinoex_ruby.h"
#endif /* USE_RUBY */
#include "dinoex_http.h"

#include <ctype.h>

#ifndef WITHOUT_HTTP

#define MAX_WEBLIST_SIZE	(2 * 1024 * 1024)

#define HTTP_DATE_LINE		"%a, %d %b %Y %T GMT"	/* NOTRANSLATE */

typedef struct {
  const char *hg_group;
  const char *hg_desc;
  off_t hg_sizes;
  off_t hg_traffic;
  unsigned int hg_packs;
  unsigned int hg_agets;
  float hg_rgets;
  float hg_dummy;
} html_group_t;

static int http_listen[MAX_VHOSTS];
static int http_family[MAX_VHOSTS];

static const char *http_header_status =
"HTTP/1.1 %u OK\r\n" /* NOTRANSLATE */
"Date: %s\r\n" /* NOTRANSLATE */
"Last-Modified: %s\r\n" /* NOTRANSLATE */
"Server: iroffer-dinoex/" VERSIONLONG "\r\n" /* NOTRANSLATE */
"Content-Type: %s\r\n" /* NOTRANSLATE */
"Connection: close\r\n" /* NOTRANSLATE */
"Content-Length: %" LLPRINTFMT "u\r\n" /* NOTRANSLATE */;

static const char *http_header_notfound =
"HTTP/1.1 404 Not Found\r\n" /* NOTRANSLATE */
"Date: %s\r\n" /* NOTRANSLATE */
"Server: iroffer-dinoex/" VERSIONLONG "\r\n" /* NOTRANSLATE */
"Content-Type: text/plain\r\n" /* NOTRANSLATE */
"Connection: close\r\n" /* NOTRANSLATE */
"Content-Length: 13\r\n" /* NOTRANSLATE */
"\r\n" /* NOTRANSLATE */
"Not Found\r\n"
"\r\n"; /* NOTRANSLATE */

static const char *http_header_forbidden =
"HTTP/1.1 403 Forbidden\r\n" /* NOTRANSLATE */
"Date: %s\r\n" /* NOTRANSLATE */
"Server: iroffer-dinoex/" VERSIONLONG "\r\n" /* NOTRANSLATE */
"Content-Type: text/plain\r\n" /* NOTRANSLATE */
"Connection: close\r\n" /* NOTRANSLATE */
"Content-Length: 13\r\n" /* NOTRANSLATE */
"\r\n" /* NOTRANSLATE */
"Forbidden\r\n"
"\r\n"; /* NOTRANSLATE */

#ifndef WITHOUT_HTTP_ADMIN
static const char *http_header_admin =
"HTTP/1.1 401 Unauthorized\r\n" /* NOTRANSLATE */
"Date: %s\r\n" /* NOTRANSLATE */
"WWW-Authenticate: Basic realm=\"iroffer admin\"\r\n" /* NOTRANSLATE */
"Content-Type: text/plain\r\n" /* NOTRANSLATE */
"Connection: close\r\n" /* NOTRANSLATE */
"Content-Length: 26\r\n" /* NOTRANSLATE */
"\r\n" /* NOTRANSLATE */
"Authorization Required\r\n"
"\r\n"; /* NOTRANSLATE */

static const char *htpp_auth_key = "Basic "; /* NOTRANSLATE */
#endif /* WITHOUT_HTTP_ADMIN */

typedef struct {
  const char *m_ext;
  const char *m_mime;
} http_magic_const_t;

static const http_magic_const_t http_magic[] = {
  { "txt", "text/plain" }, /* NOTRANSLATE */
  { "html", "text/html" }, /* NOTRANSLATE */
  { "htm", "text/html" }, /* NOTRANSLATE */
  { "ico", "image/x-icon" }, /* NOTRANSLATE */
  { "png", "image/png" }, /* NOTRANSLATE */
  { "jpg", "image/jpeg" }, /* NOTRANSLATE */
  { "jpeg", "image/jpeg" }, /* NOTRANSLATE */
  { "gif", "image/gif" }, /* NOTRANSLATE */
  { "css", "text/css" }, /* NOTRANSLATE */
  { "xml", "application/xml" }, /* NOTRANSLATE */
  { "js", "application/x-javascript" }, /* NOTRANSLATE */
  { NULL, "application/octet-stream" } /* NOTRANSLATE */
};

typedef struct {
  int s_ch;
  int dummy;
  const char *s_html;
} http_special_t;

static const http_special_t http_special[] = {
  { '&', 0, "&amp;" }, /* NOTRANSLATE */
  { '<', 0, "&lt;" }, /* NOTRANSLATE */
  { '>', 0, "&gt;" }, /* NOTRANSLATE */
  { '"', 0, "&quot;" }, /* NOTRANSLATE */
  { '\'', 0, "&#39;" }, /* NOTRANSLATE */
  { 0, 0, NULL },
};


static const unsigned char HEX_NIBBLE[] = "0123456789ABCDEF"; /* NOTRANSLATE */

#ifndef WITHOUT_HTTP_ADMIN

/*
	BASE 64

	| b64  | b64   | b64   |  b64 |
	| octect1 | octect2 | octect3 |
*/

static const unsigned char BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; /* NOTRANSLATE */

static unsigned char base64decode[ 256 ];

/* create a table for fast base64 decoding */
void init_base64decode( void )
{
  unsigned int i;

  memset(base64decode, 0, sizeof(base64decode));
  for (i = 0; i < 64; ++i) {
    base64decode[ BASE64[ i ] ] = i;
  }
}

static void b64decode_quartet(unsigned char *decoded, const unsigned char *coded )
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

static char *b64decode_string(const char *coded)
{
  char *dest;
  char *result;
  size_t len;

  len = strlen(coded);
  result = mycalloc(len);
  dest = result;
  while (len >= 4U) {
    b64decode_quartet((unsigned char *)dest, (const unsigned char *)coded);
    dest += 3;
    coded += 4;
    len -= 4;
  }
  *(dest++) = 0;
  return result;
}
#endif /* WITHOUT_HTTP_ADMIN */

static const char *html_mime(const char *file)
{
  const char *ext;
  http_magic_t *mime;
  unsigned int i;

  ext = strrchr(file, '.');
  if (ext == NULL)
    ext = file;
  else
    ++ext;

  for (mime = irlist_get_head(&gdata.mime_type);
       mime;
       mime = irlist_get_next(mime)) {
    if (strcasecmp(mime->m_ext, ext) == 0)
      return mime->m_mime;
  }

  for (i=0; http_magic[i].m_ext; ++i) {
    if (strcasecmp(http_magic[i].m_ext, ext) == 0)
      break;
  }
  return http_magic[i].m_mime;
}

static size_t html_encode(char *buffer, size_t max, const char *src)
{
  char *dest = buffer;
  size_t len;
  unsigned int i;
  char ch;

  --max;
  for (;;) {
    if (max == 0)
      break;
    ch = *(src++);
    if (ch == 0)
      break;
    for (i=0; http_special[i].s_ch; ++i) {
      if (ch != http_special[i].s_ch)
        continue;
      len = strlen(http_special[i].s_html);
      if (len > max)
        len = max;
      strncpy(dest, http_special[i].s_html, len);
      dest += len;
      max -= len;
      break;
    }
    if (http_special[i].s_ch != 0)
      continue;
    switch (ch) {
    case IRCCOLOR: /* color */
      if (!isdigit(*src)) break;
      ++src;
      if (isdigit(*src)) ++src;
      if ((*src) != ',') break;
      ++src;
      if (isdigit(*src)) ++src;
      if (isdigit(*src)) ++src;
      break;
    case IRCBOLD: /* bold */
    case IRCNORMAL: /* end formatting */
    case IRCINVERSE: /* inverse */
    case IRCITALIC: /* italic */
    case IRCUNDERLINE: /* underline */
      break;
    default:
      *(dest++) = ch;
      --max;
    }
  }
  *dest = 0;
  len = dest - buffer;
  return len;
}

static ssize_t html_encode_size(const char *src)
{
  ssize_t len = 0;
  unsigned int i;
  char ch;

  if (src == NULL)
    return len;
  for (;;) {
    ch = *(src++);
    if (ch == 0)
      break;
    for (i=0; http_special[i].s_ch; ++i) {
      if (ch != http_special[i].s_ch)
        continue;
      len += strlen(http_special[i].s_html);
      break;
    }
    if (http_special[i].s_ch != 0)
      continue;
    switch (ch) {
    case IRCCOLOR: /* color */
      if (!isdigit(*src)) break;
      ++src;
      if (isdigit(*src)) ++src;
      if ((*src) != ',') break;
      ++src;
      if (isdigit(*src)) ++src;
      if (isdigit(*src)) ++src;
      break;
    case IRCBOLD: /* bold */
    case IRCNORMAL: /* end formatting */
    case IRCINVERSE: /* inverse */
    case IRCITALIC: /* italic */
    case IRCUNDERLINE: /* underline */
      break;
    default:
      ++len;
    }
  }
  return len;
}

static size_t html_decode(char *buffer, size_t max, const char *src)
{
  char *dest = buffer;
  const char *code;
  size_t len;
  unsigned int i;
  int hex;
  char ch;

  --max;
  for (;;) {
    if (max == 0)
      break;
    code = src;
    ch = *(src++);
    if (ch == 0)
      break;
    for (i=0; http_special[i].s_ch; ++i) {
      len = strlen(http_special[i].s_html);
      if (strncmp(code, http_special[i].s_html, len) != 0)
        continue;
      *(dest++) = http_special[i].s_ch;
      src += len - 1;
      --max;
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
      sscanf(src, "%2x", &hex); /* NOTRANSLATE */
      *(dest++) = hex;
      ++src;
      ++src;
      break;
    default:
      *(dest++) = ch;
    }
    --max;
  }
  *dest = 0;
  len = dest - buffer;
  return len;
}

static size_t url_encode(char *buffer, size_t max, const char *src)
{
  char *dest = buffer;
  size_t len;
  char ch;

  --max;
  for (;;) {
    if (max == 0)
      break;
    ch = *(src++);
    if (ch == 0)
      break;
    switch (ch) {
    case IRCCOLOR: /* color */
      if (!isdigit(*src)) break;
      ++src;
      if (isdigit(*src)) ++src;
      if ((*src) != ',') break;
      ++src;
      if (isdigit(*src)) ++src;
      if (isdigit(*src)) ++src;
      break;
    case IRCBOLD: /* bold */
    case IRCNORMAL: /* end formatting */
    case IRCINVERSE: /* inverse */
    case IRCITALIC: /* italic */
    case IRCUNDERLINE: /* underline */
      break;
    case '%':
    case '&':
    case '+':
    case '?':
    case ';':
      *(dest++) = '%';
      *(dest++) = HEX_NIBBLE[ ((unsigned char)ch & 0xF0) >> 4 ];
      *(dest++) = HEX_NIBBLE[ ((unsigned char)ch & 0x0F) ];
      break;
    default:
      *(dest++) = ch;
      --max;
    }
  }
  *dest = 0;
  len = dest - buffer;
  return len;
}

static ssize_t pattern_decode(char *buffer, size_t max, const char *src)
{
  char *dest = buffer;
  ssize_t len;
  char ch;

  *dest = 0;
  if (max < 3)
    return 0;
  --max;
  --max;
  --max;
  *(dest++) = '*';
  for (;;) {
    if ((max--) == 0)
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

static char *html_str_split(char *buffer, int delimiter)
{
  char *data;

  data = strchr(buffer, delimiter);
  if (data != NULL)
    *(data++) = 0;

  return data;
}

/* close all HTTP interfaces */
void h_close_listen(void)
{
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (http_listen[i] != FD_UNUSED) {
      FD_CLR(http_listen[i], &gdata.readset);
      close(http_listen[i]);
      http_listen[i] = FD_UNUSED;
    }
  }
}

static unsigned int h_open_listen(unsigned int i)
{
  char *vhost = NULL;
  char *msg;
  unsigned int rc;
  ir_sockaddr_union_t listenaddr;

  updatecontext();

  vhost = irlist_get_nth(&gdata.http_vhost, i);
  if (vhost == NULL)
    return 1;

  rc = open_listen(0, &listenaddr, &(http_listen[i]), gdata.http_port, 1, 0, vhost);
  if (rc != 0)
    return 1;

  http_family[i] = listenaddr.sa.sa_family;
  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &listenaddr.sa);
  ioutput(OUT_S|OUT_L|OUT_H, COLOR_MAGENTA,
          "HTTP SERVER waiting for connection on %s",  msg);
  mydelete(msg);
  return 0;
}

/* setup all HTTP interfaces */
unsigned int h_setup_listen(void)
{
  unsigned int i;
  unsigned int rc = 0;

  updatecontext();

  for (i=0; i<MAX_VHOSTS; ++i) {
    http_listen[i] = FD_UNUSED;
    http_family[i] = 0;
  }

  if (gdata.http_port == 0)
    return 1;

  for (i=0; i<MAX_VHOSTS; ++i) {
    rc += h_open_listen(i);
  }
  return rc;
}

/* close and setup all HTTP interfaces */
void h_reash_listen(void)
{
  h_close_listen();
  if (gdata.http_port == 0)
    return;
  h_setup_listen();
}

/* register active connections for select() */
int h_select_fdset(int highests, int changequartersec)
{
  http *h;
  unsigned long sum;
  unsigned int overlimit;
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (http_listen[i] != FD_UNUSED) {
      FD_SET(http_listen[i], &gdata.readset);
      highests = max2(highests, http_listen[i]);
    }
  }

  sum = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 1)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 2)%XDCC_SENT_SIZE]
      + gdata.xdccsent[(gdata.curtime - 3)%XDCC_SENT_SIZE];
  overlimit = (gdata.maxb && (sum >= gdata.maxb*1024));
  for (h = irlist_get_head(&gdata.https);
       h;
       h = irlist_get_next(h)) {
    if (h->status == HTTP_STATUS_GETTING) {
      FD_SET(h->con.clientsocket, &gdata.readset);
      highests = max2(highests, h->con.clientsocket);
      continue;
    }
    if (h->status == HTTP_STATUS_POST) {
      FD_SET(h->con.clientsocket, &gdata.readset);
      highests = max2(highests, h->con.clientsocket);
      continue;
    }
    if (h->status == HTTP_STATUS_SENDING) {
      if (!overlimit && !h->overlimit) {
        FD_SET(h->con.clientsocket, &gdata.writeset);
        highests = max2(highests, h->con.clientsocket);
        continue;
      }
      if (changequartersec) {
        FD_SET(h->con.clientsocket, &gdata.writeset);
        highests = max2(highests, h->con.clientsocket);
      }
      continue;
    }
  }
  return highests;
}

static const char *get_host(http * const h)
{
  char *host;

  host = h->con.remoteaddr;
  if (host == NULL)
    return "-"; /* NOTRANSLATE */

  host = html_str_split(host, '=');
  html_str_split(host, ' ');
  return host;
}

static void http_access_log_add(const char *logfile, const char *line, size_t len)
{
  int logfd;

  logfd = open_append(logfile, "Access Log");
  if (logfd < 0)
    return;

  mylog_write(logfd, logfile, line, len);
  mylog_close(logfd, logfile);
}

static void h_access_log(http * const h)
{
  struct tm *localt;
  char *tempstr;
  char *date;
  long bytes;
  size_t len;

  updatecontext();

  if (gdata.http_access_log == NULL)
    return;

  localt = localtime(&gdata.curtime);
  tempstr = mymalloc(maxtextlength);
  date = mymalloc(maxtextlengthshort);
  strftime(date, maxtextlengthshort - 1, HTTP_DATE_LINE, localt);
  bytes = h->bytesgot + h->bytessent;
  len = snprintf(tempstr, maxtextlength, "%s - - [%s] \"%s\" %u %ld\n",
                 get_host(h), date, h->log_url, h->status_code, bytes);
  mydelete(date);
  http_access_log_add(gdata.http_access_log, tempstr, len);
  mydelete(tempstr);
}

/* connections */

static void h_closeconn(http * const h, const char *msg, int errno1)
{
  updatecontext();

  if (errno1) {
    ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "HTTP Connection closed: %s (%s)", msg, strerror(errno1));
  } else {
    ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "HTTP Connection closed: %s", msg);
  }

  if (gdata.debug > 0) {
    ioutput(OUT_S, COLOR_YELLOW, "clientsock = %d", h->con.clientsocket);
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

  h_access_log(h);
  mydelete(h->file);
  mydelete(h->log_url);
  mydelete(h->url);
  mydelete(h->authorization);
  mydelete(h->group);
  mydelete(h->order);
  mydelete(h->search);
  mydelete(h->pattern);
  mydelete(h->modified);
  mydelete(h->range);
  mydelete(h->buffer_out);
  mydelete(h->con.remoteaddr);
  h->status = HTTP_STATUS_DONE;
}

static void h_write_header(http * const h, const char *header)
{
  char *tempstr;
  char *date;
  struct tm *localt;
  size_t len;

  localt = gmtime(&gdata.curtime);
  tempstr = mymalloc(maxtextlength);
  date = mymalloc(maxtextlengthshort);
  strftime(date, maxtextlengthshort - 1, HTTP_DATE_LINE, localt); /* NOTRANSLATE */
  len = snprintf(tempstr, maxtextlength, header, date);
  mydelete(date);
  send(h->con.clientsocket, tempstr, len, MSG_NOSIGNAL);
  mydelete(tempstr);
}

#ifdef USE_RUBY
static unsigned int h_runruby(http * const h)
{
  const char *params;
  char *suffix;
  char *tmp;
  char *base;
  unsigned int rc;

  suffix = strrchr(h->file, '.' );
  if (suffix == NULL)
    return 0;

  if (strcasecmp(suffix, ".rb") != 0) /* NOTRANSLATE */
    return 0;

  params = strchr(h->url, '?');
  if (params == NULL)
    params = h->url;
  else
    ++params;
  setenv("REQUEST_METHOD", "GET", 1); /* NOTRANSLATE */
  setenv("QUERY_STRING", params, 1); /* NOTRANSLATE */

  base = mystrdup(h->file);
  suffix = strrchr(base, '.' );
  if (suffix == NULL)
    return 0;

  *suffix = 0;
  suffix = strrchr(base, '.' );
  if (suffix != NULL) {
    tmp = base;
    base = NULL;
  } else {
    tmp = mystrsuffix(base, ".html"); /* NOTRANSLATE */
    mydelete(base);
  }
  rc = http_ruby_script(h->file, tmp);
  mydelete(h->file);
  h->file = tmp;
  return rc;
}
#endif /* USE_RUBY */

static void h_start_sending(http * const h)
{
  h->status = HTTP_STATUS_SENDING;
  if (gdata.debug > 1)
    ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
            "HTTP '%s' response %ld bytes", h->url, (long)(h->range_end - h->range_start));
}

static void h_error(http * const h, const char *header)
{
  updatecontext();

  h->range_end = 0;
  h_write_header(h, header);
  h_start_sending(h);
}

static void h_herror_404(http * const h)
{
  h->filedescriptor = FD_UNUSED;
  h->status_code = 404;
  h_error(h, http_header_notfound);
}

static int h_parse_range(http * const h)
{
  char *work;
  char *base;

  if (h->range == NULL)
    return 0;

  base = h->range;
  work = strchr(base, '=');
  if (work == NULL)
    return 0;

  *work = 0;
  if (strcmp(h->range, "bytes") != 0 ) /* NOTRANSLATE */
    return 0;

  base = ++work;
  work = strchr(base, '-');
  if (work == NULL)
    return 0;

  *work = 0;
  ++work;
  h->range_start = atoull(base);
  h->range_end = atoull(work);
  if (h->range_end != 0) {
    /* include last byte */
    h->range_end += 1;
    if (h->range_end >= h->totalsize)
      h->range_end = h->totalsize;
  } else {
    h->range_end = h->totalsize;
  }
  return 1;
}

static void h_write_status(http * const h, const char *mime, time_t *now)
{
  char *tempstr;
  char *date;
  char *last = NULL;
  struct tm *localt;
  size_t len;

  tempstr = mymalloc(maxtextlength);
  localt = gmtime(&gdata.curtime);
  date = mymalloc(maxtextlengthshort);
  strftime(date, maxtextlengthshort - 1, HTTP_DATE_LINE, localt); /* NOTRANSLATE */
  if (h->status_code == 200) {
    if (h_parse_range(h)) {
      h->status_code = 206;
    }
    if (now) {
      last = mymalloc(maxtextlengthshort);
      localt = gmtime(now);
      strftime(last, maxtextlengthshort - 1, HTTP_DATE_LINE, localt); /* NOTRANSLATE */
      if (h->modified) {
        if (strcmp(last, h->modified) == 0) {
          h->status_code = 304;
          h->head = 1;
        }
      }
    }
  }
  len = snprintf(tempstr, maxtextlength, http_header_status, h->status_code, date, last ? last : date, html_mime(mime), h->range_end - h->range_start);
  if (h->attachment) {
    len += snprintf(tempstr + len, maxtextlength - len,
                    "Content-Disposition: attachment; filename=\"%s\"\r\n", /* NOTRANSLATE */
                    h->attachment);
    len += snprintf(tempstr + len, maxtextlength - len,
                    "Accept-Ranges: bytes\r\n" ); /* NOTRANSLATE */
    if (h->range != NULL)
      len += snprintf(tempstr + len, maxtextlength - len,
                      "Content-Range: bytes %" LLPRINTFMT "u-%" LLPRINTFMT "u/%" LLPRINTFMT "u\r\n", /* NOTRANSLATE */
                      h->range_start, h->range_end - 1, h->totalsize);
  }
  len += snprintf(tempstr + len, maxtextlength - len, "\r\n" ); /* NOTRANSLATE */
  mydelete(last);
  mydelete(date);
  send(h->con.clientsocket, tempstr, len, MSG_NOSIGNAL);
  mydelete(tempstr);
  if (h->head)
    h->range_end = 0;
}

static void h_readfile(http * const h, const char *file)
{
  struct stat st;

  updatecontext();

  if (file == NULL) {
    h_herror_404(h);
    return;
  }

  h->file = mystrdup(file);
#ifdef USE_RUBY
  if (h_runruby(h)) {
    h_herror_404(h);
  }
#endif /* USE_RUBY */

  h->filedescriptor = open(h->file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (h->filedescriptor < 0) {
    if (gdata.debug > 1)
      ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "File not found: '%s'", file);
    h_herror_404(h);
    return;
  }
  if (fstat(h->filedescriptor, &st) < 0) {
    if (gdata.debug > 1)
      ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "Unable to stat file '%s': %s",
              file, strerror(errno));
    close(h->filedescriptor);
    h_herror_404(h);
    return;
  }

  h->range_start = 0;
  h->filepos = 0;
  h->totalsize = st.st_size;
  h->range_end = h->totalsize;
  h_write_status(h, h->file, &st.st_mtime);
  h_start_sending(h);
}

static void h_herror_403(http * const h, const char *msg)
{
  char *tempstr;

  ioutput(OUT_S|OUT_H, COLOR_MAGENTA, "%s", msg); /* NOTRANSLATE */
  h->filedescriptor = FD_UNUSED;
  h->status_code = 403;
  if (!gdata.http_forbidden) {
    h->url = mystrdup("-"); /* NOTRANSLATE */
    h->log_url = mystrdup("-"); /* NOTRANSLATE */
    h_error(h, http_header_forbidden);
    return;
  }
  h->url = mystrdup(gdata.http_forbidden);
  h->log_url = mymalloc(maxtextlength);
  snprintf(h->log_url, maxtextlength, "GET %s HTTP/1.1", gdata.http_forbidden); /* NOTRANSLATE */
  tempstr = mymalloc(maxtextlength);
  snprintf(tempstr, maxtextlength, "%s%s", gdata.http_dir, h->url); /* NOTRANSLATE */
  h_readfile(h, tempstr);
  mydelete(tempstr);
  return;
}

static void h_accept(unsigned int i)
{
  SIGNEDSOCK int addrlen;
  ir_sockaddr_union_t remoteaddr;
  char *msg;
  http *h;
  int clientsocket;
  unsigned int blocked;

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
  } else {
    h->con.family = remoteaddr.sin.sin_family;
  }
  h->con.remoteport = get_port(&(remoteaddr));
  h->con.connecttime = gdata.curtime;
  h->con.lastcontact = gdata.curtime;
  h->status = HTTP_STATUS_GETTING;
  h->status_code = 200;

  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &remoteaddr.sa);
  h->con.remoteaddr = mystrdup(msg);
  mydelete(msg);
  ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
          "HTTP connection received from %s",  h->con.remoteaddr);

  blocked = is_in_badip(&(h->con.remote));
#ifdef USE_GEOIP
  if (blocked == 2) {
    h_herror_403(h, "HTTP connection country blocked");
    return;
  }
#endif /* USE_GEOIP */
  if (blocked > 0) {
    h_closeconn(h, "HTTP connection ignored", 0);
    return;
  }

  if (irlist_size(&gdata.http_allow) > 0) {
    if (!verify_cidr(&gdata.http_allow, &remoteaddr)) {
      h_herror_403(h, "HTTP connection not allowed");
      return;
    }
  }

  if (verify_cidr(&gdata.http_deny, &remoteaddr)) {
    h_herror_403(h, "HTTP connection denied");
    return;
  }
}

static void h_readbuffer(http * const h)
{
  updatecontext();

  h->range_start = 0;
  h->range_end = strlen(h->buffer_out);
  h_write_status(h, "html", (h->search) ? NULL : &gdata.last_update); /* NOTRANSLATE */
  h_start_sending(h);
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
/* print and ad text to send it to the htpp client */
h_respond(http * const h, const char *format, ...)
{
  va_list args;
  int len;

  va_start(args, format);
  if (h->left > 0 ) {
    len = vsnprintf(h->end, (size_t)(h->left), format, args);
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
  size_t len;
  ssize_t howmuch;
  struct stat st;
  int fd;

  fd = open(file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (fd < 0) {
    if (gdata.debug > 1)
      ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "File not found: '%s'", file);
    return;
  }
  if (fstat(fd, &st) < 0) {
    outerror(OUTERROR_TYPE_WARN, "Unable to stat file '%s': %s",
             file, strerror(errno));
    return;
  }

  len = st.st_size;
  if ((ssize_t)len > h->left) {
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
  h->end[0] = 0;
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
    len = snprintf(str, size, ";"); /* NOTRANSLATE */
  }
  tempstr = mymalloc(maxtextlength);
  url_encode(tempstr, maxtextlength, val);
  len += snprintf(str + len, size - len, "%s=%s", option, tempstr); /* NOTRANSLATE */
  mydelete(tempstr);
  return len;
}

static char *html_link_build(const char *css, const char *caption, const char *text,
				const char *group, unsigned int traffic, const char *order)
{
  char *tempstr;
  size_t len;

  tempstr = mymalloc(maxtextlength);
  len = snprintf(tempstr, maxtextlength, "<a%s title=\"%s\" href=\"/?", css, caption);
  html_link_start = 0;
  if (group)
    len += html_link_option(tempstr + len, maxtextlength - len, "g", group); /* NOTRANSLATE */
  if (traffic)
    len += html_link_option(tempstr + len, maxtextlength - len, "t", "1"); /* NOTRANSLATE */
  if (order)
    len += html_link_option(tempstr + len, maxtextlength - len, "o", order); /* NOTRANSLATE */
  len += snprintf(tempstr + len, maxtextlength - len, "\">%s</a>", text);
  return tempstr;
}

static char *h_html_link_more(http * const h, const char *caption, const char *text)
{
  unsigned int traffic;

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

static float gets_per_pack(unsigned int agets, unsigned int packs)
{
  float result = 0.0;

  if (packs > 0) {
    result = (float)agets;
    result /= packs;
  }
  return result;
}

static unsigned int html_filter_main(xdcc *xd, const char *group)
{
  if (strcmp(group, ".") == 0) { /* NOTRANSLATE */
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

static unsigned int h_html_filter_group(http * const h, xdcc *xd)
{
  if (h->group != NULL) {
    if (strcmp(h->group, "*") != 0) { /* NOTRANSLATE */
      if (strcmp(h->group, ".") == 0) { /* NOTRANSLATE */
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
  h_respond(h, "<input type=\"text\" name=\"s\" value=\"%s\" size=30>&nbsp;\n", (h->search) ? h->search : "");
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
  unsigned int groups = 0;
  unsigned int packs = 0;
  unsigned int agets = 0;
  unsigned int nogroup = 0;

  updatecontext();

  tempstr = mymalloc(maxtextlength);
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++packs;
    agets += xd->gets;
    sizes += xd->st_size;
    traffic += xd->gets * xd->st_size;
    if (xd->group == NULL) {
      if (nogroup == 0) {
        ++nogroup;
        ++groups;
        snprintf(tempstr, maxtextlength,
                 "%s %s", ".", "no group");
        hg = irlist_add(&grplist, sizeof(html_group_t));
        hg->hg_group = "."; /* NOTRANSLATE */
        hg->hg_desc = "no group";
      }
      continue;
    }
    if (xd->group_desc == NULL)
      continue;
    ++groups;

    snprintf(tempstr, maxtextlength,
             "%s %s", xd->group, xd->group_desc); /* NOTRANSLATE */
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

      if (html_filter_main(xd, hg->hg_group))
        continue;

      ++(hg->hg_packs);
      hg->hg_agets += xd->gets;
      hg->hg_sizes += xd->st_size;
      if (h->traffic)
        hg->hg_traffic += xd->gets * xd->st_size;
    }
    if (h->traffic)
      hg->hg_rgets = gets_per_pack(hg->hg_agets, hg->hg_packs);
  }
  order_func = html_order_func(h->order);
  irlist_sort2(&grplist, order_func);

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

  h_respond(h, "<th class=\"right\">%u</th>\n", packs);
  h_respond(h, "<th class=\"right\">%u</th>\n", agets);
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
  h_respond(h, "<th class=\"head\">%u</th>\n", groups);
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
    h_respond(h, "<td class=\"right\">%u</td>\n", hg->hg_packs);
    h_respond(h, "<td class=\"right\">%u</td>\n", hg->hg_agets);
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
    savegroup = mymalloc(maxtextlength);
    html_encode(savegroup, maxtextlength, hg->hg_group);
    h_respond(h, "<td class=\"content\">%s</td>\n", savegroup);
    mydelete(savegroup);
    savedesc = mymalloc(maxtextlength);
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
  size_t len;
  unsigned int packs = 0;
  unsigned int agets = 0;
  unsigned int num;

  updatecontext();

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (hide_pack(xd))
      continue;

    if (h_html_filter_group(h, xd))
      continue;

    ++packs;
    agets += xd->gets;
    sizes += xd->st_size;
    traffic += xd->gets * xd->st_size;
  }

  h_respond(h, "<h1>%s %s</h1>\n", h->nick, "File list" );
  h_html_search(h);
  h_respond(h, "<p>%s<span class=\"cmd\">/msg %s xdcc send number</span></p>\n",
            "Download in IRC with: ", h->nick);
  h_respond(h, "<table cellpadding=\"2\" cellspacing=\"0\" summary=\"list\">\n<thead>\n<tr>\n");
  h_respond(h, "<th class=\"head\">%s</th>\n", "PACKs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "DLs");
  h_respond(h, "<th class=\"head\">%s</th>\n", "Size");
  tempstr = h_html_link_group(h, "back", "(" "back" ")", NULL);
  h_respond(h, "<th class=\"head\">%s&nbsp;%s</th>\n", "DESCRIPTION", tempstr);
  mydelete(tempstr);
  h_respond(h, "</tr>\n</thead>\n<tfoot>\n<tr>\n");
  h_respond(h, "<th class=\"right\">%u</th>\n", packs);
  h_respond(h, "<th class=\"right\">%u</th>\n", agets);
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
    h_respond(h, "<td class=\"right\">#%u</td>\n", num);
    h_respond(h, "<td class=\"right\">%u</td>\n", xd->gets);
    tempstr = sizestr(0, xd->st_size);
    h_respond(h, "<td class=\"right\">%s</td>\n", tempstr);
    mydelete(tempstr);
    tlabel = mymalloc(maxtextlength);
    len = snprintf(tlabel, maxtextlength, "%s\n/msg %s xdcc send %u",
                   "Download with:", h->nick, num);
    if (xd->has_md5sum)
      len += snprintf(tlabel + len, maxtextlength - len, "\nmd5: " MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
    if (xd->has_crc32)
      len += snprintf(tlabel + len, maxtextlength - len, "\ncrc32: " CRC32_PRINT_FMT, xd->crc32);
    tempstr = mymalloc(maxtextlength);
    len = 0;
    if (gdata.show_date_added) {
       user_getdatestr(tempstr + 1, xd->xtime ? xd->xtime : xd->mtime, maxtextlength - 1);
       len = strlen(tempstr);
       tempstr[len++] = ' ';
       tempstr[len] = 0;
    }
    len += html_encode(tempstr + len, maxtextlength - len, xd->desc);
    if (xd->lock)
      len += snprintf(tempstr + len, maxtextlength - len, " (%s)", "locked");
    if (xd->note != NULL) {
      if (xd->note[0]) {
        len += snprintf(tempstr + len, maxtextlength - len, "<br>");
        len += html_encode(tempstr + len, maxtextlength - len, xd->note);
      }
    }
    javalink = mymalloc(maxtextlength);
    len = snprintf(javalink, maxtextlength,
                   "<a href=\"javascript:ToClipboard('/msg %s xdcc send %u');\">%s</a>",
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
      tempstr = mymalloc(maxtextlengthshort);
      tempstr = getuptime(tempstr, 1, gdata.startuptime, maxtextlengthshort);
      break;
    }
    if (strcmp(key, "running") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      tempstr = getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlengthshort);
      break;
    }
    if (strcmp(key, "minspeed") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.transferminspeed);
      break;
    }
    if (strcmp(key, "maxspeed") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.transfermaxspeed);
      break;
    }
    if (strcmp(key, "cap") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, "%u.0kB/s", gdata.maxb / 4);
      break;
    }
    if (strcmp(key, "record") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.record);
      break;
    }
    if (strcmp(key, "send") == 0) {
      tempstr = mymalloc(maxtextlengthshort);
      snprintf(tempstr, maxtextlengthshort, "%1.1fkB/s", gdata.sentrecord);
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

static void h_html_index(http * const h)
{
  char *info;
  char *buffer;
  char *text;
  char *tempstr;
  char *clean;
  char *tlabel;
  size_t len;

  updatecontext();

  if (gdata.support_groups == 0) {
    mydelete(h->group);
    h->group = mystrdup("*"); /* NOTRANSLATE */
  }
  if (h->search) {
    if (gdata.http_search) {
      mydelete(h->group);
      h->group = mystrdup("*"); /* NOTRANSLATE */
      len = strlen(h->search) + 1;
      clean = mymalloc(len);
      html_decode(clean, len, h->search);
      mydelete(h->search);
      len = strlen(clean) + 3;
      h->pattern = mymalloc(len);
      pattern_decode(h->pattern, len, clean);
      h->search = mymalloc(maxtextlength);
      html_encode(h->search, maxtextlength - 1, clean);
      mydelete(clean);
    } else {
      mydelete(h->search);
    }
  }

  if (h->group) {
    h_html_file(h);
  } else {
    h_html_main(h);
  }

  h_respond(h, "</tbody>\n</table>\n<table class=\"status\">\n<tbody>\n<tr><td>Version</td>\n");

  tlabel = mymalloc(maxtextlength);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  len  = snprintf(tlabel,       maxtextlength,       "%6s %s\n", tempstr, "Traffic today");
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  len += snprintf(tlabel + len, maxtextlength - len, "%6s %s\n", tempstr, "Traffic this week");
  mydelete(tempstr);
  tempstr = sizestr(0, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  len += snprintf(tlabel + len, maxtextlength - len, "%6s %s\n", tempstr, "Traffic this month");
  mydelete(tempstr);
  h_respond(h, "<td title=\"%s\">%s</td>\n", tlabel, "iroffer-dinoex " VERSIONLONG);
  mydelete(tlabel);

  tempstr = mymalloc(maxtextlength);
  user_getdatestr(tempstr, gdata.curtime, maxtextlength - 1);
  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "last update");
  h_respond(h, "<td>%s</td>\n", tempstr);
  h_respond(h, "</tr>\n");
  mydelete(tempstr);

  h_respond(h, "<tr>\n");
  h_respond(h, "<td>%s</td>\n", "slots open");
  h_respond(h, "<td>%u</td>\n", slotsfree());
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
    text = html_str_split(buffer, ' ');
    if (text == NULL) {
      mydelete(buffer);
      continue;
    }
    clean_quotes(text);
    h_html_weblist_info(h, buffer, text);
    mydelete(buffer);
  }

  h_respond(h, "</tbody>\n</table>\n<br>\n");
  h_respond(h, "<a class=\"credits\" href=\"" "http://iroffer.dinoex.net/" "\">%s</a>\n", "Sourcecode" );
}

static char *get_url_param(const char *url, const char *key)
{
  char *result;
  char *clean;
  const char *found;
  size_t len;

  found = strcasestr(url, key);
  if (found == NULL)
    return NULL;

  result = mystrdup(found + strlen(key));
  html_str_split(result, ' ');
  html_str_split(result, ';');
  html_str_split(result, '&');
  len = strlen(result) + 1;
  clean = mymalloc(len);
  html_decode(clean, len, result);
  mydelete(result);
  return clean;
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

static void h_prepare_respond(http * const h, size_t guess)
{
  h->buffer_out = mymalloc(guess);
  h->buffer_out[ 0 ] = 0;
  h->end = h->buffer_out;
  h->left = guess - 1;
}

static void h_prepare_header(http * const h, size_t guess)
{
  guess += h_stat("header.html");
  guess += h_stat("footer.html");
  h_prepare_respond(h, guess);
  h_include(h, "header.html");
}

static void h_prepare_footer(http * const h)
{
  h_include(h, "footer.html");
  h_readbuffer(h);
}

static size_t h_guess_weblist(http * const h)
{
  xdcc *xd;
  size_t len;
  size_t nicklen;

  h->nick = save_nick(gdata.config_nick);
  nicklen = html_encode_size(h->nick);
  len = nicklen;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    len += 304;
    len += nicklen;
    len += nicklen;
    len += html_encode_size(xd->desc);
    if (xd->lock) {
      len += strlen(" (%s)");
      len += html_encode_size("locked");
    }
    if (xd->note != NULL) {
      if (xd->note[0]) {
        len += strlen("<br>");
        len += html_encode_size(xd->note);
      }
    }
  }
  return len;
}

static void h_webliste(http * const h, const char *body)
{
  size_t guess;

  updatecontext();

  if (body)
    h->search = get_url_param(body, "s="); /* NOTRANSLATE */
  h->group = get_url_param(h->log_url, "g="); /* NOTRANSLATE */
  h->order = get_url_param(h->log_url, "o="); /* NOTRANSLATE */
  h->traffic = get_url_number(h->log_url, "t="); /* NOTRANSLATE */
  guess = 2048;
  guess += h_guess_weblist(h);
  h_prepare_header(h, guess);
  h_html_index(h);
  mydelete(h->group);
  mydelete(h->order);
  mydelete(h->search);
  mydelete(h->pattern);
  h_prepare_footer(h);
}

#ifndef WITHOUT_HTTP_ADMIN
static void h_admin(http * const h, unsigned int UNUSED(level), const char * UNUSED(body))
{
  char *tempstr;
  char *tmp;
  size_t guess;

  updatecontext();

  if (strcasecmp(h->url, "/") == 0) { /* NOTRANSLATE */
    guess = 2048;
    h_prepare_header(h, guess);
    h_respond(h, "<a class=\"credits\" href=\"/admin%s/\">%s</a>\n", "/help", "Help" );
    h_respond(h, "</form>\n" );
    h_prepare_footer(h);
    return;
  }
  if (strcasecmp(h->url, "/help") == 0) {
    /* send hilfe */
    h_readfile(h, "help-admin-en.txt");
    return;
  }

  if (strncasecmp(h->url, "/ddl/", 5) == 0) { /* NOTRANSLATE */
    unsigned int pack = atoi(h->url + 5);
    xdcc *xd;

    if (pack > 0) {
      xd = get_xdcc_pack(pack);
      if (xd != NULL) {
        h->attachment = getfilename(xd->file);
        ++(h->unlimited);
        h->maxspeed = xd->maxspeed;
        h_readfile(h, xd->file);
        /* force download in browser */
        h->attachment = getfilename(h->file);
        return;
      }
    }
  }

  if (gdata.http_admin_dir) {
    tempstr = mymalloc(maxtextlength);
    snprintf(tempstr, maxtextlength, "%s%s", gdata.http_admin_dir, h->url); /* NOTRANSLATE */
    tmp = strchr(tempstr, '?' );
    if (tmp != NULL)
      *tmp = 0;
    h_readfile(h, tempstr);
    mydelete(tempstr);
    return;
  }

  if (gdata.debug > 1)
    ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
            "HTTP not found: '%s'", h->url);
  h_herror_404(h);
}
#endif /* WITHOUT_HTTP_ADMIN */

static char *h_bad_request(http * const h)
{
  char *request;
  char *url;
  char *header;
  size_t len;

  h->modified = NULL;
  h->post = 0;
  h->head = 0;
  request = (char *)gdata.sendbuff;
  header = html_str_split(request, '\n');
  html_str_split(request, '\r');
  h->log_url = mystrdup(request);
  url = html_str_split(request, ' ');
  if (url == NULL)
    return NULL; /* nothing to read */

  html_str_split(url, ' ');
  len = strlen(url) + 1;
  h->url = mymalloc(len);
  html_decode(h->url, len, url);
  if (strcasecmp(request, "GET" ) == 0) { /* NOTRANSLATE */
    return header;
  }
  if (strcasecmp(request, "POST" ) == 0) { /* NOTRANSLATE */
    ++(h->post);
    return header;
  }
  if (strcasecmp(request, "HEAD" ) == 0) { /* NOTRANSLATE */
    ++(h->head);
    return header;
  }
  return NULL;
}

static char *h_read_http(http * const h)
{
  int howmuch, howmuch2;
  unsigned int i;

  updatecontext();

  h->bytesgot = 0;
  h->bytessent = 0;
  gdata.sendbuff[0] = 0;
  howmuch2 = BUFFERSIZE;
  for (i=0; i<MAXTXPERLOOP; ++i) {
    if (h->bytesgot >= BUFFERSIZE - 1)
      break;
    if (!is_fd_readable(h->con.clientsocket))
      continue;
    howmuch = recv(h->con.clientsocket, gdata.sendbuff + h->bytesgot, howmuch2, MSG_DONTWAIT);
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
      ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
              "HTTP data received data=\n%s", gdata.sendbuff);
    }
  }
  return (char *)gdata.sendbuff;
}

#ifndef WITHOUT_HTTP_ADMIN
static void html_str_prefix(char **str, size_t len)
{
  char *newstr;

  newstr = mystrdup(*str + len);
  mydelete(*str);
  *str = newstr;
}

static unsigned int h_admin_auth(http * const h, char *body)
{
  const char *auth;
  char *buffer;
  char *tempstr;
  char *passwd;

  updatecontext();

  /* check for header */
  if (h->authorization == NULL)
    return 1;

  html_str_prefix(&(h->url), 6);
  auth = strcasestr(h->authorization, htpp_auth_key);
  if (auth == NULL)
    return 1;

  auth += strlen(htpp_auth_key);
  buffer = mystrdup(auth);
  html_str_split(buffer, ' ');
  tempstr = b64decode_string(buffer);
  mydelete(buffer);
  passwd = html_str_split(tempstr, ':');

  if (strcasecmp(tempstr, gdata.http_admin) == 0) {
    if (verifypass2(gdata.hadminpass, passwd) ) {
      mydelete(tempstr);
      h_admin(h, gdata.hadminlevel, body);
      return 0;
    }

   if (verifypass2(gdata.adminpass, passwd) ) {
      mydelete(tempstr);
      h_admin(h, gdata.adminlevel, body);
      return 0;
    }

  }
  mydelete(tempstr);
  return 1;
}
#endif /* WITHOUT_HTTP_ADMIN */


static void h_parse(http * const h, char *body)
{
  char *tempstr;
  char *tmp;

  updatecontext();

  if (strcmp(h->url, "/") == 0) { /* NOTRANSLATE */
    if (gdata.http_index == NULL) {
      /* send text */
      h_readfile(h, gdata.xdcclistfile);
      return;
    }
    mydelete(h->url);
    h->url = mystrdup(gdata.http_index);
  }

  if (strncasecmp(h->url, "/?", 2) == 0) { /* NOTRANSLATE */
    /* send html */
    h_webliste(h, body);
    return;
  }

  if (strcmp(h->url, "/txt") == 0) { /* NOTRANSLATE */
    /* send text */
    h_readfile(h, gdata.xdcclistfile);
    return;
  }

  if (gdata.xdccxmlfile && (strcasecmp(h->url, "/xml") == 0)) { /* NOTRANSLATE */
    /* send XML pack list */
    h_readfile(h, gdata.xdccxmlfile);
    return;
  }

#ifndef WITHOUT_HTTP_ADMIN
  if ((gdata.http_admin) && (strncasecmp(h->url, "/admin/", 7) == 0)) { /* NOTRANSLATE */
    if (h_admin_auth(h, body)) {
      count_badip(&(h->con.remote));
      h->status_code = 401;
      h_error(h, http_header_admin);
    }
    return;
  }
#endif /* WITHOUT_HTTP_ADMIN */

  if (gdata.http_dir) {
    tempstr = mymalloc(maxtextlength);
    snprintf(tempstr, maxtextlength, "%s%s", gdata.http_dir, h->url);
    tmp = strchr(tempstr, '?' );
    if (tmp != NULL)
      *tmp = 0;
    h_readfile(h, tempstr);
    mydelete(tempstr);
    return;
  }

  if (gdata.debug > 1)
    ioutput(OUT_S|OUT_H, COLOR_MAGENTA,
            "HTTP not found: '%s'", h->url);
  h_herror_404(h);
}

static void h_get(http * const h)
{
  char *data;
  char *hval;
  char *header;

  updatecontext();

  if (h->filedescriptor != FD_UNUSED)
    return;

  data = h_read_http(h);
  if (data == NULL)
    return;

  header = h_bad_request(h);
  if (header == NULL) {
    h_closeconn(h, "Bad request", 0);
    return;
  }

  /* parse header */
  for (data = strtok(header, "\n"); /* NOTRANSLATE */
       data;
       data = strtok(NULL, "\n")) { /* NOTRANSLATE */
    html_str_split(data, '\r');
    if (data[0] == 0) {
      data = strtok(NULL, "\n"); /* NOTRANSLATE */
      break;
    }
    hval = html_str_split(data, ':');
    if (!hval)
      continue;

    while (hval[0] == ' ')
      ++hval;

    if (strcmp(data, "If-Modified-Since") == 0) { /* NOTRANSLATE */
      h->modified = mystrdup(hval);
      continue;
    }
    if (strcmp(data, "Range") == 0) { /* NOTRANSLATE */
      h->range = mystrdup(hval);
      continue;
    }
    if (strcmp(data, "Authorization") == 0) { /* NOTRANSLATE */
      h->authorization = mystrdup(hval);
      continue;
    }
  }

  if ((h->post) && (data == NULL)) {
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

static int h_bandwith(http * const h)
{
  unsigned int j;

  /* max bandwidth start.... */
  if (h->maxspeed > 0) {
    if (h->tx_bucket < TXSIZE) {
      h->overlimit = 1;
      return 1; /* over transfer limit */
    }
  } else {
    h->tx_bucket = TXSIZE * MAXTXPERLOOP;
  }
  j = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE]
    + gdata.xdccsent[(gdata.curtime-1)%XDCC_SENT_SIZE]
    + gdata.xdccsent[(gdata.curtime-2)%XDCC_SENT_SIZE]
    + gdata.xdccsent[(gdata.curtime-3)%XDCC_SENT_SIZE];
  if ( gdata.maxb && (j >= gdata.maxb*1024)) {
    if (h->unlimited == 0)
      return 1; /* over overall limit */
  }
  h->overlimit = 0;
  return 0;
}

static void h_send(http * const h)
{
  off_t offset;
  char *data;
  size_t attempt;
  ssize_t howmuch, howmuch2;
  int errno2;

  updatecontext();

  if (h->filedescriptor == FD_UNUSED) {
    if (h->buffer_out == NULL) {
      h_closeconn(h, "Complete", 0);
      return;
    }
  }

  /* close on HTTP HEAD */
  if (h->range_end == 0) {
    if (h->filedescriptor == FD_UNUSED) {
      mydelete(h->buffer_out);
    } else {
      close(h->filedescriptor);
      h->filedescriptor = FD_UNUSED;
    }
    return;
  }

  if (h_bandwith(h))
    return;

  do {
    attempt = min2(h->tx_bucket - (h->tx_bucket % TXSIZE), BUFFERSIZE);
    if (h->filedescriptor == FD_UNUSED) {
      howmuch = h->range_end - h->range_start;
      data = h->buffer_out + h->range_start;
    } else {
      if (h->filepos != h->range_start) {
        offset = lseek(h->filedescriptor, h->range_start, SEEK_SET);
        if (offset != h->range_start) {
          errno2 = errno;
          outerror(OUTERROR_TYPE_WARN, "Can't seek location in file '%s': %s",
                   h->file, strerror(errno));
          h_closeconn(h, "Unable to locate data in file", errno2);
          return;
        }
        h->filepos = h->range_start;
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
    howmuch2 = send(h->con.clientsocket, data, howmuch, MSG_NOSIGNAL);
    if (howmuch2 < 0 && errno != EAGAIN) {
      h_closeconn(h, "Connection Lost", errno);
      return;
    }

    howmuch2 = max2(0, howmuch2);
    if (howmuch2 > 0) {
      h->con.lastcontact = gdata.curtime;
    }

    h->bytessent += howmuch2;
    h->range_start += howmuch2;
    gdata.xdccsum[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
    if (h->unlimited == 0)
      gdata.xdccsent[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
    h->tx_bucket -= howmuch2;
    if (gdata.debug > 4)
      ioutput(OUT_S, COLOR_BLUE, "File %ld Write %ld", (long)howmuch, (long)howmuch2);

  } while ((h->tx_bucket >= TXSIZE) && (howmuch2 > 0));

  if (h->range_start >= h->range_end) {
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

/* process all HTTP connections */
void h_perform(int changesec, int changequartersec)
{
  http *h;
  unsigned int i;

  for (i=0; i<MAX_VHOSTS; ++i) {
    if (http_listen[i] != FD_UNUSED) {
      if (FD_ISSET(http_listen[i], &gdata.readset)) {
        h_accept(i);
      }
    }
  }
  if (changequartersec) {
    for (h = irlist_get_head(&gdata.https);
         h;
         h = irlist_get_next(h)) {
       if (h->maxspeed <= 0)
         continue;

       h->tx_bucket += h->maxspeed * (1024 / 4);
       h->tx_bucket = min2(h->tx_bucket, MAX_TRANSFER_TX_BURST_SIZE * h->maxspeed * 1024);
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

#endif /* WITHOUT_HTTP */

/* End of File */
