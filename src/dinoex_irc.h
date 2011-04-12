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

int my_getnameinfo(char *buffer, size_t len, const struct sockaddr *sa);
void update_natip (const char *var);
unsigned int bind_irc_vhost(int family, int clientsocket);
unsigned int open_listen(int family, ir_sockaddr_union_t *listenaddr, int *listen_socket, unsigned int port, unsigned int reuse, unsigned int search, const char *vhost);
unsigned int irc_open_listen(ir_connection_t *con);
void ir_setsockopt(int clientsocket);
char *setup_dcc_local(ir_sockaddr_union_t *listenaddr);
void child_resolver(int family);
void irc_resolved(void);
const char *my_dcc_ip_show(char *buffer, size_t len, ir_sockaddr_union_t *sa, unsigned int net);

char *get_local_vhost(void);
char *get_config_nick(void);
char *get_user_nick(void);

unsigned int has_closed_servers(void);

igninfo *get_ignore(const char *hostmask);
unsigned int check_ignore(const char *nick, const char *hostmask);

int irc_select(int highests);
void irc_perform(int changesec);

void identify_needed(unsigned int force);

/* End of File */
