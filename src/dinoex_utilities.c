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

#include <fnmatch.h>

char *mystrdup2(const char *str, const char *src_function, const char *src_file, int src_line)
{
   char *copy;

   copy = mymalloc2(strlen(str)+1, 0, src_function, src_file, src_line);
   strcpy(copy, str);
   return copy;
}

int verifyshell(irlist_t *list, const char *file)
{
  char *pattern;

  updatecontext();

  pattern = irlist_get_head(list);
  while (pattern)
    {
    if (fnmatch(pattern,file,FNM_CASEFOLD) == 0)
      {
        return 1;
      }
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

  if (gdata.need_level == 0)
    {
      if (gdata.need_voice == 0)
        return 1;
      /* any prefix is counted as voice */
      if ( prefix != 0 )
        return 1;
      /* no good prefix found try next chan */
      return 0;
    }

  level = 0;
  for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_symbol); ii++)
    {
      if (prefix == gnetwork->prefixes[ii].p_symbol)
        {
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
  while(xd)
    {
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

  updatecontext();

  if (!masterpass || !testpass)
    return 0;

  if ((strlen(masterpass) < 13) ||
      (strlen(testpass) < 5) ||
      (strlen(testpass) > 59))
    return 0;

  pwout = crypt(testpass, masterpass);
  if (strcmp(pwout,masterpass)) return 0;
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
   int i;

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

/* End of File */
