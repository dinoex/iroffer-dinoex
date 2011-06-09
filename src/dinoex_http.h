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
  ir_connection_t con;
  off_t bytesgot;
  off_t bytessent;
  off_t filepos;
  off_t totalsize;
  off_t range_start;
  off_t range_end;
  const char *nick;
  const char *attachment;
  char *file;
  char *buffer_out;
  char *end;
  char *log_url;
  char *url;
  char *authorization;
  char *group;
  char *order;
  char *search;
  char *pattern;
  char *modified;
  char *range;
  long tx_bucket;
  ssize_t left;
  unsigned int traffic;
  unsigned int status_code;
  unsigned int support_groups;
  unsigned int post;
  unsigned int head;
  unsigned int idummy;
  float maxspeed;
  http_status_e status;
  int filedescriptor;
  char overlimit;
  char unlimited;
  char cdummy2;
  char cdummy1;
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
void h_perform(int changesec, int changequartersec);

/* End of File */
