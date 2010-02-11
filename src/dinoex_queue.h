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

void queue_update_nick(irlist_t *list, const char *oldnick, const char *newnick);
void queue_reverify_restrictsend(irlist_t *list);
void queue_punishslowusers(irlist_t *list, int network, const char *nick);
int queue_xdcc_remove(irlist_t *list, int network, const char *nick, int number);
void queue_pack_limit(irlist_t *list, xdcc *xd);
void queue_pack_remove(irlist_t *list, xdcc *xd);
void queue_all_remove(irlist_t *list, const char *message);
int queue_count_host(irlist_t *list, int *inq, int man, const char* nick, const char *hostname, xdcc *xd);
const char *addtoidlequeue(char *tempstr, const char* nick, const char* hostname, xdcc *xd, int pack, int inq);
const char *addtomainqueue(char *tempstr, const char *nick, const char *hostname, int pack);
void send_from_queue(int type, int pos, char *lastnick);
void check_idle_queue(void);
void start_main_queue(void);
void start_sends(void);

/* End of File */
