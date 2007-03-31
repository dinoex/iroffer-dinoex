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

char *mystrdup(const char *str)
{
   char *copy;

   copy = mymalloc(strlen(str)+1);
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

/* End of File */
