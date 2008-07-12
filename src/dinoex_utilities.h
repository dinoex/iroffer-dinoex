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

#ifndef WITHOUT_MEMSAVE
#define mystrdup(x) mystrdup2(x,__FUNCTION__,__FILE__,__LINE__)
char *mystrdup2(const char *str, const char *src_function, const char *src_file, int src_line);
#else
#define mystrdup(x) strdup(x)
#endif
int verifyshell(irlist_t *list, const char *file);
const char *save_nick(const char * nick);
int verifypass2(const char *masterpass, const char *testpass);
void checkadminpass2(const char *masterpass);
char *clean_quotes(char *str);
char *to_hostmask(const char *nick, const char *hostname);
const char *get_basename(const char *pathname);
void shutdown_close(int handle);
int get_port(ir_sockaddr_union_t *listenaddr);
int strcmp_null(const char *s1, const char *s2);
int is_file_writeable(const char *f);
char *hostmask_to_fnmatch(const char *str);
int verify_group_in_grouplist(const char *group, const char *grouplist);

/* End of File */
