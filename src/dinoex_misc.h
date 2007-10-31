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
int has_closed_servers(void);
int has_joined_channels(int all);
void startup_dinoex(void);
void config_dinoex(void);
void shutdown_dinoex(void);
void rehash_dinoex(void);
void update_hour_dinoex(int hour, int minute);
void check_new_connection(transfer *const tr);
int noticeresults(const char *nick, const char *match);

char* getpart_eol(const char *line, int howmany);
int get_network(const char *arg1);

int disk_full(const char *path);

void identify_needed(int force);
void identify_check(const char *line);

xdcc *get_download_pack(const char* nick, const char* hostname, const char* hostmask, int pack, int *man, const char* text);

int packnumtonum(const char *a);

void lost_nick(char *nick);

void exit_iroffer(void);

/* End of File */
