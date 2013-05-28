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
 * @(#) iroffer_defines.h 1.118@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_defines.h|20050313225819|12365
 * 
 */

#if !defined _IROFFER_DEFINES
#define _IROFFER_DEFINES

#include "dinoex_defines.h"

/* options, these should be on */
/*       Ping The Server */
#define  PING_SRVR 1
/*       XDCC SAVE on Shutdown */
#define  SAVEQUIT  1
/*       dont allow running as root */
#define  BLOCKROOT 1


/*
 * determine how many transfers we can have based on how
 * large FD_SETSIZE is:
 *
 * 7 FDs are reserved:
 *   3 for in/out/err
 *   1 for ircserver
 *   1 for logfile
 *   1 for md5sum
 *   1 temporary use: accept(), statefile, xdcclistfile, etc..
 * 
 * 2 FDs for each transfer
 * 2 FDs for each upload
 * 1 FD for each DCC chat
 *
 */

#define  RESERVED_FDS (7)

/*       max size for xdcc list queue */
#define  MAXXLQUE  5U
/*       max config files */
#define  MAXCONFIG  10U
/*       meminfo hash size */
#define  MEMINFOHASHSIZE 256U
/*       Max context log */
#define  MAXCONTEXTS 300U
/*       Server Connection Timeout In Seconds */
#define  CTIMEOUT  5
/*       Server Connection Timeout Backoff In Seconds */
#define  CBKTIMEOUT 2U
/*       How Long to Wait Until We Giveup On A Non-responding Server */
#define  SRVRTOUT  240

#define  XDCC_SENT_SIZE 120U
#define  INAMNT_SIZE 10U

/*       Max Server Send Queue Lines */
#define  MAXSENDQ  10000U

/*       weight for speed calcualtion in dcl initial */
#define  DCL_SPDW_I  0.5
/*       weight for speed calcualtion in dcl ongoing */
#define  DCL_SPDW_O  0.9
/*       time until minspeed checking becomes active */
#define  MIN_TL    60

/*       minimum transfer size */
#define  TXSIZE 1460    /* max ethernet size tcp payload */
/*       buffer size */
#define  BUFFERSIZE  (TXSIZE * 3)
/*       max TXSIZE blocks to write per cycle */
#define  MAXTXPERLOOP  30

#ifdef HAVE_MMAP
/* how large of a mmap to do at a time, MUST BE POWER OF 2! */
#define IR_MMAP_SIZE (512*1024)
#endif

/*       excess flood protection */
#define  EXCESS_BUCKET_MAX    600    /* max burst size */
#define  EXCESS_BUCKET_ADD     25    /* ave cps */

/*       max transfer speed */
#define  MAX_TRANSFER_TX_BURST_SIZE   5U    /* 5 seconds worth */

#define MAX_HISTORY_SIZE 100U

#define MAX_PREFIX     16U
#define MAX_CHANMODES  16U

#ifndef WITHOUT_MEMSAVE
/* free'ing just leads to trouble if we dont check first and then make NULL */
#define mydelete(x) { mydelete2(x); x = NULL; }
#define mymalloc(x) mymalloc2(x, 0, __FUNCTION__, __FILE__, __LINE__)
#define mycalloc(x) mymalloc2(x, 1, __FUNCTION__, __FILE__, __LINE__)
#else
#define mydelete(x) { free(x); x = NULL; }
#define mymalloc(x) malloc(x)
#define mycalloc(x) calloc(1, x)
#endif

#define  maxtextlengthshort 60U
#define  maxtextlength 512U
#define INPUT_BUFFER_LENGTH 2048U

#define LISTEN_PORT_REUSE_TIME (30*60) /* 30 minutes */

/* type definitions for igninfo flags */
#define IGN_MANUAL      1U
#define IGN_IGNORING    2U

/* type definitions for channel_t flags */
#define CHAN_ONCHAN     1U
#define CHAN_MINIMAL    2U
#define CHAN_SUMMARY    4U
#define CHAN_KICKED     0x40U

/* buffer output flags */
#define BOUTPUT_NO_LIMIT       2U

/* type definistions: Screen, Log, DCC CHAT, HTTPD */
#define OUT_S    1U
#define OUT_L    2U
#define OUT_D    4U
#define OUT_H    8U

/* Buffer Output options */
#define IR_BOUTPUT_SEGMENT_SIZE    (1024U)
#define IR_BOUTPUT_MAX_SEGMENTS    (64)

/* color options */
#define COLOR_NO_COLOR   0
#define COLOR_BLACK      30
#define COLOR_RED        31
#define COLOR_GREEN      32
#define COLOR_YELLOW     33
#define COLOR_BLUE       34
#define COLOR_MAGENTA    35
#define COLOR_CYAN       36
#define COLOR_WHITE      37
#define COLOR_BOLD       0x80U

/* it's up to the umask to prune away.. */
#define CREAT_PERMISSIONS ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH )

/* some os's (cygwin, cough, cough, ... ) require extra flags to open() */
#define ADDED_OPEN_FLAGS 0

#define FD_UNUSED -1

#define MD5_PRINT_FMT "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x"

#define MD5_PRINT_DATA(digest) digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]

/* where would we be without these? */
#define max2(a,b) ( (a > b) ? a : b )
#define min2(a,b) ( (a < b) ? a : b )
#define between(min,a,max) ( max2(min2(a,max),min) )
#define ceiling(v,c) ( (v) + (((v) % (c)) ? ((c) - ((v) % (c))) : 0) )


#define updatecontext() updatecontext_f(__FILE__,__FUNCTION__,__LINE__)

/* set os specific values */
/* linux */
#if defined(_OS_Linux)
#define _GNU_SOURCE

/* bsd */
#elif defined(_OS_FreeBSD)   || \
    defined(_OS_OpenBSD)     || \
    defined(_OS_NetBSD)      || \
    defined(_OS_BSDI)        || \
    defined(_OS_BSD_OS)      || \
    defined(_OS_Darwin)

/* sunos */
#elif defined(_OS_SunOS)

/* HP-UX */
#elif defined(_OS_HPUX)

/* IRIX */
#elif defined(_OS_IRIX)

/* IRIX64 */
#elif defined(_OS_IRIX64)

/* Digital UNIX */
#elif defined(_OS_OSF1)

/* MacOS X Server */
#elif defined(_OS_Rhapsody)

/* MacOS X */
#elif defined(_OS_Darwin)

/* CYGWIN */
#elif defined(_OS_CYGWIN)
#undef ADDED_OPEN_FLAGS
#define ADDED_OPEN_FLAGS O_BINARY

/* AIX */
#elif defined(_OS_AIX)

/* other */
#else
#error "** ERROR: This OS Is Not Supported Or You Didn't Run Configure **"
#endif

#endif

/* End of File */
