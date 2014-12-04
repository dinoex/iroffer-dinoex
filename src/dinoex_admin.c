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

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_admin.h"
#include "dinoex_curl.h"
#include "dinoex_queue.h"
#include "dinoex_jobs.h"
#include "dinoex_upload.h"
#include "dinoex_irc.h"
#include "dinoex_config.h"
#include "dinoex_ruby.h"
#include "dinoex_transfer.h"
#include "dinoex_misc.h"
#include "strnatcmp.h"

#include <ctype.h>

static void strip_trailing_path(char *str)
{
  size_t len;

  if (str == NULL)
    return;

  if (str[0] == 0)
    return;

  len = strlen(str);
  if (str[len - 1] == '/')
    str[len - 1] = '\0';
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 0)))
#endif
voutput_fd(int fd, const char *format, va_list args)
{
  char *tempstr;
  ssize_t retval;
  ssize_t llen;
  size_t ulen;

  updatecontext();
  tempstr = mymalloc(maxtextlength);

  llen = vsnprintf(tempstr, maxtextlength - 3, format, args);
  if ((llen < 0) || (llen >= ((ssize_t)maxtextlength - 3))) {
    outerror(OUTERROR_TYPE_WARN, "string too long!");
    mydelete(tempstr);
    return;
  }
  ulen = (size_t)llen;

  if (!gdata.xdcclistfileraw) {
    ulen = removenonprintable(tempstr);
  }

  if (gdata.dos_text_files)
    tempstr[ulen++] = '\r';

  tempstr[ulen++] = '\n';
  tempstr[ulen] = '\0';

  retval = write(fd, tempstr, strlen(tempstr));
  if (retval < 0)
    outerror(OUTERROR_TYPE_WARN_LOUD, "Write failed: %s", strerror(errno));

  mydelete(tempstr);
}

void a_respond(const userinput * const u, const char *format, ...)
{
  va_list args;
  gnetwork_t *backup;
  channel_t *ch;
  char *tempnick;
  char *chan;

  updatecontext();

  va_start(args, format);
  backup = gnetwork;
  gnetwork = &(gdata.networks[u->net]);

  switch (u->method)
    {
    case method_console:
      vioutput(OUT_S, COLOR_NO_COLOR, format, args);
      break;
    case method_dcc:
      vwritedccchat(u->chat, 1, format, args);
      break;
    case method_out_all:
      vioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, format, args);
      break;
    case method_fd:
      voutput_fd(u->fd, format, args);
      break;
    case method_msg:
      vprivmsg(u->snick, format, args);
      break;
    case method_xdl_channel:
    case method_xdl_channel_min:
    case method_xdl_channel_sum:
      ch = NULL;
      tempnick = mystrdup(u->snick);
      for (chan = strtok(tempnick, ","); chan != NULL; chan = strtok(NULL, ",") ) { /* NOTRANSLATE */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (!strcasecmp(ch->name, chan)) {
            vprivmsg_chan(ch, format, args);
            va_end(args);
            va_start(args, format);
          }
        }
      }
      mydelete(tempnick);
      break;
    case method_xdl_user_privmsg:
      vprivmsg_slow(u->snick, format, args);
      break;
    case method_xdl_user_notice:
      vnotice_slow(u->snick, format, args);
      break;
    case method_allow_all:
    case method_allow_all_xdl:
    default:
      break;
    }

  gnetwork = backup;
  va_end(args);
}

void a_parse_inputline(userinput * const u, const char *line)
{
  char *part[4];

  updatecontext();

  if (line == NULL)
    return;

  get_argv(part, line, 4);
  u->cmd = caps(part[0]);
  u->arg1 = part[1];
  u->arg2 = part[2];
  u->arg3 = part[3];
  u->arg1e = getpart_eol(line, 2);
  u->arg2e = getpart_eol(line, 3);
  u->arg3e = getpart_eol(line, 4);
}

int get_network(const char *arg1)
{
  unsigned int net;

  /* default */
  if (arg1 == NULL)
    return 0;

  /* numeric */
  net = atoi(arg1);
  if ((net > 0) && (net <= gdata.networks_online))
    return --net;

  /* text */
  for (net=0; net<gdata.networks_online; ++net) {
    if (gdata.networks[net].name == NULL)
      continue;
    if (strcasecmp(gdata.networks[net].name, arg1) == 0)
      return net;
  }

  /* unknown */
  return -1;
}

static unsigned int hide_locked(const userinput * const u, const xdcc *xd)
{
  if (gdata.hidelockedpacks == 0)
    return 0;

  if (xd->lock == NULL)
    return 0;

  switch (u->method) {
  case method_fd:
  case method_xdl_channel:
  case method_xdl_user_privmsg:
  case method_xdl_user_notice:
    return 1;
  default:
    break;
  }
  return 0;
}

static unsigned int a_xdl_len(unsigned int i)
{
  if (i < 10) return 1;
  if (i < 100) return 2;
  if (i < 1000) return 3;
  if (i < 10000) return 4;
  return 5;
}

static unsigned int a_xdl_space(void)
{
  xdcc *xd;
  unsigned int i;

  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    i = max2(i, xd->gets);
  }
  return a_xdl_len(i);
}

static unsigned int a_xdl_left(void)
{
  unsigned int n;

  n = irlist_size(&gdata.xdccs);
  return a_xdl_len(n);
}

static void a_xdl_pack(const userinput * const u, char *tempstr, unsigned int i, unsigned int l, unsigned int s, const xdcc *xd)
{
  char datestr[maxtextlengthshort];
  const char *sep;
  const char *groupstr;
  char *sizestrstr;
  char *colordesc;
  size_t len;

  sizestrstr = sizestr(1, xd->st_size);
  datestr[0] = 0;
  if (gdata.show_date_added) {
    datestr[0] = ' ';
    datestr[1] = 0;
    user_getdatestr(datestr + 1, xd->xtime ? xd->xtime : xd->mtime, maxtextlengthshort - 1);
  }
  sep = "";
  groupstr = "";
  if (gdata.show_group_of_pack) {
    if (xd->group != NULL) {
      sep = gdata.group_seperator;
      groupstr = xd->group;
    }
  }
  colordesc = xd_color_description(xd);
  len = add_snprintf(tempstr, maxtextlength,
                     "\2#%-*u\2 %*ux [%s]%s %s%s%s",
                     l,
                     i,
                     s, xd->gets,
                     sizestrstr,
                     datestr,
                     colordesc,
                     sep, groupstr);
  if (colordesc != xd->desc)
    mydelete(colordesc);
  mydelete(sizestrstr);

  if (xd->minspeed > 0 && xd->minspeed != gdata.transferminspeed) {
    len += add_snprintf(tempstr + len, maxtextlength - len,
                        " [%1.1fK Min]",
                        xd->minspeed);
  }

  if ((xd->maxspeed > 0) && (xd->maxspeed != gdata.transfermaxspeed)) {
    len += add_snprintf(tempstr + len, maxtextlength - len,
                        " [%1.1fK Max]",
                        xd->maxspeed);
  }

  if (xd->dlimit_max != 0) {
    len += add_snprintf(tempstr + len, maxtextlength - len,
                        " [%u of %u DL left]",
                        xd->dlimit_used - xd->gets, xd->dlimit_max);
  }

  a_respond(u, "%s", tempstr);

  if (xd->note && xd->note[0]) {
    a_respond(u, " \2^-\2%*s%s", s, "", xd->note);
  }
}

static void a_xdl_foot(const userinput * const u)
{
  xdcc *xd;
  char *sizestrstr;
  char *totalstr;
  off_t toffered;

  if (gdata.creditline) {
    a_respond(u, "\2**\2 %s \2**\2", gdata.creditline);
  }

  if (u->method != method_xdl_channel_min) {
    toffered = 0;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      toffered += xd->st_size;
    }

    sizestrstr = sizestr(0, toffered);
    totalstr = sizestr(0, gdata.totalsent);
    a_respond(u, "Total Offered: %sB  Total Transferred: %sB",
              sizestrstr, totalstr);
    mydelete(totalstr);
    mydelete(sizestrstr);
  }
}

void a_xdl_full(const userinput * const u)
{
  char *tempstr;
  xdcc *xd;
  unsigned int i;
  unsigned int l;
  unsigned int s;

  updatecontext();

  u_xdl_head(u);

  tempstr = mymalloc(maxtextlength);
  i = 1;
  l = a_xdl_left();
  s = a_xdl_space();
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd), ++i) {
    if (hide_locked(u, xd) == 0)
      a_xdl_pack(u, tempstr, i, l, s, xd);
  }
  mydelete(tempstr);

  a_xdl_foot(u);
}

void a_xdl_group(const userinput * const u)
{
  char *tempstr;
  char *msg3;
  xdcc *xd;
  unsigned int i;
  unsigned int k;
  unsigned int l;
  unsigned int s;

  updatecontext();

  u_xdl_head(u);

  msg3 = u->arg1;
  tempstr = mymalloc(maxtextlength);
  i = 1;
  k = 0;
  l = a_xdl_left();
  s = a_xdl_space();
  xd = irlist_get_head(&gdata.xdccs);
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd), ++i) {
    if (msg3 == NULL) {
      if (xd->group != NULL)
        continue;
    } else {
      if (xd->group == NULL)
        continue;

      if (strcasecmp(xd->group, msg3) != 0 )
        continue;

      if (xd->group_desc != NULL) {
        a_respond(u, "group: %s%s%s", msg3, gdata.group_seperator, xd->group_desc);
      }
    }
    if (hide_locked(u, xd) != 0)
      continue;

    a_xdl_pack(u, tempstr, i, l, s, xd);
    ++k;
  }
  mydelete(tempstr);

  a_xdl_foot(u);

  if (k == 0) {
    a_respond(u, "Sorry, nothing was found, try a XDCC LIST");
  }
}

void a_xdl(const userinput * const u)
{
  char *tempstr;
  char *inlist;
  unsigned int i;
  unsigned int l;
  unsigned int s;
  xdcc *xd;
  irlist_t grplist = {0, 0, 0};

  updatecontext();

  u_xdl_head(u);

  if (u->method==method_xdl_channel_sum) return;

  tempstr  = mymalloc(maxtextlength);
  l = a_xdl_left();
  s = a_xdl_space();
  i = 1;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd), ++i) {
    /* skip is group is set */
    if (xd->group == NULL) {
      if (hide_locked(u, xd) == 0)
        a_xdl_pack(u, tempstr, i, l, s, xd);
    }
  }

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    /* groupe entry and entry is visible */
    if ((xd->group != NULL) && (xd->group_desc != NULL)) {
      snprintf(tempstr, maxtextlength,
               "%s%s%s", xd->group, gdata.group_seperator, xd->group_desc);
      irlist_add_string(&grplist, tempstr);
    }
  }

  irlist_sort2(&grplist, irlist_sort_cmpfunc_string);
  for (inlist = irlist_get_head(&grplist);
       inlist;
       inlist = irlist_delete(&grplist, inlist) ) {
    a_respond(u, "group: %s", inlist);
  }

  a_xdl_foot(u);

  mydelete(tempstr);
}

unsigned int reorder_new_groupdesc(const char *group, const char *desc)
{
  xdcc *xd;
  unsigned int k;

  updatecontext();

  k = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, group) != 0)
      continue;

    ++k;
    /* delete all matching entires */
    if (xd->group_desc != NULL)
      mydelete(xd->group_desc);
    /* write only the first entry */
    if (k != 1)
      continue;

    if (desc && (desc[0] != 0))
      xd->group_desc = mystrdup(desc);
  }

  return k;
}

unsigned static int reorder_groupdesc(const char *group)
{
  xdcc *xd;
  xdcc *firstxd;
  xdcc *descxd;
  unsigned int k;

  updatecontext();

  k = 0;
  firstxd = NULL;
  descxd = NULL;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, group) != 0)
      continue;

    ++k;
    if (xd->group_desc != NULL) {
      if (descxd == NULL) {
        descxd = xd;
      } else {
        /* more than one desc */
        mydelete(xd->group_desc);
      }
    }
    /* check only the first entry */
    if (k == 1)
      firstxd = xd;
  }

  if (k == 0)
    return k;

  if (descxd == NULL)
    return k;

  if (descxd == firstxd)
    return k;

  firstxd->group_desc = descxd->group_desc;
  descxd->group_desc = NULL;
  return k;
}

unsigned int add_default_groupdesc(const char *group)
{
  xdcc *xd;
  xdcc *firstxd;
  unsigned int k;

  updatecontext();

  k = 0;
  firstxd = NULL;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, group) != 0)
      continue;

    ++k;
    /* descriptions alreaay there */
    if (xd->group_desc != NULL)
      return 0;

    /* check only the first entry */
    if (k == 1)
      firstxd = xd;
  }

  if (k != 1)
    return k;

  firstxd->group_desc = mystrdup(group);
  return k;
}

static char *file_without_numbers(const char *s)
{
  const char *x;
  char *d;
  char *w;
  char ch;
  size_t l;

  if (s == NULL)
    return NULL;

  /* ignore path */
  x = strrchr(s, '/');
  if (x != NULL)
    ++x;
  else
    x = s;

  d = mystrdup(x);
  /* ignore extension */
  w = strrchr(d, '.');
  if (w != NULL)
    *w = 0;

  l = strlen(d);
  if ( l < 8U )
    return d;

  w = d + l - 1;
  ch = *w;
  switch (ch) {
  case '}':
    w = strrchr(d, '{');
    if (w != NULL)
      *w = 0;
    break;
  case ')':
    w = strrchr(d, '(');
    if (w != NULL)
      *w = 0;
    break;
  case ']':
    w = strrchr(d, '[');
    if (w != NULL)
      *w = 0;
    break;
  }

  /* strip numbers */
  x = d;
  w = d;
  for (;;) {
    ch = *(x++);
    *w = ch;
    if (ch == 0)
       break;
    if (isalpha(ch))
       ++w;
  }
  return d;
}

static char *file_to_dir(const char *s)
{
  char *d;
  char *w;

  d = mystrdup(s);
  /* ignore file */
  w = strrchr(d, '/');
  if (w != NULL)
    w[0] = 0;

  return d;
}

static unsigned int group_is_restricted(const userinput * const u, const char *group)
{
  if (u->method != method_dcc)
    return 0;

  if (u->chat->groups == NULL)
    return 0;

  if (!group)
    return 0;

  if (verify_group_in_grouplist(group, u->chat->groups))
    return 0;

  return 1;
}

static unsigned int invalid_group(const userinput * const u, char *arg)
{
  if (!arg || (arg[0] == 0)) {
    a_respond(u, "Try Specifying a Group");
    return 1;
  }

  if (group_is_restricted(u, arg)) {
    a_respond(u, "You don't have access to this group");
    return 1;
  }

  return 0;
}

static unsigned int invalid_text(const userinput * const u, const char *text, const char *arg)
{
  if (!arg || (arg[0] == 0)) {
    a_respond(u, "%s", text);
    return 1;
  }
  return 0;
}

static unsigned int invalid_dir(const userinput * const u, char *arg)
{
  if (invalid_text(u, "Try Specifying a Directory", arg))
    return 1;

  convert_to_unix_slash(arg);
  return 0;
}

#ifdef USE_CURL
static unsigned int is_upload_file(const userinput * const u, const char *arg)
{
  if (file_uploading(arg)) {
    a_respond(u, "Upload still running");
    return 1;
  }
  return 0;
}
#endif /* USE_CURL */

static unsigned int invalid_file(const userinput * const u, char *arg)
{
  if (invalid_text(u, "Try Specifying a Filename", arg))
    return 1;

  convert_to_unix_slash(arg);
  return 0;
}

static unsigned int invalid_command(const userinput * const u, char *arg)
{
  if (invalid_text(u, "Try Specifying a Command", arg))
    return 1;

  clean_quotes(arg);
  return 0;
}

unsigned int invalid_channel(const userinput * const u, const char *arg)
{
  return invalid_text(u, "Try Specifying a Channel", arg);
}

static void a_respond_badid(const userinput * const u, const char *cmd)
{
  a_respond(u, "Invalid ID number, Try \"%s\" for a list", cmd);
}

static transfer *get_transfer_by_number(const userinput * const u, unsigned int num)
{
  transfer *tr;

  tr = does_tr_id_exist(num);
  if (tr == NULL) {
    a_respond_badid(u, "DCL"); /* NOTRANSLATE */
  }
  return tr;
}

static transfer *get_transfer_by_arg(const userinput * const u, const char *arg)
{
  unsigned int num = 0;

  if (arg) num = atoi(arg);
  return get_transfer_by_number(u, num);
}

static unsigned int invalid_pack(const userinput * const u, unsigned int num)
{
  if (num == 0 || num > irlist_size(&gdata.xdccs)) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return 1;
  }
  return 0;
}

unsigned int get_pack_nr(const userinput * const u, const char *arg)
{
  unsigned int num;

  if (!arg || (arg[0] == 0))
    return 0;

  num = packnumtonum(arg);
  if (num == XDCC_SEND_LIST) {
    init_xdcc_file(&xdcc_listfile, gdata.xdcclistfile);
    return num;
  }
  if (invalid_pack(u, num) != 0)
    return 0;
  return num;
}

static unsigned int get_pack_nr1(const userinput * const u, const char *arg)
{
  unsigned int num;

  if (!arg || (arg[0] == 0))
    return 0;

  num = packnumtonum(arg);
  if (invalid_pack(u, num) != 0)
    return 0;
  return num;
}

static unsigned int get_pack_nr2(const userinput * const u, const char *arg, unsigned int num1)
{
  unsigned int num2 = num1;

  if (arg) {
    num2 = get_pack_nr1(u, arg);
    if (num2 == 0)
      return num2;
  }

  if ( num2 < num1 ) {
    a_respond(u, "Pack numbers are not in order");
    return 0;
  }
  return num2;
}

int get_network_msg(const userinput * const u, const char *arg)
{
  int net;

  net = get_network(arg);
  if (net < 0)
    a_respond(u, "Try Specifying a Valid Network");
  return net;
}

static int disabled_config(const userinput * const u)
{
  if (gdata.direct_file_access == 0) {
    a_respond(u, "Disabled in Config");
    return 1;
  }
  return 0;
}

static int group_hidden(const userinput * const u, xdcc *xd)
{
  if (u->method != method_dcc)
    return 0;

  if (u->chat->groups == NULL)
    return 0;

  if (!xd)
    return 1;

  if (verify_group_in_grouplist(xd->group, u->chat->groups))
    return 0;

  return 1;
}

unsigned int group_restricted(const userinput * const u, xdcc *xd)
{
  if (group_hidden(u, xd) == 0)
    return 0;

  a_respond(u, "You don't have access to this group");
  return 1;
}

static unsigned int queue_host_remove(const userinput * const u, irlist_t *list, const char *hostmask)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  char *hostmask2;
  unsigned int changed = 0;

  for (pq = irlist_get_head(list); pq;) {
    hostmask2 = to_hostmask(pq->nick, pq->hostname);
    if (fnmatch(hostmask, hostmask2, FNM_CASEFOLD)) {
      pq = irlist_get_next(pq);
      mydelete(hostmask2);
      continue;
    }
    mydelete(hostmask2);

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick,
                "Removed from the queue for \"%s\"", pq->xpack->desc);
    gnetwork = backup;
    ioutput(OUT_L, COLOR_YELLOW,
            "Removed from the queue for \"%s\"", pq->xpack->desc);
    a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    ++changed;
  }

  return changed;
}

static unsigned int queue_nick_remove(const userinput * const u, irlist_t *list, unsigned int network, const char *nick)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  unsigned int changed = 0;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick,
                "Removed from the queue for \"%s\"", pq->xpack->desc);
    gnetwork = backup;
    ioutput(OUT_L, COLOR_YELLOW,
            "Removed from the queue for \"%s\"", pq->xpack->desc);
    a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    ++changed;
  }

  return changed;
}

static void a_cancel_transfers(xdcc *xd, const char *msg)
{
  transfer *tr;

  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status == TRANSFER_STATUS_DONE)
      continue;

    if (tr->xpack != xd)
      continue;

    t_closeconn(tr, msg, 0);
  }
}

static void write_files_changed(void)
{
  write_files();
  get_xdcc_pack(0);
#ifdef USE_RUBY
  do_myruby_packlist();
#endif /* USE_RUBY */
}

static unsigned int a_remove_pack2(const userinput * const u, xdcc *xd, unsigned int num)
{
  char *tmpdesc;
  char *tmpgroup;

  updatecontext();

  if (group_hidden(u, xd))
    return 1; /* failed */

  write_removed_xdcc(xd);
  a_cancel_transfers(xd, "Pack removed");
  queue_pack_remove(&gdata.mainqueue, xd);
  queue_pack_remove(&gdata.idlequeue, xd);

  a_respond(u, "Removed Pack %u [%s]", num, xd->desc);

  cancel_md5_hash(xd, "REMOVE");

  mydelete(xd->file);
  mydelete(xd->desc);
  mydelete(xd->note);
  mydelete(xd->lock);
  mydelete(xd->dlimit_desc);
  mydelete(xd->trigger);
  /* keep group info for later work */
  tmpgroup = xd->group;
  xd->group = NULL;
  tmpdesc = xd->group_desc;
  xd->group_desc = NULL;
  irlist_delete(&gdata.xdccs, xd);

  if (tmpdesc != NULL) {
    if (tmpgroup != NULL)
      reorder_new_groupdesc(tmpgroup, tmpdesc);
    mydelete(tmpdesc);
  }
  if (tmpgroup != NULL)
    mydelete(tmpgroup);

  return 0;
}

static void a_remove_pack_final(void)
{
  set_support_groups();
  autotrigger_rebuild();
  write_files_changed();
}

static unsigned int a_remove_pack(const userinput * const u, xdcc *xd, unsigned int num)
{
  updatecontext();

  if (a_remove_pack2(u, xd, num))
    return 1; /* failed */

  a_remove_pack_final();
  return 0;
}

void a_remove_delayed(const userinput * const u)
{
  struct stat *st;
  xdcc *xd;
  gnetwork_t *backup;
  unsigned int n;

  updatecontext();

  backup = gnetwork;
  st = (struct stat *)(u->arg2);
  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs); xd; ) {
    ++n;
    if ((xd->st_dev == st->st_dev) &&
        (xd->st_ino == st->st_ino)) {
      gnetwork = &(gdata.networks[u->net]);
      if (a_remove_pack(u, xd, n) == 0) {
        break;
      }
    }
    xd = irlist_get_next(xd);
  }
  gnetwork = backup;
}

static void a_respond_old_new(const userinput * const u, const char *prefix, unsigned int num, const char *oldtext, const char *newtext)
{
  a_respond(u, "%s: [Pack %u] Old: %s New: %s", prefix, num, oldtext, newtext);
}

static unsigned int a_set_group(const userinput * const u, xdcc *xd, unsigned int num, const char *group)
{
  const char *newgroup;
  char *tmpdesc;
  char *tmpgroup;
  unsigned int rc;

  updatecontext();

  if (num == 0) num = number_of_pack(xd);
  newgroup = "MAIN"; /* NOTRANSLATE */
  if (group && (group[0] != 0))
    newgroup = group;

  if (xd->group != NULL) {
    a_respond_old_new(u, "GROUP", num, xd->group, newgroup); /* NOTRANSLATE */
    /* keep group info for later work */
    tmpgroup = xd->group;
    xd->group = NULL;
    tmpdesc = xd->group_desc;
    xd->group_desc = NULL;
    if (tmpdesc != NULL) {
      if (tmpgroup != NULL)
        reorder_new_groupdesc(tmpgroup, tmpdesc);
      mydelete(tmpdesc);
    }
    if (tmpgroup != NULL)
      mydelete(tmpgroup);
  } else {
    a_respond(u, "%s: [Pack %u] New: %s",
              "GROUP", num, newgroup); /* NOTRANSLATE */
  }

  if (group != newgroup)
    return 0;

  xd->group = mystrdup(group);
  reorder_groupdesc(group);
  rc = add_default_groupdesc(group);
  if (rc != 0)
    a_respond(u, "New GROUPDESC: %s", group);
  set_support_groups();
  return rc;
}

static unsigned int a_access_file(const userinput * const u, int xfiledescriptor, char **file, struct stat *st)
{
  if (xfiledescriptor < 0) {
    a_respond(u, "Cant Access File: %s: %s", *file, strerror(errno));
    mydelete(*file);
    return 1;
  }

  if (fstat(xfiledescriptor, st) < 0) {
    a_respond(u, "Cant Access File Details: %s: %s", *file, strerror(errno));
    close(xfiledescriptor);
    mydelete(*file);
    return 1;
  }
  close(xfiledescriptor);

  if (!S_ISREG(st->st_mode)) {
    a_respond(u, "%s is not a file", *file);
    mydelete(*file);
    return 1;
  }
  return 0;
}

static int a_sort_type(const char *str1, const char *str2)
{
  if (gdata.no_natural_sort)
    return strcasecmp(str1, str2);
  return strnatcasecmp((const unsigned char *)str1, (const unsigned char *)str2);
}

static int a_sort_null(const char *str1, const char *str2)
{
  if ((str1 == NULL) && (str2 == NULL))
    return 0;

  if (str1 == NULL)
    return -1;

  if (str2 == NULL)
    return 1;

  return a_sort_type(str1, str2);
}

static int a_sort_cmp(const char *k, xdcc *xd1, xdcc *xd2)
{
  xdcc *xd3 = xd1;
  xdcc *xd4 = xd2;
  int rc = 0;

  while (k != NULL) {
    switch ( *k ) {
    case '-':
      xd3 = xd2;
      xd4 = xd1;
      ++k;
      continue;
    case '+':
      xd3 = xd1;
      xd4 = xd2;
      ++k;
      break;
    case 'D':
    case 'd':
      rc = a_sort_type(xd3->desc, xd4->desc);
      if (rc != 0)
        return rc;
      break;
    case 'P':
    case 'p':
      rc = a_sort_type(xd3->file, xd4->file);
      if (rc != 0)
        return rc;
      break;
    case 'G':
    case 'g':
      rc = a_sort_null(xd3->group, xd4->group);
      if (rc != 0)
        return rc;
      break;
    case 'B':
    case 'b':
    case 'S':
    case 's':
      if (xd3->st_size < xd4->st_size) return -1;
      if (xd3->st_size > xd4->st_size) return 1;
      break;
    case 'T':
    case 't':
      if (xd3->mtime < xd4->mtime) return -1;
      if (xd3->mtime > xd4->mtime) return 1;
      break;
    case 'A':
    case 'a':
      if (xd3->xtime < xd4->xtime) return -1;
      if (xd3->xtime > xd4->xtime) return 1;
      break;
    case 'N':
    case 'n':
    case 'F':
    case 'f':
      rc = a_sort_type(getfilename(xd3->file), getfilename(xd4->file));
      if (rc != 0)
        return rc;
      break;
    }
    k = strchr(k, ' ');
    if (k == NULL)
      break;
    xd3 = xd1;
    xd4 = xd2;
    ++k;
  }
  return rc;
}

static void a_sort_insert(xdcc *xdo, const char *k)
{
  xdcc *xdn;
  xdcc *xdl;
  char *tmpdesc;

  xdl = NULL;
  for (xdn = irlist_get_head(&gdata.xdccs);
       xdn;
       xdn = irlist_get_next(xdn)) {
    if (a_sort_cmp(k, xdo, xdn) < 0)
      break;
    xdl = xdn;
  }
  if (xdl != NULL) {
    irlist_insert_after(&gdata.xdccs, xdo, xdl);
  } else {
    irlist_insert_head(&gdata.xdccs, xdo);
  }

  if (xdo->group != NULL) {
    if (xdo->group_desc != NULL) {
      tmpdesc = xdo->group_desc;
      xdo->group_desc = NULL;
      reorder_new_groupdesc(xdo->group, tmpdesc);
      mydelete(tmpdesc);
    } else {
      reorder_groupdesc(xdo->group);
    }
  }
}

static void a_make_announce(const char *cmd, unsigned int n)
{
  userinput *ui;
  char *tempstr;

  tempstr = mymalloc (maxtextlength);
  ui = mycalloc(sizeof(userinput));
  snprintf(tempstr, maxtextlength, "%s %u", cmd, n); /* NOTRANSLATE */
  a_fillwith_msg2(ui, NULL, tempstr);
  ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
  ui->net = 0;
  ui->level = ADMIN_LEVEL_AUTO;
  u_parseit(ui);
  mydelete(ui);
  mydelete(tempstr);
}

static void a_filedel_disk(const userinput * const u, char **file)
{
  struct stat st;
  int xfiledescriptor;

  updatecontext();

  xfiledescriptor = a_open_file(file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_file(u, xfiledescriptor, file, &st))
    return;

  if (save_unlink(*file) < 0) {
    a_respond(u, "File %s could not be deleted: %s", *file, strerror(errno));
  } else {
    a_respond(u, "File %s was deleted.", *file);
  }
}

static xdcc *a_oldest_xdcc(void)
{
  xdcc *xd;
  xdcc *old;

  old = NULL;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (old != NULL) {
      if (old->xtime <= xd->xtime)
        continue;
    }
    old = xd;
  }
  return old;
}

static unsigned int a_get_color(const char *definition)
{
  char *last;
  char *back;
  char *style;
  unsigned int color = 0;
  unsigned int color_fg;
  unsigned int color_bg;
  unsigned int color_st;

  if (!definition)
    return color;

  last = mystrdup(definition);
  back = strchr(last, ',');
  if (back != 0) {
    *(back++) = 0;
    style = strchr(back, ',');
    if (style != 0) {
      *(style++) = 0;
      color_st = atoi(style);
      color |= color_st << 16;
    }
    color_bg = atoi(back);
    color |= color_bg << 8;
  }
  color_fg = atoi(last);
  color |= color_fg;
  mydelete(last);
  return color;
}

/* perform delayed announce */
void a_autoaddann(xdcc *xd, unsigned int pack)
{
#ifdef USE_RUBY
  do_myruby_added(xd->file, pack);
#endif /* USE_RUBY */

  if (no_verifyshell(&gdata.autoaddann_mask, getfilename(xd->file)))
    return;

  if (gdata.autoaddann_short)
    a_make_announce("SANNOUNCE", pack); /* NOTRANSLATE */

  if (gdata.autoaddann)
    a_make_announce("ANNOUNCE", pack); /* NOTRANSLATE */
}

static unsigned int check_for_renamed_file(const userinput * const u, xdcc *xd, struct stat *st, char *file)
{
  char *old;
  int xfiledescriptor;

  updatecontext();

  if (strcmp(xd->file, file) == 0)
    return 0; /* same name */

  old = mystrdup(xd->file);
  xfiledescriptor = a_open_file(&old, O_RDONLY | ADDED_OPEN_FLAGS);
  mydelete(old);
  if (xfiledescriptor >= 0) {
    close(xfiledescriptor);
    return 0; /* hardlinked */
  }

  if (errno != ENOENT)
    return 0; /* other error */

  if (xd->st_size != st->st_size)
    return 0; /* size differs */

  if (xd->mtime != st->st_mtime)
    return 0; /* mtime differs */

  if (xd->st_dev != st->st_dev)
    return 0; /* dev differs */

  if (xd->st_ino != st->st_ino)
    return 0; /* ino differs */

  /* renamed */
  a_respond_old_new(u, "CHFILE", number_of_pack(xd), xd->file, file); /* NOTRANSLATE */
  old = xd->file;
  xd->file = file;
  /* change default description */
  if (strcmp(xd->desc, getfilename(old)) == 0) {
    mydelete(xd->desc);
    xd->desc = mystrdup(getfilename(xd->file));
  }
  mydelete(old);
  return 1;
}

static xdcc *a_add2(const userinput * const u, const char *group)
{
  struct stat st;
  xdcc *xd;
  autoadd_group_t *ag;
  char *file;
  char *a1;
  char *a2;
  const char *newfile;
  int xfiledescriptor;
  unsigned int n;

  updatecontext();

  if (invalid_file(u, u->arg1) != 0)
    return NULL;

  file = mystrdup(u->arg1);

  xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_fstat(u, xfiledescriptor, &file, &st))
    return NULL;

  if (gdata.noduplicatefiles) {
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if ((xd->st_dev == st.st_dev) &&
          (xd->st_ino == st.st_ino)) {
        if (check_for_renamed_file(u, xd, &st, file))
          return NULL;
        a_respond(u, "File '%s' is already added.", u->arg1);
        mydelete(file);
        return NULL;
      }
    }
  }

  newfile = getfilename(file);
  if (gdata.no_duplicate_filenames) {
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      if (strcasecmp(getfilename(xd->file), newfile) == 0) {
        a_respond(u, "File '%s' is already added.", u->arg1);
        mydelete(file);
        return NULL;
      }
    }
  }

  if (gdata.disk_quota) {
    off_t usedbytes = st.st_size;
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      usedbytes += xd->st_size;
    }
    if (usedbytes >= gdata.disk_quota) {
      a_respond(u, "File '%s' not added, you reached your quoata", u->arg1);
      mydelete(file);
      return NULL;
    }
  }

  if (gdata.adddir_min_size) {
    if (st.st_size < (off_t)gdata.adddir_min_size) {
      a_respond(u, "File '%s' not added, to small", u->arg1);
      mydelete(file);
      return NULL;
    }
  }

  if ((gdata.auto_default_group) && (group == NULL)) {
    a1 = file_without_numbers(newfile);
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      a2 = file_without_numbers(xd->file);
      if (!strcmp(a1, a2)) {
        group = xd->group;
        mydelete(a2);
        break;
      }
      mydelete(a2);
    }
    mydelete(a1);
  }
  if ((gdata.auto_path_group) && (group == NULL)) {
    a1 = file_to_dir(file);
    for (xd = irlist_get_head(&gdata.xdccs);
         xd;
         xd = irlist_get_next(xd)) {
      a2 = file_to_dir(xd->file);
      if (!strcmp(a1, a2)) {
        group = xd->group;
        mydelete(a2);
        break;
      }
      mydelete(a2);
    }
    mydelete(a1);
  }
  if (group == NULL) {
    for (ag = irlist_get_head(&gdata.autoadd_group_match);
         ag;
         ag = irlist_get_next(ag)) {
       if (fnmatch(ag->a_pattern, file, FNM_CASEFOLD) != 0)
         continue;

       group = ag->a_group;
       break;
    }
  }

  if (gdata.fileremove_max_packs) {
    char *filename;

    while (gdata.fileremove_max_packs <= irlist_size(&gdata.xdccs)) {
      xd = a_oldest_xdcc();
      if (xd == NULL)
        break;

      filename = mystrdup(xd->file);
      if (a_remove_pack(u, xd, number_of_pack(xd)) == 0)
        a_filedel_disk(u, &filename);
      mydelete(filename);
    }
  }

  xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
  xd->xtime = gdata.curtime;

  xd->file = file;

  xd->desc = mystrdup(newfile);

  xd->minspeed = gdata.transferminspeed;
  xd->maxspeed = gdata.transfermaxspeed;

  xd->st_size  = st.st_size;
  xd->st_dev   = st.st_dev;
  xd->st_ino   = st.st_ino;
  xd->mtime    = st.st_mtime;

  xd->file_fd = FD_UNUSED;

  n = irlist_size(&gdata.xdccs);
  if (gdata.autoadd_sort != NULL) {
    /* silently set the group for sorting */
    if (group != NULL) {
      xd->group = mystrdup(group);
    }
    irlist_remove(&gdata.xdccs, xd);
    a_sort_insert(xd, gdata.autoadd_sort);
    n = number_of_pack(xd);
    mydelete(xd->group);
  }

  a_respond(u, "Added: [Pack %u] [File %s] Use CHDESC to change description",
            n, xd->file);

  if (group != NULL) {
    a_set_group(u, xd, n, group);
  }

  set_support_groups();
  xd->color = a_get_color(gdata.autoadd_color);
  ++(xd->announce);
  write_files_changed();
  return xd;
}

void a_add_delayed(const userinput * const u)
{
  userinput u2;

  updatecontext();

  a_respond(u, "  Adding %s:", u->arg1);

  u2 = *u;
  u2.arg1 = u->arg1;
  a_add2(&u2, u->arg3);
}

void a_xdlock(const userinput * const u)
{
  xdcc *xd;
  char *tempstr;
  unsigned int i;
  unsigned int l;
  unsigned int s;

  updatecontext();

  tempstr  = mymalloc(maxtextlength);

  l = a_xdl_left();
  s = a_xdl_space();
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++i;
    if (xd->lock == NULL)
      continue;

    a_xdl_pack(u, tempstr, i, l, s, xd);
    a_respond(u, " \2^-\2%*sPassword: %s", s, "", xd->lock);
  }

  mydelete(tempstr);
}

void a_xdtrigger(const userinput * const u)
{
  xdcc *xd;
  char *tempstr;
  unsigned int i;
  unsigned int l;
  unsigned int s;

  updatecontext();

  tempstr  = mymalloc(maxtextlength);

  l = a_xdl_left();
  s = a_xdl_space();
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++i;
    if (xd->trigger == NULL)
      continue;

    if (group_hidden(u, xd))
      continue;

    a_xdl_pack(u, tempstr, i, l, s, xd);
    a_respond(u, " \2^-\2%*sTrigger: %s", s, "", xd->trigger);
  }

  mydelete(tempstr);
}

void a_find(const userinput * const u)
{
  xdcc *xd;
  char *match;
  char *tempstr;
  unsigned int i;
  unsigned int k;
  unsigned int l;
  unsigned int s;

  updatecontext();

  if (invalid_text(u, "Try Specifying a Pattern", u->arg1e))
    return;

  tempstr = mymalloc(maxtextlength);
  clean_quotes(u->arg1e);
  match = grep_to_fnmatch(u->arg1e);
  l = a_xdl_left();
  s = a_xdl_space();
  k = 0;
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++i;
    if (group_hidden(u, xd))
      continue;

    if (fnmatch_xdcc(match, xd)) {
      ++k;
      a_xdl_pack(u, tempstr, i, l, s, xd);
      /* limit matches */
      if ((gdata.max_find != 0) && (k >= gdata.max_find))
        break;
    }
  }

  if (!k)
    a_respond(u, "Sorry, nothing was found, try a XDCC LIST" );

  mydelete(match);
  mydelete(tempstr);
}

void a_xds(const userinput * const UNUSED(u))
{
  updatecontext();
  write_files();
}

static void a_qul2(const userinput * const u, irlist_t *list)
{
  ir_pqueue *pq;
  unsigned long rtime;
  unsigned int i;

  updatecontext();

  a_respond(u, "    #  User        Pack File                              Waiting     Left");
  i = 0;
  pq = irlist_get_head(list);
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    ++i;
    rtime = get_next_transfer_time();
    add_new_transfer_time(pq->xpack);
    if (rtime < 359999U) {
       a_respond(u, "   %2i  %-9s   %-4d %-32s   %2lih%2lim   %2lih%2lim",
                 i,
                 pq->nick,
                 number_of_pack(pq->xpack),
                 getfilename(pq->xpack->file),
                 (long)((gdata.curtime-pq->queuedtime)/60/60),
                 (long)(((gdata.curtime-pq->queuedtime)/60)%60),
                 (long)(rtime/60/60),
                 (long)(rtime/60)%60);
     } else {
       a_respond(u, "   %2i  %-9s   %-4d %-32s   %2lih%2lim  Unknown",
                 i,
                 pq->nick,
                 number_of_pack(pq->xpack),
                 getfilename(pq->xpack->file),
                 (long)((gdata.curtime-pq->queuedtime)/60/60),
                 (long)(((gdata.curtime-pq->queuedtime)/60)%60));
     }
  }
}

void a_qul(const userinput * const u)
{
  updatecontext();

  if (!irlist_size(&gdata.mainqueue) && !irlist_size(&gdata.idlequeue)) {
    a_respond(u, "No Users Queued");
    return;
  }

  guess_end_transfers();
  a_respond(u, "Current Main Queue:");
  a_qul2(u, &gdata.mainqueue);
  a_respond(u, "Current Idle Queue:");
  a_qul2(u, &gdata.idlequeue);
  guess_end_cleanup();
}

static char *a_valid_uploaddir(const userinput * const u)
{
  char *uploaddir;

  uploaddir = get_uploaddir(u->hostmask);
  if (uploaddir == NULL)
    a_respond(u, "No uploaddir defined.");
  return uploaddir;
}

void a_diskfree(const userinput * const u)
{
  char *uploaddir;

  updatecontext();

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  u_diskinfo(u, uploaddir);
}

/* returns true if dir use parent */
static unsigned int is_unsave_directory(const char *dir)
{
  char *line;
  char *word;
  unsigned int bad = 0;

  /* no device letters */
  if (strchr(dir, ':'))
    return 1;

  line = mystrdup(dir);
  for (word = strtok(line, "/"); /* NOTRANSLATE */
       word;
       word = strtok(NULL, "/")) { /* NOTRANSLATE */
    if (strcmp(word, "..") == 0) { /* NOTRANSLATE */
      ++bad;
      break;
    }
  }
  mydelete(line);
  return bad;
}

void a_listul(const userinput * const u)
{
  char *tempstr;
  char *uploaddir;

  updatecontext();

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  if (u->arg1 == NULL) {
    u_listdir(u, uploaddir);
    return;
  }

  clean_quotes(u->arg1);
  convert_to_unix_slash(u->arg1);

  if (is_unsave_directory(u->arg1)) {
    a_respond(u, "Try Specifying a Directory");
    return;
  }

  tempstr = mystrjoin(uploaddir, u->arg1, '/');
  u_listdir(u, tempstr);
  mydelete(tempstr);
}

void a_nomin(const userinput * const u)
{
  transfer *tr;

  updatecontext();

  tr = get_transfer_by_arg(u, u->arg1);
  if (tr == NULL)
    return;

  tr->nomin = 1;
}

void a_nomax(const userinput * const u)
{
  transfer *tr;

  updatecontext();

  tr = get_transfer_by_arg(u, u->arg1);
  if (tr == NULL)
    return;

  tr->nomax = 1;
}

void a_unlimited(const userinput * const u)
{
  transfer *tr;

  updatecontext();

  tr = get_transfer_by_arg(u, u->arg1);
  if (tr == NULL)
    return;

  tr->nomax = 1;
  tr->unlimited = 1;
}

void a_maxspeed(const userinput * const u)
{
  transfer *tr;
  float val;
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  tr = get_transfer_by_number(u, num);
  if (tr == NULL)
    return;

  if (invalid_text(u, "Try Specifying a Maxspeed", u->arg2))
    return;

  val = atof(u->arg2);
  a_respond(u, "%s: [Transfer %u] Old: %1.1f New: %1.1f",
            u->cmd, num, tr->maxspeed, val);
  tr->maxspeed = val;
}

static int a_qsend_queue(const userinput * const u, irlist_t *list)
{
  if (irlist_size(list) == 0) {
    a_respond(u, "No Users Queued");
    return 1;
  }

  if (irlist_size(&gdata.trans) >= gdata.maxtrans) {
    a_respond(u, "Too many transfers");
    return 1;
  }
  return 0;
}

void a_qsend(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (a_qsend_queue(u, &gdata.mainqueue))
    return;

  if (u->arg1) num = atoi(u->arg1);
  send_from_queue(2, num, NULL);
}

void a_iqsend(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (a_qsend_queue(u, &gdata.idlequeue))
    return;

  if (u->arg1) num = atoi(u->arg1);
  check_idle_queue(num);
}

void a_slotsmax(const userinput * const u)
{
  unsigned int val;

  updatecontext();

  if (u->arg1) {
    val = atoi(u->arg1);
    gdata.slotsmax = between(1, val, gdata.maxtrans);
  }
  a_respond(u, "SLOTSMAX now %u", gdata.slotsmax);
}

void a_queuesize(const userinput * const u)
{
  unsigned int val;

  updatecontext();

  if (u->arg1) {
    val = atoi(u->arg1);
    gdata.queuesize = between(0, val, 1000000);
  }
  a_respond(u, "QUEUESIZE now %u", gdata.queuesize);
}

static void a_requeue2(const userinput * const u, irlist_t *list)
{
  unsigned int oldp = 0, newp = 0;
  ir_pqueue *pqo;
  ir_pqueue *pqn;

  updatecontext();

  if (u->arg1) oldp = atoi(u->arg1);
  if (u->arg2) newp = atoi(u->arg2);

  if ((oldp < 1) ||
      (oldp > irlist_size(list)) ||
      (newp < 1) ||
      (newp > irlist_size(list)) ||
      (newp == oldp)) {
    a_respond(u, "Invalid Queue Entry");
    return;
  }

  a_respond(u, "** Moved Queue %u to %u", oldp, newp);

  /* get queue we are renumbering */
  pqo = irlist_get_nth(list, oldp - 1);
  irlist_remove(list, pqo);

  if (newp == 1) {
    irlist_insert_head(list, pqo);
  } else {
    pqn = irlist_get_nth(list, newp - 2);
    irlist_insert_after(list, pqo, pqn);
  }
}

void a_requeue(const userinput * const u)
{
  a_requeue2(u, &gdata.mainqueue);
}

void a_reiqueue(const userinput * const u)
{
  a_requeue2(u, &gdata.idlequeue);
}

static userinput *irlist_add_delayed(const userinput * const u, const char *cmd)
{
  userinput *u2;

  u2 = irlist_add(&gdata.packs_delayed, sizeof(userinput));
  u2->method = u->method;
  u2->fd = u->fd;
  u2->chat = u->chat;
  u2->cmd = mystrdup(cmd);
  u2->net = u->net;
  u2->level = u->level;

  if (u->snick != NULL) {
    u2->snick = mystrdup(u->snick);
  }
  return u2;
}

static unsigned int is_system_dir(const char *name)
{
  if (strcmp(name, ".") == 0) /* NOTRANSLATE */
    return 1;
  if (strcmp(name, "..") == 0) /* NOTRANSLATE */
    return 1;
  return 0;
}

static int a_readdir_sub(const userinput * const u, const char *thedir, DIR *dirp, struct dirent *entry, struct dirent **result)
{
  int rc = 0;
  unsigned int max = 3;

  for (max = 3; max > 0; --max) {
    rc = readdir_r(dirp, entry, result);
    if (rc == 0)
      break;

    a_respond(u, "Error Reading Directory %s: %s", thedir, strerror(errno));
    if (rc != EAGAIN)
      break;
  }
  return rc;
}

static void a_removedir_sub(const userinput * const u, const char *thedir, DIR *d, const char *match)
{
  struct dirent f2;
  struct dirent *f;
  char *tempstr;
  userinput *u2;

  updatecontext();

  if (d == NULL)
    d = opendir(thedir);

  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", thedir, strerror(errno));
    return;
  }

  for (;;) {
    struct stat st;

    if (a_readdir_sub(u, thedir, d, &f2, &f) != 0) {
       break;
    }

    if (f == NULL)
      break;

    tempstr = mystrjoin(thedir, f->d_name, '/');
    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access '%s', ignoring: %s",
                tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if (is_system_dir(f->d_name)) {
        mydelete(tempstr);
        continue;
      }
      if (gdata.include_subdirs != 0)
        a_removedir_sub(u, tempstr, NULL, match);
      mydelete(tempstr);
      continue;
    }
    if (!S_ISREG(st.st_mode)) {
      mydelete(tempstr);
      continue;
    }

    if (match != NULL) {
      if (fnmatch(match, f->d_name, FNM_CASEFOLD)) {
        mydelete(tempstr);
        continue;
      }
    }

    u2 = irlist_add_delayed(u, "REMOVE"); /* NOTRANSLATE */

    u2->arg1 = tempstr;
    tempstr = NULL;

    u2->arg2 = mycalloc(sizeof(struct stat));
    memcpy(u2->arg2, &st, sizeof(struct stat));
  }

  closedir(d);
  return;
}

void a_remove(const userinput * const u)
{
  xdcc *xd;
  unsigned int num1;
  unsigned int num2;
  unsigned int found;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  found = 0;
  for (; num2 >= num1; --num2) {
    xd = irlist_get_nth(&gdata.xdccs, num2 - 1);
    if (group_restricted(u, xd))
      return;

    a_remove_pack2(u, xd, num2);
    ++found;
  }
  if (found)
    a_remove_pack_final();
}

static DIR *a_open_dir(char **dir)
{
  DIR *d;
  char *adir;
  char *path;
  unsigned int n;

  strip_trailing_path(*dir);

  d = opendir(*dir);
  if ((d != NULL) || (errno != ENOENT))
    return d;

  if (errno != ENOENT)
    return d;

  n = irlist_size(&gdata.filedir);
  if (n == 0)
    return NULL;

  for (adir = irlist_get_head(&gdata.filedir);
       adir;
       adir = irlist_get_next(adir)) {
    path = mystrjoin(adir, *dir, '/');
    d = opendir(path);
    if ((d != NULL) || (errno != ENOENT)) {
      mydelete(*dir);
      *dir = path;
      return d;
    }
    mydelete(path);
  }
  return NULL;
}

void a_removedir(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
   return;

  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg1, strerror(errno));
    return;
  }

  a_removedir_sub(u, thedir, d, NULL);
  mydelete(thedir);
  return;
}

void a_removegroup(const userinput * const u)
{
  xdcc *xd;
  unsigned int n;
  unsigned int found;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  found = 0;
  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    ++n;
    if (xd->group != NULL) {
      if (strcasecmp(xd->group, u->arg1) == 0) {
        if (a_remove_pack2(u, xd, n) == 0) {
          /* continue with new pack at same position */
          ++found;
          --n;
          xd = irlist_get_nth(&gdata.xdccs, n);
          continue;
        }
      }
    }
    xd = irlist_get_next(xd);
  }
  if (found)
    a_remove_pack_final();
}

void a_removematch(const userinput * const u)
{
  DIR *d;
  char *thedir;
  char *end;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  thedir = mystrdup(u->arg1);
  end = strrchr(thedir, '/' );
  if (end == NULL) {
    a_respond(u, "Try Specifying a Directory");
    mydelete(thedir);
    return;
  }

  *(end++) = 0;
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg1, strerror(errno));
    mydelete(thedir);
    return;
  }

  a_removedir_sub(u, thedir, d, end);
  mydelete(thedir);
}

void a_removelost(const userinput * const u)
{
  xdcc *xd;
  unsigned int n;
  unsigned int found;

  updatecontext();

  found = 0;
  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    ++n;
    if (u->arg1 != NULL) {
      if (fnmatch(u->arg1, xd->file, FNM_CASEFOLD) != 0) {
        /* no match */
        xd = irlist_get_next(xd);
        continue;
      }
    }
    if (look_for_file_changes(xd) != 0) {
      if (a_remove_pack2(u, xd, n) == 0) {
        /* continue with new pack at same position */
        ++found;
        --n;
        xd = irlist_get_nth(&gdata.xdccs, n);
        continue;
      }
    }
    xd = irlist_get_next(xd);
  }
  if (found)
    a_remove_pack_final();
}

static void a_renumber1(const userinput * const u, unsigned int oldp, unsigned int newp)
{
  xdcc *xdo;
  xdcc *xdn;

  a_respond(u, "** Moved pack %u to %u", oldp, newp);
  /* get pack we are renumbering */
  xdo = irlist_get_nth(&gdata.xdccs, oldp - 1);
  irlist_remove(&gdata.xdccs, xdo);
  if (newp == 1) {
    irlist_insert_head(&gdata.xdccs, xdo);
  } else {
    xdn = irlist_get_nth(&gdata.xdccs, newp - 2);
    irlist_insert_after(&gdata.xdccs, xdo, xdn);
  }
  if (xdo->group != NULL)
    reorder_groupdesc(xdo->group);
}

void a_renumber3(const userinput * const u)
{
  unsigned int oldp;
  unsigned int endp;
  unsigned int newp;

  updatecontext();

  oldp = get_pack_nr1(u, u->arg1);
  if (oldp == 0)
    return;

  if (u->arg3) {
    endp = get_pack_nr2(u, u->arg2, oldp);
    if (endp == 0)
      return;

    newp = get_pack_nr1(u, u->arg3);
  } else {
    endp = oldp;
    newp = get_pack_nr1(u, u->arg2);
  }
  if (newp == 0)
    return;

  if ((newp >= oldp) && (newp <= endp)) {
    a_respond(u, "Pack numbers are not in order");
    return;
  }

  while (oldp <= endp) {
    if (invalid_pack(u, newp) != 0)
      break;

    a_renumber1(u, oldp, newp);

    if (oldp > newp) {
      ++oldp;
      ++newp;
    } else {
      --endp;
    }
  }

  write_files_changed();
}

void a_sort(const userinput * const u)
{
  irlist_t old_list;
  xdcc *xdo;
  const char *k = "filename"; /* NOTRANSLATE */

  updatecontext();

  if (irlist_size(&gdata.xdccs) == 0) {
    a_respond(u, "No packs to sort");
    return;
  }

  if (u->arg1e) k = u->arg1e;

  old_list = gdata.xdccs;
  /* clean start */
  gdata.xdccs.size = 0;
  gdata.xdccs.head = NULL;
  gdata.xdccs.tail = NULL;

  while (irlist_size(&old_list) > 0) {
    xdo = irlist_get_head(&old_list);
    irlist_remove(&old_list, xdo);

    a_sort_insert(xdo, k);
  }

  write_files_changed();
}

int a_open_file(char **file, int mode)
{
  char *adir;
  char *path;
  int xfiledescriptor;
  unsigned int n;

  xfiledescriptor = open(*file, mode);
  if ((xfiledescriptor >= 0) || (errno != ENOENT))
    return xfiledescriptor;

  if (errno != ENOENT)
    return xfiledescriptor;

  n = irlist_size(&gdata.filedir);
  if (n == 0)
    return -1;

  for (adir = irlist_get_head(&gdata.filedir);
       adir;
       adir = irlist_get_next(adir)) {
    path = mystrjoin(adir, *file, '/');
    xfiledescriptor = open(path, mode);
    if ((xfiledescriptor >= 0) || (errno != ENOENT)) {
      mydelete(*file);
      *file = path;
      return xfiledescriptor;
    }
    mydelete(path);
  }
  return -1;
}

unsigned int a_access_fstat(const userinput * const u, int xfiledescriptor, char **file, struct stat *st)
{
  if (a_access_file(u, xfiledescriptor, file, st))
    return 1;

  if ( st->st_size == 0 ) {
    a_respond(u, "File %s has size of 0 byte!", *file);
    mydelete(*file);
    return 1;
  }

  if ((st->st_size > gdata.max_file_size) || (st->st_size < 0)) {
    a_respond(u, "File %s is too large.", *file);
    mydelete(*file);
    return 1;
  }

  return 0;
}

void a_add(const userinput * const u)
{
  a_add2(u, NULL);
}

static unsigned int check_bad_filename(const char *filename)
{
  if (strchr(filename, '/') != NULL)
    return 1;

 if (strchr(filename, '\\') != NULL)
    return 1;

 return 0;
}

static void a_adddir_sub(const userinput * const u, const char *thedir, DIR *d, unsigned int onlynew, const char *setgroup, const char *match)
{
  userinput *u2;
  struct dirent f2;
  struct dirent *f;
  struct stat st;
  struct stat *sta;
  char *thefile, *tempstr;
  irlist_t dirlist = {0, 0, 0};

  updatecontext();

  if (d == NULL)
    d = opendir(thedir);

  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", thedir, strerror(errno));
    return;
  }

  for (;;) {
    xdcc *xd;
    unsigned int foundit;

    if (a_readdir_sub(u, thedir, d, &f2, &f) != 0) {
       break;
    }

    if (f == NULL)
      break;

    if (verifyshell(&gdata.adddir_exclude, f->d_name)) {
      if (gdata.debug > 21)
        a_respond(u, "  Ignoring adddir_exclude file: %s", f->d_name);
      continue;
    }

    tempstr = mystrjoin(thedir, f->d_name, '/');

    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access '%s', ignoring: %s",
                tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if (is_system_dir(f->d_name)) {
        mydelete(tempstr);
        continue;
      }
      if (gdata.include_subdirs == 0) {
        if (gdata.debug > 20)
          a_respond(u, "  Ignoring directory: %s", tempstr);
      } else {
        if (gdata.subdirs_delayed == 0 ) {
          a_adddir_sub(u, tempstr, NULL, onlynew, setgroup, match);
        } else {
          if (setgroup != NULL) {
            u2 = irlist_add_delayed(u, "ADDGROUP"); /* NOTRANSLATE */
            u2->arg1 = mystrdup(setgroup);
            u2->arg2 = mystrdup(tempstr);
          } else {
            u2 = irlist_add_delayed(u, "ADDNEW"); /* NOTRANSLATE */
            u2->arg1 = mystrdup(tempstr);
          }
        }
      }
      mydelete(tempstr);
      continue;
    }
    if (!S_ISREG(st.st_mode)) {
      mydelete(tempstr);
      continue;
    }
    if (match != NULL) {
      if (fnmatch(match, f->d_name, FNM_CASEFOLD)) {
        mydelete(tempstr);
        continue;
      }
    }

    if (no_verifyshell(&gdata.adddir_match, f->d_name)) {
      if (gdata.debug > 22)
        a_respond(u, "  Ignoring adddir_match file: %s", f->d_name);
      mydelete(tempstr);
      continue;
    }

    if (check_bad_filename(f->d_name)) {
      a_respond(u, "  Ignoring bad filename: %s", tempstr);
      mydelete(tempstr);
      continue;
    }
    if (file_uploading(f->d_name)) {
      a_respond(u, "  Ignoring upload file: %s", tempstr);
      mydelete(tempstr);
      continue;
    }
    if (file_uploading(tempstr)) {
      a_respond(u, "  Ignoring upload file: %s", tempstr);
      mydelete(tempstr);
      continue;
    }
    if (gdata.autoadd_delay) {
      if ((st.st_mtime) > gdata.curtime) {
        a_respond(u, "  Ignoring future date on file: %s", tempstr);
        mydelete(tempstr);
        continue;
      }
      if ((st.st_mtime + gdata.autoadd_delay) > (unsigned)gdata.curtime) {
        a_respond(u, "  Ignoring active file: %s", tempstr);
        mydelete(tempstr);
        continue;
      }
    }

    foundit = 0;
    if (onlynew != 0) {
      for (xd = irlist_get_head(&gdata.xdccs);
           xd;
           xd = irlist_get_next(xd)) {
        if ((xd->st_dev == st.st_dev) &&
            (xd->st_ino == st.st_ino)) {
          ++(foundit);
          if (check_for_renamed_file(u, xd, &st, tempstr)) {
            tempstr = NULL;
            break;
          }
          if (gdata.debug > 23)
            a_respond(u, "  Ignoring added file: %s", tempstr);
          break;
        }
      }
    }

    if (foundit == 0) {
      for (u2 = irlist_get_head(&gdata.packs_delayed);
           u2;
           u2 = irlist_get_next(u2)) {
        sta = (struct stat *)(u2->arg2);
        if ((strcmp(u2->cmd, "ADD") == 0) &&
            (sta->st_dev == st.st_dev) &&
            (sta->st_ino == st.st_ino)) {
          ++(foundit);
          a_respond(u, "  Ignoring queued file: %s", tempstr);
          break;
        }
      }
    }

    if (gdata.adddir_min_size) {
      if (st.st_size < (off_t)gdata.adddir_min_size) {
        a_respond(u, "  Ignoring small file: %s", tempstr);
        mydelete(tempstr);
        continue;
      }
    }

    if (foundit == 0) {
      irlist_add_string(&dirlist, tempstr);
    }
    mydelete(tempstr);
  }
  closedir(d);

  if (irlist_size(&dirlist) == 0)
    return;

  irlist_sort2(&dirlist, irlist_sort_cmpfunc_string);

  a_respond(u, "Adding %u files from dir %s",
            irlist_size(&dirlist), thedir);

  thefile = irlist_get_head(&dirlist);
  while (thefile) {
    u2 = irlist_add_delayed(u, "ADD"); /* NOTRANSLATE */

    u2->arg1 = mystrdup(thefile);

    if (stat(thefile, &st) == 0) {
      u2->arg2 = mycalloc(sizeof(struct stat));
      memcpy(u2->arg2, &st, sizeof(struct stat));
    }

    if (setgroup != NULL) {
      u2->arg3 = mystrdup(setgroup);
    } else {
      u2->arg3 = NULL;
    }

    thefile = irlist_delete(&dirlist, thefile);
  }
}

void a_addgroup(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_dir(u, u->arg2) != 0)
     return;

  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2);
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg2, strerror(errno));
    return;
  }

  a_adddir_sub(u, thedir, d, 1, u->arg1, NULL);
  mydelete(thedir);
}

void a_addmatch(const userinput * const u)
{
  DIR *d;
  char *thedir;
  char *end;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  thedir = mystrdup(u->arg1);
  end = strrchr(thedir, '/' );
  if (end == NULL) {
    a_respond(u, "Try Specifying a Directory");
    mydelete(thedir);
    return;
  }

  *(end++) = 0;
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg1, strerror(errno));
    mydelete(thedir);
    return;
  }

  a_adddir_sub(u, thedir, d, 1, NULL, end);
  mydelete(thedir);
}

static void a_newgroup_sub(const userinput * const u, const char *thedir, DIR *d, const char *group)
{
  struct dirent *f;
  struct stat st;
  char *tempstr;
  unsigned int num;
  unsigned int foundit = 0;

  updatecontext();

  if (d == NULL)
    d = opendir(thedir);

  if (d == NULL) {
    a_respond(u, "Can't Access Directory: %s %s", thedir, strerror(errno));
    return;
  }

  while ((f = readdir(d))) {
    xdcc *xd;

    if (verifyshell(&gdata.adddir_exclude, f->d_name))
      continue;

    tempstr = mystrjoin(thedir, f->d_name, '/');
    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access '%s', ignoring: %s",
                tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if (is_system_dir(f->d_name)) {
        mydelete(tempstr);
        continue;
      }
      a_respond(u, "  Ignoring directory: %s", tempstr);
      mydelete(tempstr);
      continue;
    }
    if (S_ISREG(st.st_mode)) {
      num = 0;
      for (xd = irlist_get_head(&gdata.xdccs);
           xd;
           xd = irlist_get_next(xd)) {
        ++num;
        if ((xd->st_dev == st.st_dev) &&
            (xd->st_ino == st.st_ino)) {
          ++foundit;
          a_set_group(u, xd, num, group);
          break;
        }
      }
    }
    mydelete(tempstr);
  }

  closedir(d);
  if (foundit == 0)
    return;

  write_files();
}

void a_newgroup(const userinput * const u)
{
  DIR *d = NULL;
  char *thedir;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (invalid_dir(u, u->arg2) != 0)
    return;

  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2);
  a_newgroup_sub(u, thedir, d, u->arg1);
  mydelete(thedir);
  return;
}

void a_chdesc(const userinput * const u)
{
  xdcc *xd;
  const char *newdesc;
  unsigned int num;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  newdesc = u->arg2e;
  if (!u->arg2e || (u->arg2e[0] == 0)) {
    newdesc = getfilename(xd->file);
    if (strcmp(newdesc, xd->desc) == 0) {
      a_respond(u, "Try Specifying a Description");
      return;
    }
  } else {
    clean_quotes(u->arg2e);
  }

  a_respond_old_new(u, u->cmd, num, xd->desc, newdesc);

  mydelete(xd->desc);
  xd->desc = mystrdup(newdesc);
  write_files_changed();
}

void a_chnote(const userinput * const u)
{
  unsigned int num;
  xdcc *xd;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num <= 0)
     return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  a_respond_old_new(u, u->cmd, num, xd->note, u->arg2e ? u->arg2e : "");

  mydelete(xd->note);
  if (u->arg2e == NULL) {
    xd->note = NULL;
  } else {
    clean_quotes(u->arg2e);
    xd->note = mystrdup(u->arg2e);
  }
  write_files_changed();
}

void a_chtime(const userinput * const u)
{
  struct tm tmval;
  xdcc *xd;
  const char *format;
  char *oldstr;
  char *newstr;
  time_t val = 0;
  unsigned int num;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  if (!u->arg2e || (u->arg2e[0] == 0)) {
    val = 0;
  } else {
    val = atoi(u->arg2e);
    if (val < 5000) {
      format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";
      bzero((char *)&tmval, sizeof(tmval));
      clean_quotes(u->arg2e);
      strptime(u->arg2e, format, &tmval);
      val = mktime(&tmval);
    }
  }

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  newstr = mymalloc(maxtextlengthshort);
  user_getdatestr(newstr, val, maxtextlengthshort);
  oldstr = mymalloc(maxtextlengthshort);
  user_getdatestr(oldstr, xd->xtime, maxtextlengthshort);
  a_respond_old_new(u, u->cmd, num, oldstr, newstr);
  mydelete(oldstr);
  mydelete(newstr);
  xd->xtime = val;
  write_files();
}

void a_chmins(const userinput * const u)
{
  xdcc *xd;
  char *last;
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  last = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    last = u->arg3;
  }
  if (invalid_text(u, "Try Specifying a Minspeed", last))
    return;

  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    xd->minspeed = gdata.transferminspeed;
    if ( atof(last) != gdata.transferminspeed )
      xd->minspeed = atof(last);
    a_respond(u, "%s: [Pack %u] Old: %1.1f New: %1.1f",
              u->cmd, num1, xd->minspeed, atof(last));
  }

  write_files();
}

void a_chmaxs(const userinput * const u)
{
  xdcc *xd;
  char *last;
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  last = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    last = u->arg3;
  }
  if (invalid_text(u, "Try Specifying a Maxspeed", last))
    return;

  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    xd->maxspeed = gdata.transfermaxspeed;
    if ( atof(last) != gdata.transfermaxspeed )
      xd->maxspeed = atof(last);
    a_respond(u, "%s: [Pack %u] Old: %1.1f New: %1.1f",
              u->cmd, num1, xd->maxspeed, atof(last));
  }

  write_files();
}

void a_chlimit(const userinput * const u)
{
  xdcc *xd;
  char *last;
  unsigned int num1;
  unsigned int num2;
  unsigned int val;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  last = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    last = u->arg3;
  }

  if (invalid_text(u, "Try Specifying a daily Downloadlimit", last))
    return;

  xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
  if (group_restricted(u, xd))
    return;

  val = atoi(last);
  for (; num1 <= num2; ++num1) {

    a_respond(u, "%s: [Pack %u] Old: %u New: %u",
              u->cmd, num1, xd->dlimit_max, val);

    xd->dlimit_max = val;
    if (val == 0)
      xd->dlimit_used = 0;
    else
      xd->dlimit_used = xd->gets + xd->dlimit_max;
  }

  write_files();
}

void a_chlimitinfo(const userinput * const u)
{
  unsigned int num;
  xdcc *xd;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  if (!u->arg2 || (u->arg2[0] == 0)) {
    a_respond(u, "%s: [Pack %u] removed", u->cmd, num);
    mydelete(xd->dlimit_desc);
    xd->dlimit_desc = NULL;
  } else {
    clean_quotes(u->arg2e);
    a_respond(u, "%s: [Pack %u] set: %s", u->cmd, num, u->arg2e);
    xd->dlimit_desc = mystrdup(u->arg2e);
  }

  write_files();
}

void a_chtrigger(const userinput * const u)
{
  xdcc *xd;
  unsigned int num;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  if (!u->arg2 || (u->arg2[0] == 0)) {
    mydelete(xd->trigger);
    a_respond(u, "%s: [Pack %u] removed", u->cmd, num);
    autotrigger_rebuild();
  } else {
    clean_quotes(u->arg2e);
    if (xd->trigger) {
      mydelete(xd->trigger);
      xd->trigger = mystrdup(u->arg2e);
      autotrigger_rebuild();
    } else {
      xd->trigger = mystrdup(u->arg2e);
      autotrigger_add(xd);
    }
    a_respond(u, "%s: [Pack %u] set: %s", u->cmd, num, u->arg2e);
  }

  write_files();
}

void a_deltrigger(const userinput * const u)
{
  xdcc *xd;
  unsigned int num1;
  unsigned int num2;
  unsigned int dirty;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  dirty = 0;
  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    if (xd->trigger == NULL)
      continue;

    ++dirty;
    mydelete(xd->trigger);
    a_respond(u, "%s: [Pack %u] removed",
              "CHTRIGGER", num1); /* NOTRANSLATE */
  }
  if (dirty > 0)
    autotrigger_rebuild();
}

void a_chgets(const userinput * const u)
{
  xdcc *xd;
  char *last;
  unsigned int num1;
  unsigned int num2;
  unsigned int val;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  last = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    last = u->arg3;
  }

  if (invalid_text(u, "Try Specifying a Count", last))
    return;

  val = atoi(last);
  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "%s: [Pack %u] Old: %u New: %u",
              u->cmd, num1, xd->gets, val);
    xd->gets = val;
  }

  write_files();
}

void a_chcolor(const userinput * const u)
{
  xdcc *xd;
  char *last;
  char *back;
  char *style;
  unsigned int num1;
  unsigned int num2;
  unsigned int color = 0;
  unsigned int color_fg = 0;
  unsigned int color_bg = 0;
  unsigned int color_st = 0;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  last = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    last = u->arg3;
  }

  if (last) {
    back = strchr(last, ',');
    if (back != 0) {
      *(back++) = 0;
      style = strchr(back, ',');
      if (style != 0) {
        *(style++) = 0;
        color_st = atoi(style);
        color |= color_st << 16;
      }
      color_bg = atoi(back);
      color |= color_bg << 8;
    }
    color_fg = atoi(last);
    color |= color_fg;
  }
  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    xd->color = color;
    a_respond(u, "%s: [Pack %u] set: %u,%u,%u", u->cmd, num1, color_fg, color_bg, color_st);
  }

  write_files();
}

void a_lock(const userinput * const u)
{
  xdcc *xd;
  char *pass;
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  pass = u->arg2;
  num2 = num1;
  if (u->arg3) {
    num2 = get_pack_nr2(u, u->arg2, num1);
    if (num2 == 0)
      return;

    pass = u->arg3;
  }

  if (invalid_text(u, "Try Specifying a Password", pass))
    return;

  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "%s: [Pack %u] Password: %s", u->cmd, num1, pass);
    mydelete(xd->lock);
    xd->lock = mystrdup(pass);
  }

  write_files_changed();
}

void a_unlock(const userinput * const u)
{
  xdcc *xd;
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "%s: [Pack %u]", u->cmd, num1);

    mydelete(xd->lock);
    xd->lock = NULL;
  }

  write_files_changed();
}

void a_lockgroup(const userinput * const u)
{
  xdcc *xd;
  unsigned int n;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (invalid_text(u, "Try Specifying a Password", u->arg2))
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++n;
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, u->arg1) != 0)
      continue;

    a_respond(u, "%s: [Pack %u] Password: %s",
              "LOCK", n, u->arg2); /* NOTRANSLATE */
    mydelete(xd->lock);
    xd->lock = mystrdup(u->arg2);
  }
  write_files_changed();
}

void a_unlockgroup(const userinput * const u)
{
  xdcc *xd;
  unsigned int n;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++n;
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, u->arg1) != 0)
      continue;

    a_respond(u, "%s: [Pack %u]", "UNLOCK", n);
    mydelete(xd->lock);
    xd->lock = NULL;
  }
  write_files_changed();
}

void a_relock(const userinput * const u)
{
  xdcc *xd;
  unsigned int n;

  updatecontext();

  if (invalid_text(u, "Try Specifying a Password", u->arg1))
    return;

  if (invalid_text(u, "Try Specifying a Password", u->arg2))
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++n;
    if (xd->lock == NULL)
      continue;

    if (strcmp(xd->lock, u->arg1) != 0)
      continue;

    mydelete(xd->lock);
    a_respond(u, "%s: [Pack %u] Password: %s", "LOCK", n, u->arg2);
    xd->lock = mystrdup(u->arg2);
  }
  write_files_changed();
}

void a_groupdesc(const userinput * const u)
{
  unsigned int k;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (u->arg2e && (u->arg2e[0] != 0)) {
    clean_quotes(u->arg2e);
    a_respond(u, "New GROUPDESC: %s", u->arg2e);
  } else {
    a_respond(u, "Removed GROUPDESC");
  }

  k = reorder_new_groupdesc(u->arg1, u->arg2e);
  if (k == 0)
    return;

  write_files();
}

void a_group(const userinput * const u)
{
  xdcc *xd;
  const char *newgroup;
  unsigned int num;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  newgroup = u->arg2;
  if (!u->arg2 || (u->arg2[0] == 0)) {
    if (xd->group == NULL) {
      a_respond(u, "Try Specifying a Group");
      return;
    }
    newgroup = NULL;
  } else {
    if (gdata.groupsincaps)
      caps(u->arg2);
  }

  a_set_group(u, xd, num, newgroup);
  write_files();
}

void a_movegroup(const userinput * const u)
{
  xdcc *xd;
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  if (u->arg3 && (u->arg3[0] != 0)) {
    if (gdata.groupsincaps)
      caps(u->arg3);
  }

  for (; num1 <= num2; ++num1) {
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    a_set_group(u, xd, num1, u->arg3);
  }

  write_files();
}

void a_regroup(const userinput * const u)
{
  xdcc *xd;
  const char *g;
  unsigned int k;
  int not_main;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
     return;

  if (invalid_group(u, u->arg2) != 0)
     return;

  if (gdata.groupsincaps) {
    caps(u->arg1);
    caps(u->arg2);
  }

  not_main = strcasecmp(u->arg2, "MAIN"); /* NOTRANSLATE */
  k = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL)
      g = xd->group;
    else
      g = "MAIN"; /* NOTRANSLATE */
    if (strcasecmp(g, u->arg1) == 0) {
      ++k;
      if (xd->group != NULL)
        mydelete(xd->group);
      if (not_main)
        xd->group = mystrdup(u->arg2);
    }
  }

  if (k == 0)
    return;

  a_respond(u, "%s: Old: %s New: %s", u->cmd, u->arg1, u->arg2);
  reorder_groupdesc(u->arg2);
  if (not_main) {
    if (strcasecmp(u->arg1, "MAIN") == 0) /* NOTRANSLATE */
      add_default_groupdesc(u->arg2);
  }
  write_files();
}

void a_md5(const userinput * const u)
{
  xdcc *xd;
  unsigned int von = 0;
  unsigned int bis;
  unsigned int num;

  updatecontext();

  /* show status */
  if (gdata.md5build.xpack) {
    a_respond(u, "calculating MD5/CRC32 for pack %u",
              number_of_pack(gdata.md5build.xpack));
  }

  if (!(u->arg1))
    return;

  von = get_pack_nr1(u, u->arg1);
  if (von == 0)
    return;

  bis = get_pack_nr2(u, u->arg2, von);
  if (bis == 0)
    return;

  for (num = von; num <= bis; ++num) {
    xd = irlist_get_nth(&gdata.xdccs, num - 1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "Rebuilding MD5 and CRC for Pack #%u:", num);
    if (gdata.md5build.xpack) {
      xd->has_md5sum  = 0;
    } else {
      start_md5_hash(xd, num);
    }
  }
}

void a_crc(const userinput * const u)
{
  const char *crcmsg;
  xdcc *xd;
  unsigned int von;
  unsigned int bis;
  unsigned int num;

  updatecontext();

  if (u->arg1) {
    von = get_pack_nr1(u, u->arg1);
    if (von == 0)
      return;

    bis = get_pack_nr2(u, u->arg2, von);
    if (bis == 0)
      return;

    for (num = von; num <= bis; ++num) {
      xd = irlist_get_nth(&gdata.xdccs, num - 1);
      if (group_restricted(u, xd))
        return;

      a_respond(u, "Validating CRC for Pack #%u:", num);
      crcmsg = validate_crc32(xd, 0);
      if (crcmsg != NULL)
        a_respond(u, "CRC32: [Pack %u] File '%s' %s.", num, xd->file, crcmsg);
    }
    return;
  }
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (group_hidden(u, xd))
      continue;

    ++num;
    crcmsg = validate_crc32(xd, 1);
    if (crcmsg != NULL)
      a_respond(u, "CRC32: [Pack %u] File '%s' %s.", num, xd->file, crcmsg);
  }
}

static void a_chfile_sub(const userinput * const u, xdcc *xd, unsigned int num, char *file, struct stat *st)
{
  char *old;

  updatecontext();

  a_cancel_transfers(xd, "Pack file changed");

  a_respond_old_new(u, "CHFILE", num, xd->file, file); /* NOTRANSLATE */

  old = xd->file;
  xd->file     = file;
  xd->st_size  = st->st_size;
  xd->st_dev   = st->st_dev;
  xd->st_ino   = st->st_ino;
  xd->mtime    = st->st_mtime;

  /* change default description */
  if (strcmp(xd->desc, getfilename(old)) == 0) {
    mydelete(xd->desc);
    xd->desc = mystrdup(getfilename(xd->file));
  }
  mydelete(old);

  cancel_md5_hash(xd, "CHFILE"); /* NOTRANSLATE */
}

void a_chfile(const userinput * const u)
{
  struct stat st;
  xdcc *xd;
  char *file;
  unsigned int num;
  int xfiledescriptor;

  updatecontext();

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  if (invalid_file(u, u->arg2) != 0)
    return;

  /* verify file is ok first */
  file = mystrdup(u->arg2);

  xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_fstat(u, xfiledescriptor, &file, &st))
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  a_chfile_sub(u, xd, num, file, &st);
  write_files_changed();
}

static unsigned int a_newdir_check(const userinput * const u, const char *dir1, const char *dir2, xdcc *xd)
{
  struct stat st;
  const char *off2;
  char *tempstr;
  size_t len1;
  int xfiledescriptor;

  updatecontext();

  len1 = strlen(dir1);
  if ( strncmp(dir1, xd->file, len1) != 0 )
    return 0;

  off2 = xd->file + len1;
  tempstr = mystrsuffix(dir2, off2);

  xfiledescriptor = open(tempstr, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_fstat(u, xfiledescriptor, &tempstr, &st))
    return 0;

  a_chfile_sub(u, xd, number_of_pack(xd), tempstr, &st);
  return 1;
}

void a_adddir(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d)
    {
      a_respond(u, "Can't Access Directory: %s %s", u->arg1, strerror(errno));
      mydelete(thedir);
      return;
    }

  a_adddir_sub(u, thedir, d, 0, NULL, NULL);
  mydelete(thedir);
  return;
}

void a_addnew(const userinput * const u)
{
  DIR *d;
  char *thedir;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg1, strerror(errno));
    mydelete(thedir);
    return;
  }

  a_adddir_sub(u, thedir, d, 1, NULL, NULL);
  mydelete(thedir);
  return;
}

void a_newdir(const userinput * const u)
{
  xdcc *xd;
  unsigned int found;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  if (invalid_text(u, "Try Specifying a new Directory", u->arg2))
    return;

  convert_to_unix_slash(u->arg2);

  found = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    found += a_newdir_check(u, u->arg1, u->arg2, xd);
  }
  a_respond(u, "%s: %u Packs found", u->cmd, found);

  if (found > 0)
    write_files_changed();
}

static void a_target_file(char **file2, const char *file1)
{
  char *end;
  char *newfile;

  if (strchr(*file2, '/') != NULL)
    return;

  if (strrchr(file1, '/') == NULL)
    return;

  newfile = mymalloc(strlen(file1)+1+strlen(*file2)+1);
  strcpy(newfile, file1);
  end = strrchr(newfile, '/');
  if (end != NULL)
    strcpy(++end, *file2);
  mydelete(*file2);
  *file2 = newfile;
}

void a_filemove(const userinput * const u)
{
  struct stat st;
  char *file1;
  char *file2;
  int xfiledescriptor;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (invalid_file(u, u->arg1) != 0)
    return;

  if (invalid_text(u, "Try Specifying a new Filename", u->arg2))
    return;

  file1 = mystrdup(u->arg1);

  xfiledescriptor = a_open_file(&file1, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_file(u, xfiledescriptor, &file1, &st))
    return;

  file2 = mystrdup(u->arg2);
  convert_to_unix_slash(file2);

  a_target_file(&file2, file1);
  if (rename(file1, file2) < 0) {
    a_respond(u, "File %s could not be moved to %s: %s", file1, file2, strerror(errno));
  } else {
    a_respond(u, "File %s was moved to %s.", file1, file2);
  }
  mydelete(file1);
  mydelete(file2);
}

static unsigned int a_movefile_sub(const userinput * const u, xdcc *xd, const char *newfile)
{
  struct stat st;
  char *file1;
  char *file2;
  int xfiledescriptor;

  file1 = xd->file;

  xfiledescriptor = a_open_file(&file1, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_file(u, xfiledescriptor, &file1, &st))
    return 1;

  file2 = mystrdup(newfile);
  convert_to_unix_slash(file2);

  a_target_file(&file2, file1);
  if (rename(file1, file2) < 0) {
    a_respond(u, "File %s could not be moved to %s: %s", file1, file2, strerror(errno));
    mydelete(file2);
    return 1;
  }

  a_respond(u, "File %s was moved to %s.", file1, file2);
  xd->file = file2;
  mydelete(file1);
  return 0;
}

void a_movefile(const userinput * const u)
{
  xdcc *xd;
  unsigned int num;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  num = get_pack_nr1(u, u->arg1);
  if (num == 0)
    return;

  if (invalid_text(u, "Try Specifying a new Filename", u->arg2))
    return;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  a_movefile_sub(u, xd, u->arg2);
}

void a_movegroupdir(const userinput * const u)
{
  DIR *d;
  xdcc *xd;
  char *thedir;
  char *tempstr;
  const char *g;
  unsigned int foundit;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (invalid_dir(u, u->arg2) != 0)
    return;

  if (gdata.groupsincaps)
    caps(u->arg1);

  thedir = mystrdup(u->arg2);
  d = a_open_dir(&thedir);
  if (!d) {
    a_respond(u, "Can't Access Directory: %s %s", u->arg2, strerror(errno));
    return;
  }

  foundit = 0;
  tempstr = mymalloc(maxtextlength);
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL)
      g = xd->group;
    else
      g = "MAIN"; /* NOTRANSLATE */
    if (strcasecmp(g, u->arg1) == 0) {
      ++foundit;
      snprintf(tempstr, maxtextlength - 2, "%s/%s", thedir, getfilename(xd->file)); /* NOTRANSLATE */
      if (strcmp(tempstr, xd->file) != 0) {
        if (a_movefile_sub(u, xd, tempstr))
          break;
      }
    }
  }
  closedir(d);
  mydelete(tempstr);
  mydelete(thedir);
  if (foundit > 0)
    write_files_changed();
}

void a_filedel(const userinput * const u)
{
  char *filename;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (invalid_file(u, u->arg1) != 0)
    return;

  filename = mystrdup(u->arg1);
  a_filedel_disk(u, &filename);
  mydelete(filename);
}

void a_fileremove(const userinput * const u)
{
  xdcc *xd;
  char *filename;
  unsigned int num1;
  unsigned int num2;
  unsigned int found;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  found = 0;
  for (; num2 >= num1; --num2) {
    xd = irlist_get_nth(&gdata.xdccs, num2 - 1);
    if (group_restricted(u, xd))
      return;

    filename = mystrdup(xd->file);
    if (a_remove_pack2(u, xd, num2) == 0)
      a_filedel_disk(u, &filename);
    mydelete(filename);
  }
  if (found)
    a_remove_pack_final();
}

void a_showdir(const userinput * const u)
{
  char *thedir;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (invalid_dir(u, u->arg1) != 0)
    return;

  strip_trailing_path(u->arg1);
  thedir = mystrdup(u->arg1);
  u_listdir(u, thedir);
  mydelete(thedir);
  return;
}

void a_makedir(const userinput * const u)
{
  char *thedir;
  char *uploaddir;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  if (invalid_dir(u, u->arg1) != 0)
    return;

  strip_trailing_path(u->arg1);
  thedir = mystrjoin(uploaddir, u->arg1, '/');
  if (mkdir(thedir, 0755) < 0) {
    a_respond(u, "Dir %s could not be created: %s", u->arg1, strerror(errno));
  } else {
    a_respond(u, "Dir %s was created.", u->arg1);
  }
  mydelete(thedir);
  return;
}

#ifdef USE_CURL
void a_fetch(const userinput * const u)
{
  char *uploaddir;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  if (disk_full(uploaddir) != 0) {
    a_respond(u, "Not enough free space on disk.");
    return;
  }

  if (invalid_file(u, u->arg1) != 0)
    return;

  if (is_upload_file(u, u->arg1) != 0)
    return;

  if (invalid_text(u, "Try Specifying a URL", u->arg2e))
    return;

  clean_quotes(u->arg2e);
  start_fetch_url(u, uploaddir);
}

void a_fetchcancel(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (fetch_cancel(num)) {
    a_respond(u, "Try Specifying a Valid Download Number");
  }
}
#endif /* USE_CURL */

static void a_announce_channels(const char *msg, const char *match, const char *group)
{
  channel_t *ch;

  updatecontext();

  if (msg == NULL)
    return;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if ((ch->flags & CHAN_ONCHAN) == 0)
      continue;
    if (match != NULL) {
      if (strcasecmp(ch->name, match) != 0)
        continue;
    } else {
      if (ch->noannounce != 0)
        continue;
      /* if announce is related to a pack, apply per-channel visibility rules */
      if (group) {
        if (!verify_group_in_grouplist(group, ch->rgroup))
          continue;
      }
    }
    privmsg_chan(ch, "%s", msg); /* NOTRANSLATE */
  }
}

void a_amsg(const userinput * const u)
{
  gnetwork_t *backup;
  unsigned int ss;

  updatecontext();

  if (invalid_text(u, "Try Specifying a Message", u->arg1e))
    return;

  clean_quotes(u->arg1e);
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);
    if (gnetwork->noannounce != 0)
      continue;

    a_announce_channels(u->arg1e, NULL, NULL);
  }
  gnetwork = backup;
  a_respond(u, "Announced %s", u->arg1e);
}

channel_t *is_not_joined_channel(const userinput * const u, const char *name)
{
  channel_t *ch;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, name) == 0)
      return ch;
  }
  a_respond(u, "Bot not in Channel %s on %s", name, gnetwork->name);
  return NULL;
}

static void a_msg_nick_or_chan(const userinput * const u, const char *name, const char *msg)
{
  channel_t *ch;

  if (invalid_text(u, "Try Specifying a Nick", name))
    return;

  if (invalid_text(u, "Try Specifying a Message", msg))
    return;

  if (name[0] != '#') {
    privmsg_fast(name, "%s", msg); /* NOTRANSLATE */
    return;
  }

  ch = is_not_joined_channel(u, name);
  if (ch == NULL)
    return;

  privmsg_chan(ch, "%s", msg); /* NOTRANSLATE */
}

void a_msg(const userinput * const u)
{
  gnetwork_t *backup;

  updatecontext();

  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
  a_msg_nick_or_chan(u, u->arg1, u->arg2e);
  gnetwork = backup;
}

void a_msgnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  a_msg_nick_or_chan(u, u->arg2, u->arg3e);
  gnetwork = backup;
}

void a_mesg(const userinput * const u)
{
  transfer *tr;
  gnetwork_t *backup;

  updatecontext();

  if (invalid_text(u, "Try Specifying a Message", u->arg1e))
    return;

  backup = gnetwork;
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    gnetwork = &(gdata.networks[tr->net]);
    notice(tr->nick, "MESSAGE FROM OWNER: %s", u->arg1e);
  }
  gnetwork = backup;

  a_respond(u, "Sent message to %u %s", irlist_size(&gdata.trans), irlist_size(&gdata.trans)!=1 ? "users" : "user");
}

void a_mesq(const userinput * const u)
{
  ir_pqueue *pq;
  gnetwork_t *backup;
  unsigned int count;

  updatecontext();

  if (invalid_text(u, "Try Specifying a Message", u->arg1e))
    return;

  backup = gnetwork;
  count = 0;
  for (pq = irlist_get_head(&gdata.mainqueue);
       pq;
       pq = irlist_get_next(pq)) {
    gnetwork = &(gdata.networks[pq->net]);
    notice(pq->nick, "MESSAGE FROM OWNER: %s", u->arg1e);
    ++count;
  }
  gnetwork = backup;
  a_respond(u, "Sent message to %u %s", count, count!=1 ? "users" : "user");
}

void a_ignore(const userinput * const u)
{
  unsigned int num=0;
  igninfo *ignore;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  if (!u->arg1) {
    a_respond(u, "Try specifying an amount of time to ignore");
    return;
  }

  if (!u->arg2 || strlen(u->arg2) < 4U) {
    a_respond(u, "Try specifying a hostmask longer than 4 characters");
    return;
  }

  ignore = get_ignore(u->arg2);
  ignore->flags |= IGN_IGNORING;
  ignore->flags |= IGN_MANUAL;
  ignore->bucket = (num*60)/gdata.autoignore_threshold;
  if (ignore->bucket < 0)
    ignore->bucket = 0x7FFFFFF;

  a_respond(u, "Ignore activated for %s which will last %u min",
            u->arg2, num);
  write_statefile();
}

static void a_bann_hostmask(const userinput * const u, const char *arg)
{
  transfer *tr;
  char *hostmask;
  unsigned int changed = 0;

  /* XDCC REMOVE */
  queue_host_remove(u, &gdata.idlequeue, arg);
  changed = queue_host_remove(u, &gdata.mainqueue, arg);
  if (changed >0)
    write_statefile();

  /* XDCC CANCEL */
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    hostmask = to_hostmask(tr->nick, tr->hostname);
    if (fnmatch(arg, hostmask, FNM_CASEFOLD) == 0) {
      if (tr->tr_status != TRANSFER_STATUS_DONE) {
         t_closeconn(tr, "Transfer canceled by admin", 0);
      }
    }
    mydelete(hostmask);
  }
}

void a_bannhost(const userinput * const u)
{
  a_ignore(u);

  if (!u->arg1) return;
  if (!u->arg2 || strlen(u->arg2) < 4U) return;

  a_bann_hostmask(u, u->arg2);
}

void a_bannnick(const userinput * const u)
{
  gnetwork_t *backup;
  transfer *tr;
  char *nick;
  unsigned int changed = 0;
  int net = 0;

  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  if (invalid_text(u, "Try Specifying a Nick", u->arg1))
    return;

  nick = u->arg1;
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);

  /* XDCC REMOVE */
  queue_nick_remove(u, &gdata.idlequeue, (unsigned int)net, nick);
  changed = queue_nick_remove(u, &gdata.mainqueue, (unsigned int)net, nick);
  if (changed > 0)
    write_statefile();

  /* XDCC CANCEL */
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status == TRANSFER_STATUS_DONE)
      continue;

    if (tr->net != (unsigned int)net)
      continue;

    if (strcasecmp(tr->nick, nick) != 0)
      continue;

    t_closeconn(tr, "Transfer canceled by admin", 0);
  }

  gnetwork = backup;
}

void a_close(const userinput * const u)
{
  transfer *tr;

  updatecontext();

  if (!u->arg1) {
    for (tr = irlist_get_head(&gdata.trans);
         tr;
         tr = irlist_get_next(tr)) {
      if (tr->tr_status == TRANSFER_STATUS_DONE)
        continue;

      t_closeconn(tr, "Owner Requested Close", 0);
    }
    return;
  }

  tr = get_transfer_by_arg(u, u->arg1);
  if (tr == NULL)
    return;

  t_closeconn(tr, "Owner Requested Close", 0);
}

void a_closeu(const userinput * const u)
{
  upload *ul;
  unsigned int num = 0;

  updatecontext();

  if (!u->arg1) {
    for (ul = irlist_get_head(&gdata.uploads);
         ul;
         ul = irlist_get_next(ul)) {
      if (ul->ul_status == UPLOAD_STATUS_DONE)
        continue;

      l_closeconn(ul, "Owner Requested Close", 0);
    }
    return;
  }

  num = atoi(u->arg1);
  ul = irlist_get_nth(&gdata.uploads, num);
  if (ul == NULL) {
    a_respond_badid(u, "DCL"); /* NOTRANSLATE */
    return;
  }
  l_closeconn(ul, "Owner Requested Close", 0);
}

void a_acceptu(const userinput * const u)
{
  tupload_t *tu;
  const char *hostmask;
  unsigned int min = 0;

  updatecontext();

  if (u->arg1) min = atoi(u->arg1);
  hostmask = u->arg2 ? u->arg2 : "*!*@*"; /* NOTRANSLATE */
  if (min > 0) {
    tu = irlist_add(&gdata.tuploadhost, sizeof(tupload_t));
    tu->u_host = mystrdup( hostmask );
    tu->u_time = gdata.curtime + (min * 60);
    a_respond(u, "Uploadhost %s valid for %u minutes", hostmask, min);
    return;
  }
  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu; ) {
    if (strcmp(tu->u_host, hostmask) != 0) {
      tu = irlist_get_next(tu);
      continue;
    }
    a_respond(u, "Uploadhost %s deactivated", hostmask);
    mydelete(tu->u_host);
    tu = irlist_delete(&gdata.tuploadhost, tu);
  }
}

void a_getl(const userinput * const u)
{
  qupload_t *qu;

  updatecontext();

  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    a_respond(u, "GET %s started for %s on %s",
              qu->q_pack, qu->q_nick, gdata.networks[ qu->q_net ].name);
    switch (qu->q_state) {
    case QUPLOAD_TRYING:
      a_respond(u, "trying");
      break;
    case QUPLOAD_WAITING:
      a_respond(u, "queued");
      break;
    case QUPLOAD_RUNNING:
      a_respond(u, "running");
      break;
    case QUPLOAD_IDLE:
      break;
    }
  }
}

void a_get(const userinput * const u)
{
  qupload_t *qu;
  char *uploaddir;
  int net;
  unsigned int found;

  updatecontext();

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (invalid_text(u, "Try Specifying a Nick", u->arg2))
    return;

  if (u->arg3e) {
    qu = irlist_add(&gdata.quploadhost, sizeof(qupload_t));
    qu->q_net = net;
    qu->q_nick = mystrdup(u->arg2);
    qu->q_pack = mystrdup(u->arg3e);
    qu->q_host = to_hostmask(qu->q_nick, "*"); /* NOTRANSLATE */
    qu->q_time = gdata.curtime;
    a_respond(u, "GET %s started for %s on %s",
              qu->q_pack, qu->q_nick, gdata.networks[ qu->q_net ].name);
    qu->q_state = QUPLOAD_WAITING;
    start_qupload();
    return;
  }
  found = close_qupload((unsigned int)net, u->arg2);
  if ( found > 0 )
    a_respond(u, "GET %s deactivated", u->arg2);
}

static void a_rmq3(irlist_t *list)
{
  ir_pqueue *pq;
  gnetwork_t *backup;

  updatecontext();

  backup = gnetwork;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_delete(list, pq)) {
     gnetwork = &(gdata.networks[pq->net]);
     notice(pq->nick, "** Removed From Queue: Owner Requested Remove");
     mydelete(pq->nick);
     mydelete(pq->hostname);
  }
  gnetwork = backup;
}

static void a_rmq2(const userinput * const u, irlist_t *list)
{
  ir_pqueue *pq;
  gnetwork_t *backup;
  unsigned int num = 0;

  updatecontext();

  if (!u->arg1) {
    a_rmq3(list);
    return;
  }

  num = atoi(u->arg1);
  if (num == 0) {
    a_respond_badid(u, "QUL"); /* NOTRANSLATE */
    return;
  }

  pq = irlist_get_nth(list, num - 1);
  if (!pq) {
    a_respond_badid(u, "QUL"); /* NOTRANSLATE */
    return;
  }

  backup = gnetwork;
  gnetwork = &(gdata.networks[pq->net]);
  notice(pq->nick, "** Removed From Queue: Owner Requested Remove");
  mydelete(pq->nick);
  mydelete(pq->hostname);
  irlist_delete(list, pq);
  gnetwork = backup;
}

void a_rmq(const userinput * const u)
{
  a_rmq2(u, &gdata.mainqueue);
}

void a_rmiq(const userinput * const u)
{
  a_rmq2(u, &gdata.idlequeue);
}

void a_rmallq(const userinput * const u)
{
  a_rmq2(u, &gdata.idlequeue);
  a_rmq2(u, &gdata.mainqueue);
}

void a_rmul(const userinput * const u)
{
  char *tempstr;
  char *uploaddir;

  updatecontext();

  uploaddir = a_valid_uploaddir(u);
  if (uploaddir == NULL)
    return;

  if (invalid_file(u, u->arg1) != 0)
    return;

  if (strchr(u->arg1, '/')) {
    a_respond(u, "Filename contains invalid characters");
    return;
  }

  clean_quotes(u->arg1);

  if (is_unsave_directory(u->arg1)) {
    a_respond(u, "Try Specifying a Directory");
    return;
  }

  tempstr = mystrjoin(uploaddir, u->arg1, '/');
  if (is_file_writeable(tempstr)) {
    if (save_unlink(tempstr) < 0)
      a_respond(u, "Unable to remove the file");
    else
      a_respond(u, "Deleted");
  } else {
    a_respond(u, "That filename doesn't exist");
  }
  mydelete(tempstr);
}

void a_raw(const userinput * const u)
{
  gnetwork_t *backup;

  updatecontext();

  if (invalid_command(u, u->arg1e) != 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
  writeserver(WRITESERVER_NOW, "%s", u->arg1e); /* NOTRANSLATE */
  gnetwork = backup;
}

void a_rawnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (invalid_command(u, u->arg2e) != 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  writeserver(WRITESERVER_NOW, "%s", u->arg2e); /* NOTRANSLATE */
  gnetwork = backup;
}

static void a_lag_net(const userinput * const u)
{
  gnetwork->lastping = gdata.curtimems;
  gnetwork->lag_method = u->method;
  pingserver();
}

void a_lag(const userinput * const u)
{
  gnetwork_t *backup;
  int net;
  unsigned int ss;

  backup = gnetwork;
  if (u->arg1 == NULL) {
    for (ss=0; ss<gdata.networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      a_lag_net(u);
    }
  } else {
    net = get_network_msg(u, u->arg1);
    if (net < 0)
      return;

    gnetwork = &(gdata.networks[net]);
    a_lag_net(u);
  }
  gnetwork = backup;
}

static void a_hop_net(const userinput * const u, const char *name)
{
  channel_t *ch;

  /* part & join channels */
  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if ((!name) || (!strcasecmp(name, ch->name))) {
      a_respond(u, "Hopping channel %s on %s", ch->name, gnetwork->name);
      writeserver(WRITESERVER_NORMAL, "PART %s", ch->name); /* NOTRANSLATE */
      clearmemberlist(ch);
      ch->flags &= ~CHAN_ONCHAN;
      ch->flags &= ~CHAN_KICKED;
      joinchannel(ch);
    }
  }
}

void a_hop(const userinput * const u)
{
  gnetwork_t *backup;
  int net;
  unsigned int ss;

  updatecontext();

  backup = gnetwork;
  if (u->arg2 == NULL) {
    for (ss=0; ss<gdata.networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      a_hop_net(u, u->arg1);
    }
  } else {
    net = get_network_msg(u, u->arg2);
    if (net < 0)
      return;

    gnetwork = &(gdata.networks[net]);
    a_hop_net(u, u->arg1);
  }
  gnetwork = backup;
}

void a_nochannel(const userinput * const u)
{
  channel_t *ch;
  gnetwork_t *backup;
  unsigned int ss;
  unsigned int val = 0;

  updatecontext();

  if (u->arg1) val = atoi(u->arg1);

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);
    /* part channels */
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if ((!u->arg2) || (!strcasecmp(u->arg2, ch->name))) {
        writeserver(WRITESERVER_NORMAL, "PART %s", ch->name); /* NOTRANSLATE */
        clearmemberlist(ch);
        ch->flags &= ~CHAN_ONCHAN;
        ch->nextjoin = gdata.curtime + (val * 60);
      }
    }
  }
  gnetwork = backup;
}

void a_join(const userinput * const u)
{
  channel_t *ch;
  gnetwork_t *backup;
  int net;

  updatecontext();

  if (invalid_channel(u, u->arg1) != 0)
    return;

  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(u->arg1, ch->name) != 0)
      continue;
    a_respond(u, "Channel already in list, Try \"HOP\" for rejoin");
    gnetwork = backup;
    return;
  }

  ch = irlist_add(&(gnetwork->channels), sizeof(channel_t));
  caps(u->arg1);
  ch->name = mystrdup(u->arg1);
  ++(ch->noannounce);
  ++(ch->notrigger);
  ch->nextjoin = gdata.curtime;
  if (u->arg3)
    ch->key = mystrdup(u->arg3);
  gnetwork = backup;
}

void a_part(const userinput * const u)
{
  channel_t *ch;
  gnetwork_t *backup;
  int net;

  updatecontext();

  if (invalid_channel(u, u->arg1) != 0)
    return;

  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(u->arg1, ch->name) != 0)
      continue;

    writeserver(WRITESERVER_NORMAL, "PART %s", ch->name); /* NOTRANSLATE */
    clearmemberlist(ch);
    free_channel_data(ch);
    irlist_delete(&(gnetwork->channels), ch);
    gnetwork = backup;
    return;
  }
  a_respond(u, "Bot not in Channel %s on %s", u->arg1, gnetwork->name);
  gnetwork = backup;
}

static void a_servqc2(const userinput * const u, unsigned int net)
{
  gnetwork_t *backup;

  a_respond(u, "Cleared server queue of %u lines",
            irlist_size(&gdata.networks[net].serverq_channel) +
            irlist_size(&gdata.networks[net].serverq_fast) +
            irlist_size(&gdata.networks[net].serverq_normal) +
            irlist_size(&gdata.networks[net].serverq_slow));

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  clean_send_buffers();
  gnetwork = backup;
}

void a_servqc(const userinput * const u)
{
  unsigned int ss;
  int net;

  updatecontext();

  if(u->arg1 != NULL) {
    net = get_network_msg(u, u->arg1);
    if (net < 0)
      return;

    a_servqc2(u, (unsigned)net);
    return;
  }

  for (ss=0; ss<gdata.networks_online; ++ss) {
    a_servqc2(u, ss);
  }
}

static void a_respond_disabled(const userinput * const u, const char *text, unsigned int num)
{
  a_respond(u, "** %s disabled for the next %u %s", text, num, num!=1 ? "minutes" : "minute");
}

void a_nosave(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.noautosave, num);
  a_respond_disabled(u, "AutoSave has been", num);
}

void a_nosend(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  mydelete(gdata.nosendmsg);
  if (u->arg2e) {
    clean_quotes(u->arg2e);
    gdata.nosendmsg=mystrdup(u->arg2e);
  }
  num = max_minutes_waits(&gdata.nonewcons, num);
  a_respond_disabled(u, "XDCC SEND has been", num);
}

void a_nolist(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.nolisting, num);
  a_respond_disabled(u, "XDCC LIST and PLIST have been", num);
}

void a_nomd5(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.nomd5_start, num);
  a_respond_disabled(u, "MD5 and CRC checksums have been", num);
}

void a_clearrecords(const userinput * const u)
{
  int ii;
  updatecontext();

  backup_statefile();

  gdata.record = 0;
  gdata.sentrecord = 0;

  for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++) {
      gdata.transferlimits[ii].ends = 0;
  }

  a_respond(u, "Cleared transfer record, bandwidth record and transfer limits");
  write_files();
}

void a_cleargets(const userinput * const u)
{
  xdcc *xd;

  backup_statefile();

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    xd->gets = 0;
  }

  gdata.totalsent = 0;
  gdata.totaluptime = 0;
  a_respond(u, "Cleared download counter for each pack, total sent and total uptime");
  write_files();
}

static void a_closec_sub(const userinput * const u, dccchat_t *chat)
{
  if (chat == u->chat) {
    a_respond(u, "Disconnecting yourself.");
  } else {
    writedccchat(chat, 0, "Disconnected due to CLOSEC\n");
  }
  shutdowndccchat(chat, 1);
}

void a_closec(const userinput * const u)
{
  dccchat_t *chat;
  unsigned int num = 0;

  updatecontext();

  if (!u->arg1) {
    for (chat = irlist_get_head(&gdata.dccchats);
         chat;
         chat = irlist_get_next(chat)) {
      a_closec_sub(u, chat);
    }
    return;
  }
  num = atoi(u->arg1);
  chat = irlist_get_nth(&gdata.dccchats, num - 1);
  if (chat == NULL) {
    a_respond_badid(u, "CHATL"); /* NOTRANSLATE */
    return;
  }
  a_closec_sub(u, chat);
}

void a_config(const userinput * const u)
{
  if (u->method != method_console) {
    if (!gdata.direct_config_access) {
      a_respond(u, "Disabled in Config");
      return;
    }
  }

  if (invalid_text(u, "Try Specifying a Key", u->arg1e))
    return;

  current_config = "ADMIN"; /* NOTRANSLATE */
  getconfig_set(u->arg1e);
}

void a_print(const userinput * const u)
{
  char *val;

  if (u->method != method_console) {
    if (!gdata.direct_config_access) {
      a_respond(u, "Disabled in Config");
      return;
    }
  }

  if (invalid_text(u, "Try Specifying a Key", u->arg1e))
    return;

  val = print_config_key(u->arg1);
  if (val != NULL) {
    a_respond(u, "CONFIG %s is set to \"%s\"", u->arg1, val);
    mydelete(val);
    return;
  }
}

void a_identify(const userinput * const u)
{
  gnetwork_t *backup;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  if (get_nickserv_pass() == NULL) {
    a_respond(u, "No nickserv_pass set!");
    gnetwork = backup;
    return;
  }

  identify_needed(1);
  gnetwork = backup;
  a_respond(u, "nickserv identify send.");
}

void a_holdqueue(const userinput * const u)
{
  unsigned int val;

  updatecontext();

  val = (gdata.holdqueue) ? 0 : 1;
  if (u->arg1) val = atoi(u->arg1);

  gdata.holdqueue = val;
  a_respond(u, "HOLDQUEUE now %u", val);

  if (val != 0)
    return;

  start_sends();
}

static void a_offline_net(unsigned int net)
{
  gnetwork_t *backup;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  a_quit_network();
  gnetwork->offline = 1;
  gnetwork = backup;
}

void a_offline(const userinput * const u)
{
  int val;
  unsigned int ss;

  updatecontext();

  if (u->arg1 != NULL) {
    val = get_network(u->arg1);
    if (val < 0)
      return;

    a_offline_net((unsigned int)val);
    return;
  }
  for (ss=0; ss<gdata.networks_online; ++ss) {
    a_offline_net(ss);
  }
}

void a_online(const userinput * const u)
{
  int val;
  unsigned int ss;

  updatecontext();

  if (u->arg1 != NULL) {
    val = get_network(u->arg1);
    if (val < 0)
      return;

    gdata.networks[val].offline = 0;
    return;
  }
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gdata.networks[ss].offline = 0;
  }
}

#ifdef USE_RUBY
void a_ruby(const userinput * const u)
{
  unsigned int rc;

  if (invalid_text(u, "Try Specifying a ruby method", u->arg1))
    return;

  rc = do_myruby_ruby(u);
  if (rc != 0)
    a_respond(u, "ruby method failed.");
}
#endif /* USE_RUBY */

void a_dump(const userinput * const u)
{
  dumpgdata();
  a_respond(u, "DUMP written into logfile.");
}

void a_version(const userinput * const u)
{
  char *text;

  text = print_config_key("features"); /* NOTRANSLATE */
  a_respond(u, "%s", text);
  mydelete(text);
}

void a_backgroud(const userinput * const u)
{
  /* fork to background if in background mode */

  if (gdata.background) {
    a_respond(u, "already in background.");
    return;
  }

  tostdout_disable_buffering();
  uninitscreen();

  ++(gdata.background);
  gobackground();

  if (gdata.pidfile)
    writepidfile(gdata.pidfile);

  gdata.debug = 0;
}

void a_autoadd(const userinput * const u)
{
  autoadd_all();
  a_respond(u, "AUTOADD done.");
}

void a_autocancel(const userinput * const u)
{
  unsigned int sum;

  sum = irlist_size(&gdata.packs_delayed);
  free_delayed();
  a_respond(u, "Cancelled %u pending add/remove", sum);
}

void a_autogroup(const userinput * const u)
{
  xdcc *xd;
  char *tempstr;
  char *newgroup;
  unsigned int num;

  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (xd->group != NULL)
      continue;

    tempstr = mystrdup(xd->file);
    newgroup = strrchr(tempstr, '/');
    if (newgroup == NULL) {
      mydelete(tempstr);
      continue;
    }

    *newgroup = 0;
    newgroup = strrchr(tempstr, '/');
    if (newgroup == NULL) {
      mydelete(tempstr);
      continue;
    }

    ++newgroup;
    if (newgroup[0] == 0) {
      mydelete(tempstr);
      continue;
    }

    if (gdata.groupsincaps)
      caps(newgroup);

    removenonprintablefile(newgroup);
    a_set_group(u, xd, num, newgroup);
    mydelete(tempstr);
  }
  write_files();
}

void a_noautoadd(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.noautoadd, num);
  a_respond_disabled(u, "AUTOADD" " has been", num);
}

void a_send(const userinput * const u)
{
  xdcc *xd;
  transfer *tr;
  gnetwork_t *backup;
  unsigned int num;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_text(u, "Try Specifying a Nick", u->arg1))
    return;

  num = get_pack_nr(u, u->arg2);
  if (num == 0)
    return;

  xd = get_xdcc_pack(num);
  if (xd == NULL)
    return;

  backup = gnetwork;
  a_respond(u, "Sending %s pack %d", u->arg1, (int)num);
  gnetwork = &(gdata.networks[net]);
  look_for_file_changes(xd);
  tr = create_transfer(xd, u->arg1, "man"); /* NOTRANSLATE */
  t_notice_transfer(tr, NULL, num, 0);
  t_unlmited(tr, NULL);
  t_setup_dcc(tr);
  gnetwork = backup;
}

static unsigned int a_queue_found(const userinput * const u, xdcc *xd, unsigned int num)
{
  const char *hostname = "man"; /* NOTRANSLATE */
  unsigned int alreadytrans;
  unsigned int inq;

  updatecontext();

  alreadytrans = queue_count_host(&gdata.mainqueue, &inq, 1, u->arg1, hostname, xd);
  alreadytrans += queue_count_host(&gdata.idlequeue, &inq, 1, u->arg1, hostname, xd);
  if (alreadytrans > 0) {
    a_respond(u, "Already Queued %s for Pack %d!", u->arg1, (int)num);
    return 1;
  }
  a_respond(u, "Queuing %s for Pack %d", u->arg1, (int)num);
  return 0;
}

/* this function imported from iroffer-lamm */
void a_queue(const userinput * const u)
{
  xdcc *xd;
  char *tempstr;
  const char *msg;
  gnetwork_t *backup;
  unsigned int num;
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_text(u, "Try Specifying a Nick", u->arg1))
    return;

  num = get_pack_nr(u, u->arg2);
  if (num == 0)
    return;

  xd = get_xdcc_pack(num);
  if (a_queue_found(u, xd, num))
    return;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  tempstr = mymalloc(maxtextlength);
  addtomainqueue(&msg, tempstr, u->arg1, NULL, num);
  notice(u->arg1, "** %s", tempstr);
  mydelete(tempstr);
  gnetwork = backup;

  start_one_send();
}

static int a_iqueue_sub(const userinput * const u, xdcc *xd, unsigned int num, unsigned int net, const char *filter)
{
  gnetwork_t *backup;
  char *tempstr;
  const char *msg;

  updatecontext();

  if (xd == NULL) {
    xd = get_xdcc_pack(num);
    if (xd == NULL)
      return 1;
  }

  if (filter != NULL) {
    if (!fnmatch_xdcc(filter, xd)) {
      return -1;
    }
  }

  if (a_queue_found(u, xd, num))
    return 1;

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  tempstr = mymalloc(maxtextlength);
  addtoidlequeue(&msg, tempstr, u->arg1, NULL, xd, num, 0);
  if (*tempstr != 0)
    notice(u->arg1, "** %s", tempstr);
  mydelete(tempstr);
  gnetwork = backup;
  return 0;
}

static unsigned int a_iqueue_group(const userinput * const u, const char *what, unsigned int net, const char *match)
{
  xdcc *xd;
  unsigned int num;
  unsigned int found;
  int queued;

  updatecontext();

  found = 0;
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    ++num;
    if (xd->group == NULL)
      continue;
    if (strcasecmp(what, xd->group) != 0)
      continue;

    queued = a_iqueue_sub(u, xd, num, net, match);
    if (queued > 0)
      break;
    if (queued < 0)
      continue;
    ++found;
  }
  return found;
}

static unsigned int a_iqueue_search(const userinput * const u, const char *what, unsigned int net)
{
  char *end;
  char *pattern;
  char *match = NULL;
  unsigned int found;
  unsigned int first;
  unsigned int last;
  int queued;

  updatecontext();

  pattern = strchr(what, '*');
  if (pattern != NULL) {
    *(pattern++) = 0; /* cut pattern */
    match = grep_to_fnmatch(pattern);
  }

  found = a_iqueue_group(u, what, net, match);
  if (found != 0) {
    mydelete(match);
    return found;
  }

  updatecontext();

  /* range */
  if (*what == '#') ++what;
  if (*what == 0)
    return found;

  /* xdcclistfile */
  if (*what == '-') {
    mydelete(match);
    first = atoi(what);
    if (first == XDCC_SEND_LIST) {
      queued = a_iqueue_sub(u, NULL, first, net, NULL);
      if (queued == 0)
        ++found;
    }
    return found;
  }

  end = strchr(what, '-');
  if (end == NULL) {
    mydelete(match);
    first = packnumtonum(what);
    if (first > 0) {
      queued = a_iqueue_sub(u, NULL, first, net, NULL);
      if (queued == 0)
        ++found;
    }
    return found;
  }
  *(end++) = 0; /* cut range */
  first = packnumtonum(what);
  last = packnumtonum(end);
  if (last < first) {
    /* count backwards */
    for (; first >= last; --first) {
      queued = a_iqueue_sub(u, NULL, first, net, match);
      if (queued > 0)
        break;
      if (queued < 0)
        continue;
      ++found;
    }
  } else {
    /* count forwards */
    for (; first <= last; ++first) {
      queued = a_iqueue_sub(u, NULL, first, net, match);
      if (queued > 0)
        break;
      if (queued < 0)
        continue;
      ++found;
    }
  }
  mydelete(match);
  return found;
}

static unsigned int a_iqueue_words(const userinput * const u, const char *what, unsigned int net)
{
  char *copy;
  char *data;
  unsigned int found;

  copy = mystrdup(what);
  found = 0;
  /* parse list of packs */
  for (data = strtok(copy, ","); /* NOTRANSLATE */
       data;
       data = strtok(NULL, ",")) { /* NOTRANSLATE */
    if (data[0] == 0)
      break;
    found += a_iqueue_search(u, data, net);
  }
  mydelete(copy);
  return found;
}

void a_iqueue(const userinput * const u)
{
  int net;

  updatecontext();

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_text(u, "Try Specifying a Nick", u->arg1))
    return;

  if (invalid_text(u, "Try Specifying a Valid Pack Number", u->arg2))
    return;

  if (a_iqueue_words(u, u->arg2, (unsigned int)net) == 0) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return;
  }

  start_one_send();
}

static void a_announce_msg(const userinput * const u, const char *match, unsigned int num, const char *msg)
{
  gnetwork_t *backup;
  xdcc *xd;
  char *prefix;
  char *colordesc;
  char *datestr;
  char *suffix;
  char *message;
  char *color_suffix;
  char *sizestrstr;
  size_t len;
  unsigned int ss;
  unsigned int color;

  updatecontext();

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  a_respond(u, "Pack Info for Pack #%u:", num);
  prefix = mymalloc(maxtextlength);
  suffix = mymalloc(maxtextlength);
  colordesc = xd_color_description(xd);
  if (msg == NULL) {
    msg = gdata.autoaddann;
    if (msg == NULL)
      msg = "added";
  }
  prefix[0] = 0;
  len = 0;
  if (gdata.show_date_added) {
    datestr = mymalloc(maxtextlengthshort);
    user_getdatestr(datestr, xd->xtime ? xd->xtime : xd->mtime, maxtextlengthshort - 1);
    len += add_snprintf(prefix + len, maxtextlength - 2 - len, "%s%s", gdata.announce_seperator, datestr); /* NOTRANSLATE */
    mydelete(datestr);
  }
  if (gdata.announce_size) {
    sizestrstr = sizestr(1, xd->st_size);
    len += add_snprintf(prefix + len, maxtextlength - 2 - len, "%s[%s]", gdata.announce_seperator, sizestrstr); /* NOTRANSLATE */
    mydelete(sizestrstr);
  }
  add_snprintf(prefix + len, maxtextlength - 2 - len, "%s", gdata.announce_seperator); /* NOTRANSLATE */
  message = mymalloc(maxtextlength);
  color = a_get_color(gdata.announce_suffix_color);

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);
    if (gnetwork->noannounce != 0)
     continue;

    add_snprintf(suffix, maxtextlength - 2, "/MSG %s XDCC SEND %u",
                 get_user_nick(), num);
    color_suffix = color_text(suffix, color);
    add_snprintf(message, maxtextlength - 2, "\2%s\2%s%s%s%s",
                 msg, prefix, colordesc, gdata.announce_seperator, color_suffix);
    if (color_suffix != suffix)
      mydelete(color_suffix);
    a_announce_channels(message, match, xd->group);
    gnetwork = backup;
    a_respond(u, "Announced %s%s%s%s%s", msg, prefix, xd->desc, gdata.announce_seperator, suffix);
  }
  gnetwork = backup;
  mydelete(message);
  if (colordesc != xd->desc)
    mydelete(colordesc);
  mydelete(suffix);
  mydelete(prefix);
}

static void a_announce_sub(const userinput * const u, const char *arg1, const char *arg2, char *msg)
{
  unsigned int num1;
  unsigned int num2;

  updatecontext();

  num1 = get_pack_nr1(u, arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, arg2, num1);
  if (num2 == 0)
    return;

  if (msg != NULL)
    clean_quotes(msg);

  for (; num1 <= num2; ++num1) {
    a_announce_msg(u, NULL, num1, msg);
  }
}

void a_announce(const userinput * const u)
{
  a_announce_sub(u, u->arg1, u->arg1, u->arg2e);
}

void a_mannounce(const userinput * const u)
{
  a_announce_sub(u, u->arg1, u->arg2, u->arg3e);
}

static unsigned int a_new_announce(unsigned int max, const char *name)
{
  irlist_t list;
  struct tm *localt = NULL;
  xdcc **best;
  xdcc *xd;
  const char *format;
  const char *grouplist;
  char *tempstr;
  char *tempstr3;
  char *colordesc;
  time_t now;
  size_t llen;
  unsigned int i;

  format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";

  grouplist = get_grouplist_channel(name);
  memset(&list, 0, sizeof(irlist_t));
  for (i=0; i<max; ++i)
    add_newest_xdcc(&list, grouplist);

  i = 0;
  for (best = irlist_get_head(&list);
       best;
       best = irlist_delete(&list, best)) {
    xd = *best;
    now = xd->xtime;
    localt = localtime(&now);
    tempstr = mymalloc(maxtextlengthshort);
    llen = strftime(tempstr, maxtextlengthshort - 1, format, localt);
    if (llen == 0)
      tempstr[0] = '\0';
    colordesc = xd_color_description(xd);
    tempstr3 = mymalloc(maxtextlength);
    add_snprintf(tempstr3, maxtextlength - 1, "Added: %s \2%u\2%s%s",
                 tempstr, number_of_pack(xd), gdata.announce_seperator, colordesc);
    if (colordesc != xd->desc)
      mydelete(colordesc);
    a_announce_channels(tempstr3, name, xd->group);
    mydelete(tempstr3);
    mydelete(tempstr);
    ++i;
  }
  return i;
}

void a_newann(const userinput * const u)
{
  gnetwork_t *backup;
  unsigned int ss;
  unsigned int max = 0;
  int net;

  updatecontext();

  if (u->arg1) max = atoi (u->arg1);
  if (max == 0)
    return;

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  backup = gnetwork;
  if (u->arg3 == NULL) {
    for (ss=0; ss<gdata.networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      if (gnetwork->noannounce == 0)
        a_new_announce(max, u->arg2);
    }
  } else {
    gnetwork = &(gdata.networks[net]);
    if (gnetwork->noannounce == 0)
      a_new_announce(max, u->arg2);
  }
  gnetwork = backup;
}

void a_cannounce(const userinput * const u)
{
  unsigned int num;

  updatecontext();

  if (invalid_channel(u, u->arg1) != 0)
    return;

  num = get_pack_nr1(u, u->arg2);
  if (num == 0)
    return;

  a_announce_msg(u, u->arg1, num, u->arg3e);
}

void a_sannounce(const userinput * const u)
{
  gnetwork_t *backup;
  xdcc *xd;
  char *tempstr;
  char *colordesc;
  unsigned int num1;
  unsigned int num2;
  unsigned int ss;

  updatecontext();

  num1 = get_pack_nr1(u, u->arg1);
  if (num1 == 0)
    return;

  num2 = get_pack_nr2(u, u->arg2, num1);
  if (num2 == 0)
    return;

  for (; num1 <= num2; ++num1) {
    a_respond(u, "Pack Info for Pack #%u:", num1);
    xd = irlist_get_nth(&gdata.xdccs, num1 - 1);
    if (group_restricted(u, xd))
      return;

    tempstr = mymalloc(maxtextlength);
    colordesc = xd_color_description(xd);
    snprintf(tempstr, maxtextlength - 2, "\2%u\2%s%s", num1, gdata.announce_seperator, colordesc);

    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ++ss) {
      gnetwork = &(gdata.networks[ss]);
      if (gnetwork->noannounce != 0)
        continue;

      a_announce_channels(tempstr, NULL, xd->group);
      gnetwork = backup;
      a_respond(u, "Announced %u%s%s", num1, gdata.announce_seperator, xd->desc);
    }
    gnetwork = backup;
    if (colordesc != xd->desc)
      mydelete(colordesc);
    mydelete(tempstr);
  }
}

/* iroffer-lamm: add-ons */
void a_addann(const userinput * const u)
{
  xdcc *xd;

  updatecontext();

  xd = a_add2(u, NULL);
  if (xd == NULL)
    return;

  a_make_announce("ANNOUNCE", number_of_pack(xd)); /* NOTRANSLATE */
}

void a_noannounce(const userinput * const u)
{
  unsigned int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.noannounce_start, num);
  a_respond_disabled(u, "ANNOUNCE" " has been", num);
}

void a_restart(const userinput * const UNUSED(u))
{
  ++(gdata.needsshutdown);
  ++(gdata.needrestart);
}

/* End of File */
