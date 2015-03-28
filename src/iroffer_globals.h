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

#include "dinoex_globals.h"

typedef struct
{
/* config */
struct
{
  ir_int64   limit;
  ir_int64   used;
  time_t     ends;
} transferlimits[NUMBER_TRANSFERLIMITS];
gnetwork_t networks[MAX_NETWORKS];

off_t uploadmaxsize;
off_t uploadminspace;
off_t disk_quota;
irlist_t downloadhost;
irlist_t nodownloadhost;
irlist_t http_allow;
irlist_t http_deny;
irlist_t telnet_allow;
irlist_t telnet_deny;
irlist_t xdcc_allow;
irlist_t xdcc_deny;
irlist_t adminhost;
irlist_t hadminhost;
irlist_t filedir;
irlist_t uploadhost;
irlist_t tuploadhost;
irlist_t quploadhost;
irlist_t fetch_queue;
irlist_t autoignore_exclude;
irlist_t adddir_exclude;
irlist_t adddir_match;
irlist_t autoqueue;
irlist_t autotrigger;
irlist_t unlimitedhost;
irlist_t geoipcountry;
irlist_t nogeoipcountry;
irlist_t geoipexcludenick;
irlist_t geoipexcludegroup;
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
irlist_t mime_type;
irlist_t autoadd_group_match;
irlist_t log_exclude_host;
irlist_t log_exclude_text;
irlist_t fish_exclude_nick;
irlist_t group_admin;
irlist_t periodicmsg;
irlist_t autoaddann_mask;
irlist_t md5sum_exclude;
irlist_t headline;
time_t configtime[MAXCONFIG];
char *configfile[MAXCONFIG];
char *osstring;
char *pidfile;
char *local_vhost;
char *logfile;
char *creditline;
char *adminpass;
char *hadminpass;
char *statefile;
char *xdcclistfile;
char *config_nick;
char *user_realname, *user_modes, *loginname;
char *restrictprivlistmsg;
char *uploaddir;
char *nickserv_pass;
char *enable_nick;
char *owner_nick;
char *admin_job_file;
char *admin_job_done_file;
char *autoaddann;
char *autoadd_group;
char *send_statefile;
char *geoipdatabase;
char *geoip6database;
char *respondtochannellistmsg;
char *xdccremovefile;
char *autoadd_sort;
char *http_admin;
char *http_dir;
char *group_seperator;
char *usenatip;
char *logfile_notices;
char *logfile_messages;
char *logfile_httpd;
char *trashcan_dir;
char *xdccxmlfile;
char *http_date;
char *announce_seperator;
char *nosendmsg;
char *privmsg_fish;
char *ruby_script;
char *charset;
char *http_admin_dir;
char *http_index;
char *download_completed_msg;
char *http_access_log;
char *autoadd_color;
char *http_forbidden;
char *announce_suffix_color;
float transferminspeed, transfermaxspeed;
unsigned char overallmaxspeeddaydays;
unsigned char cdummy;
unsigned short sdummy;
/* int */
unsigned int logrotate;
unsigned int lowbdwth;
unsigned int tcprangestart;
unsigned int tcprangelimit;
unsigned int overallmaxspeed, overallmaxspeeddayspeed;
unsigned int maxtransfersperperson, maxqueueditemsperperson;
unsigned int maxidlequeuedperperson;
unsigned int debug;
unsigned int slotsmax;
unsigned int queuesize;
unsigned int idlequeuesize;
unsigned int punishslowusers;
unsigned int need_level;
unsigned int start_of_month;
unsigned int atfind;
unsigned int waitafterjoin;
unsigned int autoadd_time;
unsigned int restrictsend_timeout;
unsigned int send_statefile_minute;
unsigned int max_uploads;
unsigned int max_upspeed;
unsigned int max_find;
unsigned int restrictsend_delay;
unsigned int adminlevel;
unsigned int hadminlevel;
unsigned int monitor_files;
unsigned int autoadd_delay;
unsigned int http_port;
unsigned int telnet_port;
unsigned int remove_dead_users;
unsigned int send_listfile;
unsigned int fileremove_max_packs;
unsigned int status_time_dcc_chat;
unsigned int notifytime;
unsigned int smallfilebypass;
unsigned int autoignore_threshold;
unsigned int reconnect_delay;
unsigned int new_trigger;
unsigned int adddir_min_size;
unsigned int ignore_duplicate_ip;
unsigned int expire_logfiles;
unsigned int tcp_buffer_size;
unsigned int flood_protection_rate;
unsigned int autoignore_rate;
unsigned int reminder_send_retry;
/* bool */
unsigned int hideos;
unsigned int lognotices;
unsigned int logmessages;
unsigned int timestampconsole;
unsigned int logstats;
unsigned int background;
unsigned int xdcclistfileraw;
unsigned int restrictlist, restrictsend, restrictprivlist;
unsigned int nomd5sum;
unsigned int getipfromserver;
unsigned int noduplicatefiles;
unsigned int dos_text_files;
unsigned int no_duplicate_filenames;
unsigned int show_list_all;
unsigned int getipfromupnp;
unsigned int hide_list_info;
unsigned int xdcclist_grouponly;
unsigned int auto_default_group;
unsigned int auto_path_group;
unsigned int restrictprivlistmain;
unsigned int restrictprivlistfull;
unsigned int groupsincaps;
unsigned int ignoreuploadbandwidth;
unsigned int holdqueue;
unsigned int removelostfiles;
unsigned int hidelockedpacks;
unsigned int disablexdccinfo;
unsigned int need_voice;
unsigned int noautorejoin;
unsigned int auto_crc_check;
unsigned int nocrc32;
unsigned int direct_file_access;
unsigned int autoaddann_short;
unsigned int spaces_in_filenames;
unsigned int restrictsend_warning;
unsigned int extend_status_line;
unsigned int include_subdirs;
unsigned int xdcclist_by_privmsg;
unsigned int balanced_queue;
unsigned int passive_dcc;
unsigned int upnp_router;
unsigned int http_search;
unsigned int old_statefile;
unsigned int direct_config_access;
unsigned int show_date_added;
unsigned int fish_only;
unsigned int privmsg_encrypt;
unsigned int verbose_crc32;
unsigned int mirc_dcc64;
unsigned int no_minspeed_on_free;
unsigned int no_status_chat;
unsigned int no_status_log;
unsigned int no_auto_rehash;
unsigned int send_batch;
unsigned int http_geoip;
unsigned int no_find_trigger;
unsigned int hide_list_stop;
unsigned int passive_dcc_chat;
unsigned int respondtochannelxdcc;
unsigned int respondtochannellist;
unsigned int quietmode;
unsigned int no_natural_sort;
unsigned int requeue_sends;
unsigned int show_group_of_pack;
unsigned int dump_all;
unsigned int tcp_nodelay;
unsigned int announce_size;
unsigned int subdirs_delayed;

context_t context_log[MAXCONTEXTS];
ir_boutput_t stdout_buffer;
off_t max_file_size;
time_t startuptime;
time_t nomd5_start;
time_t noannounce_start;
time_t noautoadd;
time_t noautosave;
time_t nonewcons;
time_t nolisting;
time_t last_logrotate;
time_t last_update;
time_t curtime;
long totaluptime;
ir_uint64 curtimems;
ir_uint64 selecttimems;
ir_int64 totalsent;
unsigned long xdccsent[XDCC_SENT_SIZE];
unsigned long xdccrecv[XDCC_SENT_SIZE];
unsigned long xdccsum[XDCC_SENT_SIZE];

irlist_t msglog;
irlist_t dccchats;
irlist_t ignorelist;
irlist_t xdccs;
irlist_t mainqueue;
irlist_t idlequeue;
irlist_t trans;
irlist_t uploads;
irlist_t console_history;
irlist_t listen_ports;

fd_set readset, writeset, execset;

struct
{
  xdcc *xpack;
  off_t bytes;
  struct MD5Context md5sum;
  int file_fd;
  int dummy;
} md5build;

crc32build_t crc32build;

meminfo_t *meminfo;
char *r_pidfile;
char *console_input_line;
unsigned char *sendbuff;
char *const *argv;
#if !defined(NO_CHROOT)
const char *chrootdir;
#endif
#if !defined(NO_SETUID)
const char *runasuser;
#endif
const char *workdir;
const char *import;

float r_transferminspeed, r_transfermaxspeed;

/* rehash temp variables */
unsigned int networks_online;
unsigned int r_networks_online;
unsigned int r_xdcclist_grouponly;
unsigned int support_groups;
unsigned int transferlimits_over;
unsigned int maxb;
unsigned int adjustcore;
unsigned int overallmaxspeeddaytimestart, overallmaxspeeddaytimeend;
unsigned int idummy2;
int ignore;

/* screen */
unsigned int attop, needsclear, termcols, termlines, nocolor, noscreen;
unsigned int curcol;
unsigned int console_history_offset;
unsigned int num_dccchats;
unsigned int stdout_buffer_init;

struct termios startup_tio;

float record;
float sentrecord;
unsigned int exiting;
unsigned int crashing;
unsigned int needsrehash;
unsigned int needsshutdown;
unsigned int needsswitch;
unsigned int needsreap;
unsigned int delayedshutdown;
unsigned int cursendptr;
unsigned int next_tr_id;
unsigned int context_cur_ptr;
unsigned int maxtrans;
unsigned int idummy3;

int max_fds_from_rlimit;
int logfd;

unsigned int meminfo_count;
unsigned int meminfo_depth;

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

unsigned int needrestart;
} gdata_t;


extern gdata_t gdata;


#endif

/* End of File */
