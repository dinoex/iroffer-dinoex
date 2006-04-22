/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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
#include <regex.h>
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

#include "iroffer_md5.h"

/*------------ structures ------------- */

typedef struct irlist_item_t2
{
  struct irlist_item_t2 *next;
  struct irlist_item_t2 *prev;
} irlist_item_t;

typedef struct
{
  int size;
  irlist_item_t *head;
  irlist_item_t *tail;
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
  int flags;
  struct MD5Context *md5sum;
  irlist_t segments;
  unsigned int count_written;
  unsigned int count_flushed;
  unsigned int count_dropped;
} ir_boutput_t;

#ifdef HAVE_MMAP
typedef struct
{
  off_t mmap_offset; /* leave first */
  unsigned char *mmap_ptr;
  size_t mmap_size;
  int ref_count;
} mmap_info_t;
#endif

typedef struct {
   char *file, *desc, *note;
   char *group;
   char *group_desc;
   char *lock;
   char *dlimit_desc;
   int gets;
   int dlimit_max;
   int dlimit_used;
   float minspeed,
         maxspeed;
   off_t st_size;
   dev_t st_dev;
   ino_t st_ino;
   time_t mtime;
   int has_md5sum;
   MD5Digest md5sum;
   int file_fd;
   int file_fd_count;
   off_t file_fd_location;
   unsigned long crc32;
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
#ifdef MULTINET
  int net;
#endif /* MULTINET */
} pqueue;

typedef struct
{
  char *hostmask;
  regex_t *regexp;
  short flags;
  long bucket;
  time_t lastcontact;
} igninfo;

typedef struct
{
  char hostmask[maxtextlength];
  short flags;
  long bucket;
  time_t lastcontact;
} igninfo_file;

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
  TRANSFER_STATUS_LISTENING,
  TRANSFER_STATUS_SENDING,
  TRANSFER_STATUS_WAITING,
  TRANSFER_STATUS_DONE
} transfer_status_e;

typedef struct
{
  int listensocket;
  int clientsocket;
  int id;
#ifdef MULTINET
  int net;
#endif /* MULTINET */
  off_t bytessent;
  off_t bytesgot;
  off_t lastack;
  off_t curack;
  off_t startresume;
  off_t lastspeedamt;
#ifdef HAVE_MMAP
  mmap_info_t *mmap_info;
#endif
  long tx_bucket;
  time_t lastcontact;
  time_t connecttime;
  time_t restrictsend_bad;
  unsigned long long connecttimems;
  unsigned short remoteport;
  unsigned short listenport;
  unsigned long remoteip;
  unsigned long localip;
  float lastspeed;
  xdcc *xpack;
  struct sockaddr_in serveraddress;
  char *nick;
  char *caps_nick;
  char *hostname;
  char nomin;
  char nomax;
  char reminded;
  char close_to_timeout;
  char overlimit;
  char unlimited;
  transfer_status_e tr_status;
} transfer;

typedef enum
{
  UPLOAD_STATUS_UNUSED,
  UPLOAD_STATUS_CONNECTING,
  UPLOAD_STATUS_GETTING,
  UPLOAD_STATUS_WAITING,
  UPLOAD_STATUS_DONE
} upload_status_e;

typedef struct
{
  int clientsocket;
  int filedescriptor;
  off_t bytesgot;
  off_t totalsize;
  off_t lastspeedamt;
  off_t resumesize;
  time_t lastcontact;
  time_t connecttime;
  unsigned short remoteport;
  unsigned short localport;
  unsigned long remoteip;
  unsigned long localip;
  float lastspeed;
  char *nick;
  char *hostname;
  char *file;
  upload_status_e ul_status;
  int resumed;
#ifdef MULTINET
  int net;
#endif /* MULTINET */
} upload;

typedef struct
{
  enum
    {
      DCCCHAT_UNUSED,
      DCCCHAT_LISTENING,
      DCCCHAT_CONNECTING,
      DCCCHAT_AUTHENTICATING,
      DCCCHAT_CONNECTED,
    } status;
  int fd;
  int net;
  ir_boutput_t boutput;
  time_t lastcontact;
  time_t connecttime;
  unsigned short remoteport;
  unsigned short localport;
  unsigned long remoteip;
  unsigned long localip;
  char *nick;
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
  CALLTYPE_NORMAL,
  CALLTYPE_MULTI_FIRST,
  CALLTYPE_MULTI_MIDDLE,
  CALLTYPE_MULTI_END
} calltype_e;

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
  userinput_method_e method;
  char *snick, *cmd;
  char *arg1, *arg2, *arg3;
#ifndef MULTINET
  char *arg1e, *arg2e;
#else /* MULTINET */
  char *arg1e, *arg2e, *arg3e;
#endif /* MULTINET */
  int fd;
  dccchat_t *chat;
} userinput;

typedef struct {
   void *ptr;
   const char *src_func;
   const char *src_file;
   int src_line;
   time_t alloctime;
   int size;
   } meminfo_t;

typedef struct
{
  char p_mode;
  char p_symbol;
} prefix_t;

typedef struct
{
  char prefixes[MAX_PREFIX];
  char nick[1]; /* last, make this bigger on alloc */
} member_t;

typedef struct
{
  char *name;
  char *key;
  char *headline;
  short flags;
  short plisttime;
  short plistoffset;
  short delay;
  long lastjoin;
  irlist_t members;
} channel_t;

typedef enum {
   how_direct = 1,
   how_bnc,
   how_wingate,
   how_custom
   } how_e;

typedef struct
{
  how_e how;
  char *host;
  unsigned short port;
  char *password;
  char *vhost;
} connectionmethod_t;

typedef struct {
  const char *file;
  const char *func;
  int line;
  struct timeval tv;
} context_t;

typedef struct
{
  ir_uint16 port;
  time_t listen_time;
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
  ir_uint16 port;
  char *password;
} server_t;

typedef struct
{
  int pack;
  char *word;
  char *message;
} autoqueue_t;

typedef struct
{
  int delay;
  char *chan;
  char *msg;
} channel_announce_t;

/*------------ function declarations ------------- */

/* iroffer.c */
void sendxdccfile(const char* nick, const char* hostname, const char* hostmask, int pack, const char* msg, const char* pwd);
void sendxdccinfo(const char* nick, const char* hostname, const char* hostmask, int pack, const char* msg);
void sendaqueue(int type);

/* display.c */
void initscreen(int startup);
void uninitscreen(void);
void checktermsize(void);
void gototop(void);
void drawbot(void);
void gotobot(void);
void parseconsole(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
tostdout(const char *format, ...);

void vtostdout(const char *format, va_list ap);
void tostdout_write(void);
void tostdout_disable_buffering(int flush);

/* utilities.c */
const char* strstrnocase (const char *str1, const char *match1);
#define getpart(x,y) getpart2(x,y,__FUNCTION__,__FILE__,__LINE__)
char* getpart2(const char *line, int howmany, const char *src_function, const char *src_file, int src_line);
char* caps(char *text);
char* nocaps(char *text);
char* sizestr(int spaces, off_t num);
void getos(void);
void floodchk(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
outerror (outerror_type_e type, const char *format, ...);

char* getdatestr(char* str, time_t Tp, int len);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
mylog(calltype_e type, const char *format, ...);

void logstat(void);
unsigned long atoul (const char *str);
unsigned long long atoull (const char *str);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 4, 5)))
#endif
ioutput(calltype_e type, int dest, unsigned int color_flags, const char *format, ...);

void vioutput(calltype_e type, int dest, unsigned int color_flags, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_fast(const char *nick, const char *format, ...);

void vprivmsg_fast(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg_slow(const char *nick, const char *format, ...);

void vprivmsg_slow(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
privmsg(const char *nick, const char *format, ...);

void vprivmsg(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice_fast(const char *nick, const char *format, ...);

void vnotice_fast(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice_slow(const char *nick, const char *format, ...);

void vnotice_slow(const char *nick, const char *format, va_list ap);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
notice(const char *nick, const char *format, ...);

void vnotice(const char *nick, const char *format, va_list ap);
char* hostmasktoregex(const char *str);
int verifyhost(irlist_t *list, const char *hmask);
int verifypass(const char *testpass);
int packnumtonum(const char *a);
int sstrlen (const char *p);
char dayofweektomask(const char a);
char *strrev(char *str);
int isprintable(char a);
char onlyprintable(char a);
char* removenonprintable(char *str1);
char* removenonprintablectrl(char *str1);
char* removenonprintablefile(char *str);
int doesfileexist(const char *f);
void joinchannel(channel_t *c);
void checkadminpass(void);
void updatecontext_f(const char *file, const char *func, int line);
void dumpcontext(void);
void dumpgdata(void);
void clearmemberlist(channel_t *c);
int isinmemberlist(const char *nick);
void addtomemberlist(channel_t *c, const char *nick);
void removefrommemberlist(channel_t *c, const char *nick);
void changeinmemberlist_mode(channel_t *c, const char *nick, char mode, int add);
void changeinmemberlist_nick(channel_t *c, const char *oldnick, const char *newnick);
int set_socket_nonblocking (int s, int nonblock);
void set_loginname(void);
int is_fd_readable(int fd);
int is_fd_writeable(int fd);
char* convert_to_unix_slash(char *ss);

void* mymalloc2(int a, int zero, const char *src_function, const char *src_file, int src_line);
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
#define irlist_add(x,y) irlist_add2(x,y,__FUNCTION__,__FILE__,__LINE__)
void* irlist_add2(irlist_t *list, unsigned int size,
                  const char *src_function, const char *src_file, int src_line);
void* irlist_delete(irlist_t *list, void *item);
void irlist_delete_all(irlist_t *list);

/* temporarily remove and re-insert items */
void* irlist_remove(irlist_t *list, void *item);
void irlist_insert_head(irlist_t *list, void *item);
void irlist_insert_tail(irlist_t *list, void *item);
void irlist_insert_before(irlist_t *list, void *item, void *before_this);
void irlist_insert_after(irlist_t *list, void *item, void *after_this);

/* get functions */
void* irlist_get_head(const irlist_t *list);
void* irlist_get_tail(const irlist_t *list);
void* irlist_get_next(const void *cur);
void* irlist_get_prev(const void *cur);
int irlist_size(const irlist_t *list);
void* irlist_get_nth(irlist_t *list, int nth); /* zero based n */

/* other */
int irlist_sort_cmpfunc_string(void *userdata, const void *a, const void *b);
int irlist_sort_cmpfunc_int(void *userdata, const void *a, const void *b);
int irlist_sort_cmpfunc_off_t(void *userdata, const void *a, const void *b);
void irlist_sort(irlist_t *list,
                 int (*cmpfunc)(void *userdata, const void *a, const void *b),
                 void *userdata);

transfer* does_tr_id_exist(int tr_id);
int get_next_tr_id(void);
void ir_listen_port_connected(ir_uint16 port);
int ir_bind_listen_socket(int fd, struct sockaddr_in *sa);

int ir_boutput_write(ir_boutput_t *bout, const void *buffer, int buffer_len);
int ir_boutput_need_flush(ir_boutput_t *bout);
int ir_boutput_attempt_flush(ir_boutput_t *bout);
void ir_boutput_init(ir_boutput_t *bout, int fd, int flags);
void ir_boutput_set_flags(ir_boutput_t *bout, int flags);
void ir_boutput_delete(ir_boutput_t *bout);
void ir_boutput_get_md5sum(ir_boutput_t *bout, MD5Digest digest);

const char *transferlimit_type_to_string(transferlimit_type_e type);

/* misc.c */
void getconfig (void);
void getconfig_set (const char *line, int rehash);
void initirc(void);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
writeserver (writeserver_type_e type, const char *format, ... );
void vwriteserver(writeserver_type_e type, const char *format, va_list ap);

void sendserver(void);
char* getsendname(char * const full);
const char* getfilename(const char * const full);
void pingserver(void);
void xdccsavetext(void);
void writepidfile (const char *filename);
char* getfline(char* str, int slen, int descr, int ret);
void gobackground(void);
char* getuptime(char *str, int type, time_t fromwhen, int len);
void shutdowniroffer(void);
void switchserver(int which);
int connectirc2 (struct in_addr *remote);
char* getstatusline(char *str, int len);
char* getstatuslinenums(char *str, int len);
void sendxdlqueue(void);
int isthisforme (const char *dest, char *msg1);
void reinit_config_vars(void);
void initprefixes(void);
void initchanmodes(void);
void initvars(void);
void startupiroffer(void);
void isrotatelog(void);
void createpassword(void);
char inttosaltchar (int n);
void notifyqueued(void);
void notifybandwidth(void);
void notifybandwidthtrans(void);
int look_for_file_changes(xdcc *xpack);
void user_changed_nick(const char *oldnick, const char *newnick);
void reverify_restrictsend(void);

/* statefile.c */
void write_statefile(void);
void read_statefile(void);

/* dccchat.c */
int setupdccchatout(const char *nick);
void setupdccchataccept(dccchat_t *chat);
int setupdccchat(const char *nick,
                 const char *line);
void setupdccchatconnected(dccchat_t *chat);
void parsedccchat(dccchat_t *chat,
                  char* line);

void
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
#endif
writedccchat(dccchat_t *chat, int add_return, const char *format, ...);

void vwritedccchat(dccchat_t *chat, int add_return, const char *format, va_list ap);
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
void u_fillwith_msg(userinput * const u, const char *n, const char *line);
void u_fillwith_clean(userinput * const u);

void u_parseit(userinput * const u);

/* main.c */
char* addtoqueue(const char* nick, const char* hostname, int pack);

#endif

/* End of File */
