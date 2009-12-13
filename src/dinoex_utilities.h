/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2009 Dirk Meyer
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
#define mystrsuffix(x,y) mystrsuffix2(x,y,__FUNCTION__,__FILE__,__LINE__)
char *mystrsuffix2(const char *str, const char *suffix, const char *src_function, const char *src_file, int src_line);
#define mystrjoin(x,y,z) mystrjoin2(x,y,z,__FUNCTION__,__FILE__,__LINE__)
char *mystrjoin2(const char *str1, const char *str2, char delimiter, const char *src_function, const char *src_file, int src_line);
#else
#define mystrdup(x) strdup(x)
char *mystrsuffix(const char *str, const char *suffix);
char *mystrjoin(const char *str1, const char *str2, char delimiter);
#endif
void irlist_add_string(irlist_t *list, const char *str);
int verifyshell(irlist_t *list, const char *file);
const char *save_nick(const char * nick);
int verifypass2(const char *masterpass, const char *testpass);
void checkadminpass2(const char *masterpass);
char *clean_quotes(char *str);
char *to_hostmask(const char *nick, const char *hostname);
const char *getfilename(const char *pathname);
void shutdown_close(int handle);
int get_port(ir_sockaddr_union_t *listenaddr);
int strcmp_null(const char *s1, const char *s2);
int is_file_writeable(const char *f);
char *hostmask_to_fnmatch(const char *str);
int verify_group_in_grouplist(const char *group, const char *grouplist);
char *sizestr(int spaces, off_t num);
int isprintable(char a);
char onlyprintable(char a);
char *removenonprintable(char *str);
char *removenonprintablectrl(char *str);
char *removenonprintablefile(char *str);
char *caps(char *str);
int max_minutes_waits(time_t *endtime, int min);

#ifndef WITHOUT_MEMSAVE
#define get_argv(x,y,z) get_argv2(x,y,z,__FUNCTION__,__FILE__,__LINE__)
int get_argv2(char **result, const char *line, int howmany,
             const char *src_function, const char *src_file, int src_line);
#else /* WITHOUT_MEMSAVE */
int get_argv(char **result, const char *line, int howmany);
#endif /* WITHOUT_MEMSAVE */
#ifndef WITHOUT_MEMSAVE
#define getpart(x,y) getpart2(x,y,__FUNCTION__,__FILE__,__LINE__)
char *getpart2(const char *line, int howmany, const char *src_function, const char *src_file, int src_line);
#else
char *getpart(const char *line, int howmany);
#endif
char *getpart_eol(const char *line, int howmany);
int convert_spaces_to_match(char *str);
ir_uint64 timeval_to_ms(struct timeval *tv);

void irlist_sort2(irlist_t *list, int (*cmpfunc)(const void *a, const void *b));

/* End of File */
