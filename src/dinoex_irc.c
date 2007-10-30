/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
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
#include "dinoex_irc.h"

void update_natip(const char *var)
{
  struct hostent *hp;
  struct in_addr old;
  struct in_addr in;
  long oldip;
  char *oldtxt;

  if (var == NULL)
    return;

  if (gnetwork != NULL) {
    if (gnetwork->myip.sa.sa_family != AF_INET)
      return;
  }

  gdata.usenatip = 1;
  if (gdata.r_ourip != 0)
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

  old.s_addr = htonl(gdata.ourip);
  if (old.s_addr == in.s_addr)
    return;

  oldip = gdata.ourip;
  gdata.ourip = ntohl(in.s_addr);
  if (oldip != 0 ) {
    oldtxt = mystrdup(inet_ntoa(old));
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "DCC IP changed from %s to %s", oldtxt, inet_ntoa(in));
    mydelete(oldtxt);
  }

  if (gdata.debug > 0) ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "ip=0x%8.8lX\n", gdata.ourip);

  /* check for 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 */
  if (((gdata.ourip & 0xFF000000UL) == 0x0A000000UL) ||
      ((gdata.ourip & 0xFFF00000UL) == 0xAC100000UL) ||
      ((gdata.ourip & 0xFFFF0000UL) == 0xC0A80000UL)) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "usenatip of %lu.%lu.%lu.%lu looks wrong, this is probably not what you want to do",
             (gdata.ourip >> 24) & 0xFF,
             (gdata.ourip >> 16) & 0xFF,
             (gdata.ourip >>  8) & 0xFF,
             (gdata.ourip      ) & 0xFF);
  }
}

static int bind_vhost(ir_sockaddr_union_t *listenaddr, int family, char *vhost)
{
  SIGNEDSOCK int addrlen;
  int e;

  if (vhost == NULL)
    return 0;

  if (family == AF_INET) {
    addrlen = sizeof(struct sockaddr_in);
    listenaddr->sin.sin_family = AF_INET;
    e = inet_pton(family, vhost, &(listenaddr->sin.sin_addr));
   } else {
    addrlen = sizeof(struct sockaddr_in6);
    listenaddr->sin6.sin6_family = AF_INET6;
    e = inet_pton(family, vhost, &(listenaddr->sin6.sin6_addr));
  }

  if (e != 1) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", vhost);
    return 1;
  }

  return 0;
}

int bind_irc_vhost(int family, int clientsocket)
{
  ir_sockaddr_union_t localaddr;
  SIGNEDSOCK int addrlen;
  int e;

  if (!gdata.local_vhost)
    return 0;

  bzero((char*)&localaddr, sizeof(ir_sockaddr_union_t));
  if (family == AF_INET ) {
    addrlen = sizeof(struct sockaddr_in);
    localaddr.sin.sin_family = AF_INET;
    localaddr.sin.sin_port = 0;
    e = inet_pton(family, gdata.local_vhost, &(localaddr.sin.sin_addr));
  } else {
    addrlen = sizeof(struct sockaddr_in6);
    localaddr.sin6.sin6_family = AF_INET6;
    localaddr.sin6.sin6_port = 0;
    e = inet_pton(family, gdata.local_vhost, &(localaddr.sin6.sin6_addr));
  }

  if (e != 1) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", gdata.local_vhost);
    return 1;
  }

  if (bind(clientsocket, &(localaddr.sa), addrlen) < 0)
    return 1;

  return 0;
}

int open_listen(int family, ir_sockaddr_union_t *listenaddr, int *listen_socket, int port, int reuse, int search, char *vhost)
{
  int rc;
  int tempc;
  SIGNEDSOCK int addrlen;

  updatecontext();

  if (vhost != NULL) {
    family = strchr(vhost, ':') ? AF_INET6 : AF_INET;
  }
  *listen_socket = socket(family, SOCK_STREAM, 0);
  if (*listen < 0) {
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

  return 0;
}

char *setup_dcc_local(ir_sockaddr_union_t *listenaddr)
{
   char *msg;

   if (listenaddr->sa.sa_family == AF_INET)
     listenaddr->sin.sin_addr = gnetwork->myip.sin.sin_addr;
   else
     listenaddr->sin6.sin6_addr = gnetwork->myip.sin6.sin6_addr;
   msg = mycalloc(maxtextlength);
   my_dcc_ip_port(msg, maxtextlength -1, listenaddr, 0);
   return msg;
}

void child_resolver(void)
{
#if !defined(NO_GETADDRINFO)
  struct addrinfo hints;
  struct addrinfo *results;
  struct addrinfo *res;
#else
  struct hostent *remotehost;
#endif
  struct sockaddr_in *remoteaddr;
  res_addrinfo_t rbuffer;
  ssize_t bytes;
  int i;
#if !defined(NO_GETADDRINFO)
  int status;
  int found;
  char portname[16];
#endif

  for (i=3; i<FD_SETSIZE; i++) {
    /* include [0], but not [1] */
    if (i != gnetwork->serv_resolv.sp_fd[1]) {
      close(i);
    }
  }

  /* enable logfile */
  gdata.logfd = FD_UNUSED;

#if !defined(NO_GETADDRINFO)
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  snprintf(portname, sizeof(portname), "%d",
           gnetwork->serv_resolv.to_port);
  status = getaddrinfo(gnetwork->serv_resolv.to_ip, portname,
                       &hints, &results);
  if ((status) || results == NULL) {
#else
  remotehost = gethostbyname(gnetwork->serv_resolv.to_ip);
  if (remotehost == NULL) {
#endif
#ifdef NO_HOSTCODES
    exit(10);
#else
    switch (h_errno) {
    case HOST_NOT_FOUND:
      exit(20);
    case NO_ADDRESS:
#if NO_ADDRESS != NO_DATA
    case NO_DATA:
#endif
      exit(21);
    case NO_RECOVERY:
      exit(22);
    case TRY_AGAIN:
      exit(23);
    default:
      exit(12);
    }
#endif
  }

  remoteaddr = (struct sockaddr_in *)(&(rbuffer.ai_addr));
  rbuffer.ai_reset = 0;
#if !defined(NO_GETADDRINFO)
  found = -1;
  for (res = results; res; res = res->ai_next) {
    found ++;
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
#else
  rbuffer.ai_family = AF_INET;
  rbuffer.ai_socktype = SOCK_STREAM;
  rbuffer.ai_protocol = 0;
  rbuffer.ai_addrlen = sizeof(struct sockaddr_in);
  rbuffer.ai_addr.sa_family = remotehost->h_addrtype;
  remoteaddr->sin_port = htons(gnetwork->serv_resolv.to_port);
  memcpy(&(remoteaddr->sin_addr), remotehost->h_addr_list[0], sizeof(struct in_addr));
#endif
  bytes = write(gnetwork->serv_resolv.sp_fd[1],
                &rbuffer,
                sizeof(res_addrinfo_t));
#if !defined(NO_GETADDRINFO)
  freeaddrinfo(results);
#endif
  if (bytes != sizeof(res_addrinfo_t)) {
     exit(11);
  }
}

int my_getnameinfo(char *buffer, size_t len, const struct sockaddr *sa, socklen_t salen)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

  if (salen == 0)
    salen = (sa->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(sa, salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "host=%s port=%s", hbuf, sbuf);
#else
  const struct sockaddr_in *remoteaddr = (const struct sockaddr_in *)sa;
  unsigned long to_ip = ntohl(remoteaddr->sin_addr.s_addr);
  return snprintf(buffer, len, "%lu.%lu.%lu.%lu:%d",
                  (to_ip >> 24) & 0xFF,
                  (to_ip >> 16) & 0xFF,
                  (to_ip >>  8) & 0xFF,
                  (to_ip      ) & 0xFF,
                  ntohs(remoteaddr->sin_port));
#endif
}

int my_dcc_ip_port(char *buffer, size_t len, ir_sockaddr_union_t *sa, socklen_t salen)
{
#if !defined(NO_GETADDRINFO)
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
#endif
  long ip;

#if !defined(NO_GETADDRINFO)
  if (sa->sa.sa_family == AF_INET) {
    ip = ntohl(sa->sin.sin_addr.s_addr);
    return snprintf(buffer, len, "%lu %d",
                    ip, ntohs(sa->sin.sin_port));
  }
  if (salen == 0)
    salen = (sa->sa.sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (getnameinfo(&(sa->sa), salen, hbuf, sizeof(hbuf), sbuf,
                  sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    return snprintf(buffer, len, "(unknown)" );
  }
  return snprintf(buffer, len, "%s %s", hbuf, sbuf);
#else
  ip = ntohl(sa->sin.sin_addr.s_addr);
  return snprintf(buffer, len, "%lu %d",
                  ip, ntohs(sa->sin.sin_port));
#endif
}

int connectirc2(res_addrinfo_t *remote)
{
  struct sockaddr_in *remoteaddr;
  int retval;
  int family;

  if (remote->ai_reset)
    gnetwork->serv_resolv.next = 0;

  family = remote->ai_addr.sa_family;
  remoteaddr = (struct sockaddr_in *)(&(remote->ai_addr));
  gnetwork->ircserver = socket(family, remote->ai_socktype, remote->ai_protocol);
  if (gnetwork->ircserver < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Socket Error");
    return 1;
  }

  if (gdata.debug > 0) {
    char *msg;
    msg = mycalloc(maxtextlength);
    my_getnameinfo(msg, maxtextlength -1, &(remote->ai_addr), remote->ai_addrlen);
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "Connecting to %s", msg);
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
    ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "ircserver socket = %d", gnetwork->ircserver);
  }

  gnetwork->lastservercontact=gdata.curtime;

  /* good */
  gnetwork->serverstatus = SERVERSTATUS_TRYING;

  return 0;
}

/* End of File */
