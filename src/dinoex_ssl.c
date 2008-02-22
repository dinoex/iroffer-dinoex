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
#include "dinoex_ssl.h"
#include "dinoex_utilities.h"

#ifdef USE_SSL
#include <openssl/err.h>
#endif /* USE_SSL */

void ssl_startup(void)
{
#ifdef USE_SSL
  SSL_library_init();
  SSL_load_error_strings();
#endif /* USE_SSL */
}

void close_server(void)
{
#ifdef USE_SSL
  if (gnetwork->ssl != NULL) {
    SSL_free(gnetwork->ssl);
    gnetwork->ssl = NULL;
  }
#endif /* USE_SSL */
  FD_CLR(gnetwork->ircserver, &gdata.readset);
  shutdown_close(gnetwork->ircserver);
  gnetwork->serverstatus = SERVERSTATUS_NEED_TO_CONNECT;
}

#ifdef USE_SSL
static void outerror_ssl(void)
{
#if !defined(_OS_CYGWIN)
  unsigned long err;

  err = ERR_get_error();
  if (err == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD, "SSL Error %ld:%s", err, ERR_error_string(err, NULL));
#endif
}
#endif /* USE_SSL */

int setup_ssl(void)
{
#ifdef USE_SSL
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
#endif /* USE_SSL */
  return 0;
}

ssize_t readserver_ssl(void *buf, size_t nbytes)
{
#ifdef USE_SSL
  if (gnetwork->ssl != NULL)
    return SSL_read(gnetwork->ssl, buf, nbytes);
#endif /* USE_SSL */
  return read(gnetwork->ircserver, buf, nbytes);
}

ssize_t writeserver_ssl(const void *buf, size_t nbytes)
{
#ifdef USE_SSL
  if (gnetwork->ssl != NULL)
    return SSL_write(gnetwork->ssl, buf, nbytes);
#endif /* USE_SSL */
  return write(gnetwork->ircserver, buf, nbytes);
}

/* End of File */
