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

typedef struct
{
  time_t lastcontact;
  time_t connecttime;
  unsigned long count;
  ir_uint32 remoteip;
  ir_uint32 dummy;
} badip4;

typedef struct
{
  time_t lastcontact;
  time_t connecttime;
  struct in6_addr remoteip;
  unsigned long count;
} badip6;

unsigned int is_in_badip(ir_sockaddr_union_t *sa);
void count_badip(ir_sockaddr_union_t *sa);
void expire_badip(void);

/* End of File */
