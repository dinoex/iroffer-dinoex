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

/* portability */
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

typedef enum {
  QUPLOAD_IDLE,
  QUPLOAD_WAITING,
  QUPLOAD_TRYING,
  QUPLOAD_RUNNING,
} quploadstatus_e;

typedef struct {
  char *q_host;
  char *q_nick;
  char *q_pack;
  quploadstatus_e q_state;
  unsigned int q_net;
  unsigned long q_time;
} qupload_t;

typedef struct {
  userinput u;
  unsigned int net;
  unsigned int dummy;
  char *name;
  char *url;
  char *uploaddir;
} fetch_queue_t;

typedef struct {
  char *a_group;
  char *a_pattern;
} autoadd_group_t;

typedef struct
{
  char *word;
  char *message;
  unsigned int pack;
  unsigned int dummy;
} autoqueue_t;

typedef struct
{
  xdcc *pack;
  char *word;
} autotrigger_t;

typedef struct
{
  char *channel;
  char *msg;
  long c_time;
} channel_announce_t;

typedef struct
{
  char *pm_nick;
  char *pm_msg;
  time_t pm_next_time;
  unsigned int pm_net;
  unsigned int pm_time;
} periodicmsg_t;

typedef struct {
  char *nick;
  char *msg;
} xlistqueue_t;

#define DCC_OPTION_IPV4     1U
#define DCC_OPTION_IPV6     2U
#define DCC_OPTION_ACTIVE   4U
#define DCC_OPTION_PASSIVE  8U
#define DCC_OPTION_QUIET   16U

typedef struct {
  char *nick;
  time_t last_seen;
  unsigned int options;
  unsigned int idummy;
} dcc_options_t;

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
} crc32build_t;

typedef struct {
  char *g_host;
  char *g_pass;
  char *g_groups;
  char *g_uploaddir;
  unsigned int g_level;
  unsigned int dummy;
} group_admin_t;

typedef struct {
  int family;
  unsigned int netmask;
  ir_sockaddr_union_t remote;
} ir_cidr_t;

typedef struct {
  char *to_ip;
  ir_uint16 to_port;
  ir_uint16 dummy;
  int sp_fd[2];
  pid_t child_pid;
  int next;
  int dummy2;
} serv_resolv_t;

typedef enum {
  SERVERSTATUS_NEED_TO_CONNECT,
  SERVERSTATUS_RESOLVING,
  SERVERSTATUS_TRYING,
  SERVERSTATUS_SSL_HANDSHAKE,
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
  irlist_t server_join_raw;
  irlist_t server_connected_raw;
  irlist_t serverq_fast;
  irlist_t serverq_normal;
  irlist_t serverq_slow;
  irlist_t serverq_channel;
  irlist_t xlistqueue;
  irlist_t channel_join_raw;
  irlist_t channels;
  irlist_t r_channels;
  irlist_t servers;
  irlist_t dcc_options;

  server_t curserver;
  serv_resolv_t serv_resolv;

  time_t connecttime;
  time_t lastservercontact;
  time_t lastnotify;
  time_t lastping;
  time_t lastslow;
  time_t lastnormal;
  time_t lastfast;
  time_t lastsend;
  time_t next_identify;
  time_t next_restrict;

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

  char *curserveractualname;
  char *user_nick;
  char *caps_nick;
  char *name;
  char *nickserv_pass;
  char *auth_name;
  char *login_name;
  char *config_nick;
  char *r_config_nick;
  char *local_vhost;
  char *r_local_vhost;
  char *user_modes;
  char *natip;

  unsigned long lag;

  unsigned int net;
  unsigned int serverconnectbackoff;
  unsigned int nocon;
  unsigned int servertime;
  unsigned int recentsent;
  unsigned int nick_number;
  unsigned int inamnt[INAMNT_SIZE];
  unsigned int r_needtojump;
  unsigned int usenatip;
  unsigned int getip_net;
  unsigned int noannounce;
  unsigned int offline;
  unsigned int plaintext;
  unsigned int slow_privmsg;
  unsigned int need_voice;
  unsigned int restrictsend;
  unsigned int restrictlist;
  unsigned int respondtochannellist;
  unsigned int respondtochannelxdcc;
  unsigned int server_connect_timeout;
  unsigned int need_level;

  int server_send_max;
  int server_send_rate;
  int serverbucket;
  int ircserver;
  serverstatus_e serverstatus;
  botstatus_e botstatus;
  how_e r_connectionmethod;
  userinput_method_e lag_method;

  ir_uint32 ourip;
  ir_uint32 r_ourip;

  ir_sockaddr_union_t myip;
  int cdummy;
} gnetwork_t;

extern gnetwork_t *gnetwork;

/* EOF */
