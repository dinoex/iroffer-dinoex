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
 * @(#) iroffer_headers.h 1.142@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_headers.h|20050313225819|56401
 * 
 */

#if !defined _IROFFER_HEADERS
#define _IROFFER_HEADERS

/*------------ includes ------------- */

#ifndef _OS_HPUX
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <grp.h>
#include <assert.h>
#include <termios.h>

#ifdef HAS_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAS_SYS_SENDFILE_H
#include <sys/sendfile.h>
#endif

#ifdef HAS_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAS_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAS_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAS_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAS_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAS_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#if defined(_OS_SunOS) \
 || defined(_OS_HPUX) \
 || defined(_OS_CYGWIN)
#include <sys/termios.h>
#endif

#if defined(_OS_SunOS) \
 || defined(_OS_HPUX) \
 || defined(_OS_IRIX) \
 || defined(_OS_IRIX64)
#include <strings.h>
#endif

#if defined(_OS_SunOS)
#ifndef FNM_CASEFOLD
#define	FNM_CASEFOLD 	FNM_IGNORECASE /* compatibility for SunOS 5.5 */
#endif
#endif

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#define USE_SSL
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#define USE_SSL
#endif /* USE_GNUTLS */

#include "plumb_md5.h"

/*------------ structures ------------- */

typedef struct irlist_item_t2
{
  struct irlist_item_t2 *next;
} irlist_item_t;

typedef struct
{
  irlist_item_t *head;
  irlist_item_t *tail;
  unsigned int size;
  unsigned int dummy;
} irlist_t;

typedef struct
{
  unsigned int begin;
  unsigned int end;
  char buffer[IR_BOUTPUT_SEGMENT_SIZE];
} ir_boutput_segment_t;

typedef struct
{
  int fd;
  unsigned int flags;
  struct MD5Context *md5sum;
  irlist_t segments;
  unsigned int count_written;
  unsigned int count_flushed;
  unsigned int count_dropped;
  unsigned int dummy;
} ir_boutput_t;

#ifdef HAVE_MMAP
typedef struct
{
  off_t mmap_offset; /* leave first */
  unsigned char *mmap_ptr;
  size_t mmap_size;
  unsigned int ref_count;
  unsigned int dummy;
} mmap_info_t;
#endif

typedef struct {
   char *file, *desc, *note;
   char *group;
   char *group_desc;
   char *lock;
   char *dlimit_desc;
   char *trigger;
   unsigned int gets;
   unsigned int color;
   unsigned int dlimit_max;
   unsigned int dlimit_used;
   float minspeed,
         maxspeed;
   off_t st_size;
   dev_t st_dev;
   ino_t st_ino;
   time_t mtime;
   time_t xtime;
   unsigned int has_md5sum;
   unsigned int has_crc32;
   MD5Digest md5sum;
   ir_uint32 crc32;
   int file_fd;
   unsigned int file_fd_count;
   unsigned int announce;
   off_t file_fd_location;
#ifdef HAVE_MMAP
   irlist_t mmaps;
#endif
   } xdcc;

typedef struct
{
  xdcc *xpack;
  char *nick;
  char *hostname;
  time_t queuedtime;
  time_t restrictsend_bad;
  unsigned int net;
  unsigned int dummy;
} ir_pqueue;

typedef struct
{
  time_t lastcontact;
  char *hostmask;
  long bucket;
  unsigned int flags;
  unsigned int dummy;
} igninfo;

typedef enum
{
  TRANSFERLIMIT_DAILY,
  TRANSFERLIMIT_WEEKLY,
  TRANSFERLIMIT_MONTHLY,
  NUMBER_TRANSFERLIMITS
} transferlimit_type_e;

typedef enum
{
  TRANSFER_STATUS_UNUSED,
  TRANSFER_STATUS_RESUME,
  TRANSFER_STATUS_LISTENING,
  TRANSFER_STATUS_CONNECTING,
  TRANSFER_STATUS_SENDING,
  TRANSFER_STATUS_WAITING,
  TRANSFER_STATUS_DONE
} transfer_status_e;

typedef union {
  struct sockaddr sa;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
} ir_sockaddr_union_t;

typedef struct {
  int family;
  int listensocket;
  int clientsocket;
  int dummy2;
  char *remoteaddr;
  char *localaddr;
  time_t lastcontact;
  time_t connecttime;
  ir_uint16 localport;
  ir_uint16 remoteport;
  ir_sockaddr_union_t remote;
  ir_sockaddr_union_t local;
  int dummy;
} ir_connection_t;

typedef struct
{
  unsigned int id;
  unsigned int net;
  off_t bytessent;
  off_t bytesgot;
  off_t lastack;
  off_t curack;
  off_t firstack;
  off_t halfack;
  off_t startresume;
  off_t lastspeedamt;
#ifdef HAVE_MMAP
  mmap_info_t *mmap_info;
#endif
  long tx_bucket;
  time_t restrictsend_bad;
  ir_uint64 connecttimems;
  ir_uint32 remoteip;
  ir_uint32 idummy;
  float lastspeed;
  float maxspeed;
  xdcc *xpack;
  ir_connection_t con;
  char *country;
  char *nick;
  char *caps_nick;
  char *hostname;
  char nomin;
  char nomax;
  unsigned char reminded;
  char close_to_timeout;
  char overlimit;
  char unlimited;
  short sdummy;
  transfer_status_e tr_status;
  unsigned int mirc_dcc64;
  unsigned int quietmode;
  unsigned int passive_dcc;
} transfer;

typedef enum
{
  UPLOAD_STATUS_UNUSED,
  UPLOAD_STATUS_RESUME,
  UPLOAD_STATUS_LISTENING,
  UPLOAD_STATUS_CONNECTING,
  UPLOAD_STATUS_GETTING,
  UPLOAD_STATUS_WAITING,
  UPLOAD_STATUS_DONE
} upload_status_e;

typedef struct
{
  ir_connection_t con;
  off_t bytessent;
  off_t bytesgot;
  off_t totalsize;
  off_t lastspeedamt;
  off_t resumesize;
  char *nick;
  char *hostname;
  char *file;
  char *uploaddir;
  float lastspeed;
  upload_status_e ul_status;
  int filedescriptor;
  unsigned int resumed;
  unsigned int net;
  unsigned int token;
  unsigned int mirc_dcc64;
  unsigned int overlimit;
} upload;

typedef enum
{
  DCCCHAT_UNUSED,
  DCCCHAT_LISTENING,
  DCCCHAT_CONNECTING,
  DCCCHAT_AUTHENTICATING,
  DCCCHAT_CONNECTED
} dccchat_e;

typedef struct
{
  ir_connection_t con;
  ir_boutput_t boutput;
  const char *name;
  char *nick;
  char *hostmask;
  char *groups;
  dccchat_e status;
  unsigned int net;
  unsigned int level;
  unsigned int dummy;
  char dcc_input_line[INPUT_BUFFER_LENGTH];
} dccchat_t;

typedef enum
{
  method_console         = (1 << 0),
  method_dcc             = (1 << 1),
  method_msg             = (1 << 2),
  method_out_all         = (1 << 3),
  method_fd              = (1 << 4),
  method_allow_all       =    0x01F, /* first 5 */
  
  method_xdl_channel     = (1 << 5),
  method_xdl_user_privmsg= (1 << 6),
  method_xdl_user_notice = (1 << 7),
  method_xdl_channel_min = (1 << 8),
  method_xdl_channel_sum = (1 << 9),
  
  method_allow_all_xdl   =    0x3FF /* everything */
} userinput_method_e;

typedef enum
{
  OUTERROR_TYPE_CRASH,
  OUTERROR_TYPE_WARN_LOUD,
  OUTERROR_TYPE_WARN,
  OUTERROR_TYPE_NOLOG = 0x80
} outerror_type_e;

typedef enum
{
  WRITESERVER_NOW = 1,
  WRITESERVER_FAST,
  WRITESERVER_NORMAL,
  WRITESERVER_SLOW,
} writeserver_type_e;

typedef struct
{
  dccchat_t *chat;
  char *hostmask;
  char *snick, *cmd;
  char *arg1, *arg2, *arg3;
  char *arg1e, *arg2e, *arg3e;
  userinput_method_e method;
  int fd;
  unsigned int net;
  unsigned int level;
} userinput;

typedef struct {
   time_t alloctime;
   void *ptr;
   const char *src_func;
   const char *src_file;
   unsigned int src_line;
   unsigned int size;
   } meminfo_t;

typedef struct
{
  char p_mode;
  char p_symbol;
} prefix_t;

typedef struct
{
  char prefixes[MAX_PREFIX];
  char *nick;
} member_t;

typedef struct
{
  char *name;
  char *key;
  char *fish;
  char *pgroup;
  char *joinmsg;
  char *listmsg;
  char *rgroup;
  unsigned short flags;
  unsigned short plisttime;
  unsigned short plistoffset;
  unsigned short delay;
  unsigned short noannounce;
  unsigned short plaintext;
  unsigned short notrigger;
  unsigned short waitjoin;
  time_t nextann;
  time_t nextmsg;
  time_t nextjoin;
  time_t lastjoin;
  irlist_t members;
  irlist_t headline;
} channel_t;

typedef enum {
   how_direct = 1,
   how_ssl,
   how_bnc,
   how_wingate,
   how_custom
   } how_e;

typedef struct
{
  char *host;
  char *password;
  char *vhost;
  how_e how;
  ir_uint16 port;
  ir_uint16 dummy;
} connectionmethod_t;

typedef struct {
  struct timeval tv;
  const char *file;
  const char *func;
  unsigned int line;
  unsigned int dummy;
} context_t;

typedef struct
{
  time_t listen_time;
  ir_uint16 port;
  ir_uint16 dummy2;
  unsigned int dummy;
} ir_listen_port_item_t;

typedef struct
{
  time_t when;
  char *hostmask;
  char *message;
} msglog_t;

typedef struct
{
  char *hostname;
  char *password;
  ir_uint16 port;
  ir_uint16 dummy2;
  unsigned int dummy;
} server_t;

/*------------ function declarations ------------- */

/* display.c */
void initscreen(int startup, int clear);
void uninitscreen(void);
void checktermsize(void);
void gototop(void);
void drawbot(void);
void gotobot(void);
void parseconsole(void);
size_t u_expand_command(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
tostdout(const char *format, ...);

void tostdout_write(void);
void tostdout_disable_buffering(void);

/* utilities.c */
void getos(void);
void floodchk(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
outerror (outerror_type_e type, const char *format, ...);

char* getdatestr(char* str, time_t Tp, size_t len);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
mylog(const char *format, ...);

void logstat(void);
unsigned long atoul (const char *str);
unsigned long long atoull (const char *str);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
ioutput(int dest, unsigned int color_flags, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 0)))
#endif
vioutput(int dest, unsigned int color_flags, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_fast(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_slow(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vprivmsg_slow(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vprivmsg(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice_fast(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice_slow(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vnotice_slow(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice(const char *nick, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vnotice(const char *nick, const char *format, va_list ap);
size_t sstrlen (const char *p);
void joinchannel(channel_t *c);
void updatecontext_f(const char *file, const char *func, unsigned int line);
void dumpcontext(void);
void dumpgdata(void);
void clearmemberlist(channel_t *c);
int isinmemberlist(const char *nick);
void addtomemberlist(channel_t *c, const char *nick);
void removefrommemberlist(channel_t *c, const char *nick);
void changeinmemberlist_mode(channel_t *c, const char *nick, int mode, unsigned int add);
void changeinmemberlist_nick(channel_t *c, const char *oldnick, const char *newnick);
int set_socket_nonblocking (int s, int nonblock);
void set_loginname(void);
int is_fd_readable(int fd);
int is_fd_writeable(int fd);
char* convert_to_unix_slash(char *ss);

void* mymalloc2(size_t a, int zero, const char *src_function, const char *src_file, unsigned int src_line);
void mydelete2(void *t);

#ifdef NO_SNPRINTF
int
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
snprintf(char *str, size_t n, const char *format, ... );

int vsnprintf(char *str, size_t n, const char *format, va_list ap );
#endif

#ifdef NO_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#endif

#ifdef NO_STRSIGNAL
const char *strsignal(int sig);
#endif

/* permanently add/delete items (includes malloc/free) */
#ifndef WITHOUT_MEMSAVE
#define irlist_add(x,y) irlist_add2(x,y,__FUNCTION__,__FILE__,__LINE__)
void* irlist_add2(irlist_t *list, size_t size,
                  const char *src_function, const char *src_file, unsigned int src_line);
#else
void* irlist_add(irlist_t *list, size_t size);
#endif
void* irlist_delete(irlist_t *list, void *item);
void irlist_delete_all(irlist_t *list);

/* temporarily remove and re-insert items */
void* irlist_remove(irlist_t *list, void *item);
void irlist_insert_head(irlist_t *list, void *item);
void irlist_insert_after(irlist_t *list, void *item, void *after_this);

/* get functions */
void* irlist_get_head(const irlist_t *list);
void* irlist_get_tail(const irlist_t *list);
void* irlist_get_next(const void *cur);
unsigned int irlist_size(const irlist_t *list);
void* irlist_get_nth(irlist_t *list, unsigned int nth); /* zero based n */

/* other */
int irlist_sort_cmpfunc_string(const void *a, const void *b);
int irlist_sort_cmpfunc_off_t(const void *a, const void *b);

transfer* does_tr_id_exist(unsigned int tr_id);
unsigned int get_next_tr_id(void);
void ir_listen_port_connected(ir_uint16 port);
int ir_bind_listen_socket(int fd, ir_sockaddr_union_t *sa);

int ir_boutput_write(ir_boutput_t *bout, const void *buffer, int buffer_len);
int ir_boutput_attempt_flush(ir_boutput_t *bout);
void ir_boutput_init(ir_boutput_t *bout, int fd, unsigned int flags);
void ir_boutput_delete(ir_boutput_t *bout);

const char *transferlimit_type_to_string(transferlimit_type_e type);

/* misc.c */
void getconfig (void);
void initirc(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
writeserver (writeserver_type_e type, const char *format, ... );
void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
vwriteserver(writeserver_type_e type, const char *format, va_list ap);

void sendserver(void);
void pingserver(void);
void xdccsavetext(void);
void writepidfile (const char *filename);
void gobackground(void);
char* getuptime(char *str, unsigned int type, time_t fromwhen, size_t len);
void shutdowniroffer(void);
void quit_server(void);
void switchserver(int which);
char* getstatusline(char *str, size_t len);
char* getstatuslinenums(char *str, size_t len);
void sendxdlqueue(void);
void initprefixes(void);
void initvars(void);
void startupiroffer(void);
void isrotatelog(void);
void createpassword(void);
int inttosaltchar (int n);
void notifybandwidth(void);
void notifybandwidthtrans(void);
int look_for_file_changes(xdcc *xpack);
void user_changed_nick(const char *oldnick, const char *newnick);
void reverify_restrictsend(void);

/* statefile.c */
void write_statefile(void);
unsigned int read_statefile(void);

/* dccchat.c */
int setupdccchatout(const char *nick, const char *hostmask, const char *token);
void setup_chat_banner(dccchat_t *chat);
void setupdccchataccept(dccchat_t *chat);
int setupdccchat(const char *nick,
                 const char *hostmask,
                 const char *line);
void setupdccchatconnected(dccchat_t *chat);
void parsedccchat(dccchat_t *chat,
                  char* line);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
writedccchat(dccchat_t *chat, int add_return, const char *format, ...);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 0)))
#endif
vwritedccchat(dccchat_t *chat, int add_return, const char *format, va_list ap);
void flushdccchat(dccchat_t *chat);
void writestatus(dccchat_t *chat);
void shutdowndccchat(dccchat_t *chat, int flush);

/* plugins.c */
void plugin_initialize (void);
void plugin_everyloop (void);
void plugin_every1sec (void);
void plugin_every20sec (void);
void plugin_ircprivmsg (const char *fullline, const char *nick, const char *hostname,
			const char *dest, const char *msg1, const char *msg2, const char *msg3,
			const char *msg4, const char *msg5);
void plugin_ircinput (const char *fullline, const char *part2, const char *part3, const char *part4);

/* transfer.c */
void t_initvalues (transfer * const t);
void t_setuplisten (transfer * const t);
void t_establishcon (transfer * const t);
void t_setup_send(transfer * const t);
void t_transfersome (transfer * const t);
void t_readjunk (transfer * const t);
void t_istimeout (transfer * const t);
void t_flushed(transfer * const t);
void t_closeconn(transfer * const t, const char *msg, int errno1);
void t_setresume(transfer * const t, const char *amt);
void t_remind(transfer * const t);
void t_checkminspeed(transfer * const t);

/* upload.c */
void l_initvalues (upload * const l);
void l_establishcon (upload * const l);
void l_transfersome (upload * const l);
void l_istimeout (upload * const l);
void l_closeconn(upload * const l, const char *msg, int errno1);

/* admin.c */
void u_fillwith_console(userinput * const u, char *line);
void u_fillwith_dcc(userinput * const u, dccchat_t *chat, char *line);

void u_parseit(userinput * const u);

void u_xdl_head(const userinput * const u);
void u_listdir(const userinput * const u, const char *dir);
void u_diskinfo(const userinput * const u, const char *dir);

#endif

/* End of File */
