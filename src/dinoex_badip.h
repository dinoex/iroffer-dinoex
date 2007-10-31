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

typedef struct
{
  time_t lastcontact;
  time_t connecttime;
  long remoteip;
  long count;
} badip4;

typedef struct
{
  time_t lastcontact;
  time_t connecttime;
  struct in6_addr remoteip;
  long count;
} badip6;

int is_in_badip4(long remoteip);
int is_in_badip6(struct in6_addr *remoteip);
void count_badip4(long remoteip);
void count_badip6(struct in6_addr *remoteip);
void expire_badip4(void);
void expire_badip6(void);
void expire_badip(void);

/* End of File */
