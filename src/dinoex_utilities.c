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

/* End of File */
