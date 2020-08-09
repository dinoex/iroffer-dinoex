/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2020 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.net/
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
#include "dinoex_kqueue.h"
#include "dinoex_chat.h"

#ifdef USE_OPENSSL
#include <openssl/err.h>
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
#include <gnutls/x509.h>
#endif /* USE_GNUTLS */

#if defined(DEBUG) && defined(USE_GNUTLS)
static void tls_log_func(int level, const char *str)
{
  outerror(OUTERROR_TYPE_WARN_LOUD, "DEBUG: |%d| %s", level, str);
}
#endif /* DEBUG and USE_GNUTLS */

#ifdef USE_OPENSSL
static unsigned long outerror_ssl(void)
{
  unsigned long err;

  err = ERR_get_error();
#if !defined(_OS_CYGWIN)
  if (err != 0)
    outerror(OUTERROR_TYPE_WARN_LOUD, "SSL Error %ld:%s", err, ERR_error_string(err, NULL));
#endif /* _OS_CYGWIN */
  return err;
}
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
static void outerror_tls(long err)
{
  if (err == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD, "SSL Error %ld:%s", err, gnutls_strerror(err));
}
#endif /* USE_GNUTLS */

/* setup SSL lib */
void ssl_startup(void)
{
#ifdef USE_OPENSSL
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100001L
  SSL_library_init();
  SSL_load_error_strings();
#else
  OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                   OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#endif /* OPENSSL_VERSION_NUMBER */
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  int ret;

  ret = gnutls_global_init();
  if (ret < 0)
    outerror_tls(ret);

#if defined(DEBUG)
  gnutls_global_set_log_function(tls_log_func);
  gnutls_global_set_log_level(10);
#endif /* DEBUG */
#endif /* USE_GNUTLS */
}

#ifdef USE_OPENSSL
static void close_ssl(ir_openssl_t *ssl)
{
  if (ssl->ssl_ctx != NULL) {
    SSL_CTX_free(ssl->ssl_ctx);
    ssl->ssl_ctx = NULL;
  }

  if (ssl->ssl != NULL) {
    SSL_free(ssl->ssl);
    ssl->ssl = NULL;
  }
}
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
static void close_tls(ir_gnutls_t *tls)
{
  if (tls->session == NULL)
    return;

  gnutls_bye(tls->session, GNUTLS_SHUT_RDWR);
  gnutls_deinit(tls->session);
  tls->session = NULL;

  if (tls->user_cred != NULL) {
    gnutls_certificate_free_credentials(tls->user_cred);
    tls->user_cred = NULL;
  }
  if (tls->user_cert != NULL) {
    gnutls_x509_crt_deinit(tls->user_cert);
    tls->user_cert = NULL;
  }
  if (tls->user_key != NULL) {
    gnutls_x509_privkey_deinit(tls->user_key);
    tls->user_key = NULL;
  }
  if (tls->priority_cache != NULL) {
    gnutls_priority_deinit(tls->priority_cache);
    tls->priority_cache = NULL;
  }
}
#endif /* USE_GNUTLS */

/* close connection to server, shutdown SSL if active */
void close_server(void)
{
#ifdef USE_OPENSSL
  close_ssl(&(gnetwork->ssl));
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  close_tls(&(gnetwork->tls));
#endif /* USE_GNUTLS */
  shutdown_close(gnetwork->ircserver);
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
  /* do not reconnect immediatly */
  gnetwork->lastservercontact = gdata.curtime + gdata.reconnect_delay;
}

#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
static char *keyfile_present(const char *name, const char *suffix)
{
  char *tempstr;
  int fd;

  tempstr = mystrsuffix(name, suffix);
  fd = open(tempstr, O_RDONLY | ADDED_OPEN_FLAGS);
  if (fd > 0) {
    close(fd);
    return tempstr;
  }
  mydelete(tempstr);
  return NULL;
}
#endif /* USE_OPENSSL or USE_GNUTLS */

#ifdef USE_OPENSSL

static void load_ssl_cert(ir_openssl_t *ssl, const char *filename)
{
  SSL_CTX_use_certificate_file(ssl->ssl_ctx, filename, SSL_FILETYPE_PEM);
  outerror_ssl();
}

static void load_ssl_key(ir_openssl_t *ssl, const char *filename)
{
  SSL_CTX_use_PrivateKey_file(ssl->ssl_ctx, filename, SSL_FILETYPE_PEM);
  outerror_ssl();
}

static void load_keys(ir_openssl_t *ssl, const char *name)
{
  char *tempstr;

  tempstr = keyfile_present(name, ".pem"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_cert(ssl, tempstr);
    load_ssl_key(ssl, tempstr);
    mydelete(tempstr);
    return;
  }
  tempstr = keyfile_present(name, ".crt"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_cert(ssl, tempstr);
    mydelete(tempstr);
  }
  tempstr = keyfile_present(name, ".key"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_key(ssl, tempstr);
    mydelete(tempstr);
  }
}

/* retry setup an SSL connection */
static int connect_retry_ssl(SSL *ssl)
{
  unsigned long err;
  int rc;

  updatecontext();

  rc = SSL_connect(ssl);
  if (rc != 1) {
    err = outerror_ssl();
    if ((err == 0) || (err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
      return -1;
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    return 1;
  }
  return 0;
}

/* setup SSL outgoing connection */
static int connect_ssl(ir_openssl_t *ssl, const char *name, int fd,
                       const SSL_METHOD *method)
{
  int rc;

  updatecontext();

  if (ssl->ssl_ctx == NULL) {
    ssl->ssl_ctx = SSL_CTX_new(method);
    if (ssl->ssl_ctx == NULL) {
      outerror_ssl();
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Create SSL context");
      return 1;
    }
  }
  (void)SSL_CTX_set_options(ssl->ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

  /* load key per network */
  load_keys(ssl, name);

  ssl->ssl = SSL_new(ssl->ssl_ctx);
  rc = SSL_set_fd(ssl->ssl, fd);
  outerror_ssl();
  if (rc == 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    return 1;
  }
  return 0;
}

/* retry setup an SSL connection */
static int accept_retry_ssl(SSL *ssl)
{
  unsigned long err;
  int rc;

  updatecontext();

  rc = SSL_accept(ssl);
  if (rc != 1) {
    err = outerror_ssl();
    if ((err == 0) || (err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
      return -1;
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    return 1;
  }
  return 0;
}

#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS

static void *keyfile_load(gnutls_datum_t *datum, const char *filename)
{
  struct stat st;
  int fd;

  fd = open(filename, O_RDONLY | ADDED_OPEN_FLAGS);
  if (fd < 0) {
    return NULL;
  }

  if (fstat(fd, &st) < 0) {
    close(fd);
    return NULL;
  }

  datum->size = (unsigned int) st.st_size;
  datum->data = mymalloc(datum->size);
  read(fd, datum->data, datum->size);
  close(fd);
  return datum->data;
}

static int load_tls_cert(ir_gnutls_t *tls, gnutls_datum_t *datum)
{
  int ret;

  gnutls_x509_crt_init(&tls->user_cert);
  ret = gnutls_x509_crt_import(tls->user_cert, datum, GNUTLS_X509_FMT_PEM);
  if (ret < 0) {
    outerror_tls(ret);
    return 1;
  }
  return 0;
}

static int load_tls_key(ir_gnutls_t *tls, gnutls_datum_t *datum)
{
  int ret;

  gnutls_x509_privkey_init(&tls->user_key);
  ret = gnutls_x509_privkey_import(tls->user_key, datum, GNUTLS_X509_FMT_PEM);
  if (ret < 0) {
    outerror_tls(ret);
    return 1;
  }
  return 0;
}

static int load_keys(ir_gnutls_t *tls, const char *name)
{
  char *tempstr;
  void *buffer;
  gnutls_datum_t datum;

  tempstr = keyfile_present(name, ".pem"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }

    if (load_tls_cert(tls, &datum)) {
      mydelete(buffer);
      return 0;
    }

    if (load_tls_key(tls, &datum)) {
      mydelete(buffer);
      return 0;
    }
    mydelete(buffer);
    return 1; /* loaded */
  }
  tempstr = keyfile_present(name, ".crt"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }

    if (load_tls_cert(tls, &datum)) {
      mydelete(buffer);
      return 0;
    }

    mydelete(buffer);
    tempstr = keyfile_present(name, ".key"); /* NOTRANSLATE */
    if (tempstr == NULL) {
      return 0;
    }
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }
    if (load_tls_key(tls, &datum)) {
      mydelete(buffer);
      return 0;
    }
    mydelete(buffer);
    return 1; /* loaded */
  }
  return 0;
}

static int cert_callback(gnutls_session_t session,
                         const gnutls_datum_t * UNUSED(req_ca_dn), int UNUSED(nreqs),
                         const gnutls_pk_algorithm_t * UNUSED(pk_algos),
                         int UNUSED(sign_algos_length), gnutls_retr2_st * st)
{
  gnutls_certificate_type_t type;

  type = gnutls_certificate_type_get(session);
  if (type != GNUTLS_CRT_X509)
    return -1;

  st->cert_type = type;
  st->ncerts = 1;
  st->cert.x509 = &(gnetwork->tls.user_cert);
  st->key.x509 = gnetwork->tls.user_key;
  st->deinit_all = 0;
  return 0;
}

/* retry setup an SSL connection */
static int connect_retry_tls(gnutls_session_t session)
{
  int ret;

  updatecontext();

  ret = gnutls_handshake(session);
  if (ret < 0) {
    if ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN))
      return -1;
    outerror_tls(ret);
    return 1;
  }
  return 0;
}

/* setup SSL outgoing connection */
static int connect_tls(ir_gnutls_t *tls, const char *name, int fd,
                       unsigned int flags)
{
  int ret;

  updatecontext();

  ret = gnutls_init(&(tls->session), flags);
  if (ret < 0) {
    outerror_tls(ret);
    return 1;
  }
  ret = gnutls_priority_init(&(tls->priority_cache), NULL, NULL);
  if (ret < 0) {
    outerror_tls(ret);
    /* gnuTLS will dump core if deinit after error */
    tls->priority_cache = NULL;
    return 1;
  }
  ret = gnutls_priority_set(tls->session, tls->priority_cache);
  if (ret < 0) {
    outerror_tls(ret);
    return 1;
  }

  /* load key per network */
  ret = gnutls_certificate_allocate_credentials (&(tls->user_cred));
  if (ret < 0) {
    outerror_tls(ret);
    tls->user_cred = NULL;
    return 1;
  }

  if (load_keys(tls, name)) {
    gnutls_certificate_set_retrieve_function(tls->user_cred, cert_callback);
  }

  ret = gnutls_credentials_set(tls->session, GNUTLS_CRD_CERTIFICATE, tls->user_cred);
  if (ret < 0) {
    outerror_tls(ret);
    return 1;
  }
  gnutls_transport_set_ptr(tls->session, (gnutls_transport_ptr_t) (long)(fd));
  return 0;
}

/* retry setup an SSL connection */
static int accept_retry_tls(gnutls_session_t session)
{
  int ret;

  updatecontext();

  ret = gnutls_handshake(session);
  if (ret < 0) {
    if ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN))
      return -1;
    outerror_tls(ret);
    return 1;
  }
  return 0;
}

#endif /* USE_GNUTLS */

/* retry setup an SSL connection */
static int server_retry_ssl(void)
{
  int ret = 0;

#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
#ifdef USE_OPENSSL
  ret = connect_retry_ssl(gnetwork->ssl.ssl);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  ret = connect_retry_tls(gnetwork->tls.session);
#endif /* USE_GNUTLS */

  if (ret > 0)
    close_server();
#endif /* USE_OPENSSL or USE_GNUTLS */

  return ret;
}

#ifdef USE_OPENSSL
#endif /* USE_OPENSSL */

/* setup an SSL connection */
static int server_setup_ssl(void)
{
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int rc;

  updatecontext();

#ifdef USE_OPENSSL
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100001L
  rc = connect_ssl(&(gnetwork->ssl), gnetwork->name, gnetwork->ircserver,
                   SSLv23_client_method());
#else
  rc = connect_ssl(&(gnetwork->ssl), gnetwork->name, gnetwork->ircserver,
                   TLS_client_method());
#endif
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  rc = connect_tls(&(gnetwork->tls), gnetwork->name, gnetwork->ircserver,
                   GNUTLS_CLIENT);
#endif /* USE_GNUTLS */
  if (rc > 0) {
    close_server();
    return rc;
  }
#endif /* USE_OPENSSL or USE_GNUTLS */
  return server_retry_ssl();
}

static int handshake2_ssl(void)
{
  int rc;

  if (gnetwork->connectionmethod.how != how_ssl)
    return 0;

  if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED) {
    rc = server_setup_ssl();
    if (rc < 0 )
      gnetwork->serverstatus = SERVERSTATUS_SSL_HANDSHAKE;
    return rc;
  }

  if (gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE) {
    rc = server_retry_ssl();
    if (rc == 0)
      gnetwork->serverstatus = SERVERSTATUS_CONNECTED;
    return rc;
  }
  return 1;
}

/* setup ssl server connection if configured */
void handshake_ssl(void)
{
  int rc;

  updatecontext();

  rc = handshake2_ssl();
  if (rc == 0)
    initirc();
}

/* close chat connection, shutdown SSL if active */
void chat_shutdown_ssl(dccchat_t *chat, int flush)
{
#ifdef USE_OPENSSL
  close_ssl(&(chat->ssl));
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  close_tls(&(chat->tls));
#endif /* USE_GNUTLS */
  chat_shutdown(chat, flush);
}

/* retry setup an SSL connection */
void chat_connect_retry_ssl(dccchat_t *chat)
{
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int ret;

#ifdef USE_OPENSSL
  ret = connect_retry_ssl(chat->ssl.ssl);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  ret = connect_retry_tls(chat->tls.session);
#endif /* USE_GNUTLS */

  if (ret > 0)
    chat_shutdown_ssl(chat, 0);

  if (ret == 0) {
    chat->status = DCCCHAT_AUTHENTICATING;
    chat_banner(chat);
  }
#endif /* USE_OPENSSL or USE_GNUTLS */
}

/* setup ssl chat connection */
void chat_handshake_ssl(dccchat_t *chat)
{
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int rc;

  updatecontext();

#ifdef USE_OPENSSL
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100001L
  rc = connect_ssl(&(chat->ssl), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   SSLv23_client_method());
#else
  rc = connect_ssl(&(chat->ssl), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   TLS_client_method());
#endif
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  rc = connect_tls(&(chat->tls), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   GNUTLS_CLIENT);
#endif /* USE_GNUTLS */
  if (rc > 0) {
    chat_shutdown_ssl(chat, 0);
    return;
  }

  chat_connect_retry_ssl(chat);
#endif /* USE_OPENSSL or USE_GNUTLS */
}

/* retry setup an SSL connection */
void chat_accept_retry_ssl(dccchat_t *chat)
{
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int ret;

#ifdef USE_OPENSSL
  ret = accept_retry_ssl(chat->ssl.ssl);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  ret = accept_retry_tls(chat->tls.session);
#endif /* USE_GNUTLS */

  if (ret > 0)
    chat_shutdown_ssl(chat, 0);

  if (ret == 0) {
    chat->status = DCCCHAT_AUTHENTICATING;
    chat_banner(chat);
  }
#endif /* USE_OPENSSL or USE_GNUTLS */
}

/* setup ssl chat connection */
void chat_accept_ssl(dccchat_t *chat)
{
#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
  int rc;

  updatecontext();

#ifdef USE_OPENSSL
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100001L
  rc = connect_ssl(&(chat->ssl), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   SSLv23_server_method());
#else
  rc = connect_ssl(&(chat->ssl), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   TLS_server_method());
#endif
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  rc = connect_tls(&(chat->tls), "chat", chat->con.clientsocket, /* NOTRANSLATE */
                   GNUTLS_SERVER);
#endif /* USE_GNUTLS */
  if (rc > 0) {
    chat_shutdown_ssl(chat, 0);
    return;
  }

  chat_accept_retry_ssl(chat);
#endif /* USE_OPENSSL or USE_GNUTLS */
}

/* read buffer from server, use SSL if active */
ssize_t readserver_ssl(void *buf, size_t nbytes)
{
#ifdef USE_OPENSSL
  if (gnetwork->ssl.ssl != NULL)
    return SSL_read(gnetwork->ssl.ssl, buf, nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (gnetwork->tls.session != NULL)
    return gnutls_record_recv(gnetwork->tls.session, buf, nbytes);
#endif /* USE_GNUTLS */
  return recv(gnetwork->ircserver, buf, nbytes, MSG_DONTWAIT);
}

/* write out buffer to server, use SSL if active */
ssize_t writeserver_ssl(const void *buf, size_t nbytes)
{
#ifdef USE_OPENSSL
  if (gnetwork->ssl.ssl != NULL)
    return SSL_write(gnetwork->ssl.ssl, buf, nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (gnetwork->tls.session != NULL)
    return gnutls_record_send(gnetwork->tls.session, buf, nbytes);
#endif /* USE_GNUTLS */
  return send(gnetwork->ircserver, buf, nbytes, MSG_NOSIGNAL);
}

/* read buffer from chat, use SSL if active */
ssize_t chat_read_ssl(dccchat_t *chat, void *buf, size_t nbytes)
{
#ifdef USE_OPENSSL
  if (chat->ssl.ssl != NULL)
    return SSL_read(chat->ssl.ssl, buf, nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  if (chat->tls.session != NULL)
    return gnutls_record_recv(chat->tls.session, buf, nbytes);
#endif /* USE_GNUTLS */
  return recv(chat->con.clientsocket, buf, nbytes, MSG_DONTWAIT);
}

#ifdef USE_OPENSSL
/* write out buffer to chat, use SSL if active */
ssize_t chat_write_ssl(ir_openssl_t *ssl, const void *buf, size_t nbytes)
{
  return SSL_write(ssl->ssl, buf, nbytes);
}
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
/* write out buffer to chat, use SSL if active */
ssize_t chat_write_tls(ir_gnutls_t *tls, const void *buf, size_t nbytes)
{
  return gnutls_record_send(tls->session, buf, nbytes);
}
#endif /* USE_GNUTLS */

/* End of File */
