/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2005 Dirk Meyer
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

int verifyshell(irlist_t *list, const char *file);
void admin_jobs(const char *job);
int check_lock(const char* lockstr, const char* pwd);

int reorder_new_groupdesc(const char *group, const char *desc);
int reorder_groupdesc(const char *group);
int add_default_groupdesc(const char *group);
void strtextcpy(char *d, const char *s);

void update_natip (const char *var);
void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_chan(const channel_t *ch, const char *format, ...);
void vprivmsg_chan(const channel_t *ch, const char *format, va_list ap);
void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
writeserver_channel (int delay, const char *chan, const char *format, ... );
void vwriteserver_channel(int delay, const char *chan, const char *format, va_list ap);
void sendannounce(void);
void stoplist(const char *nick);
void notifyqueued_nick(const char *nick);
void look_for_file_remove(void);
int has_closed_servers(void);
int has_joined_channels(int all);
void reset_download_limits(void);
void startup_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void check_new_connection(transfer *const tr);
void check_duplicateip(transfer *const newtr);
int noticeresults(const char *nick, const char *match);
void check_duplicateip(transfer *const newtr);
const char *validate_crc32(xdcc *xd, int quiet);
void autoadd_scan(const char *dir, const char *group);
#ifdef USE_CURL
void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);
void fetch_perform(void);
void start_fetch_url(const userinput *const u);
#endif /* USE_CURL */

char* getpart_eol(const char *line, int howmany);
#ifdef MULTINET
int get_network(const char *arg1);
#endif /* MULTINET */

void crc32_init(void);
void crc32_update(char *buf, unsigned long len);
void crc32_final(xdcc *xd);

/* End of File */
