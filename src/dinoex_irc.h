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

void update_natip (const char *var);
void update_server_welcome(char *line);
int bind_irc_vhost(int family, int clientsocket);
int open_listen(int family, ir_sockaddr_union_t *listenaddr, int *listen_socket, int port, int reuse, int search, char *vhost);
int irc_open_listen(ir_connection_t *con);
char *setup_dcc_local(ir_sockaddr_union_t *listenaddr);
void child_resolver(void);
int my_getnameinfo(char *buffer, size_t len, const struct sockaddr *sa, socklen_t salen);
int my_dcc_ip_port(char *buffer, size_t len, ir_sockaddr_union_t *sa, socklen_t salen);
int connectirc2(res_addrinfo_t *remote);

char *get_local_vhost(void);
char *get_config_nick(void);

int has_closed_servers(void);
int has_joined_channels(int all);

int irc_select(int highests);

/* End of File */
