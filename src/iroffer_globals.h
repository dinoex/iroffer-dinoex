/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the LICENSE file.
 * 
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.org/
 * 
 * @(#) iroffer_globals.h 1.121@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_globals.h|20051123201144|11503
 * 
 */

#if !defined _IROFFER_GLOBALS
#define _IROFFER_GLOBALS

#if !defined GEX
#define GEX extern
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
int net;
int recentsent;
int nick_number;
int inamnt[INAMNT_SIZE];
int r_needtojump;
int usenatip;
int getip_net;

long next_identify;
long next_restrict;
unsigned long ourip;
unsigned long r_ourip;

ir_sockaddr_union_t myip;
} gnetwork_t;

typedef struct
{
/* config */
char* configfile[MAXCONFIG];
char* osstring;
int hideos;
int lognotices;
int logmessages;
int timestampconsole;
char *local_vhost;
long startuptime;
int lowbdwth;
int logstats;
char *logfile;
time_t logrotate;
char *headline;
char *creditline;
int background;
int support_groups;

struct
{
  ir_uint64  limit;
  ir_uint64  used;
  time_t     ends;
} transferlimits[NUMBER_TRANSFERLIMITS];
int transferlimits_over;

char *pidfile;
int tcprangestart;
int tcprangelimit;
float transferminspeed, transfermaxspeed;
int overallmaxspeed, overallmaxspeeddayspeed;
unsigned int maxb;
int overallmaxspeeddaytimestart, overallmaxspeeddaytimeend;
char overallmaxspeeddaydays;
int maxtransfersperperson, maxqueueditemsperperson;
int maxidlequeuedperperson;
irlist_t downloadhost;
irlist_t nodownloadhost;
char *adminpass;
char *hadminpass;
irlist_t adminhost;
irlist_t hadminhost;
irlist_t filedir;
char *statefile;
char *xdcclistfile;
int xdcclistfileraw;
char *periodicmsg_nick, *periodicmsg_msg;
int  periodicmsg_time;
char *uploaddir;
off_t uploadmaxsize;
off_t uploadminspace;
irlist_t uploadhost;
char *config_nick;
char *user_realname, *user_modes, *loginname;
int restrictlist, restrictsend, restrictprivlist;
char *restrictprivlistmsg;
int punishslowusers;
int nomd5sum;
int getipfromserver;
int noduplicatefiles;
irlist_t adddir_exclude;
irlist_t autoqueue;
irlist_t autotrigger;
irlist_t unlimitedhost;
irlist_t geoipcountry;
irlist_t nogeoipcountry;
irlist_t geoipexcludenick;
irlist_t autoadd_dirs;
irlist_t packs_delayed;
irlist_t jobs_delayed;
irlist_t autocrc_exclude;
irlist_t https;
irlist_t http_bad_ip4;
irlist_t http_bad_ip6;
irlist_t http_vhost;
irlist_t telnet_vhost;
irlist_t weblist_info;
char *enable_nick;
char *owner_nick;
char *admin_job_file;
char *autoaddann;
char *autoadd_group;
char *send_statefile;
char *geoipdatabase;
char *respondtochannellistmsg;
char *xdccremovefile;
char *autoadd_sort;
char *http_admin;
char *http_dir;
char *group_seperator;
char *usenatip;
char *logfile_notices;
char *logfile_messages;
char *trashcan_dir;
char *xdccxmlfile;
long nomd5_start;
int need_voice;
int need_level;
int hide_list_info;
int xdcclist_grouponly;
int auto_default_group;
int auto_path_group;
int start_of_month;
int restrictprivlistmain;
int restrictprivlistfull;
int groupsincaps;
int ignoreuploadbandwidth;
int holdqueue;
int removelostfiles;
int ignoreduplicateip;
int hidelockedpacks;
int disablexdccinfo;
int atfind;
int waitafterjoin;
int noautorejoin;
int auto_crc_check;
int nocrc32;
int direct_file_access;
int autoaddann_short;
int spaces_in_filenames;
int autoadd_time;
int restrictsend_warning;
int restrictsend_timeout;
int send_statefile_minute;
int extend_status_line;
int max_uploads;
int max_upspeed;
int max_find;
int include_subdirs;
int restrictsend_delay;
int adminlevel;
int hadminlevel;
int monitor_files;
int xdcclist_by_privmsg;
int autoadd_delay;
int balanced_queue;
int http_port;
int passive_dcc;
int telnet_port;
int remove_dead_users;
int upnp_router;
int http_search;
int send_listfile;
char *nickserv_pass;
int notifytime;
int respondtochannelxdcc;
int respondtochannellist;
int quietmode;
int smallfilebypass;
irlist_t autoignore_exclude;
int autoignore_threshold;

/* rehash temp variables */
char *r_pidfile;
float r_transferminspeed, r_transfermaxspeed;

gnetwork_t networks[MAX_NETWORKS];
int networks_online;
int r_networks_online;

irlist_t msglog;

int adjustcore;

/* screen */
int attop, needsclear, termcols, termlines, nocolor, noscreen;
int curcol;
irlist_t console_history;
int console_history_offset;
char *console_input_line;

struct termios startup_tio;

int stdout_buffer_init;
ir_boutput_t stdout_buffer;

irlist_t dccchats;
int num_dccchats;

time_t curtime;
unsigned long long curtimems;

fd_set readset, writeset, execset;

float record;
float sentrecord;
unsigned long long totalsent;
long totaluptime;
int debug;
int exiting;
int crashing;

unsigned long xdccsent[XDCC_SENT_SIZE];
unsigned long xdccrecv[XDCC_SENT_SIZE];
unsigned long xdccsum[XDCC_SENT_SIZE];

int ignore;

int slotsmax;
int queuesize;
int idlequeuesize;


int noautosave;
long nonewcons;
long nolisting;
int needsrehash;
int needsshutdown;
int needsswitch;
int needsreap;
int delayedshutdown;
int cursendptr;
int next_tr_id;

off_t max_file_size;

unsigned int max_fds_from_rlimit;

int logfd;

time_t last_logrotate;

unsigned char *sendbuff;

context_t context_log[MAXCONTEXTS];
int context_cur_ptr;
int bracket;

irlist_t ignorelist;

irlist_t xdccs;
irlist_t mainqueue;
irlist_t idlequeue;
irlist_t trans;
irlist_t uploads;

meminfo_t *meminfo;
int meminfo_count;
int meminfo_depth;

#if !defined(NO_CHROOT)
char *chrootdir;
#endif
#if !defined(NO_SETUID)
char *runasuser;
#endif

irlist_t listen_ports;

struct
{
  xdcc *xpack;
  int file_fd;
  struct MD5Context md5sum;
} md5build;

struct
{
  ir_uint32 crc;
  ir_uint32 crc_total;
} crc32build;

enum
{
#ifdef HAVE_FREEBSD_SENDFILE
  TRANSFERMETHOD_FREEBSD_SENDFILE,
#endif
#ifdef HAVE_LINUX_SENDFILE
  TRANSFERMETHOD_LINUX_SENDFILE,
#endif
#ifdef HAVE_MMAP
  TRANSFERMETHOD_MMAP,
#endif
  TRANSFERMETHOD_READ_WRITE,
} transfermethod;

} gdata_t;


GEX gdata_t gdata;
GEX gnetwork_t *gnetwork;


#endif

/* End of File */
