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

#define ADMIN_LEVEL_CONSOLE	6
#define ADMIN_LEVEL_FULL	5
#define ADMIN_LEVEL_AUTO	4
#define ADMIN_LEVEL_MAIN	3
#define ADMIN_LEVEL_HALF	2
#define ADMIN_LEVEL_INFO	1
#define ADMIN_LEVEL_PUBLIC	0

void admin_jobs(void);
int hide_pack(const xdcc *xd);
int check_lock(const char* lockstr, const char* pwd);

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
int check_for_file_remove(int n);
void look_for_file_remove(void);
void set_default_network_name(void);
int has_closed_servers(void);
int has_joined_channels(int all);
void reset_download_limits(void);
void startup_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
void check_duplicateip(transfer *const newtr);
int noticeresults(const char *nick, const char *match);
const char *validate_crc32(xdcc *xd, int quiet);
void autoadd_scan(const char *dir, const char *group);
void autoadd_all(void);
#ifdef USE_CURL
extern int fetch_started;
void fetch_multi_fdset(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);
void fetch_cancel(int num);
void fetch_perform(void);
void start_fetch_url(const userinput *const u);
void dinoex_dcl(const userinput *const u);
void dinoex_dcld(const userinput *const u);
#endif /* USE_CURL */

char* getpart_eol(const char *line, int howmany);
int get_network(const char *arg1);

void crc32_init(void);
void crc32_update(char *buf, unsigned long len);
void crc32_final(xdcc *xd);

int disk_full(const char *path);

void identify_needed(int force);
void identify_check(const char *line);

void changesec_dinoex(void);

int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line, int line_len);

void write_removed_xdcc(xdcc *xd);

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text);

/* End of File */
