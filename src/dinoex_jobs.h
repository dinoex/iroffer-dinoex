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
void a_fillwith_plist(userinput *manplist, const char *name, channel_t *ch);

/* End of File */
