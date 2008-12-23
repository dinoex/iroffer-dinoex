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

/* include the headers */

static void create_statefile_hdr(statefile_hdr_t *hdr, statefile_tag_t tag, size_t len)
{
  hdr->tag = htonl((long)tag);
  hdr->length = htonl(len);
}

static unsigned char *start_statefile_hdr(unsigned char *next, statefile_tag_t tag, size_t len)
{
  statefile_hdr_t *hdr;

  hdr = (statefile_hdr_t *)next;
  hdr->tag = htonl((long)tag);
  hdr->length = htonl(len);
  next = (unsigned char*)(&hdr[1]);
  return next;
}

static void create_statefile_time(statefile_item_generic_time_t *g_time, statefile_tag_t tag, time_t val)
{
  create_statefile_hdr(&(g_time->hdr), tag, sizeof(statefile_item_generic_time_t));
  g_time->g_time = htonl(val);
}

static unsigned char *prepare_statefile_time(unsigned char *next, statefile_tag_t tag, time_t val)
{
  statefile_item_generic_time_t *g_time;

  g_time = (statefile_item_generic_time_t*)next;
  create_statefile_time(g_time, tag, val);
  next = (unsigned char*)(&g_time[1]);
  return next;
}

static int write_statefile_time(ir_boutput_t *bout, statefile_tag_t tag, time_t val)
{
  statefile_item_generic_time_t a_time;

  create_statefile_time(&a_time, tag, val);
  return write_statefile_item(bout, &a_time);
}

static void create_statefile_int(statefile_item_generic_int_t *g_int, statefile_tag_t tag, ir_int32 val)
{
  create_statefile_hdr(&(g_int->hdr), tag, sizeof(statefile_item_generic_int_t));
  g_int->g_int = htonl(val);
}

static unsigned char *prepare_statefile_int(unsigned char *next, statefile_tag_t tag, ir_int32 val)
{
  statefile_item_generic_int_t *g_int;

  g_int = (statefile_item_generic_int_t*)next;
  create_statefile_int(g_int, tag, val);
  next = (unsigned char*)(&g_int[1]);
  return next;
}

static void create_statefile_uint(statefile_item_generic_uint_t *g_uint, statefile_tag_t tag, ir_uint32 val)
{
  create_statefile_hdr(&(g_uint->hdr), tag, sizeof(statefile_item_generic_uint_t));
  g_uint->g_uint = htonl(val);
}

static unsigned char *prepare_statefile_uint(unsigned char *next, statefile_tag_t tag, ir_uint32 val)
{
  statefile_item_generic_uint_t *g_uint;

  g_uint = (statefile_item_generic_uint_t*)next;
  create_statefile_uint(g_uint, tag, val);
  next = (unsigned char*)(&g_uint[1]);
  return next;
}

static int write_statefile_int(ir_boutput_t *bout, statefile_tag_t tag, ir_int32 val)
{
  statefile_item_generic_int_t a_int;

  create_statefile_int(&a_int, tag, val);
  return write_statefile_item(bout, &a_int);
}

static int write_statefile_ullint(ir_boutput_t *bout, statefile_tag_t tag, unsigned long long val)
{
  statefile_item_generic_ullint_t a_ullint;

  create_statefile_hdr(&(a_ullint.hdr), tag, sizeof(statefile_item_generic_ullint_t));
  a_ullint.g_ullint.upper = htonl(val >> 32);
  a_ullint.g_ullint.lower = htonl(val & 0xFFFFFFFF);
  return write_statefile_item(bout, &a_ullint);
}

static void create_statefile_float(statefile_item_generic_float_t *g_float, statefile_tag_t tag, float val)
{
  create_statefile_hdr(&(g_float->hdr), tag, sizeof(statefile_item_generic_float_t));
  g_float->g_float = val;
}

static unsigned char *prepare_statefile_float(unsigned char *next, statefile_tag_t tag, float val)
{
  statefile_item_generic_float_t *g_float;

  g_float = (statefile_item_generic_float_t*)next;
  create_statefile_float(g_float, tag, val);
  next = (unsigned char*)(&g_float[1]);
  return next;
}

static int write_statefile_float(ir_boutput_t *bout, statefile_tag_t tag, float val)
{
  statefile_item_generic_float_t a_float;

  create_statefile_float(&a_float, tag, val);
  return write_statefile_item(bout, &a_float);
}

static unsigned char *prepare_statefile_string(unsigned char *next, statefile_tag_t tag, const char *str)
{
  statefile_hdr_t *hdr;
  size_t len;

  len = strlen(str) + 1;
  hdr = (statefile_hdr_t*)next;
  create_statefile_hdr(hdr, tag, sizeof(statefile_hdr_t) + len);
  next = (unsigned char*)(&hdr[1]);
  strcpy((char *)next, str);
  next += ceiling(len, 4);
  return next;
}

static void write_statefile_queue(ir_boutput_t *bout, irlist_t *list)
{
  unsigned char *data;
  unsigned char *next;
  ir_pqueue *pq;
  ir_int32 length;

  updatecontext();

  for (pq = irlist_get_head(list); pq; pq = irlist_get_next(pq)) {
    /*
     * need room to write:
     *  pack          int
     *  nick          string
     *  hostname      string
     *  queuedtime    time
     */
    length = sizeof(statefile_hdr_t) +
             sizeof(statefile_item_generic_int_t) +
             sizeof(statefile_hdr_t) + ceiling(strlen(pq->nick) + 1, 4) +
             sizeof(statefile_hdr_t) + ceiling(strlen(pq->hostname) + 1, 4) +
             sizeof(statefile_item_generic_time_t) +
             sizeof(statefile_item_generic_int_t);

    data = mycalloc(length);

    /* outter header */
    next = start_statefile_hdr(data, STATEFILE_TAG_QUEUE, length);

    /* pack */
    next = prepare_statefile_int(next,
           STATEFILE_TAG_QUEUE_PACK, number_of_pack(pq->xpack));

    /* nick */
    next = prepare_statefile_string(next,
           STATEFILE_TAG_QUEUE_NICK, pq->nick);

    /* hostname */
    next = prepare_statefile_string(next,
           STATEFILE_TAG_QUEUE_HOST, pq->hostname);

    /* queuedtime */
    next = prepare_statefile_time(next,
           STATEFILE_TAG_QUEUE_TIME, pq->queuedtime);

    /* net */
    next = prepare_statefile_int(next,
           STATEFILE_TAG_QUEUE_NET, pq->net);

    write_statefile_item(bout, data);
    mydelete(data);
  }
}

/* End of File */
