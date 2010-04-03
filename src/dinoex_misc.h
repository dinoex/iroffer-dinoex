/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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

extern xdcc xdcc_statefile;
extern xdcc xdcc_listfile;

int hide_pack(const xdcc *xd);
int number_of_pack(xdcc *pack);
int check_level(char prefix);

void set_support_groups(void);
unsigned long get_next_transfer_time(void);
void add_new_transfer_time(xdcc *xd);
void guess_end_transfers(void);
void guess_end_cleanup(void);
int notifyqueued_nick(const char *nick);
void notifyqueued(void);
void startup_dinoex(void);
void config_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);

int init_xdcc_file(xdcc *xd, char *file) ;
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
char *grep_to_fnmatch(const char *grep);
int fnmatch_xdcc(const char *match, xdcc *xd);

int disk_full(const char *path);

char *get_user_modes(void);
char *get_nickserv_pass(void);
int get_restrictsend(void);

xdcc *get_xdcc_pack(int pack);
int access_need_level(const char *nick, const char *text);

int is_unsave_directory(const char *dir);

void logfile_add(const char *logfile, const char *line);

char *get_current_bandwidth(void);
char *transfer_limit_exceeded_msg(int ii);

char *user_getdatestr(char* str, time_t Tp, int len);

char *get_grouplist_access(const char *nick);
const char *get_grouplist_channel(const char *dest);

int verify_cidr(irlist_t *list, const ir_sockaddr_union_t *remote);

void add_newest_xdcc(irlist_t *list, const char *grouplist);

int select_starting_transfer(int max);

void free_channel_data(channel_t *ch);

void auto_rehash(void);

/* End of File */
