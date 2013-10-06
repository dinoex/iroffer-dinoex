/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2013 Dirk Meyer
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

#ifndef WITHOUT_BLOWFISH
void init_fish64decode( void );
char *test_fish_message(const char *line, const char *channel, const char *str, const char *data);
#endif /* WITHOUT_BLOWFISH */

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vprivmsg_chan(channel_t * const ch, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_chan(channel_t * const ch, const char *format, ...);

void writeserver_privmsg(writeserver_type_e delay, const char *nick, const char *message, size_t len);
void writeserver_notice(writeserver_type_e delay, const char *nick, const char *message, size_t len);
void clean_send_buffers(void);
void sendannounce(void);

void a_fillwith_msg2(userinput * const u, const char *nick, const char *line);
void admin_jobs(void);
void look_for_file_remove(void);
void reset_download_limits(void);
const char *validate_crc32(xdcc *xd, int quiet);
void crc32_update(char *buf, size_t len);
void autoadd_all(void);
void run_delayed_jobs(void);
const char *find_groupdesc(const char *group);
void write_removed_xdcc(xdcc *xd);
unsigned int import_xdccfile(void);
void autotrigger_add(xdcc *xd);
void autotrigger_rebuild(void);
void start_md5_hash(xdcc *xd, unsigned int packnum);
void cancel_md5_hash(xdcc *xd, const char *msg);
void complete_md5_hash(void);
void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch);
int save_unlink(const char *path);
void rename_with_backup(const char *file, const char *backup, const char *tmp, const char *msg);
void write_files(void);
unsigned int max_uploads_reached(void);
void start_qupload(void);
unsigned int close_qupload(unsigned int net, const char *nick);
void lag_message(void);
void send_periodicmsg(void);

void a_rehash_prepare(void);
void a_quit_network(void);
void a_rehash_needtojump(const userinput *u);
void a_rehash_channels(void);
void a_rehash_jump(void);
void a_rehash_cleanup(const userinput *u);
void a_read_config_files(const userinput *u);

unsigned int compare_logrotate_time(void);
char *new_logfilename(const char *logfile);
void expire_logfiles(const char *logfile);
int rotatelog(const char *logfile);
void delayed_announce(void);
void backup_statefile(void);
void expire_options(void);

ir_uint32 get_zip_crc32_pack(xdcc *xd);

/* End of File */
