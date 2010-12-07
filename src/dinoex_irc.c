/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
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
#include "dinoex_misc.h"
#include "dinoex_irc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

/* writes IP address and port as text into the buffer */
int my_getnameinfo(char *buffer, size_t len, const struct sockaddr *sa)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  socklen_t salen;

  salen = (sa->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(sa, salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "host=%s port=%s", hbuf, sbuf);
#else /* NO_GETADDRINFO */
  const struct sockaddr_in *remoteaddr = (const struct sockaddr_in *)sa;
  unsigned long to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  return snprintf(buffer, len, "%lu.%lu.%lu.%lu:%d",
                  (to_ip >> 24) & 0xFF,
                  (to_ip >> 16) & 0xFF,
                  (to_ip >>  8) & 0xFF,
                  (to_ip      ) & 0xFF,
                  ntohs(remoteaddr->sin_port));
#endif /* NO_GETADDRINFO */
}

static int my_dcc_ip_port(char *buffer, size_t len, ir_sockaddr_union_t *sa)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  unsigned long ip;
  socklen_t salen;

  if (sa->sa.sa_family == AF_INET) {
    if (gnetwork->usenatip)
      ip = gnetwork->ourip;
    else
      ip = ntohl(sa->sin.sin_addr.s_addr);
    return snprintf(buffer, len, "%lu %d",
                    ip, ntohs(sa->sin.sin_port));
  }
  salen = (sa->sa.sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(&(sa->sa), salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "%s %s", hbuf, sbuf);
#else /* NO_GETADDRINFO */
  unsigned long ip;

  if (gnetwork->usenatip)
    ip = gnetwork->ourip;
  else
    ip = ntohl(sa->sin.sin_addr.s_addr);
  return snprintf(buffer, len, "%lu %d",
                  ip, ntohs(sa->sin.sin_port));
#endif /* NO_GETADDRINFO */
}

static void update_getip_net(unsigned int net, unsigned long ourip)
{
  char *oldtxt;
  gnetwork_t *backup;
  struct in_addr old;
  struct in_addr in;
  unsigned int ss;

  in.s_addr = htonl(ourip);
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);
    if (gnetwork->net == net)
      continue;

    if (gnetwork->getip_net != net)
      continue;

    if (gnetwork->ourip == ourip)
      continue;

    gnetwork->usenatip = 1;
    old.s_addr = htonl(gnetwork->ourip);
    oldtxt = mystrdup(inet_ntoa(old));
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "DCC IP changed from %s to %s on %s", oldtxt, inet_ntoa(in), gnetwork->name);
    mydelete(oldtxt);
    gnetwork->ourip = ourip;
  }
  gnetwork = backup;
}

/* check that the given text is an IP address or hostname and store it as external DCC IP */
void update_natip(const char *var)
{
  struct hostent *hp;
  struct in_addr old;
  struct in_addr in;
  long oldip;
  char *oldtxt;

  updatecontext();

  if (var == NULL)
    return;

  gnetwork->usenatip = 1;
  if (gnetwork->myip.sa.sa_family != AF_INET)
    return;

  if (gnetwork->r_ourip != 0)
    return;

  bzero((char *)&in, sizeof(in));
  if (inet_aton(var, &in) == 0) {
    hp = gethostbyname(var);
    if (hp == NULL) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid NAT Host, Ignoring: %s", hstrerror(h_errno));
      return;
    }
    if ((unsigned)hp->h_length > sizeof(in) || hp->h_length < 0) {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid DNS response, Ignoring: %s", hstrerror(h_errno));
      return;
    }
    memcpy(&in, hp->h_addr_list[0], sizeof(in));
  }

  old.s_addr = htonl(gnetwork->ourip);
  if (old.s_addr == in.s_addr)
    return;

  oldip = gnetwork->ourip;
  gnetwork->ourip = ntohl(in.s_addr);
  if (oldip != 0 ) {
    oldtxt = mystrdup(inet_ntoa(old));
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "DCC IP changed from %s to %s on %s", oldtxt, inet_ntoa(in), gnetwork->name);
    mydelete(oldtxt);
    update_getip_net(gnetwork->net, gnetwork->ourip);
  }

  if (gdata.debug > 0) ioutput(OUT_S, COLOR_YELLOW, "ip=%s\n", inet_ntoa(in));

  /* check for 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 */
  if (((gnetwork->ourip & 0xFF000000UL) == 0x0A000000UL) ||
      ((gnetwork->ourip & 0xFFF00000UL) == 0xAC100000UL) ||
      ((gnetwork->ourip & 0xFFFF0000UL) == 0xC0A80000UL)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "usenatip of %s looks wrong, this is probably not what you want to do",
             inet_ntoa(in));
  }
}

/* check the welcome message from the server for an IP address or hostname to set external DCC IP */
void update_server_welcome(char *line)
{
  const char *tptr;

  updatecontext();

#ifdef USE_UPNP
  if (gdata.getipfromupnp) {
    tptr = upnp_get_dccip();
    if (tptr != NULL) {
      ioutput(OUT_S, COLOR_NO_COLOR, "IP From UPnP = %s", tptr);
      update_natip(tptr);
      return;
    }
  }
#endif /* USE_UPNP */
  if (gdata.getipfromserver) {
    tptr = strchr(line, '@');
    if (tptr != NULL) {
      ++tptr;
      ioutput(OUT_S, COLOR_NO_COLOR, "IP From Server: %s", tptr);
      update_natip(tptr);
      return;
    }
    if (gnetwork->getip_net != 0) {
      /* copy IP from master */
      gnetwork->usenatip = 1;
      update_getip_net(gnetwork->getip_net, gdata.networks[gnetwork->getip_net].ourip);
      return;
    }
  }
  if (gnetwork->natip) {
    update_natip(gnetwork->natip);
    return;
  }
  if (gdata.usenatip) {
    /* use global */
    update_natip(gdata.usenatip);
  }
}

static unsigned int bind_vhost(ir_sockaddr_union_t *listenaddr, int family, const char *vhost)
{
  int e;

  if (vhost == NULL)
    return 0;

  if (family == AF_INET) {
    listenaddr->sin.sin_family = AF_INET;
    e = inet_pton(family, vhost, &(listenaddr->sin.sin_addr));
  } else {
    listenaddr->sin6.sin6_family = AF_INET6;
    e = inet_pton(family, vhost, &(listenaddr->sin6.sin6_addr));
  }

  if (e != 1) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", vhost);
    return 1;
  }

  return 0;
}

/* limit connection to the configured interface */
unsigned int bind_irc_vhost(int family, int clientsocket)
{
  const char *vhost;
  ir_sockaddr_union_t localaddr;
  SIGNEDSOCK int addrlen;
  int e;

  vhost = get_local_vhost();
  if (!vhost)
    return 0;

  bzero((char*)&localaddr, sizeof(ir_sockaddr_union_t));
  if (family == AF_INET ) {
    addrlen = sizeof(struct sockaddr_in);
    localaddr.sin.sin_family = AF_INET;
    localaddr.sin.sin_port = 0;
    e = inet_pton(family, vhost, &(localaddr.sin.sin_addr));
  } else {
    addrlen = sizeof(struct sockaddr_in6);
    localaddr.sin6.sin6_family = AF_INET6;
    localaddr.sin6.sin6_port = 0;
    e = inet_pton(family, vhost, &(localaddr.sin6.sin6_addr));
  }

  if (e != 1) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", vhost);
    return 1;
  }

  if (bind(clientsocket, &(localaddr.sa), addrlen) < 0)
    return 1;

  return 0;
}

#ifdef USE_UPNP
static void my_get_upnp_data(const struct sockaddr *sa)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  socklen_t salen;

  salen = (sa->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(sa, salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", "upnp_router");
    return;
  }
#else /* NO_GETADDRINFO */
  const struct sockaddr_in *remoteaddr = (const struct sockaddr_in *)sa;
  unsigned long to_ip;
  char hbuf[maxtextlengthshort], sbuf[maxtextlengthshort];

  to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  snprintf(hbuf, sizeof(hbuf), "%lu.%lu.%lu.%lu",
           (to_ip >> 24) & 0xFF,
           (to_ip >> 16) & 0xFF,
           (to_ip >>  8) & 0xFF,
           (to_ip      ) & 0xFF);
  snprintf(sbuf, sizeof(sbuf), "%d", ntohs(remoteaddr->sin_port));
#endif /* NO_GETADDRINFO */
  if (sa->sa_family != AF_INET)
    return;

  updatecontext();
  upnp_add_redir(hbuf, sbuf);
}
#endif /* USE_UPNP */

/* find a free port and open a new socket for an incoming connection */
unsigned int open_listen(int family, ir_sockaddr_union_t *listenaddr, int *listen_socket, unsigned int port, unsigned int reuse, unsigned int search, const char *vhost)
{
  int rc;
  int tempc;
  SIGNEDSOCK int addrlen;

  updatecontext();

  if (vhost != NULL) {
    family = strchr(vhost, ':') ? AF_INET6 : AF_INET;
  }
  *listen_socket = socket(family, SOCK_STREAM, 0);
  if (*listen_socket < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Could Not Create Socket, Aborting: %s", strerror(errno));
    *listen_socket = FD_UNUSED;
    return 1;
  }

  if (reuse) {
    tempc = 1;
    setsockopt(*listen_socket, SOL_SOCKET, SO_REUSEADDR, &tempc, sizeof(int));
  }

  bzero((char *) listenaddr, sizeof(ir_sockaddr_union_t));
  if (family == AF_INET) {
    addrlen = sizeof(struct sockaddr_in);
    listenaddr->sin.sin_family = AF_INET;
    listenaddr->sin.sin_addr.s_addr = INADDR_ANY;
    listenaddr->sin.sin_port = htons(port);
  } else {
    addrlen = sizeof(struct sockaddr_in6);
    listenaddr->sin6.sin6_family = AF_INET6;
    listenaddr->sin6.sin6_port = htons(port);
  }

  bind_vhost(listenaddr, family, vhost);

  if (search) {
    rc = ir_bind_listen_socket(*listen_socket, listenaddr);
  } else {
    rc = bind(*listen_socket, &(listenaddr->sa), addrlen);
  }

  if (rc < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "Couldn't Bind to Socket, Aborting: %s", strerror(errno));
    shutdown_close(*listen_socket);
    *listen_socket = FD_UNUSED;
    return 1;
  }

  if (listen(*listen_socket, 1) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't Listen, Aborting: %s", strerror(errno));
    shutdown_close(*listen_socket);
    *listen_socket = FD_UNUSED;
    return 1;
  }

#ifdef USE_UPNP
  if (gdata.upnp_router) {
    my_get_upnp_data(&(listenaddr->sa));
  }
#endif /* USE_UPNP */
  return 0;
}

/* find a free port and open a new socket for an incoming connection */
unsigned int irc_open_listen(ir_connection_t *con)
{
  unsigned int rc;

  rc = open_listen(con->family, &(con->local), &(con->listensocket), 0, gdata.tcprangestart, 1, NULL);
  if (rc != 0)
    return rc;

  con->connecttime = gdata.curtime;
  con->lastcontact = gdata.curtime;
  con->localport = get_port(&(con->local));
  return 0;
}

/* returns the external IP address and port the bot as DCC argsments */
char *setup_dcc_local(ir_sockaddr_union_t *listenaddr)
{
  char *msg;

  if (listenaddr->sa.sa_family == AF_INET)
    listenaddr->sin.sin_addr = gnetwork->myip.sin.sin_addr;
  else
    listenaddr->sin6.sin6_addr = gnetwork->myip.sin6.sin6_addr;
  msg = mymalloc(maxtextlength);
  my_dcc_ip_port(msg, maxtextlength -1, listenaddr);
  return msg;
}

/* the DNS resolution as a separate process */
void child_resolver(int family)
{
#if !defined(NO_GETADDRINFO)
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *res;
#else /* NO_GETADDRINFO */
  struct hostent *remotehost;
#endif /* NO_GETADDRINFO */
  struct sockaddr_in *remoteaddr;
  res_addrinfo_t rbuffer;
  ssize_t bytes;
  int i;
#if !defined(NO_GETADDRINFO)
  int status;
  int found;
  char portname[16];
#endif /* NO_GETADDRINFO */

  close(gnetwork->serv_resolv.sp_fd[0]);
  for (i=3; i<((int)FD_SETSIZE); ++i) {
    /* include [0], but not [1] */
    if (i != gnetwork->serv_resolv.sp_fd[1]) {
      close(i);
    }
  }

  memset(&rbuffer, 0, sizeof(res_addrinfo_t));
  /* enable logfile */
  gdata.logfd = FD_UNUSED;

#if !defined(NO_GETADDRINFO)
  memset(&hints, 0, sizeof(hints));
  switch (family) {
  case 0:
    hints.ai_family = PF_UNSPEC;
    break;
  case AF_INET:
    hints.ai_family = PF_INET;
    break;
  default:
    hints.ai_family = PF_INET6;
    break;
  }
  hints.ai_socktype = SOCK_STREAM;
  snprintf(portname, sizeof(portname), "%d",
           gnetwork->serv_resolv.to_port);
  status = getaddrinfo(gnetwork->serv_resolv.to_ip, portname,
                       &hints, &results);
  if ((status) || results == NULL) {
#else /* NO_GETADDRINFO */
  remotehost = gethostbyname(gnetwork->serv_resolv.to_ip);
  if (remotehost == NULL) {
#endif /* NO_GETADDRINFO */
#ifdef NO_HOSTCODES
    _exit(10);
#else /* NO_HOSTCODES */
    switch (h_errno) {
    case HOST_NOT_FOUND:
      _exit(20);
    case NO_ADDRESS:
#if NO_ADDRESS != NO_DATA
    case NO_DATA:
#endif /* NO_ADDRESS != NO_DATA */
      _exit(21);
    case NO_RECOVERY:
      _exit(22);
    case TRY_AGAIN:
      _exit(23);
    default:
      _exit(12);
    }
#endif /* NO_HOSTCODES */
  }

  remoteaddr = (struct sockaddr_in *)(&(rbuffer.ai_addr));
  rbuffer.ai_reset = 0;
#if !defined(NO_GETADDRINFO)
  found = -1;
  for (res = results; res; res = res->ai_next) {
    ++found;
    if (found < gnetwork->serv_resolv.next)
      continue;
    if (res->ai_next == NULL)
      rbuffer.ai_reset = 1;
    break;
  }
  if (res == NULL) {
    res = results;
    rbuffer.ai_reset = 1;
  }

  rbuffer.ai_family = res->ai_family;
  rbuffer.ai_socktype = res->ai_socktype;
  rbuffer.ai_protocol = res->ai_protocol;
  rbuffer.ai_addrlen = res->ai_addrlen;
  rbuffer.ai_addr = *(res->ai_addr);
  memcpy(remoteaddr, res->ai_addr, res->ai_addrlen);
#else /* NO_GETADDRINFO */
  rbuffer.ai_family = AF_INET;
  rbuffer.ai_socktype = SOCK_STREAM;
  rbuffer.ai_protocol = 0;
  rbuffer.ai_addrlen = sizeof(struct sockaddr_in);
  rbuffer.ai_addr.sa_family = remotehost->h_addrtype;
  remoteaddr->sin_port = htons(gnetwork->serv_resolv.to_port);
  memcpy(&(remoteaddr->sin_addr), remotehost->h_addr_list[0], sizeof(struct in_addr));
#endif /* NO_GETADDRINFO */
  bytes = write(gnetwork->serv_resolv.sp_fd[1],
                &rbuffer,
                sizeof(res_addrinfo_t));
#if !defined(NO_GETADDRINFO)
  freeaddrinfo(results);
#endif /* NO_GETADDRINFO */
  if (bytes != sizeof(res_addrinfo_t)) {
     _exit(11);
  }
}

/* returns a text with the external IP address of the bot */
const char *my_dcc_ip_show(char *buffer, size_t len, ir_sockaddr_union_t *sa, unsigned int net)
{
  long ip;

  if (sa->sa.sa_family == AF_INET) {
    if (gdata.networks[net].usenatip)
      ip = htonl(gdata.networks[net].ourip);
    else
      ip = sa->sin.sin_addr.s_addr;
    return inet_ntop(sa->sa.sa_family, &(ip), buffer, len);
  }
  return inet_ntop(sa->sa.sa_family, &(sa->sin6.sin6_addr), buffer, len);
}

/* complete the connection to the IRC server */
unsigned int connectirc2(res_addrinfo_t *remote)
{
  int retval;
  int family;

  if (remote->ai_reset)
    gnetwork->serv_resolv.next = 0;

  family = remote->ai_addr.sa_family;
  gnetwork->ircserver = socket(family, remote->ai_socktype, remote->ai_protocol);
  if (gnetwork->ircserver < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Socket Error: %s", strerror(errno));
    return 1;
  }

  if (gdata.debug > 0) {
    char *msg;
    msg = mymalloc(maxtextlength);
    my_getnameinfo(msg, maxtextlength -1, &(remote->ai_addr));
    ioutput(OUT_S, COLOR_YELLOW, "Connecting to %s", msg);
    mydelete(msg);
  }

  if (bind_irc_vhost(family, gnetwork->ircserver) != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Couldn't Bind To Virtual Host");
    close(gnetwork->ircserver);
    return 1;
  }

  if (set_socket_nonblocking(gnetwork->ircserver, 1) < 0 )
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");

  alarm(CTIMEOUT);
  retval = connect(gnetwork->ircserver, &(remote->ai_addr), remote->ai_addrlen);
  if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) ) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Connection to Server Failed: %s", strerror(errno));
    alarm(0);
    close(gnetwork->ircserver);
    return 1;
  }
  alarm(0);

  if (gdata.debug > 0) {
    ioutput(OUT_S, COLOR_YELLOW, "ircserver socket = %d", gnetwork->ircserver);
  }

  gnetwork->lastservercontact=gdata.curtime;

  /* good */
  gnetwork->serverstatus = SERVERSTATUS_TRYING;

  return 0;
}

/* get the local vhost for the current network */
char *get_local_vhost(void)
{
  return (gnetwork->local_vhost) ? gnetwork->local_vhost : gdata.local_vhost;
}

/* get the configured nick of the bot for the current network */
char *get_config_nick(void)
{
  if (gnetwork == NULL)
    return gdata.config_nick;

  return (gnetwork->config_nick) ? gnetwork->config_nick : gdata.config_nick;
}

/* get the active nick of the bot for the current network */
char *get_user_nick(void)
{
  if (gnetwork == NULL)
    return gdata.config_nick;

  return (gnetwork->user_nick) ? gnetwork->user_nick : get_config_nick();
}

/* check if all networks are disconnected */
unsigned int has_closed_servers(void)
{
  unsigned int ss;

  for (ss=0; ss<gdata.networks_online; ++ss) {
    if (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED)
      return 0;
  }
  return 1;
}

/* update or create an entry in the ignore list */
igninfo *get_ignore(const char *hostmask)
{
  igninfo *ignore;

  for (ignore = irlist_get_head(&gdata.ignorelist);
       ignore;
       ignore = irlist_get_next(ignore)) {

    if (fnmatch(ignore->hostmask, hostmask, FNM_CASEFOLD) != 0)
      continue;

    ignore->lastcontact = gdata.curtime;
    return ignore;
  }

  ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
  ignore->hostmask = mystrdup(hostmask);
  ignore->lastcontact = gdata.curtime;
  return ignore;
}

/* count actions for ignore list */
unsigned int check_ignore(const char *nick, const char *hostmask)
{
  igninfo *ignore;
  int left;

  if (*nick == '#') /* don't count channel */
    return 0;

  if (verifyshell(&gdata.autoignore_exclude, hostmask))
    return 0; /* host matches autoignore_exclude */

  ignore = get_ignore(hostmask);
  ++(ignore->bucket);
  ignore->lastcontact = gdata.curtime;

  if (ignore->flags & IGN_IGNORING)
    return 1;

  if (ignore->bucket < IGN_ON)
    return 0;

  left = gdata.autoignore_threshold * (ignore->bucket + 1);
  ignore->flags |= IGN_IGNORING;

  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "Auto-ignore activated for %s (%s) lasting %i%c%i%c",
          nick, hostmask,
          left < 3600 ? left/60 : left/60/60 ,
          left < 3600 ? 'm' : 'h',
          left < 3600 ? left%60 : (left/60)%60 ,
          left < 3600 ? 's' : 'm');

  notice(nick,
         "Auto-ignore activated for %s (%s) lasting %i%c%i%c" ". Further messages will increase duration.",
         nick, hostmask,
         left < 3600 ? left/60 : left/60/60 ,
         left < 3600 ? 'm' : 'h',
         left < 3600 ? left%60 : (left/60)%60 ,
         left < 3600 ? 's' : 'm');

  write_statefile();
  return 1;
}

/* register active connections for select() */
int irc_select(int highests)
{
  unsigned int ss;

  for (ss=0; ss<gdata.networks_online; ++ss) {
    if (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED) {
      FD_SET(gdata.networks[ss].ircserver, &gdata.readset);
      highests = max2(highests, gdata.networks[ss].ircserver);
      continue;
    }
    if (gdata.networks[ss].serverstatus == SERVERSTATUS_SSL_HANDSHAKE) {
      FD_SET(gdata.networks[ss].ircserver, &gdata.writeset);
      FD_SET(gdata.networks[ss].ircserver, &gdata.readset);
      highests = max2(highests, gdata.networks[ss].ircserver);
      continue;
    }
    if (gdata.networks[ss].serverstatus == SERVERSTATUS_TRYING) {
      FD_SET(gdata.networks[ss].ircserver, &gdata.writeset);
      highests = max2(highests, gdata.networks[ss].ircserver);
      continue;
    }
    if (gdata.networks[ss].serverstatus == SERVERSTATUS_RESOLVING) {
      FD_SET(gdata.networks[ss].serv_resolv.sp_fd[0], &gdata.readset);
      highests = max2(highests, gdata.networks[ss].serv_resolv.sp_fd[0]);
      continue;
    }
  }
  return highests;
}

/* try to identify at Nickserv */
void identify_needed(unsigned int force)
{
  char *pwd;

  pwd = get_nickserv_pass();
  if (pwd == NULL)
    return;

  if (force == 0) {
    if ((gnetwork->next_identify > 0) && (gnetwork->next_identify >= gdata.curtime))
      return;
  }
  /* wait 1 sec before idetify again */
  gnetwork->next_identify = gdata.curtime + 1;
  if (gnetwork->auth_name != NULL) {
    writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :AUTH %s %s",
                gnetwork->auth_name, save_nick(gnetwork->user_nick), pwd);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "AUTH send to %s on %s.", gnetwork->auth_name, gnetwork->name);
    return;
  }
  if (gnetwork->login_name != NULL) {
    writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :LOGIN %s %s",
                gnetwork->login_name, save_nick(gnetwork->user_nick), pwd);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "LOGIN send to %s on %s.", gnetwork->login_name, gnetwork->name);
    return;
  }
  writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :IDENTIFY %s", "nickserv", pwd);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "IDENTIFY send to nickserv on %s.", gnetwork->name);
}

/* check line from server to see if the bots need to identify again */
void identify_check(const char *line)
{
  char *pwd;

  pwd = get_nickserv_pass();
  if (pwd == NULL)
    return;

  if (strstr(line, "Nickname is registered to someone else") != NULL) {
      identify_needed(0);
  }
  if (strstr(line, "This nickname has been registered") != NULL) {
      identify_needed(0);
  }
  if (strstr(line, "This nickname is registered and protected") != NULL) {
      identify_needed(0);
  }
  if (strcasestr(line, "please choose a different nick") != NULL) {
      identify_needed(0);
  }
}

/* End of File */
