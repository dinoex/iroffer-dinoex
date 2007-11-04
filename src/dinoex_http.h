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

typedef enum
{
  HTTP_STATUS_UNUSED,
  HTTP_STATUS_GETTING,
  HTTP_STATUS_SENDING,
  HTTP_STATUS_DONE
} http_status_e;

typedef struct
{
  int clientsocket;
  int filedescriptor;
  off_t bytesgot;
  off_t bytessent;
  off_t filepos;
  off_t totalsize;
  time_t lastcontact;
  time_t connecttime;
  unsigned short family;
  unsigned short remoteport;
  long remoteip4;
  struct in6_addr remoteip6;
  http_status_e status;
  int support_groups;
  char *file;
  char *buffer;
  char *end;
  const char *nick;
  char *group;
  char *order;
  ssize_t left;
  int traffic;
} http;

void h_close_listen(void);
int h_setup_listen(void);
void h_reash_listen(void);
int h_listen(int highests);
void h_done_select(int changesec);

/* End of File */
