/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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

#ifndef WITHOUT_MEMSAVE

char *mystrdup2(const char *str, const char *src_function, const char *src_file, int src_line)
{
  char *copy;

  copy = mymalloc2(strlen(str)+1, 0, src_function, src_file, src_line);
  strcpy(copy, str);
  return copy;
}

#endif

int verifyshell(irlist_t *list, const char *file)
{
  char *pattern;

  updatecontext();

  pattern = irlist_get_head(list);
  while (pattern) {
    if (fnmatch(pattern, file, FNM_CASEFOLD) == 0)
      return 1;
    pattern = irlist_get_next(pattern);
  }
  return 0;
}

const char *save_nick(const char * nick)
{
  if (nick)
    return nick;
  return "??";
}

int check_level(char prefix)
{
  int ii;
  int level;

  if (gdata.need_level == 0) {
    if (gdata.need_voice == 0)
      return 1;
    /* any prefix is counted as voice */
    if ( prefix != 0 )
      return 1;
    /* no good prefix found try next chan */
    return 0;
  }

  level = 0;
  for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_symbol); ii++) {
    if (prefix == gnetwork->prefixes[ii].p_symbol) {
      /* found a nick mode */
      switch (gnetwork->prefixes[ii].p_mode) {
      case 'q':
      case 'a':
      case 'o':
        level = 3;
        break;
      case 'h':
        level = 2;
        break;
      case 'v':
        level = 1;
        break;
      default:
        level = 0;
      }
    }
  }

  if (level >= gdata.need_level)
    return 1;

  return 0;
}

int number_of_pack(xdcc *pack)
{
  xdcc *xd;
  int n;

  updatecontext();

  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    n++;
    if (xd == pack)
      return n;

    xd = irlist_get_next(xd);
  }

  return 0;
}

int verifypass2(const char *masterpass, const char *testpass)
{
#ifndef NO_CRYPT
  char *pwout;
#endif

  updatecontext();

  if (!masterpass || !testpass)
    return 0;

#ifndef NO_CRYPT
  if ((strlen(masterpass) < 13) ||
      (strlen(testpass) < 5) ||
      (strlen(testpass) > 59))
    return 0;

  pwout = crypt(testpass, masterpass);
  if (strcmp(pwout, masterpass)) return 0;
#else
  if (!masterpass || !testpass)
    return 0;

  if (strcmp(testpass, masterpass)) return 0;
#endif

  /* allow */
  return 1;
}

void checkadminpass2(const char *masterpass)
{
#ifndef NO_CRYPT
  int err=0;
  unsigned int i;

  updatecontext();

  if (!masterpass || strlen(masterpass) < 13) err++;

  for (i=0; !err && i<strlen(masterpass); i++) {
    if (!((masterpass[i] >= 'a' && masterpass[i] <= 'z') ||
          (masterpass[i] >= 'A' && masterpass[i] <= 'Z') ||
          (masterpass[i] >= '0' && masterpass[i] <= '9') ||
          (masterpass[i] == '.') ||
          (masterpass[i] == '$') ||
          (masterpass[i] == '/')))
      err++;
  }

  if (err) outerror(OUTERROR_TYPE_CRASH, "adminpass is not encrypted!");
#endif
}

char *clean_quotes(char *str)
{
  char *src;
  char *dest;
  int len;

  if (str[0] != '"')
    return str;

  src = str + 1;
  len = strlen(src) - 1;
  if (src[len] == '"')
    src[len] = 0;

  for (dest = str; *src; ) {
    *(dest++) = *(src++);
  }
  *(dest++) = 0;
  return str;
}

char *to_hostmask(const char *nick, const char *hostname)
{
  char *hostmask;
  int len;

  len = strlen(hostname) + strlen(nick) + 4;
  hostmask = mymalloc(len + 1);
  snprintf(hostmask, len, "%s!*@%s", nick, hostname);
  return hostmask;
}

const char *get_basename(const char *pathname)
{
  const char *work;

  work = strrchr(pathname, '/');
  if (work == NULL)
    return pathname;

  return ++work;
}

void shutdown_close(int handle)
{
  /*
   * cygwin close() is broke, if outstanding data is present
   * it will block until the TCP connection is dead, sometimes
   * upto 10-20 minutes, calling shutdown() first seems to help
   */
  shutdown(handle, SHUT_RDWR);
  close(handle);
}

int get_port(ir_sockaddr_union_t *listenaddr)
{
  if (listenaddr->sa.sa_family == AF_INET)
    return ntohs(listenaddr->sin.sin_port);

  return ntohs(listenaddr->sin6.sin6_port);
}

int strcmp_null(const char *s1, const char *s2)
{
  if ((s1 == NULL) && (s2 == NULL))
    return 0;

  if (s1 == NULL)
    return -1;

  if (s2 == NULL)
    return 1;

  return strcmp(s1, s2);
}

int is_file_writeable(const char *f)
{
   int fd;

   updatecontext();

   if (!f) return 0;

   fd = open(f, O_WRONLY | O_EXCL | O_APPEND | ADDED_OPEN_FLAGS);
   if (fd < 0)
     return 0;

   close(fd);
   return 1;
}

char *hostmask_to_fnmatch(const char *str)
{
  char *base;
  char *dest;
  const char *src;
  size_t maxlen;

  updatecontext();

  if (!str) return NULL;

  maxlen = 0;
  for (src=str; *src; src++, maxlen++) {
    if (*src == '#') {
      maxlen += 10;
      continue;
    }
    if ( (*src == '[') || (*src == ']') )
      maxlen ++;
  }
  base = mycalloc(maxlen + 1);
  for (src=str, dest=base; *src; src++) {
    if (*src == '#') {
      *(dest++) = '[';
      *(dest++) = '0';
      *(dest++) = '-';
      *(dest++) = '9';
      *(dest++) = ']';
      *(dest++) = '[';
      *(dest++) = '0';
      *(dest++) = '-';
      *(dest++) = '9';
      *(dest++) = ']';
      *(dest++) = '*';
      continue;
    }
    if ( (*src == '[') || (*src == ']') ) {
      *(dest++) = '\\';
    }
    *(dest++) = *src;
  }
  *dest = 0;
  return base;
}

/*
	BASE 64

	| b64  | b64   | b64   |  b64 |
	| octect1 | octect2 | octect3 |
*/

#if !defined(WITHOUT_HTTP_ADMIN) && !defined(WITHOUT_BLOWFISH)
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

char *
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
#endif

/* End of File */
