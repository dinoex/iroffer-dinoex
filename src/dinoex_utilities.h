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

char *mystrdup(const char *str);
int verifyshell(irlist_t *list, const char *file);
const char *save_nick(const char * nick);
int check_level(char prefix);
int number_of_pack(xdcc *pack);
int verifypass2(const char *masterpass, const char *testpass);

/* End of File */
