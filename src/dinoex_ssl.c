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
#include "dinoex_ssl.h"
#include "dinoex_utilities.h"
#include "dinoex_kqueue.h"

#ifdef USE_OPENSSL
#include <openssl/err.h>
#endif /* USE_OPENSSL */

#ifdef USE_GNUTLS
#include <gnutls/x509.h>

static const char *iroffer_priority = "NORMAL"; /* NOTRANSLATE */
static gnutls_priority_t iroffer_priority_cache = NULL;

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
static void outerror_ssl(long err)
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
  gnutls_global_set_log_level(1);
#endif /* DEBUG */

  ret = gnutls_certificate_allocate_credentials (&iroffer_cred);
  if (ret < 0)
    outerror_ssl(ret);
#endif /* USE_GNUTLS */
}

/* close connection to server, shutdown SSL if active */
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
    if (gnetwork->user_cred) {
      gnutls_certificate_free_credentials(gnetwork->user_cred);
      gnetwork->user_cred = 0;
    }
    if (gnetwork->nick_cert) {
      gnutls_x509_crt_deinit(gnetwork->nick_cert);
      gnetwork->nick_cert = 0;
    }
    if (gnetwork->nick_key) {
      gnutls_x509_privkey_deinit(gnetwork->nick_key);
      gnetwork->nick_key = 0;
    }
    gnutls_bye(gnetwork->session, GNUTLS_SHUT_RDWR);
    if (iroffer_priority_cache != NULL) {
      gnutls_priority_deinit(iroffer_priority_cache);
      iroffer_priority_cache = NULL;
    }
    gnutls_deinit(gnetwork->session);
    gnetwork->session = NULL;
  }
#endif /* USE_GNUTLS */
  shutdown_close(gnetwork->ircserver);
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
  /* do not reconnect immediatly */
  gnetwork->lastservercontact = gdata.curtime + gdata.reconnect_delay;
}

#if defined(USE_OPENSSL) || defined(USE_GNUTLS)
static char *keyfile_present(const char *suffix)
{
  char *tempstr;
  int fd;

  tempstr = mystrsuffix(gnetwork->name,  suffix);
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
static void load_ssl_cert(const char *filename)
{
  SSL_CTX_use_certificate_file(gnetwork->ssl_ctx, filename, SSL_FILETYPE_PEM);
  outerror_ssl();
}

static void load_ssl_key(const char *filename)
{
  SSL_CTX_use_PrivateKey_file(gnetwork->ssl_ctx, filename, SSL_FILETYPE_PEM);
  outerror_ssl();
}

static void load_network_key(void)
{
  char *tempstr;

  tempstr = keyfile_present(".pem"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_cert(tempstr);
    load_ssl_key(tempstr);
    mydelete(tempstr);
    return;
  }
  tempstr = keyfile_present(".crt"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_cert(tempstr);
    mydelete(tempstr);
  }
  tempstr = keyfile_present(".key"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    load_ssl_key(tempstr);
    mydelete(tempstr);
  }
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

static int load_ssl_cert(gnutls_datum_t *datum)
{
  int ret;

  gnutls_x509_crt_init(&gnetwork->nick_cert);
  ret = gnutls_x509_crt_import(gnetwork->nick_cert, datum, GNUTLS_X509_FMT_PEM);
  if (ret < 0) {
    outerror_ssl(ret);
    return 1;
  }
  return 0;
}

static int load_ssl_key(gnutls_datum_t *datum)
{
  int ret;

  gnutls_x509_privkey_init(&gnetwork->nick_key);
  ret = gnutls_x509_privkey_import(gnetwork->nick_key, datum, GNUTLS_X509_FMT_PEM);
  if (ret < 0) {
    outerror_ssl(ret);
    return 1;
  }
  return 0;
}

static int load_network_key(void)
{
  char *tempstr;
  void *buffer;
  gnutls_datum_t datum;

  tempstr = keyfile_present(".pem"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }

    if (load_ssl_cert(&datum)) {
      mydelete(buffer);
      return 0;
    }

    if (load_ssl_key(&datum)) {
      mydelete(buffer);
      return 0;
    }
    mydelete(buffer);
    return 1; /* loaded */
  }
  tempstr = keyfile_present(".crt"); /* NOTRANSLATE */
  if (tempstr != NULL) {
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }

    if (load_ssl_cert(&datum)) {
      mydelete(buffer);
      return 0;
    }

    mydelete(buffer);
    tempstr = keyfile_present(".key"); /* NOTRANSLATE */
    if (tempstr == NULL) {
      return 0;
    }
    buffer = keyfile_load(&datum, tempstr);
    mydelete(tempstr);
    if (buffer == NULL) {
      return 0;
    }
    if (load_ssl_key(&datum)) {
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
  st->cert.x509 = &(gnetwork->nick_cert);
  st->key.x509 = gnetwork->nick_key;
  st->deinit_all = 0;
  return 0;
}

#endif /* USE_GNUTLS */

/* retry setup an SSL connection */
static int retry_ssl(void)
{
#ifdef USE_OPENSSL
  unsigned long err;
  int rc;

  updatecontext();

  rc = SSL_connect(gnetwork->ssl);
  if (rc != 1) {
    err = outerror_ssl();
    if ((err == 0) || (err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
      return -1;
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    close_server();
    return 1;
  }
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  int ret;

  updatecontext();

  ret = gnutls_handshake(gnetwork->session);
  if (ret < 0) {
    if ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN))
      return -1;
    outerror_ssl(ret);
    close_server();
    return 1;
  }
#endif /* USE_GNUTLS */
  return 0;
}

/* setup an SSL connection */
static int setup_ssl(void)
{
#ifdef USE_OPENSSL
  int rc;

  updatecontext();

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
  (void)SSL_CTX_set_options(gnetwork->ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

  /* load key per network */
  load_network_key();

  gnetwork->ssl = SSL_new(gnetwork->ssl_ctx);
  rc = SSL_set_fd(gnetwork->ssl, gnetwork->ircserver);
  outerror_ssl();
  if (rc == 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant set SSL socket");
    close_server();
    return 1;
  }
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
  const char *bad;
  int ret;

  updatecontext();

  ret = gnutls_init(&(gnetwork->session), GNUTLS_CLIENT);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  ret = gnutls_priority_init(&iroffer_priority_cache, iroffer_priority, &bad);
  if (ret < 0) {
    outerror_ssl(ret);
    /* gnuTLS will dump core if deinit after error */
    iroffer_priority_cache = NULL;
    close_server();
    return 1;
  }
  ret = gnutls_priority_set(gnetwork->session, iroffer_priority_cache);
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }

  /* load key per network */
  if (load_network_key()) {
    ret = gnutls_certificate_allocate_credentials (&(gnetwork->user_cred));
    if (ret < 0) {
      outerror_ssl(ret);
      gnutls_transport_set_ptr(gnetwork->session, (gnutls_transport_ptr_t) (long)(gnetwork->ircserver));
      return 1;
    }
    gnutls_certificate_set_retrieve_function(gnetwork->user_cred, cert_callback);
    ret = gnutls_credentials_set(gnetwork->session, GNUTLS_CRD_CERTIFICATE, gnetwork->user_cred);
  } else {
    gnetwork->user_cred = 0;
    ret = gnutls_credentials_set(gnetwork->session, GNUTLS_CRD_CERTIFICATE, iroffer_cred);
  }
  if (ret < 0) {
    outerror_ssl(ret);
    close_server();
    return 1;
  }
  gnutls_transport_set_ptr(gnetwork->session, (gnutls_transport_ptr_t) (long)(gnetwork->ircserver));
#endif /* USE_GNUTLS */
  return retry_ssl();
}

static int handshake2_ssl(void)
{
  int rc;

  if (gnetwork->connectionmethod.how != how_ssl)
    return 0;

  if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED) {
    rc = setup_ssl();
    if (rc < 0 )
      gnetwork->serverstatus = SERVERSTATUS_SSL_HANDSHAKE;
    return rc;
  }

  if (gnetwork->serverstatus == SERVERSTATUS_SSL_HANDSHAKE) {
    rc = retry_ssl();
    if (rc == 0)
      gnetwork->serverstatus = SERVERSTATUS_CONNECTED;
    return rc;
  }
  return 1;
}

/* setup ssl connection if configured */
void handshake_ssl(void)
{
  int rc;

  updatecontext();

  rc = handshake2_ssl();
  if (rc == 0)
    initirc();
}

/* read buffer from server, use SSL if active */
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
  return recv(gnetwork->ircserver, buf, nbytes, MSG_DONTWAIT);
}

/* write out buffer to server, use SSL if active */
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
  return send(gnetwork->ircserver, buf, nbytes, MSG_NOSIGNAL);
}

/* End of File */
