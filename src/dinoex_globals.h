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

#ifdef USE_SSL
#include <openssl/ssl.h>
#endif

typedef struct
{
connectionmethod_t connectionmethod;
irlist_t proxyinfo;

/* raw on join */
irlist_t server_join_raw;
irlist_t server_connected_raw;
irlist_t channel_join_raw;

/* channel */
irlist_t channels;
irlist_t r_channels;

/* server */
irlist_t servers;
server_t curserver;
char *curserveractualname;
int nocon;
int servertime;

struct
{
  char *to_ip;
  unsigned short to_port;
  int sp_fd[2];
  pid_t child_pid;
  int next;
} serv_resolv;

enum
{
  SERVERSTATUS_NEED_TO_CONNECT,
  SERVERSTATUS_RESOLVING,
  SERVERSTATUS_TRYING,
  SERVERSTATUS_CONNECTED,
  SERVERSTATUS_EXIT,
} serverstatus;
long lastservercontact;
long lastnotify;
irlist_t serverq_fast;
irlist_t serverq_normal;
irlist_t serverq_slow;
irlist_t serverq_channel;
irlist_t xlistqueue;
int serverbucket;
int ircserver;
int serverconnectbackoff;
#ifdef USE_SSL
SSL_CTX *ssl_ctx;
SSL *ssl;
#endif
prefix_t prefixes[MAX_PREFIX];
char chanmodes[MAX_CHANMODES];
char server_input_line[INPUT_BUFFER_LENGTH];

char *user_nick;
char *caps_nick;
char *name;
char *nickserv_pass;
char *config_nick;
char *r_config_nick;
char *local_vhost;
char *r_local_vhost;
char *user_modes;
char *natip;
int net;
int recentsent;
int nick_number;
int inamnt[INAMNT_SIZE];
int r_needtojump;
int usenatip;
int getip_net;
how_e r_connectionmethod;

long next_identify;
long next_restrict;
unsigned long ourip;
unsigned long r_ourip;

ir_sockaddr_union_t myip;
} gnetwork_t;

GEX gnetwork_t *gnetwork;

/* EOF */
