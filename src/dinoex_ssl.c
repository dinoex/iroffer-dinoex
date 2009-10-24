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
#include "dinoex_ssl.h"
#include "dinoex_utilities.h"

#ifdef USE_OPENSSL
#include <openssl/err.h>
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
int iroffer_protocol_priority[] = {
  GNUTLS_TLS1,
  GNUTLS_SSL3, 0 };
int iroffer_cipher_priority[] = {
  GNUTLS_CIPHER_AES_128_CBC,
  GNUTLS_CIPHER_3DES_CBC,
  GNUTLS_CIPHER_AES_256_CBC,
  GNUTLS_CIPHER_ARCFOUR_128, 0 };
int iroffer_compression_priority[] = {
  GNUTLS_COMP_ZLIB,
  GNUTLS_COMP_NULL, 0 };
int iroffer_kx_priority[] = {
  GNUTLS_KX_DHE_RSA,
  GNUTLS_KX_RSA,
  GNUTLS_KX_DHE_DSS, 0 };
int iroffer_mac_priority[] = {
  GNUTLS_MAC_SHA1,
  GNUTLS_MAC_MD5, 0 };
static gnutls_certificate_credentials_t iroffer_cred;
#endif /* USE_GNUTLS */

#if defined(DEBUG) && defined(USE_GNUTLS)
static void tls_log_func(int level, const char *str)
{
  outerror(OUTERROR_TYPE_WARN_LOUD, "DEBUG: |%d| %s", level, str);
}
#endif /* DEBUG and USE_GNUTLS */

#ifdef USE_OPENSSL
static void outerror_ssl(void)
{
#if !defined(_OS_CYGWIN)
  unsigned long err;

  err = ERR_get_error();
  if (err == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD, "SSL Error %ld:%s", err, ERR_error_string(err, NULL));
#endif /* _OS_CYGWIN */
}
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
static void outerror_ssl(int err)
{
  outerror(OUTERROR_TYPE_WARN_LOUD, "SSL Error %d:%s", err, gnutls_strerror(err));
}
#endif /* USE_GNUTLS */

void ssl_startup(void)
{
#ifdef USE_OPENSSL
  SSL_library_init();
  SSL_load_error_strings();
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  int ret;

  ret = gnutls_global_init();
  if (ret < 0)
    outerror_ssl(ret);

#if defined(DEBUG)
  gnutls_global_set_log_function(tls_log_func);
  gnutls_global_set_log_level(10);
#endif /* DEBUG */

  ret = gnutls_certificate_allocate_credentials (&iroffer_cred);
  if (ret < 0)
    outerror_ssl(ret);
#endif /* USE_GNUTLS */
}

void close_server(void)
{
#ifdef USE_OPENSSL
  if (gnetwork->ssl != NULL) {
    SSL_free(gnetwork->ssl);
    gnetwork->ssl = NULL;
  }
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (gnetwork->session != NULL) {
    gnutls_bye(gnetwork->session, GNUTLS_SHUT_RDWR);
    gnutls_deinit(gnetwork->session);
    gnetwork->session = NULL;
  }
#endif /* USE_GNUTLS */
  FD_CLR(gnetwork->ircserver, &gdata.readset);
  shutdown_close(gnetwork->ircserver);
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
  /* do not reconnect immediatly */
  gnetwork->lastservercontact = gdata.curtime + gdata.reconnect_delay;
}

int setup_ssl(void)
{
#ifdef USE_OPENSSL
  int rc;

  updatecontext();

  if (gnetwork->connectionmethod.how != how_ssl)
    return 0;

  if (gnetwork->ssl_ctx == NULL) {
    gnetwork->ssl_ctx = SSL_CTX_new( SSLv23_client_method() );
    if (gnetwork->ssl_ctx == NULL) {
      outerror_ssl();
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Create SSL context");
      close_server();
      return 1;
    }
  }
  /* SSL_CTX_free() ist not called */

  gnetwork->ssl = SSL_new(gnetwork->ssl_ctx);
  rc = SSL_set_fd(gnetwork->ssl, gnetwork->ircserver);
  outerror_ssl();
  if (rc == 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    close_server();
    return 1;
  }
  rc = SSL_connect(gnetwork->ssl);
  if (rc == 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    close_server();
    return 1;
  }
  outerror_ssl();
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  int ret;

  ret = gnutls_init(&(gnetwork->session), GNUTLS_CLIENT);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_protocol_set_priority(gnetwork->session, iroffer_protocol_priority);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_cipher_set_priority(gnetwork->session, iroffer_cipher_priority);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_compression_set_priority(gnetwork->session, iroffer_compression_priority);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_kx_set_priority(gnetwork->session, iroffer_kx_priority);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_mac_set_priority(gnetwork->session, iroffer_mac_priority);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_credentials_set(gnetwork->session, GNUTLS_CRD_CERTIFICATE, iroffer_cred);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  gnutls_transport_set_ptr(gnetwork->session, (gnutls_transport_ptr_t) (gnetwork->ircserver));
  ret = gnutls_handshake(gnetwork->session);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
#endif /* USE_GNUTLS */
  return 0;
}

ssize_t readserver_ssl(void *buf, size_t nbytes)
{
#ifdef USE_OPENSSL
  if (gnetwork->ssl != NULL)
    return SSL_read(gnetwork->ssl, buf, nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (gnetwork->session != NULL)
    return gnutls_record_recv(gnetwork->session, buf, nbytes);
#endif /* USE_GNUTLS */
  return read(gnetwork->ircserver, buf, nbytes);
}

ssize_t writeserver_ssl(const void *buf, size_t nbytes)
{
#ifdef USE_OPENSSL
  if (gnetwork->ssl != NULL)
    return SSL_write(gnetwork->ssl, buf, nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (gnetwork->session != NULL)
    return gnutls_record_send(gnetwork->session, buf, nbytes);
#endif /* USE_GNUTLS */
  return write(gnetwork->ircserver, buf, nbytes);
}

/* End of File */
