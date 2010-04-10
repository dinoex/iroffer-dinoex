/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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

#include <ctype.h>

#ifndef WITHOUT_MEMSAVE

/* copy a string */
char *mystrdup2(const char *str, const char *src_function, const char *src_file, int src_line)
{
  char *copy;

  copy = (char *)mymalloc2(strlen(str)+1, 0, src_function, src_file, src_line);
  strcpy(copy, str);
  return copy;
}

#endif /* WITHOUT_MEMSAVE */

#ifndef WITHOUT_MEMSAVE
/* append a suffix to a string, recording origin */
char *mystrsuffix2(const char *str, const char *suffix, const char *src_function, const char *src_file, int src_line)
#else
/* append a suffix to a string */
char *mystrsuffix(const char *str, const char *suffix)
#endif /* WITHOUT_MEMSAVE */
{
  char *copy;
  size_t max;

  max = strlen(str) + strlen(suffix) + 1;
#ifndef WITHOUT_MEMSAVE
  copy = (char *)mymalloc2(max, 0, src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
  copy = mymalloc(max);
#endif /* WITHOUT_MEMSAVE */
  snprintf(copy, max, "%s%s", str, suffix);
  return copy;
}

#ifndef WITHOUT_MEMSAVE
/* append a suffix to a string and adding a separation character, recording origin */
char *mystrjoin2(const char *str1, const char *str2, int delimiter, const char *src_function, const char *src_file, int src_line)
#else
/* append a suffix to a string and adding a separation character */
char *mystrjoin(const char *str1, const char *str2, int delimiter)
#endif /* WITHOUT_MEMSAVE */
{
  char *copy;
  size_t len1;
  size_t max;

  len1 = strlen(str1);
  max = len1 + strlen(str2) + 1;
  if ((delimiter != 0 ) && (str1[len1] != delimiter))
    max ++;
#ifndef WITHOUT_MEMSAVE
  copy = (char *)mymalloc2(max, 0, src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
  copy = mymalloc(max);
#endif /* WITHOUT_MEMSAVE */
  if ((delimiter != 0 ) && (str1[len1 - 1] != delimiter)) {
    snprintf(copy, max, "%s%c%s", str1, delimiter, str2);
  } else {
    snprintf(copy, max, "%s%s", str1, str2);
  }
  return copy;
}

/* add a string to a simple list */
void irlist_add_string(irlist_t *list, const char *str)
{
  char *copy;

  copy = irlist_add(list, strlen(str) + 1);
  strcpy(copy, str);
}

/* match a given file against a list of patterns */
int verifyshell(irlist_t *list, const char *file)
{
  char *pattern;

  updatecontext();

  pattern = (char *)irlist_get_head(list);
  while (pattern) {
    if (fnmatch(pattern, file, FNM_CASEFOLD) == 0)
      return 1;
    pattern = (char *)irlist_get_next(pattern);
  }
  return 0;
}

/* returns a nickname fail-safe */
const char *save_nick(const char * nick)
{
  if (nick)
    return nick;
  return "??";
}

/* verify a password against the stored hash */
int verifypass2(const char *masterpass, const char *testpass)
{
#ifndef NO_CRYPT
  char *pwout;
#endif /* NO_CRYPT */

  updatecontext();

  if (!masterpass || !testpass)
    return 0;

#ifndef NO_CRYPT
  if ((strlen(masterpass) < 13U) ||
      (strlen(testpass) < 5U) ||
      (strlen(testpass) > 59U))
    return 0;

  pwout = crypt(testpass, masterpass);
  if (strcmp(pwout, masterpass)) return 0;
#else /* NO_CRYPT */
  if (!masterpass || !testpass)
    return 0;

  if (strcmp(testpass, masterpass)) return 0;
#endif /* NO_CRYPT */

  /* allow */
  return 1;
}

/* validate config and warn if password is not encrypted */
void checkadminpass2(const char *masterpass)
{
#ifndef NO_CRYPT
  int err=0;
  unsigned int i;

  updatecontext();

  if (!masterpass || strlen(masterpass) < 13U) err++;

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
#endif /* NO_CRYPT */
}

/* strip quotes from a string */
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

static void replace_char(char *str, int ch1, int ch2)
{
  char *work;

  for (work=str; *work; work++) {
    if (*work == ch1)
      *work = ch2;
  }
}

/* build a pattern to match this user and hostname */
char *to_hostmask(const char *nick, const char *hostname)
{
  char *nick2;
  char *hostmask;
  size_t len;

  len = strlen(hostname) + strlen(nick) + 4;
  hostmask = (char *)mymalloc(len);
  nick2 = mystrdup(nick);
  replace_char(nick2, '[', '?');
  replace_char(nick2, ']', '?');
  snprintf(hostmask, len, "%s!*@%s", nick2, hostname);
  mydelete(nick2);
  return hostmask;
}

/* returns the filename part of a full pathname */
const char *getfilename(const char *pathname)
{
  const char *work;

  work = strrchr(pathname, '/');
  if (work == NULL)
    return pathname;

  return ++work;
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
  close(handle);
}

/* get the port number from a socket */
int get_port(ir_sockaddr_union_t *listenaddr)
{
  if (listenaddr->sa.sa_family == AF_INET)
    return ntohs(listenaddr->sin.sin_port);

  return ntohs(listenaddr->sin6.sin6_port);
}

/* strcmp fail-safe */
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

/* check if a file is writeable */
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

/* convert hostmask to fnmatch pattern */
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
  base = (char *)mycalloc(maxlen + 1);
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

/* check if given group can be found in a list of allowed groups */
int verify_group_in_grouplist(const char *group, const char *grouplist)
{
  const char *tlptr; /* character to compare */
  const char *sptr; /* character to find */

  /* null grouplist means no restrictions */
  if (!grouplist)
    return 1;

  /* packs with no group set are always visible */
  if (!group)
    return 1;

  /* case insensitive token search */
  /* delimiters: space or coma */
  sptr = group;
  for (tlptr = grouplist; *tlptr; tlptr++) {
    if ((*tlptr == ' ') || (*tlptr == ',')) {
      sptr = group; /* end of token, reset search */
      continue;
    }
    if (sptr == NULL)
      continue; /* skip comparison until next token of tokenlist  */

    if (toupper(*tlptr) != toupper(*sptr)) {
      sptr = NULL; /* strings differ */
      continue;
    }

    sptr++;
    if (*sptr != 0)
      continue;

    if ((*(tlptr+1) == ' ') || (*(tlptr+1) == ',') || (*(tlptr+1) == '\0'))
      return 1; /* token found */

    sptr = NULL; /* string length mismatch */
  }
  return 0; /* token not found */
}

static const char const size_units[] = { 'K', 'M', 'G', 'T', 'E', 0 };

/* returns the size in a human readable form */
char *sizestr(int spaces, off_t num)
{
#define SIZESTR_SIZE 5
  char *str = (char *)mycalloc(SIZESTR_SIZE);
  float val;
  int i;

  if (num <= 0) {
    snprintf(str, SIZESTR_SIZE, spaces ? "%4s" : "%s", "0");
    return str;
  }
  if (num < 1024) {
    snprintf(str, SIZESTR_SIZE, spaces ?  "%4s" : "%s", "<1K");
    return str;
  }
  /* KB */
  val = (float)num;
  for (i = 0; size_units[i]; i++) {
    val /= 1024.0;
    if ((i > 0) && (val < 9.5)) {
      snprintf(str, SIZESTR_SIZE, spaces ?  "%2.1f%c" : "%.1f%c", val, size_units[i]);
      return str;
    }
    if (val < 999.5) {
      snprintf(str, SIZESTR_SIZE, spaces ?  "%3.0f%c" : "%.0f%c", val, size_units[i]);
      return str;
    }
  }
  mydelete(str);
  str = (char *)mycalloc(SIZESTR_SIZE + SIZESTR_SIZE);
  snprintf(str, SIZESTR_SIZE + SIZESTR_SIZE , "%.0fE", val);
  return str;
}

/* check for non ASCII chars */
int isprintable(int a)
{
  if ( (unsigned char)a < 0x20U )
     return 0;
  if ( (unsigned char)a == 0x7FU )
     return 0;
  return 1;
}

/* convert to ASCII char */
int onlyprintable(int a)
{
  if ( (unsigned char)a < 0x20U )
     return '.';
  if ( (unsigned char)a == 0x7FU )
     return '.';
  return a;
}

/* remove unknown control codes */
char *removenonprintable(char *str)
{
  unsigned char *copy;

  if (str != NULL) {
    for (copy=(unsigned char *)str; *copy != 0; copy++) {
      if (*copy == 0x7FU) {
        *copy = '.';
        continue;
      }
      if (*copy >= 0x20U)
        continue;

      switch (*copy) {
      case 0x01U: /* ctcp */
      case 0x02U: /* bold */
      case 0x03U: /* color */
      case 0x09U: /* tab */
      case 0x0AU: /* lf */
      case 0x0DU: /* cr */
      case 0x0FU: /* end formatting */
      case 0x16U: /* inverse */
      case 0x1FU: /* underline */
        /* good */
        break;
      default:
        *copy = '.';
        break;
      }
    }
  }
  return str;
}

/* remove all control codes */
char *removenonprintablectrl(char *str)
{
  unsigned char *copy;

  if (str != NULL) {
    for (copy=(unsigned char *)str; *copy != 0; copy++) {
      if (*copy == 0x7FU) {
        *copy = ' ';
        continue;
      }
      if (*copy < 0x20U)
        *copy = ' ';
    }
  }
  return str;
}

/* remove control codes and color */
char *removenonprintablefile(char *str)
{
  unsigned char *copy;
  char last = '/';

  if (!str)
    return NULL;

  for (copy = (unsigned char*)str;
       *copy != 0;
       copy++) {
    if (*copy == 0x03U) { /* color */
      if (!isdigit(copy[1])) continue;
      copy++;
      if (isdigit(copy[1])) copy++;
      if (copy[1] != ',') continue;
      copy++;
      if (isdigit(copy[1])) copy++;
      if (isdigit(copy[1])) copy++;
      continue;
    }
    if (*copy < 0x20U) {
      *copy = '_';
      continue;
    }
    switch (*copy) {
    case '.':
      /* don't start any name with '.' */
      if (last == '/')
        *copy = '_';
      break;
    case ' ':
      if (gdata.spaces_in_filenames)
        break;
    case '|':
    case ':':
    case '?':
    case '*':
    case '<':
    case '/':
    case '\\':
    case '"':
    case '\'':
    case '`':
    case 0x7FU:
      *copy = '_';
      break;
    }
    last = *copy;
  }
  return str;
}

/* convert a string to uppercase */
char *caps(char *str)
{
  unsigned char *copy;

  if (!str)
    return NULL;

  for (copy = (unsigned char*)str;
       *copy != 0;
       copy++) {
    if ( islower( *copy ) )
      *copy = toupper( *copy );
  }
  return str;
}

/* calculate minutes left for reaching a timestamp */
int max_minutes_waits(time_t *endtime, int min)
{
  *endtime = gdata.curtime;
  *endtime += (60 * min);
  if (*endtime < gdata.curtime) {
    *endtime = 0x7FFFFFFF;
    min = (*endtime - gdata.curtime)/60;
  }
  (*endtime)--;
  return min;
}

static void clean_missing_parts(char **result, int part, int howmany)
{
  int i;
  for (i = part; i < howmany; i++)
    result[i] = NULL;
}

#ifndef WITHOUT_MEMSAVE
/* split a line in a number of arguments, recording orign */
int get_argv2(char **result, const char *line, int howmany, const char *src_function, const char *src_file, int src_line)
#else /* WITHOUT_MEMSAVE */
/* split a line in a number of arguments */
int get_argv(char **result, const char *line, int howmany)
#endif /* WITHOUT_MEMSAVE */
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  int inquotes;
  int moreargs;
  int morequote;
  int part;

  if (howmany <= 0)
    return 0;

  if (line == NULL) {
    clean_missing_parts(result, 0, howmany);
    return 0;
  }

  inquotes = 0;
  moreargs = 0;
  morequote = 0;
  part = 0;

  start = line;
  for (src = start; ; src++) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        inquotes ++;
        continue;
      }
      if (inquotes == 0)
        continue;
      inquotes --;
      if (part + 1 == howmany) {
        morequote ++;
        continue;
      }
      start ++;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          start ++;
          continue;
        }
        if (part + 1 == howmany) {
          moreargs ++;
          continue;
        }
      }
    }
    plen = src - start;
    if (plen == 0)
      continue;

    if (*src == '"')
      src ++;

    /* found end */
#ifndef WITHOUT_MEMSAVE
    dest = (char *)mymalloc2(plen + 1, 0, src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
    dest = mymalloc(plen + 1);
#endif /* WITHOUT_MEMSAVE */
    memcpy(dest, start, plen);
    dest[plen] = '\0';
    if ((morequote > 0) && (moreargs == 0))
      clean_quotes(dest);
    result[part++] = dest;
    if (part >= howmany)
      return part;
    if (*src == 0) {
      clean_missing_parts(result, part, howmany);
      return part;
    }
    start = src + 1;
  }
}

#ifndef WITHOUT_MEMSAVE
/* extract one argument from a line, recording orign */
char *getpart2(const char *line, int howmany, const char *src_function, const char *src_file, int src_line)
#else /* WITHOUT_MEMSAVE */
/* extract one argument from a line */
char *getpart(const char *line, int howmany)
#endif /* WITHOUT_MEMSAVE */
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  int inquotes;
  int part;

  if (line == NULL)
    return NULL;

  if (howmany <= 0)
    return NULL;

  inquotes = 0;
  part = 0;

  start = line;
  for (src = start; ; src++) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        inquotes ++;
        continue;
      }
      if (inquotes == 0)
        continue;
      inquotes --;
      start ++;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          start ++;
          continue;
        }
      }
    }
    plen = src - start;
    if (plen == 0)
      continue;

    if (++part < howmany) {
      if (*src == 0)
        return NULL;
      start = src + 1;
      continue;
    }

    /* found end */
    break;
  }
#ifndef WITHOUT_MEMSAVE
  dest = (char *)mymalloc2(plen + 1, 0, src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
  dest = mymalloc(plen + 1);
#endif /* WITHOUT_MEMSAVE */
  memcpy(dest, start, plen);
  dest[plen] = '\0';
  return dest;
}

/* extract everything starting with the given argument */
char *getpart_eol(const char *line, int howmany)
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  int inquotes;
  int moreargs;
  int morequote;
  int part;

  if (line == NULL)
    return NULL;

  if (howmany <= 0)
    return NULL;

  inquotes = 0;
  moreargs = 0;
  morequote = 0;
  part = 0;

  start = line;
  for (src = start; ; src++) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        inquotes ++;
        continue;
      }
      if (inquotes == 0)
        continue;
      inquotes --;
      if (part + 1 == howmany) {
        morequote ++;
        continue;
      }
      start ++;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          start ++;
          continue;
        }
        if (part + 1 == howmany) {
          moreargs ++;
          continue;
        }
      }
    }
    plen = src - start;
    if (plen == 0)
      continue;

   if (++part < howmany) {
      if (*src == 0)
        return NULL;
      start = src + 1;
      continue;
    }

    /* found end */
    break;
  }
  dest = mystrdup(start);
  if ((morequote > 0) && (moreargs == 0))
    clean_quotes(dest);
  return dest;
}

/* count length of pattern without wildcards */
int convert_spaces_to_match(char *str)
{
  int k;

  for (k = 0; *str; str++) {
    if (*str == ' ') *str = '*';
    if (*str == '*')
      continue;
    if (*str == '#')
      continue;
    if (*str == '?')
      continue;
    k++;
  }
  return k;
}

/* convert a timeval into ms */
ir_uint64 timeval_to_ms(struct timeval *tv)
{
  return (((ir_uint64)(tv->tv_sec)) * 1000) + (((ir_uint64)(tv->tv_usec)) / 1000);
}

/* sort a linked list with selection sort */
void irlist_sort2(irlist_t *list, int (*cmpfunc)(const void *a, const void *b))
{
  irlist_t newlist = {0, 0};
  void *cur;
  void *try;
  void *last;

  while ((cur = irlist_get_head(list))) {
    irlist_remove(list, cur);

    try = irlist_get_head(&newlist);
    if (!try) {
      irlist_insert_head(&newlist, cur);
      continue;
    }

    last = NULL;
    while (try) {
      if (cmpfunc(cur, try) < 0) {
        break;
      }
      last = try;
      try = irlist_get_next(try);
    }

    if (!last) {
      irlist_insert_head(&newlist, cur);
    } else {
      irlist_insert_after(&newlist, cur, last);
    }
  }

  *list = newlist;
  return;
}

/* End of File */
