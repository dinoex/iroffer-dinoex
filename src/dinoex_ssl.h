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

void ssl_startup(void);
void close_server(void);
void handshake_ssl(void);
void chat_shutdown_ssl(dccchat_t *chat, int flush);
void chat_connect_retry_ssl(dccchat_t *chat);
void chat_handshake_ssl(dccchat_t *chat);
void chat_accept_retry_ssl(dccchat_t *chat);
void chat_accept_ssl(dccchat_t *chat);
ssize_t readserver_ssl(void *buf, size_t nbytes);
ssize_t writeserver_ssl(const void *buf, size_t nbytes);
ssize_t chat_read_ssl(dccchat_t *chat, void *buf, size_t nbytes);
#ifdef USE_OPENSSL
ssize_t chat_write_ssl(ir_openssl_t *ssl, const void *buf, size_t nbytes);
#endif /* USE_OPENSSL */
#ifdef USE_GNUTLS
ssize_t chat_write_tls(ir_gnutls_t *tls, const void *buf, size_t nbytes);
#endif /* USE_GNUTLS */

/* End of File */
