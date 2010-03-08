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

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#ifndef MSG_DONTWAIT
# define MSG_DONTWAIT 0
#endif
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

typedef struct {
  char *u_host;
  long u_time;
} tupload_t;

typedef struct {
  char *q_host;
  char *q_nick;
  char *q_pack;
  int q_state;
  int q_net;
  long q_time;
} qupload_t;

typedef struct {
  char *a_group;
  char *a_pattern;
} autoadd_group_t;

typedef struct
{
  int pack;
  char *word;
  char *message;
} autoqueue_t;

typedef struct
{
  xdcc *pack;
  char *word;
} autotrigger_t;

typedef struct
{
  int delay;
  char *msg;
} channel_announce_t;

typedef struct {
  char *nick;
  char *msg;
} xlistqueue_t;

typedef struct {
  int ai_reset;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  size_t ai_addrlen;
  struct sockaddr ai_addr;
  char ai_buffer[16];
} res_addrinfo_t;

typedef struct {
  ir_uint32 crc;
  ir_uint32 crc_total;
} crc32build_t;

typedef struct {
  char *g_host;
  char *g_pass;
  char *g_groups;
  char *g_uploaddir;
  int g_level;
} group_admin_t;

typedef struct {
  int family;
  int netmask;
  ir_sockaddr_union_t remote;
} ir_cidr_t;

typedef struct {
  char *to_ip;
  ir_uint16 to_port;
  ir_uint16 dummy;
  int sp_fd[2];
  pid_t child_pid;
  int next;
} serv_resolv_t;

typedef enum {
  SERVERSTATUS_NEED_TO_CONNECT,
  SERVERSTATUS_RESOLVING,
  SERVERSTATUS_TRYING,
  SERVERSTATUS_CONNECTED,
  SERVERSTATUS_EXIT,
} serverstatus_e;

typedef enum {
  BOTSTATUS_LOGIN,
  BOTSTATUS_JOINED,
} botstatus_e;

typedef struct {
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

  serv_resolv_t serv_resolv;
  serverstatus_e serverstatus;
  botstatus_e botstatus;

  long connecttime;
  long lastservercontact;
  long lastnotify;
  long lastping;
  long lastslow;
  long lag;
  irlist_t serverq_fast;
  irlist_t serverq_normal;
  irlist_t serverq_slow;
  irlist_t serverq_channel;
  irlist_t xlistqueue;
  int serverbucket;
  int ircserver;
  int serverconnectbackoff;
#ifdef USE_OPENSSL
  SSL_CTX *ssl_ctx;
  SSL *ssl;
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  gnutls_session_t session;
  gnutls_certificate_credentials_t user_cred;
  gnutls_x509_crt_t nick_cert;
  gnutls_x509_privkey_t nick_key;
#endif /* USE_GNUTLS */
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
  int need_voice;
  int need_level;
  int noannounce;
  int slow_privmsg;
  int restrictsend;
  int restrictlist;
  how_e r_connectionmethod;
  userinput_method_e lag_method;

  long next_identify;
  long next_restrict;
  unsigned long ourip;
  unsigned long r_ourip;

  ir_sockaddr_union_t myip;
} gnetwork_t;

extern gnetwork_t *gnetwork;

/* EOF */
