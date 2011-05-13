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

/* include the headers */


typedef struct
{
  int fd;
  int error;
  struct MD5Context *md5sum;
} ir_moutput_t;

static void ir_moutput_init(ir_moutput_t *bout, int fd)
{
  bout->fd = fd;
  bout->error = 0;
  bout->md5sum = mycalloc(sizeof(struct MD5Context));
  MD5Init(bout->md5sum);
}

static void write_statefile_raw(ir_moutput_t *bout, const void *buf, size_t nbytes)
{
  ssize_t saved;

  saved = write(bout->fd, buf, nbytes);
  if (saved != (ssize_t)nbytes) {
    outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Write To State File (%d != %d) %s",
             (int)saved, (int)nbytes, strerror(errno));
    ++(bout->error);
  }
}

static void write_statefile_md5(ir_moutput_t *bout, const void *buf, size_t nbytes)
{
  MD5Update(bout->md5sum, buf, (unsigned)nbytes);
  write_statefile_raw(bout, buf, nbytes);
}

static void ir_moutput_get_md5sum(ir_moutput_t *bout, MD5Digest digest)
{
  MD5Final(digest, bout->md5sum);
  mydelete(bout->md5sum);
}

static void write_statefile_direct(ir_moutput_t *bout, ir_uint32 val)
{
  ir_uint32 netval;

  netval = htonl(val);
  write_statefile_md5(bout, &netval, sizeof(netval));
}

static void write_statefile_item(ir_moutput_t *bout, void *item)
{
  statefile_hdr_t *hdr = (statefile_hdr_t*)item;
  ir_int32 length;

  length = ntohl(hdr->length);
  write_statefile_md5(bout, item, (size_t)length);
}

static void read_statefile_unknown_tag(statefile_hdr_t *hdr, const char *tag)
{
  outerror(OUTERROR_TYPE_WARN,
           "Ignoring Unknown %s Tag 0x%X (len = %d)", tag, hdr->tag, hdr->length);
  /* in case of 0 bytes len we loop forever */
  if ( hdr->length == 0 ) ++(hdr->length);
}

static void read_statefile_bad_tag(statefile_hdr_t *hdr, const char *tag)
{
  outerror(OUTERROR_TYPE_WARN,
           "Ignoring Bad %s Tag (len = %d)", tag, hdr->length);
}

static void read_statefile_incomplete_tag(const char *tag)
{
  outerror(OUTERROR_TYPE_WARN,
           "Ignoring Incomplete %s Tag", tag);
}

static void read_statefile_llint(statefile_hdr_t *hdr, const char *tag, ir_int64 *pval, const char *debug)
{
  statefile_item_generic_llint_t *g_llint;

  if (hdr->length != sizeof(statefile_item_generic_llint_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  g_llint = (statefile_item_generic_llint_t*)hdr;
  *pval = (((ir_uint64)ntohl(g_llint->g_llint.upper)) << 32) | ((ir_uint64)ntohl(g_llint->g_llint.lower));

  if (debug == NULL)
    return;

  if (gdata.debug > 0) {
    char *tempstr;
    tempstr = sizestr(0, *pval);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "  [%s %s]", debug, tempstr);
    mydelete(tempstr);
  }
}

static time_t fix_time64(statefile_uint64_t *val64)
{
  ir_uint32 netval = 0;

  netval = ntohl(val64->lower);
  if (netval == 0) {
    netval = ntohl(val64->upper);
  }
  return netval;
}

static void read_statefile_time(statefile_hdr_t *hdr, const char *tag, time_t *pval, const char *debug)
{
  statefile_item_generic_llint_t *g_llint;
  statefile_item_generic_uint_t *g_uint;

  if (hdr->length != sizeof(statefile_item_generic_uint_t)) {
    if (hdr->length != sizeof(statefile_item_generic_llint_t)) {
      read_statefile_bad_tag(hdr, tag);
      return;
    }
    /* read new 64 bit timestamps */
    g_llint = (statefile_item_generic_llint_t*)hdr;
    *pval = fix_time64(&(g_llint->g_llint));
  } else {
    g_uint = (statefile_item_generic_uint_t*)hdr;
    *pval = (time_t) ntohl(g_uint->g_uint);
  }
  if (debug == NULL)
    return;

  if (gdata.debug > 0) {
    char *tempstr;
    tempstr = mymalloc(maxtextlength);
    getdatestr(tempstr, *pval, maxtextlength);
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "  [%s %s]", debug, tempstr);
    mydelete(tempstr);
  }
}

static void read_statefile_md5info(statefile_hdr_t *hdr, const char *tag, xdcc *xd)
{
  if (hdr->length == sizeof(statefile_item_md5sum_info32_t)) {
    statefile_item_md5sum_info32_t *md5sum_info = (statefile_item_md5sum_info32_t*)hdr;
    ir_uint32 netval = 0;

    ++(xd->has_md5sum);
    xd->st_size = (off_t)((((ir_uint64)ntohl(md5sum_info->st_size.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_size.lower)));
    xd->st_dev  = (dev_t)((((ir_uint64)ntohl(md5sum_info->st_dev.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_dev.lower)));
    xd->st_ino  = (ino_t)((((ir_uint64)ntohl(md5sum_info->st_ino.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_ino.lower)));
    netval = (ir_int32)md5sum_info->mtime;
    xd->mtime   = ntohl(netval);
    memcpy(xd->md5sum, md5sum_info->md5sum, sizeof(MD5Digest));
    return;
  }
  if (hdr->length == sizeof(statefile_item_md5sum_info64_t)) {
    statefile_item_md5sum_info64_t *md5sum_info = (statefile_item_md5sum_info64_t*)hdr;

    ++(xd->has_md5sum);
    xd->st_size = (off_t)((((ir_uint64)ntohl(md5sum_info->st_size.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_size.lower)));
    xd->st_dev  = (dev_t)((((ir_uint64)ntohl(md5sum_info->st_dev.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_dev.lower)));
    xd->st_ino  = (ino_t)((((ir_uint64)ntohl(md5sum_info->st_ino.upper)) << 32) | ((ir_uint64)ntohl(md5sum_info->st_ino.lower)));
    xd->mtime   = fix_time64(&(md5sum_info->mtime));
    memcpy(xd->md5sum, md5sum_info->md5sum, sizeof(MD5Digest));
    return;
  }
  read_statefile_bad_tag(hdr, tag);
}

static void read_statefile_float(statefile_hdr_t *hdr, const char *tag, float *pval, const char *debug)
{
  statefile_item_generic_float_t *g_float;

  if (hdr->length != sizeof(statefile_item_generic_float_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  g_float = (statefile_item_generic_float_t*)hdr;
  *pval = g_float->g_float;

  if (gdata.debug > 0) {
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
            "  [%s %1.1fkB/s]", debug, *pval);
  }
}

static void read_statefile_float_set(statefile_hdr_t *hdr, const char *tag, float *pval)
{
  statefile_item_generic_float_t *g_float;

  if (hdr->length != sizeof(statefile_item_generic_float_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  g_float = (statefile_item_generic_float_t*)hdr;
  if (g_float->g_float)
    *pval = g_float->g_float;
}

static void read_statefile_int(statefile_hdr_t *hdr, const char *tag, unsigned int *pval)
{
  statefile_item_generic_uint_t *g_int;

  if (hdr->length != sizeof(statefile_item_generic_uint_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  g_int = (statefile_item_generic_uint_t*)hdr;
  *pval = ntohl(g_int->g_uint);
}

static void read_statefile_long(statefile_hdr_t *hdr, const char *tag, long *pval)
{
  statefile_item_generic_uint_t *g_uint;

  if (hdr->length != sizeof(statefile_item_generic_int_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  g_uint = (statefile_item_generic_uint_t*)hdr;
  *pval = ntohl(g_uint->g_uint);
}

static void read_statefile_string(statefile_hdr_t *hdr, const char *tag, char **pval)
{
  char *text;

  if (hdr->length <= sizeof(statefile_hdr_t)) {
    read_statefile_bad_tag(hdr, tag);
    return;
  }

  text = (char*)(&hdr[1]);
  text[hdr->length - sizeof(statefile_hdr_t) - 1] = '\0';
  if (text[0] == 0)
    return;

  *pval = mystrdup(text);
}

static void read_statefile_queue(statefile_hdr_t *hdr)
{
  ir_pqueue *pq;
  statefile_hdr_t *ihdr;
  unsigned int num;

  if (gdata.idlequeuesize > 0 )
    pq = irlist_add(&gdata.idlequeue, sizeof(ir_pqueue));
  else
    pq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));

  pq->restrictsend_bad = gdata.curtime;

  hdr->length -= sizeof(*hdr);
  ihdr = &hdr[1];

  while (hdr->length >= sizeof(*hdr)) {
    ihdr->tag = ntohl(ihdr->tag);
    ihdr->length = ntohl(ihdr->length);
    switch (ihdr->tag) {
    case STATEFILE_TAG_QUEUE_PACK:
      num = 0;
      read_statefile_int(ihdr, "Queue Pack Nr", &(num));
      if (num == XDCC_SEND_LIST) {
        pq->xpack = get_xdcc_pack(num);
        break;
      }
      if (num == 0 || num > irlist_size(&gdata.xdccs)) {
        outerror(OUTERROR_TYPE_WARN, "Ignoring Bad Queue Pack Nr (%u)", num);
        break;
      }
      pq->xpack = get_xdcc_pack(num);
      break;

    case STATEFILE_TAG_QUEUE_NICK:
      read_statefile_string(ihdr, "Queue Nick", &(pq->nick));
      break;

    case STATEFILE_TAG_QUEUE_HOST:
      read_statefile_string(ihdr, "Queue Host", &(pq->hostname));
      break;

    case STATEFILE_TAG_QUEUE_TIME:
      read_statefile_time(ihdr, "Queue Time", &(pq->queuedtime), NULL);
      break;

    case STATEFILE_TAG_QUEUE_NET:
      num = 0;
      read_statefile_int(ihdr, "Queue Net", &(num));
      if ((unsigned int)num > gdata.networks_online) {
        outerror(OUTERROR_TYPE_WARN, "Ignoring Bad Queue Net Nr (%u)", num);
        break;
      }
      pq->net = num;
      break;

    default:
      read_statefile_unknown_tag(ihdr, "Queue" );
      /* in case of 0 bytes len we loop forever */
      if ( ihdr->length == 0 ) ++(ihdr->length);
    }
    hdr->length -= ceiling(ihdr->length, 4);
    ihdr = (statefile_hdr_t*)(((char*)ihdr) + ceiling(ihdr->length, 4));
  }
  /* remove bad entry */
  if (pq->xpack == NULL) {
    mydelete(pq->nick);
    mydelete(pq->hostname);
    if (gdata.idlequeuesize > 0 )
      irlist_delete(&gdata.idlequeue, pq);
    else
      irlist_delete(&gdata.mainqueue, pq);
  }
}

static void create_statefile_hdr(statefile_hdr_t *hdr, statefile_tag_t tag, size_t len)
{
  hdr->tag = htonl((ir_uint32)tag);
  hdr->length = htonl((ir_uint32)len);
}

static unsigned char *start_statefile_hdr(unsigned char *next, statefile_tag_t tag, size_t len)
{
  statefile_hdr_t *hdr;

  hdr = (statefile_hdr_t *)next;
  create_statefile_hdr(hdr, tag, len);
  next = (unsigned char*)(&hdr[1]);
  return next;
}

static void create_statefile_int(statefile_item_generic_uint_t *g_int, statefile_tag_t tag, ir_uint32 val)
{
  create_statefile_hdr(&(g_int->hdr), tag, sizeof(statefile_item_generic_uint_t));
  g_int->g_uint = htonl(val);
}

static unsigned char *prepare_statefile_int(unsigned char *next, statefile_tag_t tag, ir_uint32 val)
{
  statefile_item_generic_uint_t *g_int;

  g_int = (statefile_item_generic_uint_t*)next;
  create_statefile_int(g_int, tag, val);
  next = (unsigned char*)(&g_int[1]);
  return next;
}

static void write_statefile_int(ir_moutput_t *bout, statefile_tag_t tag, ir_uint32 val)
{
  statefile_item_generic_uint_t a_int;

  create_statefile_int(&a_int, tag, val);
  write_statefile_item(bout, &a_int);
}

static void fix_uint64(statefile_uint64_t *val64, ir_int64 val)
{
  ir_uint32 netval;

  netval = (ir_uint32)(val >> 32);
  val64->upper = htonl(netval);
  netval = (ir_uint32)(val & 0xFFFFFFFF);
  val64->lower = htonl(netval);
}

static void write_statefile_llint(ir_moutput_t *bout, statefile_tag_t tag, ir_int64 val)
{
  statefile_item_generic_llint_t a_llint;

  create_statefile_hdr(&(a_llint.hdr), tag, sizeof(statefile_item_generic_llint_t));
  fix_uint64(&(a_llint.g_llint), val);
  write_statefile_item(bout, &a_llint);
}

#define prepare_statefile_time prepare_statefile_int

static void write_statefile_time(ir_moutput_t *bout, statefile_tag_t tag, time_t val)
{
  ir_uint32 netval;

  netval = (ir_uint32)val;
  write_statefile_int(bout, tag, netval);
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

static void write_statefile_float(ir_moutput_t *bout, statefile_tag_t tag, float val)
{
  statefile_item_generic_float_t a_float;

  create_statefile_float(&a_float, tag, val);
  write_statefile_item(bout, &a_float);
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

static void write_statefile_queue(ir_moutput_t *bout, irlist_t *list)
{
  unsigned char *data;
  unsigned char *next;
  ir_pqueue *pq;
  size_t length;

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

static void write_statefile_globals(ir_moutput_t *bout)
{
  write_statefile_time(bout, STATEFILE_TAG_TIMESTAMP, gdata.curtime);
  write_statefile_float(bout, STATEFILE_TAG_XFR_RECORD, gdata.record);
  write_statefile_float(bout, STATEFILE_TAG_SENT_RECORD, gdata.sentrecord);
  write_statefile_llint(bout, STATEFILE_TAG_TOTAL_SENT, gdata.totalsent );
  write_statefile_int(bout, STATEFILE_TAG_TOTAL_UPTIME, gdata.totaluptime);
  write_statefile_time(bout, STATEFILE_TAG_LAST_LOGROTATE, gdata.last_logrotate);
}

static void write_statefile_ignore(ir_moutput_t *bout)
{
  unsigned char *data;
  unsigned char *next;
  igninfo *ignore;
  size_t length;

  updatecontext();
  ignore = irlist_get_head(&gdata.ignorelist);

  for (ignore = irlist_get_head(&gdata.ignorelist);
       ignore;
       ignore = irlist_get_next(ignore)) {
    if ((ignore->flags & IGN_IGNORING) == 0)
      continue;
    /*
     * need room to write:
     *  flags         uint
     *  bucket        int
     *  lastcontact   time_t
     *  hostmask      string
     */
    length = sizeof(statefile_hdr_t) +
             sizeof(statefile_item_generic_uint_t) +
             sizeof(statefile_item_generic_int_t) +
             sizeof(statefile_item_generic_time_t) +
             sizeof(statefile_hdr_t) + ceiling(strlen(ignore->hostmask) + 1, 4);

    data = mycalloc(length);

    /* outter header */
    next = start_statefile_hdr(data, STATEFILE_TAG_IGNORE, length);

    next = prepare_statefile_int(next, STATEFILE_TAG_IGNORE_FLAGS, ignore->flags);
    next = prepare_statefile_int(next, STATEFILE_TAG_IGNORE_BUCKET, ignore->bucket);
    next = prepare_statefile_time(next, STATEFILE_TAG_IGNORE_LASTCONTACT, ignore->lastcontact);
    next = prepare_statefile_string(next, STATEFILE_TAG_IGNORE_HOSTMASK, ignore->hostmask);

    write_statefile_item(bout, data);
    mydelete(data);
  }
}

static void write_statefile_msglog(ir_moutput_t *bout)
{
  unsigned char *data;
  unsigned char *next;
  msglog_t *msglog;
  size_t length;

  updatecontext();
  msglog = irlist_get_head(&gdata.msglog);

  for (msglog = irlist_get_head(&gdata.msglog);
       msglog;
       msglog = irlist_get_next(msglog)) {
    /*
     * need room to write:
     *  when          time_t
     *  hostmask      string
     *  message       string
     */
    length = sizeof(statefile_hdr_t) +
             sizeof(statefile_item_generic_time_t) +
             sizeof(statefile_hdr_t) + ceiling(strlen(msglog->hostmask) + 1, 4) +
             sizeof(statefile_hdr_t) + ceiling(strlen(msglog->message) + 1, 4);

    data = mycalloc(length);

    /* outter header */
    next = start_statefile_hdr(data, STATEFILE_TAG_MSGLOG, length);

    next = prepare_statefile_time(next, STATEFILE_TAG_MSGLOG_WHEN, msglog->when);
    next = prepare_statefile_string(next, STATEFILE_TAG_MSGLOG_HOSTMASK, msglog->hostmask);
    next = prepare_statefile_string(next, STATEFILE_TAG_MSGLOG_MESSAGE, msglog->message);

    write_statefile_item(bout, data);
    mydelete(data);
  }
}


static void write_statefile_xdccs(ir_moutput_t *bout)
{
  unsigned char *data;
  unsigned char *next;
  xdcc *xd;
  statefile_item_md5sum_info32_t *md5sum_info;
  unsigned int has_desc = 1;
  unsigned int has_note = 1;
  unsigned int has_minspeed;
  unsigned int has_maxspeed;
  size_t length;

  updatecontext();
  for (xd = irlist_get_head(&gdata.xdccs);
       xd;
       xd = irlist_get_next(xd)) {
    if (!gdata.old_statefile) {
      has_desc = strcmp(xd->desc, getfilename(xd->file));
      has_note = (xd->note != NULL) && xd->note[0];
    }
    has_minspeed = xd->minspeed && (gdata.transferminspeed != xd->minspeed);
    has_maxspeed = xd->maxspeed && (gdata.transfermaxspeed != xd->maxspeed);
    /*
     * need room to write:
     *  file            string
     *  desc            string
     *  note            string
     *  gets            int
     *  minspeed        float
     *  maxspeed        float
     *  mds5            struct
     *  crc32           ulong
     *  group           string
     *  group_desc      string
     *  lock            string
     *  download limit  int
     *  download limit  int
     *  dlimit_desc     string
     *  trigger         string
     *  xtime           time
     *  color           int
     */
    length = sizeof(statefile_hdr_t) +
             sizeof(statefile_hdr_t) + ceiling(strlen(xd->file) + 1, 4) +
             sizeof(statefile_item_generic_int_t);

    if (has_desc) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->desc) + 1, 4);
    }
    if (has_note) {
      length += sizeof(statefile_hdr_t) + ceiling( xd->note ? strlen(xd->note) + 1 : 2, 4);
    }
    if (has_minspeed) {
      length += sizeof(statefile_item_generic_float_t);
    }
    if (has_maxspeed) {
      length += sizeof(statefile_item_generic_float_t);
    }
    if (xd->has_md5sum) {
      length += ceiling(sizeof(statefile_item_md5sum_info32_t), 4);
    }
    if (xd->has_crc32) {
      length += sizeof(statefile_item_generic_int_t);
    }
    if (xd->group != NULL) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->group) + 1, 4);
    }
    if (xd->group_desc != NULL) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->group_desc) + 1, 4);
    }
    if (xd->lock != NULL) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->lock) + 1, 4);
    }
    if (xd->dlimit_max != 0) {
      length += sizeof(statefile_item_generic_int_t);
      length += sizeof(statefile_item_generic_int_t);
    }
    if (xd->dlimit_desc != NULL) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->dlimit_desc) + 1, 4);
    }
    if (xd->trigger != NULL) {
      length += sizeof(statefile_hdr_t) + ceiling(strlen(xd->trigger) + 1, 4);
    }
    if (xd->xtime != 0) {
      length += sizeof(statefile_item_generic_time_t);
    }
    if (xd->color != 0) {
      length += sizeof(statefile_item_generic_int_t);
    }

    data = mycalloc(length);

    /* outter header */
    next = start_statefile_hdr(data, STATEFILE_TAG_XDCCS, length);

    next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_FILE, xd->file);
    if (has_desc) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_DESC, xd->desc);
    }
    if (has_note) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_NOTE,
                                      xd->note ? xd->note : "");
    }
    next = prepare_statefile_int(next, STATEFILE_TAG_XDCCS_GETS, xd->gets);
    if (has_minspeed) {
      next = prepare_statefile_float(next, STATEFILE_TAG_XDCCS_MINSPEED, xd->minspeed);
    }
    if (has_maxspeed) {
      next = prepare_statefile_float(next, STATEFILE_TAG_XDCCS_MAXSPEED, xd->maxspeed);
    }

    if (xd->has_md5sum) {
      md5sum_info = (statefile_item_md5sum_info32_t*)next;
      md5sum_info->hdr.tag = htonl((ir_uint32)STATEFILE_TAG_XDCCS_MD5SUM_INFO);
      md5sum_info->hdr.length = htonl((ir_uint32)sizeof(*md5sum_info));
      fix_uint64(&(md5sum_info->st_size), xd->st_size);
      fix_uint64(&(md5sum_info->st_dev), (ir_int64)xd->st_dev);
      fix_uint64(&(md5sum_info->st_ino), (ir_int64)xd->st_ino);
      md5sum_info->mtime         = htonl((ir_uint32)xd->mtime);
      memcpy(md5sum_info->md5sum, xd->md5sum, sizeof(MD5Digest));
      next = (unsigned char*)(&md5sum_info[1]);
    }

    if (xd->has_crc32) {
      next = prepare_statefile_int(next, STATEFILE_TAG_XDCCS_CRC32, xd->crc32);
    }

    if (xd->group != NULL) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_GROUP, xd->group);
      gdata.support_groups = 1;
    }
    if (xd->group_desc != NULL) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_GROUP_DESC, xd->group_desc);
    }
    if (xd->lock != NULL) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_LOCK, xd->lock);
    }

    if (xd->dlimit_max != 0) {
      next = prepare_statefile_int(next, STATEFILE_TAG_XDCCS_DLIMIT_MAX, xd->dlimit_max);
      next = prepare_statefile_int(next, STATEFILE_TAG_XDCCS_DLIMIT_USED, xd->dlimit_used);
    }
    if (xd->dlimit_desc != NULL) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_DLIMIT_DESC, xd->dlimit_desc);
    }

    if (xd->trigger != NULL) {
      next = prepare_statefile_string(next, STATEFILE_TAG_XDCCS_TRIGGER, xd->trigger);
    }

    if (xd->xtime != 0) {
      next = prepare_statefile_time(next, STATEFILE_TAG_XDCCS_XTIME, xd->xtime);
    }

    if (xd->color != 0) {
      next = prepare_statefile_int(next, STATEFILE_TAG_XDCCS_COLOR, xd->color);
    }

    write_statefile_item(bout, data);
    mydelete(data);
  }
}

static void write_statefile_dinoex(ir_moutput_t *bout)
{
  write_statefile_globals(bout);
  write_statefile_ignore(bout);
  write_statefile_msglog(bout);
  write_statefile_xdccs(bout);

  write_statefile_llint(bout, STATEFILE_TAG_TLIMIT_DAILY_USED, gdata.transferlimits[TRANSFERLIMIT_DAILY].used);
  write_statefile_time(bout, STATEFILE_TAG_TLIMIT_DAILY_ENDS, gdata.transferlimits[TRANSFERLIMIT_DAILY].ends);

  write_statefile_llint(bout, STATEFILE_TAG_TLIMIT_WEEKLY_USED, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used);
  write_statefile_time(bout, STATEFILE_TAG_TLIMIT_WEEKLY_ENDS, gdata.transferlimits[TRANSFERLIMIT_WEEKLY].ends);

  write_statefile_llint(bout, STATEFILE_TAG_TLIMIT_MONTHLY_USED, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used);
  write_statefile_time(bout, STATEFILE_TAG_TLIMIT_MONTHLY_ENDS, gdata.transferlimits[TRANSFERLIMIT_MONTHLY].ends);

  write_statefile_queue(bout, &gdata.mainqueue);
  write_statefile_queue(bout, &gdata.idlequeue);
}

/* End of File */
