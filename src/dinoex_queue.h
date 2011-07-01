/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2011 Dirk Meyer
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
void queue_punish_abuse(const char *msg, unsigned int network, const char *nick);
unsigned int queue_xdcc_remove(irlist_t *list, unsigned int network, const char *nick, unsigned int number);
void queue_pack_limit(irlist_t *list, xdcc *xd);
void queue_pack_remove(irlist_t *list, xdcc *xd);
void queue_all_remove(irlist_t *list, const char *message);
unsigned int queue_count_host(irlist_t *list, unsigned int *inq, unsigned int man, const char* nick, const char *hostname, xdcc *xd);
unsigned int addtoidlequeue(const char **msg, char *tempstr, const char* nick, const char* hostname, xdcc *xd, unsigned int pack, unsigned int inq);
unsigned int addtomainqueue(const char **msg, char *tempstr, const char *nick, const char *hostname, unsigned int pack);
void send_from_queue(unsigned int type, unsigned int pos, char *lastnick);
void start_one_send(void);
void start_sends(void);
void check_idle_queue(unsigned int pos);
void start_main_queue(void);
ir_pqueue *requeue(transfer *tr, ir_pqueue *old);

/* End of File */
