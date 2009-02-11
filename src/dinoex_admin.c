/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2009 Dirk Meyer
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
#include "dinoex_misc.h"

#include <ctype.h>

void a_respond(const userinput * const u, const char *format, ...)
{
  va_list args;
  gnetwork_t *backup;

  updatecontext();

  va_start(args, format);
  backup = gnetwork;
  gnetwork = &(gdata.networks[u->net]);

  switch (u->method)
    {
    case method_console:
      vioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR, format, args);
      break;
    case method_dcc:
      vwritedccchat(u->chat, 1, format, args);
      break;
    case method_out_all:
      vioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, format, args);
      break;
    case method_fd:
      {
        ssize_t retval;
        char tempstr[maxtextlength];
        int llen;

        llen = vsnprintf(tempstr, maxtextlength-3, format, args);
        if ((llen < 0) || (llen >= maxtextlength-3))
          {
            outerror(OUTERROR_TYPE_WARN, "string too long!");
            tempstr[0] = '\0';
            llen = 0;
          }

        if (!gdata.xdcclistfileraw)
          {
            removenonprintablectrl(tempstr);
          }

        if (gdata.dos_text_files)
          tempstr[llen++] = '\r';
        tempstr[llen++] = '\n';
        tempstr[llen] = '\0';

        retval = write(u->fd, tempstr, strlen(tempstr));
        if (retval < 0)
          {
            outerror(OUTERROR_TYPE_WARN_LOUD, "Write failed: %s", strerror(errno));
          }
      }
      break;
    case method_msg:
      vprivmsg(u->snick, format, args);
      break;
    default:
      break;
    }

  gnetwork = backup;
  va_end(args);
}

void a_parse_inputline(userinput * const u, const char *line)
{
  char *part[4] = { NULL, NULL, NULL, NULL };

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
  int net;

  /* default */
  if (arg1 == NULL)
    return 0;

  /* numeric */
  net = atoi(arg1);
  if ((net > 0) && (net <= gdata.networks_online))
    return --net;

  /* text */
  for (net=0; net<gdata.networks_online; net++) {
    if (gdata.networks[net].name == NULL)
      continue;
    if (strcasecmp(gdata.networks[net].name, arg1) == 0)
      return net;
  }

  /* unknown */
  return -1;
}

int hide_locked(const userinput * const u, const xdcc *xd)
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

int a_xdl_space(void)
{
  int i, s;
  xdcc *xd;

  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    i = max2(i, xd->gets);
  }
  s = 5;
  if (i < 10000) s = 4;
  if (i < 1000) s = 3;
  if (i < 100) s = 2;
  if (i < 10) s = 1;
  return s;
}

int a_xdl_left(void)
{
  int n;
  int l;

  n = irlist_size(&gdata.xdccs);
  l = 5;
  if (n < 10000) l = 4;
  if (n < 1000) l = 3;
  if (n < 100) l = 2;
  if (n < 10) l = 1;
  return l;
}

int reorder_new_groupdesc(const char *group, const char *desc)
{
  xdcc *xd;
  int k;

  updatecontext();

  k = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, group) != 0)
      continue;

    k++;
    /* delete all matching entires */
    if (xd->group_desc != NULL)
      mydelete(xd->group_desc);
    /* write only the first entry */
    if (k != 1)
      continue;

    if (desc && strlen(desc))
      xd->group_desc = mystrdup(desc);
  }

  return k;
}

static int reorder_groupdesc(const char *group)
{
  xdcc *xd;
  xdcc *firstxd;
  xdcc *descxd;
  int k;

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

    k++;
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

int add_default_groupdesc(const char *group)
{
  xdcc *xd;
  xdcc *firstxd;
  int k;

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

    k++;
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
    x ++;
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
       w++;
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

static int group_is_restricted(const userinput * const u, const char *group)
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

static int invalid_group(const userinput * const u, char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Group");
    return 1;
  }

  if (group_is_restricted(u, arg)) {
    a_respond(u, "You don't have access to this group");
    return 1;
  }

  return 0;
}

int invalid_dir(const userinput * const u, char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Directory");
    return 1;
  }
  convert_to_unix_slash(arg);
  return 0;
}

static int is_upload_file(const userinput * const u, const char *arg)
{
  if (file_uploading(arg)) {
    a_respond(u, "Upload still running");
    return 1;
  }
  return 0;
}

int invalid_file(const userinput * const u, char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Filename");
    return 1;
  }
  convert_to_unix_slash(arg);
  return 0;
}

static int invalid_pwd(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Password");
    return 1;
  }
  return 0;
}

int invalid_nick(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Nick");
    return 1;
  }
  return 0;
}

int invalid_message(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Message");
    return 1;
  }
  return 0;
}

static int invalid_announce(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Message (e.g. NEW)");
    return 1;
  }
  clean_quotes(u->arg1e);
  return 0;
}

int invalid_command(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Command");
    return 1;
  }
  clean_quotes(u->arg1e);
  return 0;
}

int invalid_channel(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Channel");
    return 1;
  }
  return 0;
}

int invalid_maxspeed(const userinput * const u, const char *arg)
{
  if (!arg || !strlen(arg)) {
    a_respond(u, "Try Specifying a Maxspeed");
    return 1;
  }
  return 0;
}

int invalid_pack(const userinput * const u, int num)
{
  if (num < 1 || num > irlist_size(&gdata.xdccs)) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return 1;
  }
  return 0;
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

int group_restricted(const userinput * const u, xdcc *xd)
{
  if (group_hidden(u, xd) == 0)
    return 0;

  a_respond(u, "You don't have access to this group");
  return 1;
}

static int queue_host_remove(const userinput * const u, irlist_t *list, const char *hostmask)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  char *hostmask2;
  int changed = 0;

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
    ioutput(CALLTYPE_NORMAL, OUT_L, COLOR_YELLOW,
            "Removed from the queue for \"%s\"", pq->xpack->desc);
    a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    changed ++;
  }

  return changed;
}

static int queue_nick_remove(const userinput * const u, irlist_t *list, int network, const char *nick)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  int changed = 0;

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
    ioutput(CALLTYPE_NORMAL, OUT_L, COLOR_YELLOW,
            "Removed from the queue for \"%s\"", pq->xpack->desc);
    a_respond(u, "Removed from the queue for \"%s\"", pq->xpack->desc);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    changed ++;
  }

  return changed;
}

void a_cancel_transfers(xdcc *xd, const char *msg)
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

static int a_remove_pack(const userinput * const u, xdcc *xd, int num)
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

  a_respond(u, "Removed Pack %i [%s]", num, xd->desc);

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

  set_support_groups();
  autotrigger_rebuild();
  write_files();
  return 0;
}

void a_remove_delayed(const userinput * const u)
{
  struct stat *st;
  xdcc *xd;
  gnetwork_t *backup;
  int n;

  updatecontext();

  backup = gnetwork;
  st = (struct stat *)(u->arg2);
  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs); xd; ) {
    n++;
    if ((xd->st_dev == st->st_dev) &&
        (xd->st_ino == st->st_ino)) {
      gnetwork = &(gdata.networks[u->net]);
      if (a_remove_pack(u, xd, n) == 0) {
        /* start over, the list has changed */
        n = 0;
        xd = irlist_get_head(&gdata.xdccs);
        continue;
      }
    }
    xd = irlist_get_next(xd);
  }
  gnetwork = backup;
}

static int a_set_group(const userinput * const u, xdcc *xd, int num, const char *group)
{
  const char *newgroup;
  char *tmpdesc;
  char *tmpgroup;
  int rc;

  updatecontext();

  if (num == 0) num = number_of_pack(xd);
  newgroup = "MAIN";
  if (group && strlen(group))
    newgroup = group;

  if (xd->group != NULL) {
    a_respond(u, "GROUP: [Pack %i] Old: %s New: %s",
              num, xd->group, newgroup);
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
    a_respond(u, "GROUP: [Pack %i] New: %s",
              num, newgroup);
  }

  if (group != newgroup)
    return 0;

  xd->group = mystrdup(group);
  reorder_groupdesc(group);
  rc = add_default_groupdesc(group);
  if (rc == 1)
    a_respond(u, "New GROUPDESC: %s", group);
  set_support_groups();
  return rc;
}

static int a_access_file(const userinput * const u, int xfiledescriptor, char **file, struct stat *st)
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

static int a_sort_null(const char *str1, const char *str2)
{
  if ((str1 == NULL) && (str2 == NULL))
    return 0;

  if (str1 == NULL)
    return -1;

  if (str2 == NULL)
    return 1;

  return strcasecmp(str1, str2);
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
      k ++;
      continue;
    case '+':
      xd3 = xd1;
      xd4 = xd2;
      k ++;
      break;
    case 'D':
    case 'd':
      rc = strcasecmp(xd3->desc, xd4->desc);
      if (rc != 0)
        return rc;
      break;
    case 'P':
    case 'p':
      rc = strcasecmp(xd3->file, xd4->file);
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
      rc = strcasecmp(getfilename(xd3->file), getfilename(xd4->file));
      if (rc != 0)
        return rc;
      break;
    }
    k = strchr(k, ' ');
    if (k == NULL)
      break;
    xd3 = xd1;
    xd4 = xd2;
    k ++;
  }
  return rc;
}

static void a_sort_insert(xdcc *xdo, const char *k)
{
  xdcc *xdn;
  char *tmpdesc;
  int n;

  n = 0;
  for (xdn = irlist_get_head(&gdata.xdccs);
       xdn;
       xdn = irlist_get_next(xdn)) {
    n++;
    if (a_sort_cmp(k, xdo, xdn) < 0)
      break;
  }
  if (xdn != NULL) {
    irlist_insert_before(&gdata.xdccs, xdo, xdn);
  } else {
    if (n == 0)
      irlist_insert_head(&gdata.xdccs, xdo);
    else
      irlist_insert_tail(&gdata.xdccs, xdo);
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

static void a_make_announce_short(const userinput * const u, int n)
{
  char *tempstr;
  userinput *ui;

  tempstr = mycalloc (maxtextlength);
  ui = mycalloc(sizeof(userinput));
  snprintf(tempstr, maxtextlength, "SANNOUNCE %i", n);
  a_fillwith_msg2(ui, NULL, tempstr);
  ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
  ui->net = u->net;
  ui->level = u->level;
  u_parseit(ui);
  mydelete(ui);
  mydelete(tempstr);
}

/* iroffer-lamm: autoaddann */
static void a_make_announce_long(const userinput * const u, int n)
{
  char *tempstr;
  userinput *ui;

  tempstr = mycalloc (maxtextlength);
  ui = mycalloc(sizeof(userinput));
  snprintf(tempstr, maxtextlength, "ANNOUNCE %i", n);
  a_fillwith_msg2(ui, NULL, tempstr);
  ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
  ui->net = u->net;
  ui->level = u->level;
  u_parseit(ui);
  mydelete(ui);
  mydelete(tempstr);
}

static void a_filedel_disk(const userinput * const u, char **file)
{
  int xfiledescriptor;
  struct stat st;

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

static xdcc *a_add2(const userinput * const u, const char *group)
{
  int xfiledescriptor;
  struct stat st;
  xdcc *xd;
  autoadd_group_t *ag;
  char *file;
  char *a1;
  char *a2;
  const char *newfile;
  int n;

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

  a_respond(u, "ADD PACK: [Pack %i] [File %s] Use CHDESC to change description",
            n, xd->file);

  if (group != NULL) {
    a_set_group(u, xd, n, group);
  }

  set_support_groups();
  write_files();

  if (gdata.autoaddann_short)
    a_make_announce_short(u, n);

  if (gdata.autoaddann)
    a_make_announce_long(u, n);

  return xd;
}

void a_add_delayed(const userinput * const u)
{
  userinput u2;

  updatecontext();

  a_respond(u, "  Adding %s:", u->arg1);

  u2 = *u;
  u2.arg1e = u->arg1;
  a_add2(&u2, u->arg3);
}

void a_xdlock(const userinput * const u)
{
  char *tempstr;
  int i;
  int l;
  int s;
  xdcc *xd;

  updatecontext();

  tempstr  = mycalloc(maxtextlength);

  l = a_xdl_left();
  s = a_xdl_space();
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    i++;
    if (xd->lock == NULL)
      continue;

    u_xdl_pack(u, tempstr, i, l, s, xd);
    a_respond(u, " \2^-\2%*sPassword: %s", s, "", xd->lock);
  }

  mydelete(tempstr);
}

void a_xdtrigger(const userinput * const u)
{
  char *tempstr;
  int i;
  int l;
  int s;
  xdcc *xd;

  updatecontext();

  tempstr  = mycalloc(maxtextlength);

  l = a_xdl_left();
  s = a_xdl_space();
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    i++;
    if (xd->trigger == NULL)
      continue;

    if (group_hidden(u, xd))
      continue;

    u_xdl_pack(u, tempstr, i, l, s, xd);
    a_respond(u, " \2^-\2%*sTrigger: %s", s, "", xd->trigger);
  }

  mydelete(tempstr);
}

void a_find(const userinput * const u)
{
  char *match;
  char *tempstr;
  int i;
  int k;
  int l;
  int s;
  xdcc *xd;

  updatecontext();

  if (!u->arg1e || !strlen(u->arg1e)) {
    a_respond(u, "Try Specifying a Pattern");
    return;
  }

  tempstr = mycalloc(maxtextlength);
  clean_quotes(u->arg1e);
  match = grep_to_fnmatch(u->arg1e);
  l = a_xdl_left();
  s = a_xdl_space();
  k = 0;
  i = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    i++;
    if (group_hidden(u, xd))
      continue;

    if (fnmatch_xdcc(match, xd)) {
      k++;
      u_xdl_pack(u, tempstr, i, l, s, xd);
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

static void a_qul2(const userinput * const u, irlist_t *list)
{
  ir_pqueue *pq;
  unsigned long rtime;
  int i;

  updatecontext();

  a_respond(u, "    #  User        Pack File                              Waiting     Left");
  i = 0;
  pq = irlist_get_head(list);
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    i++;
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

void a_listul(const userinput * const u)
{
  char *tempstr;
  size_t len;

  updatecontext();

  if (!gdata.uploaddir) {
    a_respond(u, "No uploaddir defined.");
    return;
  }

  if (u->arg1 == NULL) {
    u_listdir(u, gdata.uploaddir);
    return;
  }

  clean_quotes(u->arg1e);
  convert_to_unix_slash(u->arg1e);

  if (is_unsave_directory(u->arg1e)) {
    a_respond(u, "Try Specifying a Directory");
    return;
  }

  len = strlen(gdata.uploaddir) + strlen(u->arg1e) + 2;
  tempstr = mycalloc(len);
  snprintf(tempstr, len, "%s/%s", gdata.uploaddir, u->arg1e);
  u_listdir(u, tempstr);
  mydelete(tempstr);
}

void a_unlimited(const userinput * const u)
{
  int num = -1;
  transfer *tr;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  tr = does_tr_id_exist(num);
  if (tr == NULL) {
    a_respond(u, "Invalid ID number, Try \"DCL\" for a list");
    return;
  }

  tr->nomax = 1;
  tr->unlimited = 1;
}

void a_maxspeed(const userinput * const u)
{
  transfer *tr;
  float val;
  int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);

  if (invalid_maxspeed(u, u->arg2) != 0)
    return;

  tr = does_tr_id_exist(num);
  if (tr == NULL) {
    a_respond(u, "Invalid ID number, Try \"DCL\" for a list");
    return;
  }

  val = atof(u->arg2);
  a_respond(u, "MAXSPEED: [Transfer %i] Old: %1.1f New: %1.1f",
            num, tr->maxspeed, val);
  tr->maxspeed = val;
}

void a_slotsmax(const userinput * const u)
{
  int val;

  updatecontext();

  if (u->arg1) {
    val = atoi(u->arg1);
    gdata.slotsmax = between(1, val, MAXTRANS);
  }
  a_respond(u, "SLOTSMAX now %d", gdata.slotsmax);
}

void a_queuesize(const userinput * const u)
{
  int val;

  updatecontext();

  if (u->arg1) {
    val = atoi(u->arg1);
    gdata.queuesize = between(0, val, 1000000);
  }
  a_respond(u, "QUEUESIZE now %d", gdata.queuesize);
}

static void a_requeue2(const userinput * const u, irlist_t *list)
{
  int oldp = 0, newp = 0;
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

  a_respond(u, "** Moved Queue %i to %i", oldp, newp);

  /* get queue we are renumbering */
  pqo = irlist_get_nth(list, oldp-1);
  irlist_remove(list, pqo);

  if (newp == 1) {
    irlist_insert_head(list, pqo);
  } else {
    pqn = irlist_get_nth(list, newp-2);
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

void a_removedir_sub(const userinput * const u, const char *thedir, DIR *d)
{
  struct dirent *f;
  char *tempstr;
  userinput *u2;
  int thedirlen;

  updatecontext();

  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);

  if (!d) {
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
    return;
  }

  while ((f = readdir(d))) {
    struct stat st;
    int len = strlen(f->d_name);

    tempstr = mycalloc(len + thedirlen + 2);

    snprintf(tempstr, len + thedirlen + 2,
             "%s/%s", thedir, f->d_name);

    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access %s, ignoring: %s",
                tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if ((strcmp(f->d_name, ".") == 0 ) ||
          (strcmp(f->d_name, "..") == 0)) {
        mydelete(tempstr);
        continue;
      }
      if (gdata.include_subdirs != 0)
            a_removedir_sub(u, tempstr, NULL);
      mydelete(tempstr);
      continue;
    }
    if (!S_ISREG(st.st_mode)) {
      mydelete(tempstr);
      continue;
    }

    u2 = irlist_add(&gdata.packs_delayed, sizeof(userinput));
    u2->method = u->method;
    u2->fd = u->fd;
    u2->chat = u->chat;
    u2->cmd = mystrdup( "REMOVE" );
    u2->net = gnetwork->net;
    u2->level = u->level;

    u2->arg1 = tempstr;
    tempstr = NULL;

    if (u->snick != NULL) {
      u2->snick = mystrdup(u->snick);
    }

    u2->arg2 = mycalloc(sizeof(struct stat));
    memcpy(u2->arg2, &st, sizeof(struct stat));
  }

  closedir(d);
  return;
}

void a_remove(const userinput * const u)
{
  int num1 = 0;
  int num2 = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (u->arg2) num2 = atoi(u->arg2);
  if ( num2 < 0 || num2 > irlist_size(&gdata.xdccs) ) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return;
  }

  if (num2 == 0) {
    xd = irlist_get_nth(&gdata.xdccs, num1-1);
    if (group_restricted(u, xd))
      return;

    a_remove_pack(u, xd, num1);
    return;
  }

  if ( num2 < num1 ) {
    a_respond(u, "Pack numbers are not in order");
    return;
  }

  for (; num2 >= num1; num2--) {
    xd = irlist_get_nth(&gdata.xdccs, num2-1);
    if (group_restricted(u, xd))
      return;

    a_remove_pack(u, xd, num2);
  }
}

void a_removegroup(const userinput * const u)
{
  xdcc *xd;
  int n;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  n = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd) {
    n++;
    if (xd->group != NULL) {
      if (strcasecmp(xd->group, u->arg1) == 0) {
        if (a_remove_pack(u, xd, n) == 0) {
          /* start over, the list has changed */
          n = 0;
          xd = irlist_get_head(&gdata.xdccs);
          continue;
        }
      }
    }
    xd = irlist_get_next(xd);
  }
}

static void a_renumber1(const userinput * const u, int oldp, int newp)
{
  xdcc *xdo;
  xdcc *xdn;

  a_respond(u, "** Moved pack %i to %i", oldp, newp);
  /* get pack we are renumbering */
  xdo = irlist_get_nth(&gdata.xdccs, oldp-1);
  irlist_remove(&gdata.xdccs, xdo);
  if (newp == 1) {
    irlist_insert_head(&gdata.xdccs, xdo);
  } else {
    xdn = irlist_get_nth(&gdata.xdccs, newp-2);
    irlist_insert_after(&gdata.xdccs, xdo, xdn);
  }
  if (xdo->group != NULL)
    reorder_groupdesc(xdo->group);
}

void a_renumber3(const userinput * const u)
{
  int oldp = 0;
  int endp = 0;
  int newp = 0;

  updatecontext();

  if (u->arg1) oldp = atoi(u->arg1);
  if (invalid_pack(u, oldp) != 0)
    return;

  if (u->arg3) {
    if (u->arg2) endp = atoi(u->arg2);
    if (invalid_pack(u, endp) != 0)
      return;

    if (endp < oldp) {
      a_respond(u, "Invalid pack number");
      return;
    }

    if (u->arg3) newp = atoi(u->arg3);
  } else {
    endp = oldp;
    if (u->arg2) newp = atoi(u->arg2);
  }
  if (invalid_pack(u, newp) != 0)
    return;

  if ((newp >= oldp) && (newp <= endp)) {
    a_respond(u, "Invalid pack number");
    return;
  }

  while (oldp <= endp) {
    if (invalid_pack(u, newp) != 0)
      break;

    a_renumber1(u, oldp, newp);

    if (oldp > newp) {
      oldp++;
      newp++;
    } else {
      endp--;
    }
  }

  write_files();
}

void a_sort(const userinput * const u)
{
  irlist_t old_list;
  xdcc *xdo;
  const char *k = "filename";

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

  write_files();
}

int a_open_file(char **file, int mode)
{
  int xfiledescriptor;
  int n;
  char *adir;
  char *path;

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

int a_access_fstat(const userinput * const u, int xfiledescriptor, char **file, struct stat *st)
{
  if (a_access_file(u, xfiledescriptor, file, st))
    return 1;

  if ( st->st_size == 0 ) {
    a_respond(u, "File %s has size of 0 bytes!", *file);
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

void a_adddir_sub(const userinput * const u, const char *thedir, DIR *d, int onlynew, const char *setgroup, const char *match)
{
  userinput *u2;
  struct dirent *f;
  struct stat st;
  struct stat *sta;
  char *thefile, *tempstr;
  irlist_t dirlist = {0, 0, 0};
  int thedirlen;

  updatecontext();

  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);

  if (!d) {
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
    return;
  }

  while ((f = readdir(d))) {
    xdcc *xd;
    int len = strlen(f->d_name);
    int foundit;

    if (verifyshell(&gdata.adddir_exclude, f->d_name))
      continue;

    if (match != NULL) {
      if (fnmatch(match, f->d_name, FNM_CASEFOLD))
        continue;
    }
#ifdef USE_CURL
    if (fetch_is_running(f->d_name))
      continue;
#endif /* USE_CURL */

    tempstr = mycalloc(len + thedirlen + 2);
    snprintf(tempstr, len + thedirlen + 2,
             "%s/%s", thedir, f->d_name);

    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access %s, ignoring: %s",
                tempstr, strerror(errno));
        mydelete(tempstr);
        continue;
      }
      if (S_ISDIR(st.st_mode)) {
        if ((strcmp(f->d_name, ".") == 0 ) ||
            (strcmp(f->d_name, "..") == 0)) {
          mydelete(tempstr);
          continue;
        }
        if (gdata.include_subdirs == 0) {
          a_respond(u, "  Ignoring directory: %s", tempstr);
        } else {
          a_adddir_sub(u, tempstr, NULL, onlynew, setgroup, match);
        }
        mydelete(tempstr);
        continue;
      }
      if (file_uploading(f->d_name)) {
        mydelete(tempstr);
        continue;
      }
      if (file_uploading(tempstr)) {
        mydelete(tempstr);
        continue;
      }
      if (S_ISREG(st.st_mode)) {
        if (gdata.autoadd_delay) {
          if ((st.st_mtime + gdata.autoadd_delay) > gdata.curtime) {
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
              foundit = 1;
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
              foundit = 1;
              break;
            }
          }
        }

        if (foundit == 0) {
          thefile = irlist_add(&dirlist, len + thedirlen + 2);
          strcpy(thefile, tempstr);
        }
      }
      mydelete(tempstr);
  }
  closedir(d);

  if (irlist_size(&dirlist) == 0)
    return;

  irlist_sort(&dirlist, irlist_sort_cmpfunc_string);

  a_respond(u, "Adding %d files from dir %s",
            irlist_size(&dirlist), thedir);

  thefile = irlist_get_head(&dirlist);
  while (thefile) {
    u2 = irlist_add(&gdata.packs_delayed, sizeof(userinput));
    u2->method = u->method;
    u2->fd = u->fd;
    u2->chat = u->chat;
    u2->cmd = mystrdup( "ADD" );
    u2->net = gnetwork->net;
    u2->level = u->level;

    if (u2->snick != NULL) {
      u2->snick = mystrdup(u->snick);
    }

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

DIR *a_open_dir(char **dir)
{
  DIR *d;
  char *adir;
  char *path;
  int n;

  if ((*dir)[strlen(*dir)-1] == '/')
    (*dir)[strlen(*dir)-1] = 0;

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
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
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
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
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
  int thedirlen;
  int num;
  int foundit = 0;

  updatecontext();

  thedirlen = strlen(thedir);
  if (d == NULL)
    d = opendir(thedir);

  if (!d) {
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
    return;
  }

  while ((f = readdir(d))) {
    xdcc *xd;
    int len = strlen(f->d_name);

    if (verifyshell(&gdata.adddir_exclude, f->d_name))
      continue;

    tempstr = mycalloc(len + thedirlen + 2);

    snprintf(tempstr, len + thedirlen + 2,
             "%s/%s", thedir, f->d_name);

    if (stat(tempstr, &st) < 0) {
      a_respond(u, "cannot access %s, ignoring: %s",
                tempstr, strerror(errno));
      mydelete(tempstr);
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      if ((strcmp(f->d_name, ".") == 0 ) ||
          (strcmp(f->d_name, "..") == 0)) {
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
        num ++;
        if ((xd->st_dev == st.st_dev) &&
            (xd->st_ino == st.st_ino)) {
          foundit ++;
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
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
  } else {
    a_newgroup_sub(u, thedir, d, u->arg1);
  }
  mydelete(thedir);
  return;
}

void a_chtime(const userinput * const u)
{
  const char *format;
  char *oldstr;
  char *newstr;
  int num = 0;
  unsigned long val = 0;
  xdcc *xd;
  struct tm tmval;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  if (!u->arg2e || !strlen(u->arg2e)) {
    val = 0;
  } else {
    val = atoi(u->arg2e);
    if (val < 5000U) {
      format = gdata.http_date ? gdata.http_date : "%Y-%m-%d %H:%M";
      bzero((char *)&tmval, sizeof(tmval));
      clean_quotes(u->arg2e);
      strptime(u->arg2e, format, &tmval);
      val = mktime(&tmval);
    }
  }

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;

  newstr = mycalloc(maxtextlengthshort);
  user_getdatestr(newstr, val, maxtextlengthshort);
  oldstr = mycalloc(maxtextlengthshort);
  user_getdatestr(oldstr, xd->xtime, maxtextlengthshort);
  a_respond(u, "CHTIME: [Pack %i] Old: %s New: %s",
            num, oldstr, newstr);
  mydelete(oldstr);
  mydelete(newstr);
  xd->xtime = val;
  write_files();
}

void a_chlimit(const userinput * const u)
{
  int num = 0;
  int val = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    a_respond(u, "Try Specifying a daily Downloadlimit");
    return;
  }

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;

  val = atoi(u->arg2);

  a_respond(u, "CHLIMIT: [Pack %i] Old: %d New: %d",
            num, xd->dlimit_max, val);

  xd->dlimit_max = val;
  if (val == 0)
    xd->dlimit_used = 0;
  else
    xd->dlimit_used = xd->gets + xd->dlimit_max;

  write_files();
}

void a_chlimitinfo(const userinput * const u)
{
  int num = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    a_respond(u, "DLIMIT: [Pack %i] descr removed", num);
    mydelete(xd->dlimit_desc);
    xd->dlimit_desc = NULL;
  } else {
    clean_quotes(u->arg2e);
    a_respond(u, "DLIMIT: [Pack %i] descr: %s", num, u->arg2e);
    xd->dlimit_desc = mystrdup(u->arg2e);
  }

  write_files();
}

void a_chtrigger(const userinput * const u)
{
  int num = 0;
  xdcc *xd;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    mydelete(xd->trigger);
    a_respond(u, "TRIGGER: [Pack %i] removed", num);
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
    a_respond(u, "TRIGGER: [Pack %i] set: %s", num, u->arg2e);
  }

  write_files();
}

void a_lock(const userinput * const u)
{
  xdcc *xd;
  char *pass;
  int num1 = 0;
  int num2 = 0;

  updatecontext();

  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  pass = u->arg2;
  num2 = num1;
  if (u->arg3) {
    if (u->arg2) num2 = atoi(u->arg2);
    if (invalid_pack(u, num2) != 0)
      return;

    if (num2 == 0)
      num2 = num1;

    if ( num2 < num1 ) {
      a_respond(u, "Pack numbers are not in order");
      return;
    }

    pass = u->arg3;
  }

  if (invalid_pwd(u, pass) != 0)
    return;

  for (; num1 <= num2; num1++) {
    xd = irlist_get_nth(&gdata.xdccs, num1-1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "LOCK: [Pack %i] Password: %s", num1, pass);
    mydelete(xd->lock);
    xd->lock = mystrdup(pass);
  }

  write_files();
}

void a_unlock(const userinput * const u)
{
  xdcc *xd;
  int num1 = 0;
  int num2 = 0;

  updatecontext();

  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  num2 = num1;
  if (u->arg2) {
    num2 = atoi(u->arg2);
    if (invalid_pack(u, num2) != 0)
      return;
  }

  for (; num1 <= num2; num1++) {
    xd = irlist_get_nth(&gdata.xdccs, num1-1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "UNLOCK: [Pack %i]", num1);

    mydelete(xd->lock);
    xd->lock = NULL;
  }

  write_files();
}

void a_lockgroup(const userinput * const u)
{
  xdcc *xd;
  int n;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (invalid_pwd(u, u->arg2) != 0)
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    n++;
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, u->arg1) != 0)
      continue;

    a_respond(u, "LOCK: [Pack %i] Password: %s", n, u->arg2);
    mydelete(xd->lock);
    xd->lock = mystrdup(u->arg2);
  }
  write_files();
}

void a_unlockgroup(const userinput * const u)
{
  xdcc *xd;
  int n;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    n++;
    if (xd->group == NULL)
      continue;

    if (strcasecmp(xd->group, u->arg1) != 0)
      continue;

    a_respond(u, "UNLOCK: [Pack %i]", n);
    mydelete(xd->lock);
    xd->lock = NULL;
  }
  write_files();
}

void a_relock(const userinput * const u)
{
  xdcc *xd;
  int n;

  updatecontext();

  if (invalid_pwd(u, u->arg1) != 0)
    return;

  if (invalid_pwd(u, u->arg2) != 0)
    return;

  n = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    n++;
    if (xd->lock == NULL)
      continue;

    if (strcmp(xd->lock, u->arg1) != 0)
      continue;

    mydelete(xd->lock);
    a_respond(u, "LOCK: [Pack %i] Password: %s", n, u->arg2);
    xd->lock = mystrdup(u->arg2);
  }
  write_files();
}

void a_groupdesc(const userinput * const u)
{
  int k;

  updatecontext();

  if (invalid_group(u, u->arg1) != 0)
    return;

  if (u->arg2e && strlen(u->arg2e)) {
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
  int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;

  newgroup = u->arg2;
  if (!u->arg2 || !strlen(u->arg2)) {
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
  int num;
  int num1 = 0;
  int num2 = 0;

  updatecontext();

  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (u->arg2) num2 = atoi(u->arg2);
  if (invalid_pack(u, num2) != 0)
    return;

  if (u->arg3 && strlen(u->arg3)) {
    if (gdata.groupsincaps)
      caps(u->arg3);
  }

  for (num = num1; num <= num2; num ++) {
    xd = irlist_get_nth(&gdata.xdccs, num-1);
    if (group_restricted(u, xd))
      return;

    a_set_group(u, xd, num, u->arg3);
  }

  write_files();
}

void a_regroup(const userinput * const u)
{
  xdcc *xd;
  const char *g;
  int k;
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

  not_main = strcasecmp(u->arg2, "main");
  k = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL)
      g = xd->group;
    else
      g = "main";
    if (strcasecmp(g, u->arg1) == 0) {
      k++;
      if (xd->group != NULL)
        mydelete(xd->group);
      if (not_main)
        xd->group = mystrdup(u->arg2);
    }
  }

  if (k == 0)
    return;

  a_respond(u, "GROUP: Old: %s New: %s", u->arg1, u->arg2);
  reorder_groupdesc(u->arg2);
  if (not_main) {
    if (strcasecmp(u->arg1, "main") == 0)
      add_default_groupdesc(u->arg2);
  }
  write_files();
}

void a_md5(const userinput * const u)
{
  xdcc *xd;
  int von = 0;
  int bis;
  int num;

  updatecontext();

  /* show status */
  if (gdata.md5build.xpack) {
    a_respond(u, "calculating MD5/CRC32 for pack %d",
              number_of_pack(gdata.md5build.xpack));
  }

  if (!(u->arg1))
    return;

  von = atoi (u->arg1);
  if (von == 0)
    return;

  if (invalid_pack(u, von) != 0)
    return;

  bis = von;
  if (u->arg2) {
    bis = atoi (u->arg2);
    if (invalid_pack(u, bis) != 0)
      return;
  }

  for (num = von; num <= bis; num++) {
    xd = irlist_get_nth(&gdata.xdccs, num-1);
    if (group_restricted(u, xd))
      return;

    a_respond(u, "Rebuilding MD5 and CRC for Pack #%i:", num);
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
  int von = 0;
  int bis;
  int num;

  updatecontext();

  if (u->arg1) {
    von = atoi (u->arg1);
    if (invalid_pack(u, von) != 0)
      return;

    bis = von;
    if (u->arg2) {
      bis = atoi (u->arg2);
      if (invalid_pack(u, bis) != 0)
        return;
    }

    for (num = von; num <= bis; num++) {
      xd = irlist_get_nth(&gdata.xdccs, num-1);
      if (group_restricted(u, xd))
        return;

      a_respond(u, "Validating CRC for Pack #%i:", num);
      crcmsg = validate_crc32(xd, 0);
      if (crcmsg != NULL)
        a_respond(u, "[CRC32 Pack %d]: File '%s' %s.", num, xd->file, crcmsg);
    }
    return;
  }
  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (group_hidden(u, xd))
      continue;

    num ++;
    crcmsg = validate_crc32(xd, 1);
    if (crcmsg != NULL)
      a_respond(u, "[CRC32 Pack %d]: File '%s' %s.", num, xd->file, crcmsg);
  }
}

static int a_newdir_check(const userinput * const u, const char *dir1, const char *dir2, xdcc *xd)
{
  struct stat st;
  const char *off2;
  char *tempstr;
  int len1;
  int len2;
  int max;
  int xfiledescriptor;

  updatecontext();

  len1 = strlen(dir1);
  if ( strncmp(dir1, xd->file, len1) != 0 )
    return 0;

  off2 = xd->file + len1;
  len2 = strlen(off2);
  max = strlen(dir2) + len2 + 1;
  tempstr = mymalloc(max + 1);
  snprintf(tempstr, max,
           "%s%s", dir2, off2);

  xfiledescriptor = open(tempstr, O_RDONLY | ADDED_OPEN_FLAGS);
  if (a_access_fstat(u, xfiledescriptor, &tempstr, &st))
    return 0;

  a_respond(u, "CHFILE: [Pack %i] Old: %s New: %s",
            number_of_pack(xd), xd->file, tempstr);

  a_cancel_transfers(xd, "Pack file changed");

  mydelete(xd->file);
  xd->file = tempstr;
  xd->st_size  = st.st_size;
  xd->st_dev   = st.st_dev;
  xd->st_ino   = st.st_ino;
  xd->mtime    = st.st_mtime;

  cancel_md5_hash(xd, "CHFILE");
  return 1;
}

void a_newdir(const userinput * const u)
{
  xdcc *xd;
  int found;

  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    a_respond(u, "Try Specifying a new Directory");
    return;
  }

  convert_to_unix_slash(u->arg2);

  found = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    found += a_newdir_check(u, u->arg1, u->arg2, xd);
  }
  a_respond(u, "NEWDIR: %d Packs found", found);

  if (found > 0)
    write_files();
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
  int xfiledescriptor;
  struct stat st;
  char *file1;
  char *file2;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (invalid_file(u, u->arg1) != 0)
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    a_respond(u, "Try Specifying a new Filename");
    return;
  }

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

static int a_movefile_sub(const userinput * const u, xdcc *xd, const char *newfile)
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
  int num = 0;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (u->arg1) num = atoi(u->arg1);
  if (invalid_pack(u, num) != 0)
    return;

  if (!u->arg2 || !strlen(u->arg2)) {
    a_respond(u, "Try Specifying a new Filename");
    return;
  }

  xd = irlist_get_nth(&gdata.xdccs, num-1);
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
  int foundit;

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
    a_respond(u, "Can't Access Directory: %s", strerror(errno));
    return;
  }

  foundit = 0;
  tempstr = mycalloc(maxtextlength);
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (xd->group != NULL)
      g = xd->group;
    else
      g = "main";
    if (strcasecmp(g, u->arg1) == 0) {
      foundit++;
      if (u->arg2[strlen(u->arg2)-1] == '/')
        snprintf(tempstr, maxtextlength - 2, "%s%s", u->arg2, getfilename(xd->file));
      else
        snprintf(tempstr, maxtextlength - 2, "%s/%s", u->arg2, getfilename(xd->file));
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
    write_files();
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
  int num1 = 0;
  int num2 = 0;
  xdcc *xd;
  char *filename;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (u->arg1) num1 = atoi(u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (u->arg2) num2 = atoi(u->arg2);
  if ( num2 < 0 || num2 > irlist_size(&gdata.xdccs) ) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return;
  }

  if (num2 == 0)
    num2 = num1;

  if ( num2 < num1 ) {
    a_respond(u, "Pack numbers are not in order");
    return;
  }

  for (; num2 >= num1; num2--) {
    xd = irlist_get_nth(&gdata.xdccs, num2-1);
    if (group_restricted(u, xd))
      return;

    filename = mystrdup(xd->file);
    if (a_remove_pack(u, xd, num2) == 0)
      a_filedel_disk(u, &filename);
    mydelete(filename);
  }
}

void a_showdir(const userinput * const u)
{
  char *thedir;

  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (invalid_dir(u, u->arg1) != 0)
    return;

  if (u->arg1[strlen(u->arg1)-1] == '/') {
    u->arg1[strlen(u->arg1)-1] = '\0';
  }

  thedir = mystrdup(u->arg1);
  u_listdir(u, thedir);
  mydelete(thedir);
  return;
}

#ifdef USE_CURL
void a_fetch(const userinput * const u)
{
  updatecontext();

  if (disabled_config(u) != 0)
    return;

  if (!gdata.uploaddir) {
    a_respond(u, "No uploaddir defined.");
    return;
  }

  if (disk_full(gdata.uploaddir) != 0) {
    a_respond(u, "Not enough free space on disk.");
    return;
  }

  if (invalid_file(u, u->arg1) != 0)
    return;

  if (is_upload_file(u, u->arg1) != 0)
    return;

  if (!u->arg2e || !strlen(u->arg2e)) {
    a_respond(u, "Try Specifying a URL");
    return;
  }
  clean_quotes(u->arg2e);
  start_fetch_url(u);
}

void a_fetchcancel(const userinput * const u)
{
  int num = 0;

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
    privmsg_chan(ch, "%s", msg);
  }
}

void a_amsg(const userinput * const u)
{
  int ss;
  gnetwork_t *backup;

  updatecontext();

  if (invalid_announce(u, u->arg1e) != 0)
    return;

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    a_announce_channels(u->arg1e, NULL, NULL);
  }
  gnetwork = backup;
  a_respond(u, "Announced [%s]", u->arg1e);
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

  if (invalid_nick(u, name) != 0)
    return;

  if (invalid_message(u, msg) != 0)
    return;

  if (name[0] != '#') {
    privmsg_fast(name, "%s", msg);
    return;
  }

  ch = is_not_joined_channel(u, name);
  if (ch == NULL)
    return;

  privmsg_chan(ch, "%s", msg);
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

void a_bann_hostmask(const userinput * const u, const char *arg)
{
  transfer *tr;
  char *hostmask;
  int changed = 0;

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

void a_bannnick(const userinput * const u)
{
  gnetwork_t *backup;
  transfer *tr;
  char *nick;
  int changed = 0;
  int net = 0;

  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  if (!u->arg1 || !strlen(u->arg1)) {
    a_respond(u, "Try Specifying a Nick");
    return;
  }

  nick = u->arg1;
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);

  /* XDCC REMOVE */
  queue_nick_remove(u, &gdata.idlequeue, net, nick);
  changed = queue_nick_remove(u, &gdata.mainqueue, net, nick);
  if (changed >0)
    write_statefile();

  /* XDCC CANCEL */
  for (tr = irlist_get_head(&gdata.trans);
       tr;
       tr = irlist_get_next(tr)) {
    if (tr->tr_status == TRANSFER_STATUS_DONE)
      continue;

    if (tr->net != net)
      continue;

    if (strcasecmp(tr->nick, nick) != 0)
      continue;

    t_closeconn(tr, "Transfer canceled by admin", 0);
  }

  gnetwork = backup;
}

void a_acceptu(const userinput * const u)
{
  int min = 0;
  tupload_t *tu;
  const char *hostmask;

  updatecontext();

  if (u->arg1) min = atoi(u->arg1);
  hostmask = u->arg2 ? u->arg2 : "*!*@*";
  if (min > 0) {
    tu = irlist_add(&gdata.tuploadhost, sizeof(tupload_t));
    tu->u_host = mystrdup( hostmask );
    tu->u_time = gdata.curtime + (min * 60);
    a_respond(u, "Uploadhost %s valid for %d minutes", hostmask, min);
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
    case 1:
      a_respond(u, "queued");
      break;
    case 2:
      a_respond(u, "running");
      break;
    }
  }
}

void a_get(const userinput * const u)
{
  qupload_t *qu;
  int net;
  int found;

  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  if (invalid_nick(u, u->arg2) != 0)
    return;

  if (u->arg3e) {
    qu = irlist_add(&gdata.quploadhost, sizeof(qupload_t));
    qu->q_net = net;
    qu->q_nick = mystrdup(u->arg2);
    qu->q_pack = mystrdup(u->arg3e);
    qu->q_host = to_hostmask(qu->q_nick, "*");
    qu->q_time = gdata.curtime;
    a_respond(u, "GET %s started for %s on %s",
              qu->q_pack, qu->q_nick, gdata.networks[ qu->q_net ].name);
    qu->q_state = 1;
    start_qupload();
    return;
  }
  found = close_qupload(net, u->arg2);
  if ( found > 0 )
    a_respond(u, "GET %s deactivated", u->arg2);
}

static void a_rmq2(const userinput * const u, irlist_t *list)
{
  int num = 0;
  ir_pqueue *pq;
  gnetwork_t *backup;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  if (num < 1) {
    a_respond(u, "Invalid ID number, Try \"QUL\" for a list");
    return;
  }

  pq = irlist_get_nth(list, num-1);
  if (!pq) {
     a_respond(u, "Invalid ID number, Try \"QUL\" for a list");
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
  writeserver(WRITESERVER_NOW, "%s", u->arg2e);
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
      writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
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
  int ss;

  updatecontext();

  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  backup = gnetwork;
  if (net < 0) {
    for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      a_hop_net(u, u->arg1);
    }
  } else {
    gnetwork = &(gdata.networks[net]);
    a_hop_net(u, u->arg1);
  }
  gnetwork = backup;
}

void a_nochannel(const userinput * const u)
{
  channel_t *ch;
  gnetwork_t *backup;
  int ss;
  int val = 0;

  updatecontext();

  if (u->arg1) val = atoi(u->arg1);

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    /* part channels */
    for (ch = irlist_get_head(&(gnetwork->channels));
         ch;
         ch = irlist_get_next(ch)) {
      if ((!u->arg2) || (!strcasecmp(u->arg2, ch->name))) {
        writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
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
  ch->noannounce = 1;
  ch->notrigger = 1;
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

    writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
    clearmemberlist(ch);
    free_channel_data(ch);
    irlist_delete(&(gnetwork->channels), ch);
    gnetwork = backup;
    return;
  }
  a_respond(u, "Bot not in Channel %s on %s", u->arg1, gnetwork->name);
  gnetwork = backup;
}

void a_servqc(const userinput * const u)
{
  gnetwork_t *backup;
  int ss;

  updatecontext();

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    a_respond(u, "Cleared server queue of %d lines",
              irlist_size(&gdata.networks[ss].serverq_channel) +
              irlist_size(&gdata.networks[ss].serverq_fast) +
              irlist_size(&gdata.networks[ss].serverq_normal) +
              irlist_size(&gdata.networks[ss].serverq_slow));

    gnetwork = &(gdata.networks[ss]);
    cleanannounce();
    gnetwork = backup;
    irlist_delete_all(&gdata.networks[ss].serverq_channel);
    irlist_delete_all(&gdata.networks[ss].serverq_fast);
    irlist_delete_all(&gdata.networks[ss].serverq_normal);
    irlist_delete_all(&gdata.networks[ss].serverq_slow);
  }
}

void a_nomd5(const userinput * const u)
{
  int num = 0;

  updatecontext();

  if (u->arg1) num = atoi(u->arg1);
  num = max_minutes_waits(&gdata.nomd5_start, num);
  a_respond(u, "** MD5 and CRC checksums have been disabled for the next %i %s", num, num!=1 ? "minutes" : "minute");
}

void a_cleargets(const userinput * const u)
{
  xdcc *xd;

  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    xd->gets = 0;
  }

  a_respond(u, "Cleared download counter for each pack");
  write_files();
}

void a_config(const userinput * const u)
{
  if (u->method != method_console) {
    if (!gdata.direct_config_access) {
      a_respond(u, "Disabled in Config");
      return;
    }
  }

  if (!u->arg1e || !strlen(u->arg1e)) {
    a_respond(u, "Try Specifying a Key");
    return;
  }

  current_config = "ADMIN";
  current_line = 0;
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

  if (!u->arg1 || !strlen(u->arg1)) {
    a_respond(u, "Try Specifying a Key");
    return;
  }

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
  int val;
  int i;

  updatecontext();

  val = (gdata.holdqueue) ? 0 : 1;
  if (u->arg1) val = atoi(u->arg1);

  gdata.holdqueue = val;
  a_respond(u, "HOLDQUEUE now %d", val);

  if (val != 0)
    return;

  for (i=0; i<100; i++) {
    if (!gdata.exiting &&
        irlist_size(&gdata.mainqueue) &&
        (irlist_size(&gdata.trans) < min2(MAXTRANS, gdata.slotsmax))) {
      sendaqueue(0, 0, NULL);
    }
  }
}

void a_dump(const userinput * const u)
{
  dumpgdata();
  a_respond(u, "DUMP written into logfile.");
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

  gdata.background = 1;
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

void a_autogroup(const userinput * const u)
{
  char *tempstr;
  char *newgroup;
  xdcc *xd;
  int num;

  num = 0;
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    num ++;
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

    newgroup ++;
    if (strlen(newgroup) == 0) {
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

/* this function imported from iroffer-lamm */
void a_queue(const userinput * const u)
{
  int num = 0;
  int alreadytrans;
  xdcc *xd;
  char *tempstr;
  const char *hostname = "man";
  gnetwork_t *backup;
  int net;
  int inq;

  updatecontext();

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_nick(u, u->arg1) != 0)
    return;

  if (u->arg2) num = atoi(u->arg2);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);

  alreadytrans = queue_count_host(&gdata.mainqueue, &inq, 1, u->arg1, hostname, xd);
  alreadytrans += queue_count_host(&gdata.idlequeue, &inq, 1, u->arg1, hostname, xd);
  if (alreadytrans > 0) {
    a_respond(u, "Already Queued %s for Pack %i!", u->arg1, num);
    return;
  }

  a_respond(u, "Queuing %s for Pack %i", u->arg1, num);

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  tempstr = addtoqueue(u->arg1, hostname, num);
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "%s (%s on %s)",
          u->arg1, hostname, gnetwork->name);
  notice(u->arg1, "** %s", tempstr);
  mydelete(tempstr);
  gnetwork = backup;

  if (!gdata.exiting &&
      irlist_size(&gdata.mainqueue) &&
      (irlist_size(&gdata.trans) < min2(MAXTRANS, gdata.slotsmax))) {
    sendaqueue(0, 0, NULL);
  }
}

void a_iqueue(const userinput * const u)
{
  int num = 0;
  int alreadytrans;
  xdcc *xd;
  char *tempstr;
  const char *hostname = "man";
  gnetwork_t *backup;
  int net;
  int inq;

  updatecontext();

  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_nick(u, u->arg1) != 0)
    return;

  if (u->arg2) num = atoi(u->arg2);
  if (invalid_pack(u, num) != 0)
    return;

  xd = irlist_get_nth(&gdata.xdccs, num-1);

  alreadytrans = queue_count_host(&gdata.mainqueue, &inq, 1, u->arg1, hostname, xd);
  alreadytrans += queue_count_host(&gdata.idlequeue, &inq, 1, u->arg1, hostname, xd);
  if (alreadytrans > 0) {
      a_respond(u, "Already Queued %s for Pack %i!", u->arg1, num);
      return;
  }

  a_respond(u, "Queuing %s for Pack %i", u->arg1, num);

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  tempstr = mycalloc(maxtextlength);
  tempstr = addtoidlequeue(tempstr, u->arg1, hostname, xd, num, 0);
  ioutput(CALLTYPE_MULTI_END, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "%s (%s on %s)",
          u->arg1, hostname, gnetwork->name);
  notice(u->arg1, "** %s", tempstr);
  mydelete(tempstr);
  gnetwork = backup;

  if (!gdata.exiting &&
       irlist_size(&gdata.idlequeue) &&
       (irlist_size(&gdata.trans) < min2(MAXTRANS, gdata.slotsmax)))
     {
       sendaqueue(0, 0, NULL);
  }
}

static void a_announce_msg(const userinput * const u, const char *match, int num, const char *msg)
{
  gnetwork_t *backup;
  xdcc *xd;
  char *tempstr;
  char *tempstr2;
  char *tempstr3;
  char *datestr;
  int ss;

  xd = irlist_get_nth(&gdata.xdccs, num - 1);
  if (group_restricted(u, xd))
    return;

  a_respond(u, "Pack Info for Pack #%i:", num);
  tempstr = mycalloc(maxtextlength);
  tempstr2 = mycalloc(maxtextlength);
  tempstr3 = mycalloc(maxtextlength);
  if (gdata.show_date_added) {
    datestr = mycalloc(maxtextlengthshort);
    user_getdatestr(datestr, xd->xtime ? xd->xtime : xd->mtime, maxtextlengthshort - 1);
    snprintf(tempstr3, maxtextlength - 1, "%s%s%s%s",
             gdata.announce_seperator, datestr, gdata.announce_seperator, xd->desc);
    mydelete(datestr);
  } else {
    snprintf(tempstr3, maxtextlength - 1, "%s%s",
             gdata.announce_seperator, xd->desc);
  }
  if (msg == NULL) {
    msg = gdata.autoaddann;
    if (msg == NULL)
      msg = "added";
  }
  snprintf(tempstr2, maxtextlength-2, "[\2%s\2]%s", msg, tempstr3);

  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
    gnetwork = &(gdata.networks[ss]);
    snprintf(tempstr, maxtextlength-2, "%s - /MSG %s XDCC SEND %i",
             tempstr2, get_user_nick(), num);
    a_announce_channels(tempstr, match, xd->group);
    gnetwork = backup;
    a_respond(u, "Announced [%s]%s", msg, tempstr3);
  }
  gnetwork = backup;
  mydelete(tempstr3);
  mydelete(tempstr2);
  mydelete(tempstr);
}

static void a_announce_sub(const userinput * const u, const char *arg1, const char *arg2, char *msg)
{
  int num1 = 0;
  int num2 = 0;

  updatecontext();

  if (arg1) num1 = atoi (arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (arg2) num2 = atoi(arg2);
  if (invalid_pack(u, num2) != 0)
    return;

  if ( num2 < num1 ) {
    a_respond(u, "Pack numbers are not in order");
    return;
  }

  if (msg != NULL)
    clean_quotes(msg);

  for (; num1 <= num2; num1++) {
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

void a_cannounce(const userinput * const u)
{
  int num = 0;

  updatecontext();

  if (invalid_channel(u, u->arg1) != 0)
    return;

  if (u->arg2) num = atoi (u->arg2);
  if (invalid_pack(u, num) != 0)
    return;

  a_announce_msg(u, u->arg1, num, u->arg3e);
}

void a_sannounce(const userinput * const u)
{
  int num1 = 0;
  int num2 = 0;
  xdcc *xd;
  char *tempstr;
  char *tempstr3;
  int ss;
  gnetwork_t *backup;

  updatecontext();

  if (u->arg1) num1 = atoi (u->arg1);
  if (invalid_pack(u, num1) != 0)
    return;

  if (u->arg2) num2 = atoi(u->arg2);
  if ( num2 < 0 || num2 > irlist_size(&gdata.xdccs) ) {
    a_respond(u, "Try Specifying a Valid Pack Number");
    return;
  }

  if (num2 == 0)
    num2 = num1;

  if ( num2 < num1 ) {
    a_respond(u, "Pack numbers are not in order");
    return;
  }

  for (; num1 <= num2; num1++) {
    a_respond(u, "Pack Info for Pack #%i:", num1);
    xd = irlist_get_nth(&gdata.xdccs, num1-1);
    if (group_restricted(u, xd))
      return;

    tempstr = mycalloc(maxtextlength);
    tempstr3 = mycalloc(maxtextlength);
    snprintf(tempstr3, maxtextlength - 1, "%s%s", gdata.announce_seperator, xd->desc);
    snprintf(tempstr, maxtextlength-2, "\2%i\2%s", num1, tempstr3);

    backup = gnetwork;
    for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      a_announce_channels(tempstr, NULL, xd->group);
      gnetwork = backup;
      a_respond(u, "Announced [%i]%s", num1, tempstr3);
    }
    gnetwork = backup;
    mydelete(tempstr);
    mydelete(tempstr3);
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

  a_make_announce_long(u, number_of_pack(xd));
}

/* End of File */
