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

/*       max number of networks */
#define  MAX_NETWORKS  10U

/*       max number of vhosts */
#define  MAX_VHOSTS    2U

#define  MAX_WAKEUP_WARN	2
#define  MAX_WAKEUP_ERR		10

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#define XDCC_SEND_LIST ((unsigned int)-1)
#define MAX_XML_CHUNK  8192

#define IPV4_PRINT_FMT "%u.%u.%u.%u"
#define IPV4_PRINT_DATA(x) (x >> 24) & 0xFF, (x >> 16) & 0xFF, (x >> 8) & 0xFF, (x) & 0xFF
#define CRC32_PRINT_FMT "%.8X"

/* terminal */
#define IRVT_CURSOR_HOME0       "\x1b[H"
#define IRVT_CURSOR_HOME1       "\x1b[%u;1H"
#define IRVT_CURSOR_HOME2       "\x1b[%u;%uH"
#define IRVT_SAVE_CURSOR        "\x1b[s"
#define IRVT_UNSAVE_CURSOR      "\x1b[u"
#define IRVT_SCROLL_ALL         "\x1b[r"
#define IRVT_SCROLL_LINES1      "\x1b[1;%ur"
#define IRVT_ERASE_DOWN         "\x1b[J"
#define IRVT_COLOR_RESET        "\x1b[0m"
#define IRVT_COLOR_YELLOW       "\x1b[1;33m"
#define IRVT_COLOR_SET1         "\x1b[%u;%um"

/* IRC text color chars */
#define IRCCTCP		'\1'
#define IRCBOLD		0x02
#define IRCCOLOR	0x03
#define IRCNORMAL	0x0F
#define IRCINVERSE	0x16
#define IRCITALIC	0x1D
#define IRCUNDERLINE	0x1F

/* IRC text color strings */
#define IRC_CTCP	"\1"
#define IRC_BOLD	"\x02"
#define IRC_COLOR	"\x03"
#define IRC_NORMAL	"\x0F"
#define IRC_INVERSE	"\x16"
#define IRC_ITALIC	"\x1D"
#define IRC_UNDERLINE	"\x1F"

/* EOF */
