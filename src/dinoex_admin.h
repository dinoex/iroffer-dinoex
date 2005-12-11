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
int has_joined_channels(int all);
void reset_download_limits(void);
void check_duplicateip(transfer *const newtr);
int noticeresults(const char *nick, const char *match);

char* getpart_eol(const char *line, int howmany);

/* End of File */
