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

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_queue.h"
#include "dinoex_ssl.h"
#include "dinoex_ruby.h"
#include "dinoex_jobs.h"
#include "dinoex_user.h"
#include "dinoex_misc.h"
#include "dinoex_irc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

#include <netinet/tcp.h>

#define MAX_IRCMSG_PARTS 6

typedef struct {
  char *line;
  char *part[MAX_IRCMSG_PARTS];
} ir_parseline_t;

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
  ir_uint32 to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  return snprintf(buffer, len, IPV4_PRINT_FMT ":%d",
                  IPV4_PRINT_DATA(to_ip),
                  ntohs(remoteaddr->sin_port));
#endif /* NO_GETADDRINFO */
}

static int my_dcc_ip_port(char *buffer, size_t len, ir_sockaddr_union_t *sa)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  ir_uint32 ip;
  socklen_t salen;

  if (sa->sa.sa_family == AF_INET) {
    if (gnetwork->usenatip)
      ip = gnetwork->ourip;
    else
      ip = ntohl(sa->sin.sin_addr.s_addr);
    return snprintf(buffer, len, "%u %d", /* NOTRANSLATE */
                    ip, ntohs(sa->sin.sin_port));
  }
  salen = (sa->sa.sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(&(sa->sa), salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "%s %s", hbuf, sbuf); /* NOTRANSLATE */
#else /* NO_GETADDRINFO */
  ir_uint32 ip;

  if (gnetwork->usenatip)
    ip = gnetwork->ourip;
  else
    ip = ntohl(sa->sin.sin_addr.s_addr);
  return snprintf(buffer, len, "%u %d", /* NOTRANSLATE */
                  ip, ntohs(sa->sin.sin_port));
#endif /* NO_GETADDRINFO */
}

static void update_getip_net(unsigned int net, ir_uint32 ourip)
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
  ir_uint32 oldip;
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
  if (oldip != gnetwork->ourip) {
    oldtxt = mystrdup(inet_ntoa(old));
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "DCC IP changed from %s to %s on %s", oldtxt, inet_ntoa(in), gnetwork->name);
    mydelete(oldtxt);
    update_getip_net(gnetwork->net, gnetwork->ourip);
  }

  if (gdata.debug > 0) ioutput(OUT_S, COLOR_YELLOW, "ip=%s\n", inet_ntoa(in));

  /* check for 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 */
  if (((gnetwork->ourip & 0xFF000000U) == 0x0A000000U) ||
      ((gnetwork->ourip & 0xFFF00000U) == 0xAC100000U) ||
      ((gnetwork->ourip & 0xFFFF0000U) == 0xC0A80000U)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "usenatip of %s looks wrong, this is probably not what you want to do",
             inet_ntoa(in));
  }
}

/* check the welcome message from the server for an IP address or hostname to set external DCC IP */
static void update_server_welcome(char *line)
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
    if (gnetwork->getip_net != gnetwork->net) {
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
  ir_uint32 to_ip;
  char hbuf[maxtextlengthshort], sbuf[maxtextlengthshort];

  to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  snprintf(hbuf, sizeof(hbuf), IPV4_PRINT_FMT,
           IPV4_PRINT_DATA(to_ip));
  snprintf(sbuf, sizeof(sbuf), "%d", ntohs(remoteaddr->sin_port)); /* NOTRANSLATE */
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

static void ir_setsockopt2(int clientsocket, int optint, const char *optname, int val)
{
  SIGNEDSOCK int tempi;
  int tempc1;
  int tempc2;
  int tempc3;

  tempi = sizeof(int);
  getsockopt(clientsocket, SOL_SOCKET, optint, &tempc1, &tempi);

  tempc2 = val;
  setsockopt(clientsocket, SOL_SOCKET, optint, &tempc2, sizeof(int));

  getsockopt(clientsocket, SOL_SOCKET, optint, &tempc3, &tempi);
  if (gdata.debug > 0)
    ioutput(OUT_S, COLOR_YELLOW, "%s a %i b %i c %i", optname, tempc1, tempc2, tempc3);
}

/* set all options for a transfer connection */
void ir_setsockopt(int clientsocket)
{
#if !defined(CANT_SET_TOS)
  int tempc;
#endif
  int nodelay = 1;
  int rc;

  updatecontext();

  if (gdata.tcp_buffer_size > 0) {
    ir_setsockopt2(clientsocket, SO_SNDBUF, "SO_SNDBUF", gdata.tcp_buffer_size); /* NOTRANSLATE */
    ir_setsockopt2(clientsocket, SO_RCVBUF, "SO_RCVBUF", gdata.tcp_buffer_size); /* NOTRANSLATE */
  }

#if !defined(CANT_SET_TOS)
  /* Set TOS socket option to max throughput */
  tempc = 0x8; /* IPTOS_THROUGHPUT */
  setsockopt(clientsocket, IPPROTO_IP, IP_TOS, &tempc, sizeof(int));
#endif

  if (gdata.tcp_nodelay != 0) {
    rc = setsockopt(clientsocket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    if (rc < 0)
      outerror(OUTERROR_TYPE_WARN, "Couldn't Set TCP_NODELAY");
  }

  if (set_socket_nonblocking(clientsocket, 1) < 0 )
    outerror(OUTERROR_TYPE_WARN, "Couldn't Set Non-Blocking");
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
  snprintf(portname, sizeof(portname), "%d", /* NOTRANSLATE */
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

#ifndef NO_HOSTCODES
static  const char *irc_resolved_errormsg[] = {
  "host not found",
  "no ip address",
  "non-recoverable name server",
  "try again later",
};
#endif

/* collect the the DNS resolution from the child process */
void irc_resolved(void)
{
  pid_t child;
  unsigned int ss;
  int status;

  while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
    for (ss=0; ss<gdata.networks_online; ++ss) {
      if (child != gdata.networks[ss].serv_resolv.child_pid)
        continue;

      if (gdata.networks[ss].serverstatus == SERVERSTATUS_RESOLVING) {
        /* lookup failed */
#ifdef NO_WSTATUS_CODES
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                "Unable to resolve server %s on %s " "(status=0x%.8X)",
                gdata.networks[ss].curserver.hostname, gdata.networks[ss].name,
                status);
#else
        int hasexited = WIFEXITED(status);
        int ecode = WEXITSTATUS(status);
#ifndef NO_HOSTCODES
        if (hasexited && (ecode >= 20) && (ecode <= 23)) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                  "Unable to resolve server %s on %s " "(%s)",
                  gdata.networks[ss].curserver.hostname, gdata.networks[ss].name,
                  irc_resolved_errormsg[ecode - 20]);
        } else
#endif
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                  "Unable to resolve server %s on %s " "(status=0x%.8X, %s: %d)",
                  gdata.networks[ss].curserver.hostname, gdata.networks[ss].name,
                  status,
                  hasexited ? "exit" : WIFSIGNALED(status) ? "signaled" : "??",
                  hasexited ? ecode : WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        }
#endif
        gdata.networks[ss].serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
      }

      /* cleanup */
      close(gdata.networks[ss].serv_resolv.sp_fd[0]);
      FD_CLR(gdata.networks[ss].serv_resolv.sp_fd[0], &gdata.readset);
      gdata.networks[ss].serv_resolv.sp_fd[0] = 0;
      gdata.networks[ss].serv_resolv.child_pid = 0;
    }
  }
}

/* returns a text with the external IP address of the bot */
const char *my_dcc_ip_show(char *buffer, size_t len, ir_sockaddr_union_t *sa, unsigned int net)
{
  ir_uint32 ip;

  if (sa->sa.sa_family == AF_INET) {
    if (gdata.networks[net].usenatip)
      ip = htonl(gdata.networks[net].ourip);
    else
      ip = sa->sin.sin_addr.s_addr;
    buffer[0] = 0;
    return inet_ntop(sa->sa.sa_family, &(ip), buffer, len);
  }
  buffer[0] = 0;
  return inet_ntop(sa->sa.sa_family, &(sa->sin6.sin6_addr), buffer, len);
}

/* complete the connection to the IRC server */
static unsigned int connectirc2(res_addrinfo_t *remote)
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

static int irc_server_is_timeout(void)
{
  int timeout;
  timeout = CTIMEOUT + (gnetwork->serverconnectbackoff * CBKTIMEOUT);

  if (gnetwork->lastservercontact + timeout < gdata.curtime)
    return timeout;

  return 0;
}

static void irc_server_timeout(void)
{
  int timeout;

  timeout = irc_server_is_timeout();
  if (timeout > 0) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "Server Connection Timed Out (%u seconds) on %s", timeout, gnetwork->name);
    close_server();
  }
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
    writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :AUTH %s %s", /* NOTRANSLATE */
                gnetwork->auth_name, save_nick(gnetwork->user_nick), pwd);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "AUTH send to %s on %s.", gnetwork->auth_name, gnetwork->name);
    return;
  }
  if (gnetwork->login_name != NULL) {
    writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :LOGIN %s %s", /* NOTRANSLATE */
                gnetwork->login_name, save_nick(gnetwork->user_nick), pwd);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "LOGIN send to %s on %s.", gnetwork->login_name, gnetwork->name);
    return;
  }
  writeserver(WRITESERVER_NORMAL, "PRIVMSG %s :IDENTIFY %s", "nickserv", pwd); /* NOTRANSLATE */
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "IDENTIFY send to nickserv on %s.", gnetwork->name);
}

/* check line from server to see if the bots need to identify again */
static void identify_check(const char *line)
{
  char *pwd;

  pwd = get_nickserv_pass();
  if (pwd == NULL)
    return;

  if (strstr(line, "Nickname is registered to someone else") != NULL) { /* NOTRANSLATE */
      identify_needed(0);
  }
  if (strstr(line, "This nickname has been registered") != NULL) { /* NOTRANSLATE */
      identify_needed(0);
  }
  if (strstr(line, "This nickname is registered and protected") != NULL) { /* NOTRANSLATE */
      identify_needed(0);
  }
  if (strcasestr(line, "please choose a different nick") != NULL) { /* NOTRANSLATE */
      identify_needed(0);
  }
}

static void irc_001(ir_parseline_t *ipl)
{
  char *tptr;

  ioutput(OUT_S|OUT_L, COLOR_NO_COLOR, "Server welcome: %s", ipl->line);
  update_server_welcome(ipl->line);

  /* update server name */
  mydelete(gnetwork->curserveractualname);
  gnetwork->curserveractualname = getpart(ipl->line + 1, 1);

  /* update nick */
  mydelete(gnetwork->user_nick);
  mydelete(gnetwork->caps_nick);
  gnetwork->user_nick = mystrdup(ipl->part[2]);
  gnetwork->caps_nick = mystrdup(ipl->part[2]);
  caps(gnetwork->caps_nick);
  gnetwork->nick_number = 0;
  gnetwork->next_restrict = gdata.curtime + gdata.restrictsend_delay;
  ++(gdata.needsclear);

  tptr = get_user_modes();
  if (tptr && tptr[0]) {
    writeserver(WRITESERVER_NOW, "MODE %s %s",
                gnetwork->user_nick, tptr);
  }

  /* server connected raw command */
  for (tptr = irlist_get_head(&(gnetwork->server_connected_raw));
       tptr;
       tptr = irlist_get_next(tptr)) {
    writeserver(WRITESERVER_NORMAL, "%s", tptr);
  }

  /* nickserv */
  identify_needed(0);
}

static void irc_005(ir_parseline_t *ipl)
{
  unsigned int ii = 3;
  char *item;

  while((item = getpart(ipl->line, ++ii))) {
    if (item[0] == ':') {
      mydelete(item);
      break;
    }

    if (!strncmp("PREFIX=(", item, 8)) {
      char *ptr = item+8;
      unsigned int pi;

      memset(&(gnetwork->prefixes), 0, sizeof(gnetwork->prefixes));
      for (pi = 0; (ptr[pi] && (ptr[pi] != ')') && (pi < MAX_PREFIX)); ++pi) {
        gnetwork->prefixes[pi].p_mode = ptr[pi];
      }
      if (ptr[pi] == ')') {
        ptr += pi + 1;
        for (pi = 0; (ptr[pi] && (pi < MAX_PREFIX)); ++pi) {
          gnetwork->prefixes[pi].p_symbol = ptr[pi];
        }
      }
      for (pi = 0; pi < MAX_PREFIX; ++pi) {
        if ((gnetwork->prefixes[pi].p_mode && !gnetwork->prefixes[pi].p_symbol) ||
           (!gnetwork->prefixes[pi].p_mode && gnetwork->prefixes[pi].p_symbol)) {
          outerror(OUTERROR_TYPE_WARN,
                   "Server prefix list on %s doesn't make sense, using defaults: %s",
                   gnetwork->name, item);
          initprefixes();
        }
      }
    }

    if (!strncmp("CHANMODES=", item, 10)) {
      char *ptr = item+10;
      unsigned int ci;
      unsigned int cm;

      memset(&(gnetwork->chanmodes), 0, sizeof(gnetwork->chanmodes));
      for (ci = cm = 0; (ptr[ci] && (cm < MAX_CHANMODES)); ++ci) {
        if (ptr[ci+1] == ',')
                     {
                       /* we only care about ones with arguments */
                       gnetwork->chanmodes[cm++] = ptr[ci++];
                     }
      }
    }

    mydelete(item);
  }
}

static char *ir_get_nickarg(const char *line)
{
  char* nick;
  size_t len;
  unsigned int j;

  len = strlen(line);
  nick = mymalloc(len + 1);
  for (j=1; line[j] != '!' && j<len; ++j) {
    nick[j-1] = line[j];
  }
  nick[j-1]='\0';
  return nick;
}

static void ir_unknown_channel(const char *chname)
{
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
          "%s is not a known channel on %s!",
          chname, gnetwork->name);
}

static void ir_parseline2(ir_parseline_t *ipl)
{
  channel_t *ch;
  char *nick;
  char *part3a;
  char *t;
  unsigned int i;

  caps(ipl->part[1]);

  /* NOTICE nick */
  if (!strcmp(ipl->part[1], "NOTICE")) {
    if (gnetwork->caps_nick && ipl->part[2]) {
      if (!strcmp(caps(ipl->part[2]), gnetwork->caps_nick)) {
        /* nickserv */
        identify_check(ipl->line);
#ifdef USE_RUBY
        if (do_myruby_notice(ipl->line) == 0)
#endif /* USE_RUBY */
          privmsgparse(0, 0, ipl->line);
      }
    }
  }

  /* PRIVMSG */
  if (!strcmp(ipl->part[1], "PRIVMSG")) {
#ifndef WITHOUT_BLOWFISH
    char *line2;

    line2 = test_fish_message(ipl->line, ipl->part[2], ipl->part[3], ipl->part[4]);
    if (line2) {
#ifdef USE_RUBY
      if (do_myruby_privmsg(line2) == 0)
#endif /* USE_RUBY */
        privmsgparse(1, 1, line2);
      mydelete(line2);
    } else {
#endif /* WITHOUT_BLOWFISH */
#ifdef USE_RUBY
      if (do_myruby_privmsg(ipl->line) == 0)
#endif /* USE_RUBY */
        privmsgparse(1, 0, ipl->line);
#ifndef WITHOUT_BLOWFISH
    }
#endif /* WITHOUT_BLOWFISH */
  }

  /* :server 001  xxxx :welcome.... */
  if ( !strcmp(ipl->part[1], "001") ) {
    irc_001(ipl);
    return;
  }

  /* :server 005 xxxx aaa bbb=x ccc=y :are supported... */
  if ( !strcmp(ipl->part[1], "005") ) {
    irc_005(ipl);
    return;
  }

  /* :server 401 botnick usernick :No such nick/channel */
  if ( !strcmp(ipl->part[1], "401") ) {
    if (ipl->part[2] && ipl->part[3]) {
      if (!strcmp(ipl->part[2], "*")) {
        lost_nick(ipl->part[3]);
      }
    }
    return;
  }

  /* :server 433 old new :Nickname is already in use. */
  if ( !strcmp(ipl->part[1], "433") ) {
    if (ipl->part[2] && ipl->part[3]) {
      if (!strcmp(ipl->part[2], "*")) {
        ioutput(OUT_S, COLOR_NO_COLOR,
                "Nickname %s already in use on %s, trying %s%u",
                ipl->part[3],
                gnetwork->name,
                get_config_nick(),
                gnetwork->nick_number);

        /* generate new nick and retry */
        writeserver(WRITESERVER_NORMAL, "NICK %s%u",
                    get_config_nick(),
                    (gnetwork->nick_number)++);
      }
    }
    return;
  }

  /* :server 470 botnick #channel :(you are banned) transfering you to #newchannel */
  if ( !strcmp(ipl->part[1], "470") ) {
    if (ipl->part[2] && ipl->part[3]) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "channel on %s: %s", gnetwork->name, strstr(ipl->line, "470"));
      for (ch = irlist_get_head(&(gnetwork->channels));
           ch;
           ch = irlist_get_next(ch)) {
        if (strcmp(caps(ipl->part[2]), gnetwork->caps_nick))
             continue;
        if (strcmp(caps(ipl->part[3]), ch->name))
             continue;
        ch->flags |= CHAN_KICKED;
      }
    }
    return;
  }

  /* names list for a channel */
  /* :server 353 our_nick = #channel :nick @nick +nick nick */
  if ( !strcmp(ipl->part[1], "353") ) {
    if (ipl->part[2] && ipl->part[3] && ipl->part[5]) {
      caps(ipl->part[4]);

      for (ch = irlist_get_head(&(gnetwork->channels));
           ch;
           ch = irlist_get_next(ch)) {
        if (strcmp(ipl->part[4], ch->name) != 0)
          continue;
        for (i=0; (t = getpart(ipl->line, 6+i)); ++i) {
          addtomemberlist(ch, i == 0 ? t+1 : t);
          mydelete(t);
        }
        return;
      }
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
              "Got name data for %s which is not a known channel on %s!",
              ipl->part[4], gnetwork->name);
      writeserver(WRITESERVER_NORMAL, "PART %s", ipl->part[4]);
    }
    return;
  }

  if (gnetwork->lastping != 0) {
    if (strcmp(ipl->part[1], "PONG") == 0) {
      lag_message();
      return;
    }
  }

#if PING_SRVR
  /* server ping */
  if (strncmp(ipl->line, "PING :", 6) == 0) {
    if (gdata.debug > 0)
      ioutput(OUT_S, COLOR_NO_COLOR,
              "Server Ping on %s: %s",
              gnetwork->name, ipl->line);
    writeserver(WRITESERVER_NOW, "PO%s", ipl->line+2);
    return;
  }
#endif

  /* QUIT */
  if (strcmp(ipl->part[1], "QUIT") == 0) {
    if (gnetwork->caps_nick) {
      nick = ir_get_nickarg(ipl->line);
      if (!strcmp(caps(nick), gnetwork->caps_nick)) {
        /* we quit? */
        outerror(OUTERROR_TYPE_WARN_LOUD,
                 "Forced quit on %s: %s", gnetwork->name, ipl->line);
        close_server();
        clean_send_buffers();
        /* do not reconnect */
        gnetwork->serverstatus = SERVERSTATUS_EXIT;
      } else {
        /* someone else quit */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          removefrommemberlist(ch, nick);
        }
        reverify_restrictsend();
      }
      mydelete(nick);
    }
    return;
  }

  /* MODE #channel +x ... */
  if (strcmp(ipl->part[1], "MODE") == 0) {
    if (ipl->part[2] && ipl->part[3]) {
      /* find channel */
      for (ch = irlist_get_head(&(gnetwork->channels)); ch; ch = irlist_get_next(ch)) {
        char *ptr;
        unsigned int plus;
        unsigned int part;
        unsigned int ii;

        if (strcasecmp(ch->name, ipl->part[2]) != 0)
          continue;

        plus = 0;
        part = 4;
        for (ptr = ipl->part[3]; *ptr; ++ptr) {
          if (*ptr == '+') {
            plus = 1;
          } else if (*ptr == '-') {
            plus = 0;
          } else {
            for (ii = 0; (ii < MAX_PREFIX && gnetwork->prefixes[ii].p_mode); ++ii) {
              if (*ptr == gnetwork->prefixes[ii].p_mode) {
                /* found a nick mode */
                nick = getpart(ipl->line, ++part);
                if (nick) {
                  if (nick[strlen(nick)-1] == IRCCTCP) {
                    nick[strlen(nick)-1] = '\0';
                  }
                  if (plus == 0) {
                    if (strcasecmp(nick, get_config_nick()) == 0) {
                      identify_needed(0);
                    }
                  }
                  changeinmemberlist_mode(ch, nick,
                                          gnetwork->prefixes[ii].p_symbol,
                                          plus);
                  mydelete(nick);
                }
                break;
              }
            }
            for (ii = 0; (ii < MAX_CHANMODES && gnetwork->chanmodes[ii]); ++ii) {
              if (*ptr == gnetwork->chanmodes[ii]) {
                /* found a channel mode that has an argument */
                ++part;
                break;
              }
            }
          }
        }
        return;
      }
      if (strcasecmp(ipl->part[2], get_config_nick()) == 0) {
        if (ipl->part[3][0] == '-') {
          identify_needed(0);
        }
      }
    }
    return;
  }

  if (ipl->part[2] && ipl->part[2][0] == ':') {
    part3a = ipl->part[2] + 1;
  } else {
    part3a = ipl->part[2];
  }

  /* JOIN */
  if (strcmp(ipl->part[1], "JOIN") == 0) {
    if (gnetwork->caps_nick && part3a) {
      caps(part3a);
      nick = ir_get_nickarg(ipl->line);
      if (!strcmp(caps(nick), gnetwork->caps_nick)) {
        /* we joined */
        /* clear now, we have succesfully logged in */
        gnetwork->serverconnectbackoff = 0;
        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                "Joined %s on %s", part3a, gnetwork->name);

        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (strcmp(part3a, ch->name) != 0)
            continue;

          ch->flags |= CHAN_ONCHAN;
          ch->lastjoin = gdata.curtime;
          ch->nextann = gdata.curtime + gdata.waitafterjoin;
          if (ch->joinmsg) {
            writeserver(WRITESERVER_NOW, "PRIVMSG %s :%s", ch->name, ch->joinmsg);
          }
          gnetwork->botstatus = BOTSTATUS_JOINED;
          start_sends();
          mydelete(nick);
          return;
        }
      } else {
        /* someone else joined */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (strcmp(part3a, ch->name) != 0)
            continue;

          addtomemberlist(ch, nick);
          mydelete(nick);
          return;
        }
      }
      ir_unknown_channel(part3a);
      mydelete(nick);
    }
    return;
  }

  /* PART */
  if (strcmp(ipl->part[1], "PART") == 0) {
    if (gnetwork->caps_nick && part3a) {
      nick = ir_get_nickarg(ipl->line);
      if (!strcmp(caps(nick), gnetwork->caps_nick)) {
        /* we left? */
        mydelete(nick);
        return;
      } else {
        /* someone else left */
        caps(part3a);
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (strcmp(part3a, ch->name) != 0)
            continue;

          removefrommemberlist(ch, nick);
          mydelete(nick);
          reverify_restrictsend();
          return;
        }
      }
      ir_unknown_channel(part3a);
      mydelete(nick);
    }
    return;
  }

  /* NICK */
  if (strcmp(ipl->part[1], "NICK") == 0) {
    if (gnetwork->caps_nick && part3a) {
      nick = ir_get_nickarg(ipl->line);
      if (!strcmp(caps(nick), gnetwork->caps_nick)) {
        /* we changed, update nick */
        mydelete(gnetwork->user_nick);
        mydelete(gnetwork->caps_nick);
        gnetwork->user_nick = mystrdup(part3a);
        gnetwork->caps_nick = mystrdup(part3a);
        caps(gnetwork->caps_nick);
        gnetwork->nick_number = 0;
        ++(gdata.needsclear);
        identify_needed(0);
      } else {
        /* someone else has a new nick */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          changeinmemberlist_nick(ch, nick, part3a);
        }
        user_changed_nick(nick, part3a);
      }
      mydelete(nick);
    }
    return;
  }

  /* KICK */
  if (strcmp(ipl->part[1], "KICK") == 0) {
    if (gnetwork->caps_nick && part3a && ipl->part[3]) {
      caps(part3a);
      if (!strcmp(caps(ipl->part[3]), gnetwork->caps_nick)) {
        /* we were kicked */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (strcmp(part3a, ch->name) != 0)
            continue;

          if (gdata.noautorejoin) {
            outerror(OUTERROR_TYPE_WARN_LOUD,
                     "Kicked on %s: %s", gnetwork->name, ipl->line);
            ch->flags |= CHAN_KICKED;
            clearmemberlist(ch);
            reverify_restrictsend();
          } else {
            outerror(OUTERROR_TYPE_WARN_LOUD,
                     "Kicked on %s, Rejoining: %s", gnetwork->name, ipl->line);
            ch->flags &= ~CHAN_ONCHAN;
            joinchannel(ch);
          }
        }
      } else {
        /* someone else was kicked */
        for (ch = irlist_get_head(&(gnetwork->channels));
             ch;
             ch = irlist_get_next(ch)) {
          if (strcmp(part3a, ch->name) != 0)
            continue;

          removefrommemberlist(ch, ipl->part[3]);
          reverify_restrictsend();
          return;
        }
      }
    }
    return;
  }

  /* ERROR :Closing Link */
  if (strncmp(ipl->line, "ERROR :Closing Link", strlen("ERROR :Closing Link")) == 0) {
    if (gdata.exiting) {
      gnetwork->recentsent = 0;
      return;
    }
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
            "Server Closed Connection on %s: %s",
            gnetwork->name, ipl->line);
    close_server();
    return;
  }
}

/* handle message from irc server */
static void ir_parseline(char *line)
{
  ir_parseline_t ipl;
  unsigned int m;

  updatecontext();

  removenonprintable(line);
#ifdef USE_RUBY
  if (do_myruby_server(line))
    return;
#endif /* USE_RUBY */

  /* we only support lines upto maxtextlength, truncate line */
  line[maxtextlength-1] = '\0';

  if (gdata.debug > 0)
    ioutput(OUT_S, COLOR_CYAN, ">IRC>: %u, %s", gnetwork->net + 1, line);

  bzero((char *)&ipl, sizeof(ipl));
  ipl.line = line;
  m = get_argv(ipl.part, line, MAX_IRCMSG_PARTS);

  if (ipl.part[1] != NULL)
    ir_parseline2(&ipl);

  for (m = 0; m < MAX_IRCMSG_PARTS; ++m)
    mydelete(ipl.part[m]);
}

/* handle irc server connectipn */
void irc_perform(int changesec)
{
  channel_t *ch;
  unsigned int ss;
  unsigned int i;
  unsigned int j;
  int length;
  int timeout;

  updatecontext();

  for (ss=0; ss<gdata.networks_online; ++ss) {
    gnetwork = &(gdata.networks[ss]);

    if (gdata.needsswitch) {
      switchserver(-1);
      continue;
    }

    /*----- see if gdata.ircserver is sending anything to us ----- */
    if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED) {
      if (FD_ISSET(gnetwork->ircserver, &gdata.readset)) {
        char tempbuffa[INPUT_BUFFER_LENGTH];
        gnetwork->lastservercontact = gdata.curtime;
        gnetwork->servertime = 0;
        memset(&tempbuffa, 0, INPUT_BUFFER_LENGTH);
        length = readserver_ssl(&tempbuffa, INPUT_BUFFER_LENGTH);

        if (length < 1) {
          if (errno != EAGAIN) {
            ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                    "Closing Server Connection on %s: %s",
                    gnetwork->name, (length<0) ? strerror(errno) : "Closed");
            if (gdata.exiting) {
              gnetwork->recentsent = 0;
            }
            close_server();
            mydelete(gnetwork->curserveractualname);
          }
          continue;
        }

        j = strlen(gnetwork->server_input_line);
        for (i=0; i<(unsigned int)length; ++i) {
          if ((tempbuffa[i] == '\n') || (j == (INPUT_BUFFER_LENGTH-1))) {
            if (j && (gnetwork->server_input_line[j-1] == 0x0D)) {
              --j;
            }
            gnetwork->server_input_line[j] = '\0';
            ir_parseline(gnetwork->server_input_line);
            j = 0;
          } else {
            gnetwork->server_input_line[j] = tempbuffa[i];
            ++j;
          }
        }
        gnetwork->server_input_line[j] = '\0';
      }
      continue;
    }

    if (gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE) {
      if ((FD_ISSET(gnetwork->ircserver, &gdata.writeset)) || (FD_ISSET(gnetwork->ircserver, &gdata.readset))) {
        handshake_ssl();
      }
      if (changesec)
        irc_server_timeout();
      continue;
    }

    if (gnetwork->serverstatus == SERVERSTATUS_TRYING) {
      if (FD_ISSET(gnetwork->ircserver, &gdata.writeset)) {
        int callval_i;
        int connect_error;
        SIGNEDSOCK int connect_error_len = sizeof(connect_error);
        SIGNEDSOCK int addrlen;

        callval_i = getsockopt(gnetwork->ircserver,
                               SOL_SOCKET, SO_ERROR,
                               &connect_error, &connect_error_len);

        if (callval_i < 0) {
          outerror(OUTERROR_TYPE_WARN,
                   "Couldn't determine connection status: %s on %s",
                   strerror(errno), gnetwork->name);
          close_server();
          continue;
        }
        if (connect_error) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "Server Connection Failed: %s on %s", strerror(connect_error), gnetwork->name);
          close_server();
          continue;
        }

        ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                "Server Connection to %s Established, Logging In",  gnetwork->name);
        gnetwork->serverstatus = SERVERSTATUS_CONNECTED;
        gnetwork->connecttime = gdata.curtime;
        gnetwork->botstatus = BOTSTATUS_LOGIN;
        ch = irlist_get_head(&(gnetwork->channels));
        if (ch == NULL) {
          gnetwork->botstatus = BOTSTATUS_JOINED;
          start_sends();
        }
        FD_CLR(gnetwork->ircserver, &gdata.writeset);

        addrlen = sizeof(gnetwork->myip);
        bzero((char *) &(gnetwork->myip), sizeof(gnetwork->myip));
        if (getsockname(gnetwork->ircserver, &(gnetwork->myip.sa), &addrlen) >= 0) {
          if (gdata.debug > 0) {
            char *msg;
            msg = mymalloc(maxtextlength);
            my_getnameinfo(msg, maxtextlength -1, &(gnetwork->myip.sa));
            ioutput(OUT_S, COLOR_YELLOW, "using %s", msg);
            mydelete(msg);
          }
          if (!gnetwork->usenatip) {
            gnetwork->ourip = ntohl(gnetwork->myip.sin.sin_addr.s_addr);
            if (gdata.debug > 0) {
              ioutput(OUT_S, COLOR_YELLOW, "ourip = " IPV4_PRINT_FMT,
                      IPV4_PRINT_DATA(gnetwork->ourip));
            }
          }
        } else {
          outerror(OUTERROR_TYPE_WARN, "couldn't get ourip on %s", gnetwork->name);
        }

        handshake_ssl();
      }
      if (changesec)
        irc_server_timeout();
      continue;
    }

    if (gnetwork->serverstatus == SERVERSTATUS_RESOLVING) {
      if (FD_ISSET(gnetwork->serv_resolv.sp_fd[0], &gdata.readset)) {
        res_addrinfo_t remote;
        length = read(gnetwork->serv_resolv.sp_fd[0],
                      &remote, sizeof(res_addrinfo_t));

        kill(gnetwork->serv_resolv.child_pid, SIGKILL);
        FD_CLR(gnetwork->serv_resolv.sp_fd[0], &gdata.readset);

        if (length != sizeof(res_addrinfo_t)) {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                  "Error resolving server %s on %s",
                  gnetwork->curserver.hostname, gnetwork->name);
          gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
        } else {
          /* continue with connect */
          if (connectirc2(&remote)) {
            /* failed */
            gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
          }
        }
      }
      if (changesec) {
        timeout = irc_server_is_timeout();
        if (timeout > 0) {
          kill(gnetwork->serv_resolv.child_pid, SIGKILL);
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                  "Server Resolve Timed Out (%u seconds) on %s", timeout, gnetwork->name);
          gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
        }
      }
      continue;
    }

    if (gnetwork->offline)
      continue;

    if (gnetwork->serverstatus == SERVERSTATUS_NEED_TO_CONNECT) {
      if (changesec) {
        timeout = irc_server_is_timeout();
        if (timeout > 0) {
          if (gdata.debug > 0) {
            ioutput(OUT_S, COLOR_YELLOW,
                    "Reconnecting to server (%u seconds) on %s",
                    timeout, gnetwork->name);
          }
          switchserver(-1);
        }
      }
      continue;
    }

  } /* networks */
  gnetwork = NULL;

  /* reset after done on all networks */
  if (gdata.needsswitch)
    gdata.needsswitch = 0;
}

/* End of File */
