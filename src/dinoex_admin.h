/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2014 Dirk Meyer
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

void a_parse_inputline(userinput * const u, const char *line);

int get_network(const char *arg1);
void a_xdl_full(const userinput * const u);
void a_xdl_group(const userinput * const u);
void a_xdl(const userinput * const u);
unsigned int reorder_new_groupdesc(const char *group, const char *desc);
unsigned int add_default_groupdesc(const char *group);

unsigned int invalid_channel(const userinput * const u, const char *arg);
unsigned int get_pack_nr(const userinput * const u, const char *arg);
int get_network_msg(const userinput * const u, const char *arg);
unsigned int group_restricted(const userinput * const u, xdcc *xd);

void a_remove_delayed(const userinput * const u);
void a_autoaddann(xdcc *xd, unsigned int pack);
void a_add_delayed(const userinput * const u);
void a_xdlock(const userinput * const u);
void a_xdtrigger(const userinput * const u);
void a_find(const userinput * const u);
void a_xds(const userinput * const u);
void a_qul(const userinput * const u);
void a_diskfree(const userinput * const u);
void a_listul(const userinput * const u);

void a_nomin(const userinput * const u);
void a_nomax(const userinput * const u);
void a_unlimited(const userinput * const u);
void a_maxspeed(const userinput * const u);
void a_qsend(const userinput * const u);
void a_iqsend(const userinput * const u);
void a_slotsmax(const userinput * const u);
void a_queuesize(const userinput * const u);
void a_requeue(const userinput * const u);
void a_reiqueue(const userinput * const u);

void a_remove(const userinput * const u);
void a_removedir(const userinput * const u);
void a_removegroup(const userinput * const u);
void a_removematch(const userinput * const u);
void a_removelost(const userinput * const u);
void a_renumber3(const userinput * const u);
void a_sort(const userinput * const u);
int a_open_file(char **file, int mode);
unsigned int a_access_fstat(const userinput * const u, int xfiledescriptor, char **file, struct stat *st);
void a_add(const userinput * const u);
void a_addgroup(const userinput * const u);
void a_addmatch(const userinput * const u);
void a_newgroup(const userinput * const u);
void a_chdesc(const userinput * const u);
void a_chnote(const userinput * const u);
void a_chtime(const userinput * const u);
void a_chmins(const userinput * const u);
void a_chmaxs(const userinput * const u);
void a_chlimit(const userinput * const u);
void a_chlimitinfo(const userinput * const u);
void a_chtrigger(const userinput * const u);
void a_deltrigger(const userinput * const u);
void a_chgets(const userinput * const u);
void a_chcolor(const userinput * const u);
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
void a_chfile(const userinput * const u);
void a_adddir(const userinput * const u);
void a_addnew(const userinput * const u);
void a_newdir(const userinput * const u);
void a_filemove(const userinput * const u);
void a_movefile(const userinput * const u);
void a_movegroupdir(const userinput * const u);
void a_filedel(const userinput * const u);
void a_fileremove(const userinput * const u);
void a_showdir(const userinput * const u);
void a_makedir(const userinput * const u);
#ifdef USE_CURL
void a_fetch(const userinput * const u);
void a_fetchcancel(const userinput * const u);
#endif /* USE_CURL */

void a_amsg(const userinput * const u);
channel_t *is_not_joined_channel(const userinput * const u, const char *name);
void a_msg(const userinput * const u);
void a_msgnet(const userinput * const u);
void a_mesg(const userinput * const u);
void a_mesq(const userinput * const u);
void a_acceptu(const userinput * const u);
void a_close(const userinput * const u);
void a_closeu(const userinput * const u);
void a_getl(const userinput * const u);
void a_get(const userinput * const u);
void a_rmq(const userinput * const u);
void a_rmiq(const userinput * const u);
void a_rmallq(const userinput * const u);
void a_rmul(const userinput * const u);
void a_raw(const userinput * const u);
void a_rawnet(const userinput * const u);
void a_lag(const userinput * const u);
void a_ignore(const userinput * const u);
void a_bannhost(const userinput * const u);
void a_bannnick(const userinput * const u);
void a_hop(const userinput * const u);
void a_nochannel(const userinput * const u);
void a_join(const userinput * const u);
void a_part(const userinput * const u);
void a_servqc(const userinput * const u);
void a_nosave(const userinput * const u);
void a_nosend(const userinput * const u);
void a_nolist(const userinput * const u);
void a_nomd5(const userinput * const u);
void a_clearrecords(const userinput * const u);
void a_cleargets(const userinput * const u);
void a_closec(const userinput * const u);
void a_config(const userinput * const u);
void a_print(const userinput * const u);
void a_identify(const userinput * const u);
void a_holdqueue(const userinput * const u);
void a_offline(const userinput * const u);
void a_online(const userinput * const u);
#ifdef USE_RUBY
void a_ruby(const userinput * const u);
#endif /* USE_RUBY */
void a_dump(const userinput * const u);
void a_version(const userinput * const u);
void a_backgroud(const userinput * const u);
void a_autoadd(const userinput * const u);
void a_autocancel(const userinput * const u);
void a_autogroup(const userinput * const u);
void a_noautoadd(const userinput * const u);

void a_send(const userinput * const u);
void a_queue(const userinput * const u);
void a_iqueue(const userinput * const u);
void a_announce(const userinput * const u);
void a_newann(const userinput * const u);
void a_mannounce(const userinput * const u);
void a_cannounce(const userinput * const u);
void a_sannounce(const userinput * const u);
void a_addann(const userinput * const u);
void a_noannounce(const userinput * const u);

void a_restart(const userinput * const u);

/* End of File */
