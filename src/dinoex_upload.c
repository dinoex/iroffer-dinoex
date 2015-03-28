/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2013 Dirk Meyer
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
#include "dinoex_kqueue.h"
#include "dinoex_upload.h"
#include "dinoex_irc.h"
#include "dinoex_curl.h"
#include "dinoex_jobs.h"
#include "dinoex_misc.h"

/* search for a custom uploaddir of a group admin */
static char *verifyupload_group(const char *hostmask)
{
  group_admin_t *ga;

  updatecontext();

  for (ga = irlist_get_head(&gdata.group_admin);
       ga;
       ga = irlist_get_next(ga)) {

    if (fnmatch(ga->g_host, hostmask, FNM_CASEFOLD) == 0)
      return ga->g_uploaddir;
  }
  return NULL;
}

/* returns custom uploaddir or default */
char *get_uploaddir(const char *hostmask)
{
  char *group_uploaddir;

  if (hostmask != NULL) {
    group_uploaddir = verifyupload_group(hostmask);
    if (group_uploaddir != NULL)
      return group_uploaddir;
  }
  return gdata.uploaddir;
}

/* open file on disk for upload */
int l_setup_file(upload * const l, struct stat *stp)
{
  char *fullfile;
  int retval;

  updatecontext();

  if (l->uploaddir == NULL) {
    l_closeconn(l, "No uploaddir defined.", 0);
    return 1;
  }

  /* local file already exists? */
  fullfile = mystrjoin(l->uploaddir, l->file, '/');

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

static int l_setup_listen(upload * const l)
{
  char *tempstr;
  char *msg;
  unsigned int rc;

  updatecontext();

  rc = irc_open_listen(&(l->con));
  if (rc != 0) {
    l_closeconn(l, "Connection Lost", 0);
    return 1;
  }

  msg = setup_dcc_local(&(l->con.local));
  tempstr = getsendname(l->file);
  privmsg_fast(l->nick, IRC_CTCP "DCC SEND %s %s %" LLPRINTFMT "d %u" IRC_CTCP, /* NOTRANSLATE */
               tempstr, msg, l->totalsize, l->token);
  mydelete(msg);
  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(l->con.local.sa));
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND sent to %s on %s, waiting for connection on %s",
          l->nick, gnetwork->name, msg);
  mydelete(tempstr);
  mydelete(msg);

  l->ul_status = UPLOAD_STATUS_LISTENING;
  return 0;
}

/* open new port for incoming connection */
int l_setup_passive(upload * const l, char *token)
{
  char *tempstr;
  struct stat s;
  size_t len;
  int retval;

  updatecontext();

  /* strip T */
  if (token != NULL) {
    len = strlen(token) - 1;
    if (token[len] == 'T')
      token[len] = '\0';
    l->token = atoi(token);
  } else {
    l->token = 0;
  }

  retval = l_setup_file(l, &s);
  if (retval == 2)
    {
      tempstr = getsendname(l->file);
      privmsg_fast(l->nick, IRC_CTCP "DCC RESUME %s %d %" LLPRINTFMT "d %u" IRC_CTCP, /* NOTRANSLATE */
                   tempstr, l->con.remoteport, s.st_size, l->token);
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

/* accept incoming connection */
static void l_setup_accept(upload * const l)
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

  event_close(l->con.listensocket);
  l->con.listensocket = FD_UNUSED;

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC SEND connection received");

  ir_setsockopt(l->con.clientsocket);

  notice(l->nick, "DCC Send Accepted, Connecting...");

  msg = mymalloc(maxtextlength);
  my_getnameinfo(msg, maxtextlength -1, &(l->con.remote.sa));
  mydelete(l->con.remoteaddr);
  l->con.remoteaddr = mystrdup(msg);
  mydelete(msg);
  l->con.remoteport = get_port(&(l->con.remote));
  l->con.connecttime = gdata.curtime;
  l->con.lastcontact = gdata.curtime;
  l->ul_status = UPLOAD_STATUS_GETTING;
}

/* returns a text for the state of the upload */
const char *l_print_state(upload * const l)
{
  switch (l->ul_status) {
  case UPLOAD_STATUS_CONNECTING:
    return "Connecting";
  case UPLOAD_STATUS_GETTING:
    return "Getting";
  case UPLOAD_STATUS_WAITING:
    return "Finishing";
  case UPLOAD_STATUS_DONE:
    return "Done";
  case UPLOAD_STATUS_RESUME:
    return "Resume";
  case UPLOAD_STATUS_LISTENING:
    return "Listening";
  case UPLOAD_STATUS_UNUSED:
  default:
    return "Unknown!";
  }
}

/* register active connections for select() */
int l_select_fdset(int highests, int changequartersec)
{
  upload *ul;

  for (ul = irlist_get_head(&gdata.uploads);
       ul;
       ul = irlist_get_next(ul)) {
    if (ul->ul_status == UPLOAD_STATUS_GETTING) {
      if (!ul->overlimit) {
        FD_SET(ul->con.clientsocket, &gdata.readset);
        highests = max2(highests, ul->con.clientsocket);
        continue;
      }
      if (changequartersec) {
        FD_SET(ul->con.clientsocket, &gdata.readset);
        highests = max2(highests, ul->con.clientsocket);
      }
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

/* perfrom io for upload */
void l_perform(int changesec)
{
  upload *ul;

  updatecontext();

  ul = irlist_get_head(&gdata.uploads);
  while(ul) {
    gnetwork = &(gdata.networks[ul->net]);

    /*----- see if uploads are sending anything to us ----- */
    if (ul->ul_status == UPLOAD_STATUS_GETTING) {
      if (FD_ISSET(ul->con.clientsocket, &gdata.readset)) {
        l_transfersome(ul);
      }
    }

    if (ul->ul_status == UPLOAD_STATUS_LISTENING) {
      if (FD_ISSET(ul->con.listensocket, &gdata.readset)) {
        l_setup_accept(ul);
      }
    }

    if (ul->ul_status == UPLOAD_STATUS_CONNECTING) {
      if (FD_ISSET(ul->con.clientsocket, &gdata.writeset)) {
        int callval_i;
        int connect_error;
        SIGNEDSOCK int connect_error_len = sizeof(connect_error);

        callval_i = getsockopt(ul->con.clientsocket,
                               SOL_SOCKET, SO_ERROR,
                               &connect_error, &connect_error_len);

        if (callval_i < 0) {
          int errno2 = errno;
          outerror(OUTERROR_TYPE_WARN,
                   "Couldn't determine upload connection status on %s: %s",
                   gnetwork->name, strerror(errno));
          l_closeconn(ul, "Upload Connection Failed status:", errno2);
          continue;
        }
        if (connect_error) {
          l_closeconn(ul, "Upload Connection Failed", connect_error);
          continue;
        }
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                          "Upload Connection Established on %s", gnetwork->name);
        ul->ul_status = UPLOAD_STATUS_GETTING;
        FD_CLR(ul->con.clientsocket, &gdata.writeset);
        notice(ul->nick, "DCC Connection Established");
        ul->con.connecttime = gdata.curtime;
      }
      if (changesec && ul->con.lastcontact + CTIMEOUT < gdata.curtime) {
        l_closeconn(ul, "Upload Connection Timed Out", 0);
        continue;
      }
    }

    if (changesec) {
      l_istimeout(ul);

      if (ul->ul_status == UPLOAD_STATUS_DONE) {
        unsigned int net = ul->net;
        char *nick = ul->nick;
        ul->nick = NULL;
        mydelete(ul->hostname);
        mydelete(ul->uploaddir);
        mydelete(ul->file);
        mydelete(ul->con.remoteaddr);
        ul = irlist_delete(&gdata.uploads, ul);
	close_qupload(net, nick);
        mydelete(nick);
        continue;
      }
    }
    ul = irlist_get_next(ul);
  }
  gnetwork = NULL;
}

/* check if a filename is already in a upload */
unsigned int file_uploading(const char *file)
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

static void error_upload_start(const char *nick, const char *hostmask, const char *key, const char *msg)
{
  notice(nick, "DCC Send Denied, %s", msg);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
          "DCC Send Denied (%s) from %s on %s",
          key, hostmask, gnetwork->name);
}

/* check if this host is allowed to send */
static int verify_uploadhost(const char *hostmask)
{
  tupload_t *tu;
  qupload_t *qu;

  updatecontext();

  if ( verifyupload_group(hostmask) != NULL )
    return 0;

  if ( verifyshell(&gdata.uploadhost, hostmask) != 0 )
    return 0;

  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu;
       tu = irlist_get_next(tu)) {
    if (tu->u_time <= gdata.curtime)
      continue;

    if (fnmatch(tu->u_host, hostmask, FNM_CASEFOLD) == 0)
      return 0;
  }
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (gnetwork->net != qu->q_net)
      continue;

    if (fnmatch(qu->q_host, hostmask, FNM_CASEFOLD) == 0)
      return 0;
  }
  return 1;
}

/* check for valid upload user and size */
unsigned int invalid_upload(const char *nick, const char *hostmask, off_t len)
{
  updatecontext();

  if (verify_uploadhost(hostmask)) {
    notice(nick, "DCC Send Denied, I don't accept transfers from %s", hostmask);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
            "DCC Send Denied from %s on %s",
            hostmask, gnetwork->name);
    return 1;
  }
  if (gdata.uploadmaxsize && (len > gdata.uploadmaxsize)) {
    error_upload_start(nick, hostmask, "too big", "I don't accept transfers that big");
    return 1;
  }
  if (len > gdata.max_file_size) {
    error_upload_start(nick, hostmask, "too large", "I can't accept transfers that large");
    return 1;
  }
  return 0;
}

static void qupload_started(unsigned int net, const char *nick)
{
  qupload_t *qu;

  /* start next XDCC GET */
  for (qu = irlist_get_head(&gdata.quploadhost);
       qu;
       qu = irlist_get_next(qu)) {
    if (qu->q_state != QUPLOAD_TRYING)
      continue;

    if (qu->q_net != net)
      continue;

    if (strcasecmp(qu->q_nick, nick) == 0) {
      qu->q_state = QUPLOAD_RUNNING;
      return;
    }
  }
}

/* check permissions and setup the upload transfer */
void upload_start(const char *nick, const char *hostname, const char *hostmask,
                  const char *filename, const char *remoteip, const char *remoteport, const char *bytes, char *token)
{
  upload *ul;
  char *uploaddir;
  char *tempstr;
  off_t len;

  updatecontext();

  len = atoull(bytes);
  if (invalid_upload(nick, hostmask, len))
    return;
  uploaddir = get_uploaddir(hostmask);
  if (uploaddir == NULL) {
    error_upload_start(nick, hostmask, "no uploaddir", "No uploaddir defined.");
    return;
  }
  if (disk_full(uploaddir) != 0) {
    error_upload_start(nick, hostmask, "disk full", "not enough free space on disk");
    return;
  }
  if (file_uploading(filename) != 0) {
    error_upload_start(nick, hostmask, "upload running", "I'm already getting this file");
    return;
  }
  if (max_uploads_reached() != 0) {
    error_upload_start(nick, hostmask, "too many uploads", "I'm already getting too many files");
    return;
  }
  ul = irlist_add(&gdata.uploads, sizeof(upload));
  l_initvalues(ul);
  ul->file = mystrdup(getfilename(filename));
  ul->con.family = (strchr(remoteip, ':')) ? AF_INET6 : AF_INET;
  ul->con.remoteaddr = mystrdup(remoteip);
  ul->con.remoteport = atoi(remoteport);
  ul->totalsize = len;
  ul->nick = mystrdup(nick);
  ul->hostname = mystrdup(hostname);
  ul->uploaddir = mystrdup(uploaddir);
  ul->net = gnetwork->net;
  qupload_started(gnetwork->net, nick);

  tempstr = getsendname(ul->file);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "DCC Send Accepted from %s on %s: %s (%" LLPRINTFMT "dkB)",
          nick, gnetwork->name, tempstr,
          (ul->totalsize / 1024));
  mydelete(tempstr);
  if (gdata.mirc_dcc64)
    if (ul->totalsize > 0xFFFFFFFFL)
      ul->mirc_dcc64 = 1;

  if (ul->con.remoteport > 0U) {
    l_establishcon(ul);
  } else {
    /* Passive DCC */
    l_setup_passive(ul, token);
  }
}

/* remove temp uploadhosts */
void clean_uploadhost(void)
{
  tupload_t *tu;

  updatecontext();

  for (tu = irlist_get_head(&gdata.tuploadhost);
       tu; ) {
    if (tu->u_time >= gdata.curtime) {
      tu = irlist_get_next(tu);
      continue;
    }
    mydelete(tu->u_host);
    tu = irlist_delete(&gdata.tuploadhost, tu);
  }
}

/* End of File */
