/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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
  HTTP_STATUS_WAITING,
  HTTP_STATUS_DONE
} http_status_e;

typedef struct
{
  int clientsocket;
  int filedescriptor;
  off_t bytesgot;
  off_t bytessent;
  off_t totalsize;
  time_t lastcontact;
  time_t connecttime;
  unsigned short remoteport;
  unsigned short localport;
  unsigned long remoteip;
  unsigned long localip;
  http_status_e ul_status;
  char *file;
} http;

void h_close_listen(void);
int h_setup_listen(void);
int h_listen(int highests);

void h_accept(void);
void h_closeconn(http * const h, const char *msg, int errno1);
void h_establishcon (http * const h);
void h_get(http * const h);
void h_send(http * const h);
void h_istimeout(http * const h);
void h_done_select(int changesec);

/* End of File */
