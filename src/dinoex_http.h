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

typedef enum
{
  HTTP_STATUS_UNUSED,
  HTTP_STATUS_GETTING,
  HTTP_STATUS_POST,
  HTTP_STATUS_SENDING,
  HTTP_STATUS_DONE
} http_status_e;

typedef struct
{
  off_t bytesgot;
  off_t bytessent;
  off_t filepos;
  off_t totalsize;
  ir_connection_t con;
  int filedescriptor;
  http_status_e status;
  char *file;
  char *buffer_out;
  char *end;
  const char *nick;
  const char *attachment;
  char *log_url;
  char *url;
  char *authorization;
  char *group;
  char *order;
  char *search;
  char *pattern;
  char *modified;
  ssize_t left;
  unsigned int traffic;
  unsigned int status_code;
  unsigned int support_groups;
  unsigned int post;
  unsigned int head;
  unsigned int idummy;
} http;

typedef struct {
  char *m_ext;
  char *m_mime;
} http_magic_t;

#ifndef WITHOUT_HTTP
#ifndef WITHOUT_HTTP_ADMIN
void init_base64decode( void );
#endif /* #ifndef WITHOUT_HTTP_ADMIN */
#endif /* #ifndef WITHOUT_HTTP */

void h_close_listen(void);
unsigned int h_setup_listen(void);
void h_reash_listen(void);
int h_select_fdset(int highests);
void h_perform(int changesec);

/* End of File */
