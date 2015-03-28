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

#ifndef WITHOUT_MEMSAVE
#define mystrdup(x) mystrdup2(x,__FUNCTION__,__FILE__,__LINE__)
char *mystrdup2(const char *str, const char *src_function, const char *src_file, unsigned int src_line);
#define mystrsuffix(x,y) mystrsuffix2(x,y,__FUNCTION__,__FILE__,__LINE__)
char *mystrsuffix2(const char *str, const char *suffix, const char *src_function, const char *src_file, unsigned int src_line);
#define mystrjoin(x,y,z) mystrjoin2(x,y,z,__FUNCTION__,__FILE__,__LINE__)
char *mystrjoin2(const char *str1, const char *str2, unsigned int delimiter, const char *src_function, const char *src_file, unsigned int src_line);
#else
#define mystrdup(x) strdup(x)
char *mystrsuffix(const char *str, const char *suffix);
char *mystrjoin(const char *str1, const char *str2, unsigned int delimiter);
#endif
void irlist_add_string(irlist_t *list, const char *str);
unsigned int verifyshell(irlist_t *list, const char *file);
unsigned int no_verifyshell(irlist_t *list, const char *file);
const char *save_nick(const char * nick);
unsigned int verifypass2(const char *masterpass, const char *testpass);
char *clean_quotes(char *str);
char *to_hostmask(const char *nick, const char *hostname);
const char *getfilename(const char *pathname);
ir_uint16 get_port(ir_sockaddr_union_t *listenaddr);
int strcmp_null(const char *s1, const char *s2);
unsigned int is_file_writeable(const char *f);
int open_append(const char *filename, const char *text);
int open_append_log(const char *filename, const char *text);
void mylog_write(int fd, const char *filename, const char *msg, size_t len);
void mylog_close(int fd, const char *filename);

char *grep_to_fnmatch(const char *grep);
char *hostmask_to_fnmatch(const char *str);
unsigned int verify_group_in_grouplist(const char *group, const char *grouplist);
char *sizestr(unsigned int spaces, off_t num);
unsigned int isprintable(unsigned int a);
size_t removenonprintable(char *str);
void removenonprintablefile(char *str);
char *getsendname(const char * const full);
char *caps(char *str);
unsigned int max_minutes_waits(time_t *endtime, unsigned int min);

#ifndef WITHOUT_MEMSAVE
#define get_argv(x,y,z) get_argv2(x,y,z,__FUNCTION__,__FILE__,__LINE__)
unsigned int get_argv2(char **result, const char *line, unsigned int howmany,
             const char *src_function, const char *src_file, unsigned int src_line);
#else /* WITHOUT_MEMSAVE */
unsigned int get_argv(char **result, const char *line, unsigned int howmany);
#endif /* WITHOUT_MEMSAVE */
#ifndef WITHOUT_MEMSAVE
#define getpart(x,y) getpart2(x,y,__FUNCTION__,__FILE__,__LINE__)
char *getpart2(const char *line, unsigned int howmany, const char *src_function, const char *src_file, unsigned int src_line);
#else
char *getpart(const char *line, unsigned int howmany);
#endif
char *getpart_eol(const char *line, unsigned int howmany);
unsigned int convert_spaces_to_match(char *str);
ir_uint64 timeval_to_ms(struct timeval *tv);
ir_uint64 get_time_in_ms(void);
char *user_getdatestr(char* str, time_t Tp, size_t len);
unsigned int get_random_uint( unsigned int max );
size_t
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
add_snprintf(char * str, size_t size, const char * format, ...);

void irlist_sort2(irlist_t *list, int (*cmpfunc)(const void *a, const void *b));

/* End of File */
