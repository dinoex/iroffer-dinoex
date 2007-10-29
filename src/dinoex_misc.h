/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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
void cleanannounce(void);
void sendannounce(void);
void stoplist(const char *nick);
void notifyqueued_nick(const char *nick);
int check_for_file_remove(int n);
void look_for_file_remove(void);
int has_closed_servers(void);
int has_joined_channels(int all);
void reset_download_limits(void);
void startup_dinoex(void);
void config_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
int noticeresults(const char *nick, const char *match);
const char *validate_crc32(xdcc *xd, int quiet);
void autoadd_scan(const char *dir, const char *group);
void autoadd_all(void);

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

void autotrigger_add(xdcc *xd);
void autotrigger_rebuild(void);

void start_md5_hash(xdcc *xd, int packnum);
void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch);

int packnumtonum(const char *a);

void lost_nick(char *nick);

void exit_iroffer(void);

/* End of File */
