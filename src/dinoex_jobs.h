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

#define ADMIN_LEVEL_CONSOLE	6
#define ADMIN_LEVEL_FULL	5
#define ADMIN_LEVEL_AUTO	4
#define ADMIN_LEVEL_MAIN	3
#define ADMIN_LEVEL_HALF	2
#define ADMIN_LEVEL_INFO	1
#define ADMIN_LEVEL_PUBLIC	0

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_chan(const channel_t *ch, const char *format, ...);
void vprivmsg_chan(int delay, const char *name, const char *fish, const char *format, va_list ap);
void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
writeserver_channel (int delay, const char *format, ... );
void vwriteserver_channel(int delay, const char *format, va_list ap);
void cleanannounce(void);
void sendannounce(void);

void admin_jobs(void);
int check_for_file_remove(int n);
void look_for_file_remove(void);
void reset_download_limits(void);
const char *validate_crc32(xdcc *xd, int quiet);
void crc32_init(void);
void crc32_update(char *buf, unsigned long len);
void crc32_final(xdcc *xd);
void autoadd_scan(const char *dir, const char *group);
void autoadd_all(void);
void run_delayed_jobs(void);
int admin_message(const char *nick, const char *hostmask, const char *passwd, char *line, int line_len);
void write_removed_xdcc(xdcc *xd);
void autotrigger_add(xdcc *xd);
void autotrigger_rebuild(void);
void start_md5_hash(xdcc *xd, int packnum);
void cancel_md5_hash(xdcc *xd, const char *msg);
void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch);
int save_unlink(const char *path);
void rename_with_backup(const char *file, const char *backup, const char *tmp, const char *msg);
void write_files(void);
void start_qupload(void);
int close_qupload(int net, const char *nick);

void a_rehash_prepare(void);
void a_rehash_needtojump(const userinput *u);
void a_rehash_jump(void);
void a_rehash_cleanup(const userinput *u);

/* End of File */
