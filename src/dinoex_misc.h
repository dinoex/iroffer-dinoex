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

int hide_pack(const xdcc *xd);
int check_lock(const char* lockstr, const char* pwd);

void send_help(const char *nick);

void stoplist(const char *nick);
void notifyqueued_nick(const char *nick);
void startup_dinoex(void);
void config_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
int noticeresults(const char *nick, const char *match);

char* getpart_eol(const char *line, int howmany);

int disk_full(const char *path);

char *get_user_modes(void);
char *get_nickserv_pass(void);
void identify_needed(int force);
void identify_check(const char *line);

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text);

int packnumtonum(const char *a);

void lost_nick(char *nick);

int is_unsave_directory(const char *dir);

void logfile_add(const char *logfile, const char *line);

char *get_current_bandwidth(void);

void exit_iroffer(void);

/* End of File */
