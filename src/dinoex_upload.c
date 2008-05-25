/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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
#include "dinoex_upload.h"
#include "dinoex_irc.h"
#include "dinoex_curl.h"
#include "dinoex_misc.h"

int l_setup_file(upload * const l, struct stat *stp)
{
  char *fullfile;
  int retval;

  updatecontext();

  if (gdata.uploaddir == NULL) {
    l_closeconn(l, "No uploaddir defined.", 0);
    return 1;
  }

  /* local file already exists? */
  fullfile = mymalloc(strlen(gdata.uploaddir) + strlen(l->file) + 2);
  sprintf(fullfile, "%s/%s", gdata.uploaddir, l->file);

  l->filedescriptor = open(fullfile,
                           O_WRONLY | O_CREAT | O_EXCL | ADDED_OPEN_FLAGS,
                           CREAT_PERMISSIONS );

  if ((l->filedescriptor < 0) && (errno == EEXIST)) {
    retval = stat(fullfile, stp);
    if (retval < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Stat Upload File '%s': %s",
               fullfile, strerror(errno));
      l_closeconn(l, "File Error, File couldn't be opened for writing", errno);
      mydelete(fullfile);
      return 1;
    }
    if (!S_ISREG(stp->st_mode) || (stp->st_size >= l->totalsize)) {
      l_closeconn(l, "File Error, That filename already exists", 0);
      mydelete(fullfile);
      return 1;
    }
    l->filedescriptor = open(fullfile, O_WRONLY | O_APPEND | ADDED_OPEN_FLAGS);
    if (l->filedescriptor >= 0) {
      l->resumesize = l->bytesgot = stp->st_size;
      if (l->resumed <= 0) {
        close(l->filedescriptor);
        mydelete(fullfile);
        return 2; /* RESUME */
      }
    }
  }
  if (l->filedescriptor < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access Upload File '%s': %s",
             fullfile, strerror(errno));
    l_closeconn(l, "File Error, File couldn't be opened for writing", errno);
    mydelete(fullfile);
    return 1;
  }
  mydelete(fullfile);
  return 0;
}

int l_setup_listen(upload * const l)
{
  char *tempstr;
  char *msg;
  int rc;

  updatecontext();

  rc = irc_open_listen(&(l->con));
  if (rc != 0) {
    l_closeconn(l, "Connection Lost", 0);
    return 1;
  }

  msg = setup_dcc_local(&(l->con.local));
  tempstr = getsendname(l->file);
  privmsg_fast(l->nick, "\1DCC SEND %s %s %" LLPRINTFMT "u %d\1",
               tempstr, msg, l->totalsize, l->token);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND sent to %s on %s, waiting for connection on %s",
          l->nick, gnetwork->name, msg);
  mydelete(tempstr);
  mydelete(msg);

  l->ul_status = UPLOAD_STATUS_LISTENING;
  return 0;
}

int l_setup_passive(upload * const l, char *token)
{
  char *tempstr;
  struct stat s;
  int retval;

  updatecontext();

  /* strip T */
  if (token != NULL) {
    if (token[strlen(token)-1] == 'T')
      token[strlen(token)-1] = '\0';
    l->token = atoi(token);
  } else {
    l->token = 0;
  }

  retval = l_setup_file(l, &s);
  if (retval == 2)
    {
      tempstr = getsendname(l->file);
      privmsg_fast(l->nick, "\1DCC RESUME %s %i %" LLPRINTFMT "u %d\1",
                   tempstr, l->con.remoteport, (unsigned long long)s.st_size, l->token);
      mydelete(tempstr);
      l->con.connecttime = gdata.curtime;
      l->con.lastcontact = gdata.curtime;
      l->ul_status = UPLOAD_STATUS_RESUME;
      return 0;
    }
  if (retval != 0)
    {
      return 1;
    }

  return l_setup_listen(l);
}

void l_setup_accept(upload * const l)
{
  SIGNEDSOCK int addrlen;
  char *msg;

  updatecontext();

  addrlen = sizeof(l->con.remote);
  if ((l->con.clientsocket = accept(l->con.listensocket, &(l->con.remote.sa), &addrlen)) < 0) {
    outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
    l->con.clientsocket = FD_UNUSED;
    l_closeconn(l, "Connection Lost", 0);
    return;
  }

  ir_listen_port_connected(l->con.localport);

  FD_CLR(l->con.listensocket, &gdata.readset);
  close(l->con.listensocket);
  l->con.clientsocket = FD_UNUSED;

  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND connection received");

  if (set_socket_nonblocking(l->con.clientsocket, 1) < 0 ) {
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
  }

  notice(l->nick, "DCC Send Accepted, Connecting...");

  msg = mycalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(l->con.remote.sa), addrlen);
  l->con.remoteaddr = mystrdup(msg);
  mydelete(msg);
  l->con.remoteport = get_port(&(l->con.remote));
  l->con.connecttime = gdata.curtime;
  l->con.lastcontact = gdata.curtime;
  l->ul_status = UPLOAD_STATUS_GETTING;
}

int l_select_fdset(int highests)
{
  upload *ul;

  for (ul = irlist_get_head(&gdata.uploads);
       ul;
       ul = irlist_get_next(ul)) {
    if (ul->ul_status == UPLOAD_STATUS_GETTING) {
      FD_SET(ul->con.clientsocket, &gdata.readset);
      highests = max2(highests, ul->con.clientsocket);
      continue;
    }
    if (ul->ul_status == UPLOAD_STATUS_CONNECTING) {
      FD_SET(ul->con.clientsocket, &gdata.writeset);
      highests = max2(highests, ul->con.clientsocket);
      continue;
    }
    if (ul->ul_status == UPLOAD_STATUS_LISTENING) {
      FD_SET(ul->con.listensocket, &gdata.readset);
      highests = max2(highests, ul->con.listensocket);
      continue;
    }
  }
  return highests;
}

int file_uploading(const char *file)
{
  upload *ul;

  for (ul = irlist_get_head(&gdata.uploads);
       ul;
       ul = irlist_get_next(ul)) {
    if (ul->ul_status == UPLOAD_STATUS_DONE)
      continue;

    if (ul->file == NULL)
      continue;

    if (strcmp(ul->file, file))
      continue;

    return 1;
  }
#ifdef USE_CURL
  return fetch_is_running(file);
#else
  return 0;
#endif /* USE_CURL */
}

void upload_start(const char *nick, const char *hostname, const char *hostmask,
                  const char *filename, const char *remoteip, const char *remoteport, const char *bytes, char *token)
{
  upload *ul;
  char *tempstr;
  off_t len;

  if (verify_uploadhost(hostmask)) {
    notice(nick, "DCC Send Denied, I don't accept transfers from %s", hostmask);
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  len = atoull(bytes);
  if (gdata.uploadmaxsize && (len > gdata.uploadmaxsize)) {
    notice(nick, "DCC Send Denied, I don't accept transfers that big");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (Too Big) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  if (len > gdata.max_file_size) {
    notice(nick, "DCC Send Denied, I can't accept transfers that large");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (Too Large) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  if (irlist_size(&gdata.uploads) >= MAXUPLDS) {
    notice(nick, "DCC Send Denied, I'm already getting too many files");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (too many uploads) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  if (irlist_size(&gdata.uploads) >= gdata.max_uploads) {
    notice(nick, "DCC Send Denied, I'm already getting too many files");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (too many uploads) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  if (disk_full(gdata.uploaddir) != 0) {
    notice(nick, "DCC Send Denied, not enough free space on disk");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (not enough free space on disk) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  if (file_uploading(filename) != 0) {
    notice(nick, "DCC Send Denied, I'm already getting this file");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied (upload running) from %s on %s",
            hostmask, gnetwork->name);
    return;
  }
  ul = irlist_add(&gdata.uploads, sizeof(upload));
  l_initvalues(ul);
  ul->file = mystrdup(filename);
  ul->con.family = (strchr(remoteip, ':')) ? AF_INET6 : AF_INET;
  ul->con.remoteaddr = mystrdup(remoteip);
  ul->con.remoteport = atoi(remoteport);
  ul->totalsize = len;
  ul->nick = mystrdup(nick);
  ul->hostname = mystrdup(hostname);
  ul->net = gnetwork->net;
  tempstr = getsendname(ul->file);
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "DCC Send Accepted from %s on %s: %s (%" LLPRINTFMT "uKB)",
          nick, gnetwork->name, tempstr,
          (ul->totalsize / 1024));
  mydelete(tempstr);

  if (ul->con.remoteport > 0) {
    l_establishcon(ul);
  } else {
    /* Passive DCC */
    l_setup_passive(ul, token);
  }
}

/* End of File */
