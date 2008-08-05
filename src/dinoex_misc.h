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

int hide_pack(const xdcc *xd);
int check_lock(const char* lockstr, const char* pwd);
int number_of_pack(xdcc *pack);
int get_level(void);
int get_voice(void);
int check_level(char prefix);

void set_support_groups(void);
void send_help(const char *nick);

void stoplist(const char *nick);
int notifyqueued_nick(const char *nick);
void notifyqueued(void);
void startup_dinoex(void);
void config_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
char *grep_to_fnmatch(const char *grep);
int fnmatch_xdcc(const char *match, xdcc *xd);
int noticeresults(const char *nick, const char *pattern, const char *dest);
int run_new_trigger(const char *nick, const char *grouplist);

char* getpart_eol(const char *line, int howmany);

int disk_full(const char *path);

char *get_user_modes(void);
char *get_nickserv_pass(void);
void identify_needed(int force);
void identify_check(const char *line);

int parse_xdcc_list(const char *nick, char*msg3);
xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text, int restr);

int packnumtonum(const char *a);
int check_trigger(const char *line, int type, const char *nick, const char *hostmask, const char *msg);

void lost_nick(const char *nick);

int is_unsave_directory(const char *dir);

void logfile_add(const char *logfile, const char *line);

char *get_current_bandwidth(void);
char *transfer_limit_exceeded_msg(int ii);

int verify_uploadhost(const char *hostmask);
void clean_uploadhost(void);

char *user_getdatestr(char* str, time_t Tp, int len);

char *get_grouplist_access(const char *nick);
const char *get_listmsg_channel(const char *dest);
const char *get_grouplist_channel(const char *dest);

int verifyhost_group(const char *hostmask);
group_admin_t *verifypass_group(const char *hostmask, const char *passwd);

void free_channel_data(channel_t *ch);

void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
exit_iroffer(void);

/* End of File */
