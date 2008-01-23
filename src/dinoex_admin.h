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

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
a_respond(const userinput * const u, const char *format, ...);

int hide_locked(const userinput * const u, const xdcc *xd);
int get_network(const char *arg1);
int a_xdl_space(void);
int a_xdl_left(void);
int reorder_new_groupdesc(const char *group, const char *desc);
int reorder_groupdesc(const char *group);
int add_default_groupdesc(const char *group);
void strtextcpy(char *d, const char *s);
void strpathcpy(char *d, const char *s);

int invalid_group(const userinput * const u, const char *arg);
int invalid_dir(const userinput * const u, const char *arg);
int is_upload_file(const userinput * const u, const char *arg);
int invalid_file(const userinput * const u, const char *arg);
int invalid_pwd(const userinput * const u, const char *arg);
int invalid_nick(const userinput * const u, const char *arg);
int invalid_message(const userinput * const u, const char *arg);
int invalid_announce(const userinput * const u, const char *arg);
int invalid_command(const userinput * const u, const char *arg);
int invalid_channel(const userinput * const u, const char *arg);
int invalid_maxspeed(const userinput * const u, const char *arg);
int invalid_pack(const userinput * const u, int num);
int get_network_msg(const userinput * const u, const char *arg);
int disabled_config(const userinput * const u);

int queue_host_remove(const userinput * const u, irlist_t *list, regex_t *regexp);
int queue_nick_remove(const userinput * const u, irlist_t *list, int network, const char *nick);

void a_cancel_transfers(xdcc *xd, const char *msg);
void a_remove_pack(const userinput * const u, xdcc *xd, int num);
void a_remove_delayed(const userinput * const u);
void a_add_delayed(const userinput * const u);
void a_xdlock(const userinput * const u);
void a_xdtrigger(const userinput * const u);
void a_find(const userinput * const u);
void a_listul(const userinput * const u);

void a_unlimited(const userinput * const u);
void a_maxspeed(const userinput * const u);
void a_slotsmax(const userinput * const u);
void a_queuesize(const userinput * const u);
void a_requeue(const userinput * const u);
void a_reiqueue(const userinput * const u);

void a_removedir_sub(const userinput * const u, const char *thedir, DIR *d);
void a_remove(const userinput * const u);
void a_removegroup(const userinput * const u);
void a_renumber1(const userinput * const u, int oldp, int newp);
void a_renumber3(const userinput * const u);
void a_sort(const userinput * const u);
int a_open_file(char **file, int mode);
int a_access_fstat(const userinput * const u, int xfiledescriptor, char **file, struct stat *st);
xdcc *a_add2(const userinput * const u);
void a_add(const userinput * const u);
void a_adddir_sub(const userinput * const u, const char *thedir, DIR *d, int new, const char *setgroup, const char *match);
DIR *a_open_dir(char **dir);
void a_addgroup(const userinput * const u);
void a_addmatch(const userinput * const u);
void a_newgroup(const userinput * const u);
void a_chlimit(const userinput * const u);
void a_chlimitinfo(const userinput * const u);
void a_chtrigger(const userinput * const u);
void a_lock(const userinput * const u);
void a_unlock(const userinput * const u);
void a_lockgroup(const userinput * const u);
void a_unlockgroup(const userinput * const u);
void a_relock(const userinput * const u);
void a_groupdesc(const userinput * const u);
void a_group(const userinput * const u);
void a_movegroup(const userinput * const u);
void a_regroup(const userinput * const u);
void a_md5(const userinput * const u);
void a_crc(const userinput * const u);
void a_newdir(const userinput * const u);
void a_filemove(const userinput * const u);
void a_movefile(const userinput * const u);
void a_movegroupdir(const userinput * const u);
void a_filedel(const userinput * const u);
void a_fileremove(const userinput * const u);
void a_showdir(const userinput * const u);
#ifdef USE_CURL
void a_fetch(const userinput * const u);
void a_fetchcancel(const userinput * const u);
#endif /* USE_CURL */

void a_amsg(const userinput * const u);
channel_t *is_not_joined_channel(const userinput * const u, const char *name);
void a_msg_nick_or_chan(const userinput * const u, const char *name, const char *msg);
void a_msg(const userinput * const u);
void a_msgnet(const userinput * const u);
void a_rmiq(const userinput * const u);
void a_rawnet(const userinput * const u);
void a_bannnick(const userinput * const u);
void a_bann_hostmask(const userinput * const u, const char *arg);
void a_hop(const userinput * const u);
void a_nochannel(const userinput * const u);
void a_join(const userinput * const u);
void a_part(const userinput * const u);
void a_servqc(const userinput * const u);
void a_nomd5(const userinput * const u);
void a_cleargets(const userinput * const u);
void a_identify(const userinput * const u);
void a_holdqueue(const userinput * const u);
void a_dump(const userinput * const u);
void a_backgroud(const userinput * const u);
void a_autoadd(const userinput * const u);
void a_autogroup(const userinput * const u);

void a_queue(const userinput * const u);
void a_iqueue(const userinput * const u);
void a_announce(const userinput * const u);
void a_cannounce(const userinput * const u);
void a_sannounce(const userinput * const u);
void a_addann(const userinput * const u);

/* End of File */
