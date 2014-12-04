/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2014 Dirk Meyer
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
char *mystrdup2(const char *str, const char *src_function, const char *src_file, unsigned int src_line)
{
  char *copy;

  copy = (char *)mymalloc2(strlen(str)+1, 0, src_function, src_file, src_line);
  strcpy(copy, str);
  return copy;
}

#endif /* WITHOUT_MEMSAVE */

#ifndef WITHOUT_MEMSAVE
/* append a suffix to a string, recording origin */
char *mystrsuffix2(const char *str, const char *suffix, const char *src_function, const char *src_file, unsigned int src_line)
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
  snprintf(copy, max, "%s%s", str, suffix); /* NOTRANSLATE */
  return copy;
}

#ifndef WITHOUT_MEMSAVE
/* append a suffix to a string and adding a separation character, recording origin */
char *mystrjoin2(const char *str1, const char *str2, unsigned int delimiter, const char *src_function, const char *src_file, unsigned int src_line)
#else
/* append a suffix to a string and adding a separation character */
char *mystrjoin(const char *str1, const char *str2, unsigned int delimiter)
#endif /* WITHOUT_MEMSAVE */
{
  char *copy;
  size_t len1;
  size_t max;

  len1 = strlen(str1);
  max = len1 + strlen(str2) + 1;
  if ((delimiter != 0 ) && ((unsigned char)str1[len1] != (unsigned char)delimiter))
    ++max;
#ifndef WITHOUT_MEMSAVE
  copy = (char *)mymalloc2(max, 0, src_function, src_file, src_line);
#else /* WITHOUT_MEMSAVE */
  copy = mymalloc(max);
#endif /* WITHOUT_MEMSAVE */
  if ((delimiter != 0 ) && ((unsigned char)str1[len1 - 1] != (unsigned char)delimiter)) {
    snprintf(copy, max, "%s%c%s", str1, delimiter, str2); /* NOTRANSLATE */
  } else {
    snprintf(copy, max, "%s%s", str1, str2); /* NOTRANSLATE */
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
unsigned int verifyshell(irlist_t *list, const char *file)
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

/* match a given file against a list of required patterns */
unsigned int no_verifyshell(irlist_t *list, const char *file)
{
  if (irlist_size(list)) {
    if (!verifyshell(list, file))
      return 1;
  }
  return 0;
}

/* returns a nickname fail-safe */
const char *save_nick(const char * nick)
{
  if (nick)
    return nick;
  return "??"; /* NOTRANSLATE */
}

/* verify a password against the stored hash */
unsigned int verifypass2(const char *masterpass, const char *testpass)
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

/* strip quotes from a string */
char *clean_quotes(char *str)
{
  char *src;
  char *dest;
  size_t len;

  if (str[0] != '"')
    return str;

  src = str + 1;
  if (str[0] == 0)
    return str;

  len = strlen(src);
  --len;
  if (src[len] == '"')
    src[len] = 0;

  for (dest = str; *src; ) {
    *(dest++) = *(src++);
  }
  *dest = 0;
  return str;
}

static void replace_char(char *str, unsigned int ch1, unsigned int ch2)
{
  unsigned char *work;

  for (work=(unsigned char *)str; *work; ++work) {
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

/* get the port number from a socket */
ir_uint16 get_port(ir_sockaddr_union_t *listenaddr)
{
  ir_uint16 netval;

  if (listenaddr->sa.sa_family == AF_INET)
    netval = listenaddr->sin.sin_port;
  else
    netval = listenaddr->sin6.sin6_port;

  return ntohs(netval);
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
unsigned int is_file_writeable(const char *f)
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

/* open file to append data */
int open_append(const char *filename, const char *text)
{
  int fd;

  fd = open(filename,
            O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Cant Create %s File '%s': %s",
             text, filename, strerror(errno));
  }
  return fd;
}

/* open log file to append data */
int open_append_log(const char *filename, const char *text)
{
  int fd;

  fd = open(filename,
            O_WRONLY | O_CREAT | O_APPEND | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  if (fd < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD | OUTERROR_TYPE_NOLOG,
             "Cant Create %s File '%s': %s",
             text, filename, strerror(errno));
  }
  return fd;
}

static void mylog_write_failed(const char *filename)
{
  outerror(OUTERROR_TYPE_WARN_LOUD | OUTERROR_TYPE_NOLOG,
           "Cant Write Log File '%s': %s",
           filename, strerror(errno));
}

/* write data and warn if not all bytes has been written */
void mylog_write(int fd, const char *filename, const char *msg, size_t len)
{
  ssize_t len2;

  len2 = write(fd, msg, len);
  if (len2 > 0) {
    if ((size_t)len2 == len)
      return;
  }
  mylog_write_failed(filename);
}

/* close logfile and warn if not all bytes has been written */
void mylog_close(int fd, const char *filename)
{
  int rc;

  rc = close(fd);
  if (rc != 0)
    mylog_write_failed(filename);
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
  for (src=str; *src; ++src, ++maxlen) {
    if (*src == '#') {
      maxlen += 10;
      continue;
    }
    if ( (*src == '[') || (*src == ']') )
      ++maxlen;
  }
  base = (char *)mymalloc(maxlen + 1);
  for (src=str, dest=base; *src; ++src) {
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
unsigned int verify_group_in_grouplist(const char *group, const char *grouplist)
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
  for (tlptr = grouplist; *tlptr; ++tlptr) {
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

    ++sptr;
    if (*sptr != 0)
      continue;

    if ((*(tlptr+1) == ' ') || (*(tlptr+1) == ',') || (*(tlptr+1) == '\0'))
      return 1; /* token found */

    sptr = NULL; /* string length mismatch */
  }
  return 0; /* token not found */
}

static const char size_units[] = { 'K', 'M', 'G', 'T', 'P', 0 };

/* returns the size in a human readable form */
char *sizestr(unsigned int spaces, off_t num)
{
#define SIZESTR_SIZE 5L
  char *str = (char *)mymalloc(SIZESTR_SIZE);
  float val;
  unsigned int i;

  if (num <= 0) {
    snprintf(str, SIZESTR_SIZE, spaces ? "%4s" : "%s", "0"); /* NOTRANSLATE */
    return str;
  }
  if (num < 1024) {
    snprintf(str, SIZESTR_SIZE, spaces ?  "%4s" : "%s", "<1k"); /* NOTRANSLATE */
    return str;
  }
  /* KB */
  val = (float)num;
  for (i = 0; size_units[i]; ++i) {
    val /= 1024.0;
    if ((i > 0) && (val < 9.5)) {
      snprintf(str, SIZESTR_SIZE, spaces ?  "%2.1f%c" : "%.1f%c", val, size_units[i]); /* NOTRANSLATE */
      return str;
    }
    if (val < 999.5) {
      snprintf(str, SIZESTR_SIZE, spaces ?  "%3.0f%c" : "%.0f%c", val, size_units[i]); /* NOTRANSLATE */
      return str;
    }
  }
  mydelete(str);
  str = (char *)mymalloc(SIZESTR_SIZE + SIZESTR_SIZE);
  snprintf(str, SIZESTR_SIZE + SIZESTR_SIZE , "%.0fE", val); /* NOTRANSLATE */
  return str;
}

/* check for non ASCII chars */
unsigned int isprintable(unsigned int a)
{
  if ( (unsigned char)a < 0x20U )
     return 0;
  if ( (unsigned char)a == 0x7FU )
     return 0;
  return 1;
}

/* remove unknown control codes */
size_t removenonprintable(char *str)
{
  unsigned char *copy;
  unsigned char *dest;

  if (str == NULL)
    return 0;

  dest = (unsigned char *)str;
  for (copy=dest; *copy != 0; ++copy) {
    if (*copy == IRCCOLOR) { /* color */
      if (!isdigit(copy[1])) continue;
      ++copy;
      if (!isdigit(copy[1])) continue;
      ++copy;
      if (copy[1] != ',') continue;
      ++copy;
      if (!isdigit(copy[1])) continue;
      ++copy;
      if (!isdigit(copy[1])) continue;
      ++copy;
      continue;
    }
    switch (*copy) {
    case IRCBOLD: /* bold */
    case IRCNORMAL: /* end formatting */
    case IRCINVERSE: /* inverse */
    case IRCITALIC: /* italic */
    case IRCUNDERLINE: /* underline */
    case 0x7FU:
      break;
    default:
      *(dest++) = *copy;
      break;
    }
  }
  *dest = 0;
  return (dest - (unsigned char *)str);
}

/* remove control codes and color */
void removenonprintablefile(char *str)
{
  unsigned char *copy;
  unsigned char last = '/';

  if (str == NULL)
    return;

  for (copy = (unsigned char*)str; *copy != 0; ++copy) {
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
    case '>':
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
}

/* create a filename without special characters for transfers */
char *getsendname(const char * const full)
{
  char *copy;
  size_t i;
  size_t lastslash;
  size_t spaced;
  size_t len;

  updatecontext();

  len = sstrlen(full);
  lastslash = 0;
  spaced = 0;
  for (i = 0 ; i < len ; i++) {
    switch (full[i]) {
    case '/':
    case '\\':
      lastslash = i + 1;
      spaced = 0;
      break;
    case ' ':
      spaced = 1;
      break;
    }
  }

  len -= lastslash;
  copy = (char *)mymalloc(len + 1 + spaced + spaced);

  if ((spaced != 0) && (gdata.spaces_in_filenames != 0))
    sprintf(copy, "\"%s\"", full + lastslash);
  else
    strcpy(copy, full + lastslash);

  /* replace any evil characters in the filename with underscores */
  for (i = spaced; i < len; i++) {
    switch (copy[i]) {
    case  ' ':
      if (gdata.spaces_in_filenames == 0)
        copy[i] = '_';
      break;
    case '|':
    case ':':
    case '?':
    case '*':
    case '<':
    case '>':
    case '/':
    case '\\':
    case '"':
    case '\'':
    case '`':
    case 0x7FU:
      copy[i] = '_';
      break;
    }
  }
  return copy;
}

/* convert a string to uppercase */
char *caps(char *str)
{
  unsigned char *copy;

  if (str == NULL)
    return str;

  for (copy = (unsigned char*)str; *copy != 0; ++copy) {
    if ( islower( *copy ) )
      *copy = toupper( *copy );
  }
  return str;
}

/* calculate minutes left for reaching a timestamp */
unsigned int max_minutes_waits(time_t *endtime, unsigned int min)
{
  *endtime = gdata.curtime;
  *endtime += (60 * min);
  if (*endtime < gdata.curtime) {
    *endtime = 0x7FFFFFFF;
    min = (unsigned int)((*endtime - gdata.curtime) / 60);
  }
  --(*endtime);
  return min;
}

static void clean_missing_parts(char **result, unsigned int part, unsigned int howmany)
{
  unsigned int i;
  for (i = part; i < howmany; ++i)
    result[i] = NULL;
}

#ifndef WITHOUT_MEMSAVE
/* split a line in a number of arguments, recording orign */
unsigned int get_argv2(char **result, const char *line, unsigned int howmany, const char *src_function, const char *src_file, unsigned int src_line)
#else /* WITHOUT_MEMSAVE */
/* split a line in a number of arguments */
unsigned int get_argv(char **result, const char *line, unsigned int howmany)
#endif /* WITHOUT_MEMSAVE */
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  unsigned int inquotes;
  unsigned int moreargs;
  unsigned int morequote;
  unsigned int part;

  if (howmany == 0)
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
  for (src = start; ; ++src) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        ++inquotes;
        continue;
      }
      if (inquotes == 0)
        continue;
      --inquotes;
      if (part + 1 == howmany) {
        ++morequote;
        continue;
      }
      ++start;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          ++start;
          continue;
        }
        if (part + 1 == howmany) {
          ++moreargs;
          continue;
        }
      }
    }
    plen = src - start;
    if (plen == 0)
      continue;

    if (*src == '"')
      ++src;

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
char *getpart2(const char *line, unsigned int howmany, const char *src_function, const char *src_file, unsigned int src_line)
#else /* WITHOUT_MEMSAVE */
/* extract one argument from a line */
char *getpart(const char *line, unsigned int howmany)
#endif /* WITHOUT_MEMSAVE */
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  unsigned int inquotes;
  unsigned int part;

  if (line == NULL)
    return NULL;

  if (howmany == 0)
    return NULL;

  inquotes = 0;
  part = 0;

  start = line;
  for (src = start; ; ++src) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        ++inquotes;
        continue;
      }
      if (inquotes == 0)
        continue;
      --inquotes;
      ++start;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          ++start;
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
char *getpart_eol(const char *line, unsigned int howmany)
{
  const char *start;
  const char *src;
  char *dest;
  size_t plen;
  unsigned int inquotes;
  unsigned int moreargs;
  unsigned int morequote;
  unsigned int part;

  if (line == NULL)
    return NULL;

  if (howmany <= 0)
    return NULL;

  inquotes = 0;
  moreargs = 0;
  morequote = 0;
  part = 0;

  start = line;
  for (src = start; ; ++src) {
    if ((*src == ' ') && (inquotes != 0))
      continue;
    if (*src == '"') {
      if ((start == src) && (inquotes == 0)) {
        ++inquotes;
        continue;
      }
      if (inquotes == 0)
        continue;
      --inquotes;
      if (part + 1 == howmany) {
        ++morequote;
        continue;
      }
      ++start;
    } else {
      if (*src) {
        if (*src != ' ')
          continue;
        if (src == start) {
          /* skip leading spaces */
          ++start;
          continue;
        }
        if (part + 1 == howmany) {
          ++moreargs;
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
unsigned int convert_spaces_to_match(char *str)
{
  unsigned int k;

  for (k = 0; *str; ++str) {
    if (*str == ' ') *str = '*';
    if (*str == '*')
      continue;
    if (*str == '#')
      continue;
    if (*str == '?')
      continue;
    ++k;
  }
  return k;
}

/* convert a timeval into ms */
ir_uint64 timeval_to_ms(struct timeval *tv)
{
  return (((ir_uint64)(tv->tv_sec)) * 1000) + (((ir_uint64)(tv->tv_usec)) / 1000);
}

/* get current time in ms */
ir_uint64 get_time_in_ms(void)
{
  struct timeval timestruct;

  gettimeofday(&timestruct, NULL);
  return timeval_to_ms(&timestruct);
}

/* get local date and time in ISO */
char *user_getdatestr(char* str, time_t Tp, size_t len)
{
  const char *format;
  struct tm *localt = NULL;
  size_t llen;

  if (Tp == 0)
    Tp = gdata.curtime;
  localt = localtime(&Tp);
  format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";
  llen = strftime(str, len, format, localt);
  if ((llen == 0) || (llen == len))
    str[0] = '\0';
  return str;
}

/* convert a search string into fnmatch */
char *grep_to_fnmatch(const char *grep)
{
  char *raw;
  char *match;
  size_t len;

  len = strlen(grep) + 3;
  raw = mymalloc(len);
  snprintf(raw, len, "*%s*", grep); /* NOTRANSLATE */
  match = hostmask_to_fnmatch(raw);
  mydelete(raw);
  return match;
}

/* get a random number between 0 and max, max excluded */
unsigned int get_random_uint( unsigned int max )
{
  unsigned int val;

  val = (unsigned int)( (((float)(max)) * rand() ) / (0.0 + RAND_MAX) );
  if (val >= max)
    val = max - 1;
  return val;
}

static size_t
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 0)))
#endif
/* add a printf text to a string buffer */
add_vsnprintf(char * str, size_t size, const char * format, va_list ap)
{
  ssize_t slen;
  size_t ulen;

  slen = vsnprintf(str, size, format, ap);
  if (slen < 0) {
    *str = 0; /* terminating 0-byte */
    return 0;
  }
  ulen = (size_t)slen;
  --size; /* terminating 0-byte */
  if (ulen > size)
    return size;
  return ulen;
}

size_t
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
/* add a printf text to a string buffer */
add_snprintf(char * str, size_t size, const char * format, ...)
{
  va_list args;
  size_t len;

  va_start(args, format);
  len = add_vsnprintf(str, size, format, args);
  va_end(args);
  return len;
}

#ifndef USE_MERGESORT
/* sort a linked list with selection sort */
void irlist_sort2(irlist_t *list, int (*cmpfunc)(const void *a, const void *b))
{
  irlist_t newlist = {0, 0, 0};
  void *cur;
  void *ltry;
  void *last;

  while ((cur = irlist_get_head(list))) {
    irlist_remove(list, cur);

    ltry = irlist_get_head(&newlist);
    if (!ltry) {
      irlist_insert_head(&newlist, cur);
      continue;
    }

    last = NULL;
    while (ltry) {
      if (cmpfunc(cur, ltry) < 0) {
        break;
      }
      last = ltry;
      ltry = irlist_get_next(ltry);
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
#else /* USE_MERGESORT */
/* sort a linked list with merge sort */
void irlist_sort2(irlist_t *list, int (*cmpfunc)(const void *a, const void *b))
{
  irlist_t newlist = {0, 0, 0};
  irlist_t *p;
  irlist_t *q;
  irlist_t *e;
  irlist_t *tail;
  int insize;
  int nmerges;
  int psize;
  int qsize;
  int i;

  insize = 1;
  for(;;) {
    p = irlist_get_head(list);
    list = NULL;
    tail = NULL;
    nmerges = 0; /* count number of merges we do in this pass */

    while (p) {
      ++nmerges;  /* there exists a merge to be done */
      /* step `insize' places along from p */
      q = p;
      psize = 0;
      for (i = 0; i < insize; ++i) {
        ++psize;
        q = irlist_get_next(q);
        if (!q) break;
      }

      /* if q hasn't fallen off end, we have two lists to merge */
      qsize = insize;

      /* now we have two lists; merge them */
      while (psize > 0 || (qsize > 0 && q)) {

        /* decide whether next element of merge comes from p or q */
        if (psize == 0) {
          /* p is empty; e must come from q. */
          e = q;
          q = irlist_get_next(q);
	  irlist_remove(list, e);
          qsize--;
        } else if (qsize == 0 || !q) {
          /* q is empty; e must come from p. */
          e = p;
          p = irlist_get_next(p);
	  irlist_remove(list, e);
          psize--;
        } else if (cmpfunc(p, q) <= 0) {
          /* First element of p is lower (or same);
          * e must come from p. */
          e = p;
          p = irlist_get_next(p);
	  irlist_remove(list, e);
          psize--;
        } else {
          /* First element of q is lower; e must come from q. */
          e = q;
          q = irlist_get_next(q);
	  irlist_remove(list, e);
          qsize--;
        }

        /* add the next element to the merged list */
        if (tail) {
          irlist_insert_after(&newlist, e, tail);
        } else {
          list = e;
        }
        tail = e;
      }

      /* now p has stepped `insize' places along, and q has too */
      p = q;
    }

    /* If we have done only one merge, we're finished. */
    if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
      break;

    /* Otherwise repeat, merging lists twice the size */
    insize *= 2;
  }
  *list = newlist;
}
#endif /* USE_MERGESORT */

/* End of File */
