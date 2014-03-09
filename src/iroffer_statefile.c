/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the LICENSE file.
 * 
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.org/
 * 
 * @(#) iroffer_statefile.c 1.23@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_statefile.c|20050313183435|62320
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_jobs.h"
#include "dinoex_misc.h"


/*
 *      ******  FILE FORMAT  ******
 *
 * Word size:     4 Bytes
 * Byte order:    Network Order
 *
 * == OVERALL ==
 * Word 0:        Magic
 * Word 1:        Version
 * Word 2..n-4:   TLV Data
 * Word n-3..n:   md5sum
 *
 * == TLV Format ==
 * Word 0:        Tag
 * Word 1:        Length in Bytes (Tag+Length+Value, but not Padding)
 * Word 2..n-1:   Value
 * Word n:        Value + Zero Padding
 *
 * Some tags are compound, some are not.  Compound tags contain TLV
 * data of their own.
 *
 */

typedef enum
{
  STATEFILE_TAG_IROFFER_VERSION      =  1 << 8,
  STATEFILE_TAG_TIMESTAMP,
  
  STATEFILE_TAG_XFR_RECORD           =  2 << 8,
  STATEFILE_TAG_SENT_RECORD,
  STATEFILE_TAG_TOTAL_SENT,
  STATEFILE_TAG_TOTAL_UPTIME,
  STATEFILE_TAG_LAST_LOGROTATE,
  
  STATEFILE_TAG_IGNORE               = 10 << 8, /* compound */
  STATEFILE_TAG_IGNORE_FLAGS,
  STATEFILE_TAG_IGNORE_BUCKET,
  STATEFILE_TAG_IGNORE_LASTCONTACT,
  STATEFILE_TAG_IGNORE_HOSTMASK,
  
  STATEFILE_TAG_MSGLOG               = 11 << 8, /* compound */
  STATEFILE_TAG_MSGLOG_WHEN,
  STATEFILE_TAG_MSGLOG_HOSTMASK,
  STATEFILE_TAG_MSGLOG_MESSAGE,
  
  STATEFILE_TAG_XDCCS                = 12 << 8, /* compound */
  STATEFILE_TAG_XDCCS_FILE,
  STATEFILE_TAG_XDCCS_DESC,
  STATEFILE_TAG_XDCCS_NOTE,
  STATEFILE_TAG_XDCCS_GETS,
  STATEFILE_TAG_XDCCS_MINSPEED,
  STATEFILE_TAG_XDCCS_MAXSPEED,
  STATEFILE_TAG_XDCCS_MD5SUM_INFO,
  STATEFILE_TAG_XDCCS_GROUP,
  STATEFILE_TAG_XDCCS_GROUP_DESC,
  STATEFILE_TAG_XDCCS_LOCK,
  STATEFILE_TAG_XDCCS_DLIMIT_MAX,
  STATEFILE_TAG_XDCCS_DLIMIT_USED,
  STATEFILE_TAG_XDCCS_DLIMIT_DESC,
  STATEFILE_TAG_XDCCS_CRC32,
  STATEFILE_TAG_XDCCS_TRIGGER,
  STATEFILE_TAG_XDCCS_XTIME,
  STATEFILE_TAG_XDCCS_COLOR,
  
  STATEFILE_TAG_TLIMIT_DAILY_USED    = 13 << 8,
  STATEFILE_TAG_TLIMIT_DAILY_ENDS,
  STATEFILE_TAG_TLIMIT_WEEKLY_USED,
  STATEFILE_TAG_TLIMIT_WEEKLY_ENDS,
  STATEFILE_TAG_TLIMIT_MONTHLY_USED,
  STATEFILE_TAG_TLIMIT_MONTHLY_ENDS,
  
  STATEFILE_TAG_QUEUE                = 14 << 8,
  STATEFILE_TAG_QUEUE_PACK,
  STATEFILE_TAG_QUEUE_NICK,
  STATEFILE_TAG_QUEUE_HOST,
  STATEFILE_TAG_QUEUE_TIME,
  STATEFILE_TAG_QUEUE_NET,
} statefile_tag_t;

typedef struct
{
  statefile_tag_t tag;
  ir_uint32 length; /* includes header */
} statefile_hdr_t;

#define STATEFILE_MAGIC (unsigned)(('I' << 24) | ('R' << 16) | ('F' << 8) | 'R')
#define STATEFILE_OLD_VERSION 1U
#define STATEFILE_VERSION 2U

typedef struct
{
  ir_uint32 upper;
  ir_uint32 lower;
} statefile_uint64_t;

typedef struct
{
  statefile_hdr_t hdr;
  ir_int32 g_int;
} statefile_item_generic_int_t;

typedef struct
{
  statefile_hdr_t hdr;
  ir_uint32 g_uint;
} statefile_item_generic_uint_t, statefile_item_generic_time_t;

typedef struct
{
  statefile_hdr_t hdr;
  statefile_uint64_t g_llint;
} statefile_item_generic_llint_t;

typedef struct
{
  statefile_hdr_t hdr;
  float g_float;
} statefile_item_generic_float_t;

typedef struct
{
  statefile_hdr_t hdr;
  statefile_uint64_t st_size;
  statefile_uint64_t st_dev;
  statefile_uint64_t st_ino;
  ir_int32 mtime;
  MD5Digest md5sum;
} statefile_item_md5sum_info32_t;

typedef struct
{
  statefile_hdr_t hdr;
  statefile_uint64_t st_size;
  statefile_uint64_t st_dev;
  statefile_uint64_t st_ino;
  statefile_uint64_t mtime;
  MD5Digest md5sum;
} statefile_item_md5sum_info64_t;

#include "dinoex_statefile.c"

void write_statefile(void)
{
#ifdef DEBUG
  ir_uint64 ms1;
  ir_uint64 ms2;
#endif /* DEBUG */
  char *statefile_tmp, *statefile_bkup;
  int fd;
  int callval;
  statefile_hdr_t *hdr;
  size_t length;
  ir_moutput_t bout;
  
  updatecontext();
  
  if (gdata.statefile == NULL)
    {
      return;
    }
  
#ifdef DEBUG
  ms1 = get_time_in_ms();
#endif /* DEBUG */
  statefile_tmp = mystrsuffix(gdata.statefile, ".tmp");
  statefile_bkup = mystrsuffix(gdata.statefile, "~");
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Saving State File... ");
    }
  
  fd = open(statefile_tmp,
            O_WRONLY | O_CREAT | O_TRUNC | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS);
  
  if (fd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Create State File '%s': %s",
               statefile_tmp, strerror(errno));
      goto error_out;
    }
  
  ir_moutput_init(&bout, fd);
  
  /*** write ***/
  write_statefile_direct(&bout, STATEFILE_MAGIC);
  write_statefile_direct(&bout, gdata.old_statefile ? STATEFILE_OLD_VERSION : STATEFILE_VERSION);
  
  updatecontext();
  {
    unsigned char *data;
    unsigned char *next;

    length = sizeof(statefile_hdr_t) + maxtextlength;
    
    data = mycalloc(length);
    
    /* header */
    hdr = (statefile_hdr_t*)data;
    next = (unsigned char*)(&hdr[1]);
    
    length = add_snprintf((char *)next, maxtextlength,
                          "iroffer-dinoex " VERSIONLONG ", %s", gdata.osstring);
    
        create_statefile_hdr(hdr, STATEFILE_TAG_IROFFER_VERSION, sizeof(statefile_hdr_t) + ceiling(length+1, 4));
        write_statefile_item(&bout, data);
    
    mydelete(data);
  }
   
  write_statefile_dinoex(&bout);
   
  updatecontext();
  {
    MD5Digest digest;
    
    bzero(digest, sizeof(digest));
    ir_moutput_get_md5sum(&bout, digest);
    write_statefile_raw(&bout, &digest, sizeof(digest));
  }
  
  /*** end write ***/
  updatecontext();
  
  close(fd);
  /* abort if statefile was not written in full */
  if (bout.error > 0)
    {
      goto error_out;
    }
  
  /* remove old bkup */
  callval = unlink(statefile_bkup);
  if ((callval < 0) && (errno != ENOENT))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Remove Old State File '%s': %s",
               statefile_bkup, strerror(errno));
      /* ignore, continue */
    }
  
  /* backup old -> bkup */
  callval = link(gdata.statefile, statefile_bkup);
  if ((callval < 0) && (errno != ENOENT))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Backup Old State File '%s' -> '%s': %s",
               gdata.statefile, statefile_bkup, strerror(errno));
      /* ignore, continue */
    }
  
  /* rename new -> current */
  callval = rename(statefile_tmp, gdata.statefile);
  if (callval < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Save New State File '%s': %s",
               gdata.statefile, strerror(errno));
      /* ignore, continue */
    }
  
  if (gdata.debug > 0)
    {
#ifdef DEBUG
      ms2 = get_time_in_ms();
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Done"
              " running: %ld ms", (long)(ms2 - ms1));
#else
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Done");
#endif /* DEBUG */
    }
  
 error_out:
  
  mydelete(statefile_tmp);
  mydelete(statefile_bkup);
  return;
}


static statefile_hdr_t* read_statefile_item(ir_uint32 **buffer, ir_uint32 *buffer_len)
{
  statefile_hdr_t *all;
  
  if (*buffer_len < sizeof(statefile_hdr_t))
    {
      return NULL;
    }
  
  all = (statefile_hdr_t*)*buffer;
  
  all->tag = ntohl(all->tag);
  all->length = ntohl(all->length);
  
  if (*buffer_len < all->length)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Bad Header From State File (%u < %u)",
               *buffer_len, all->length);
      return NULL;
    }
  
  *buffer += ceiling(all->length, 4) / sizeof(ir_uint32);
  *buffer_len -= ceiling(all->length, 4);
  
  return all;
}


unsigned int read_statefile(void)
{
  int fd;
  ir_uint32 *buffer, *buffer_begin;
  ir_uint32 buffer_len;
  struct MD5Context md5sum;
  MD5Digest digest;
  struct stat st;
  statefile_hdr_t *hdr;
  ir_uint32 calluval;
  unsigned int save = 0;
  time_t timestamp = 0;
  
  updatecontext();
  
  if (gdata.statefile == NULL)
    {
      return save;
    }
  
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Loading State File... ");
  
  fd = open(gdata.statefile,
            O_RDONLY | O_CREAT | ADDED_OPEN_FLAGS,
            CREAT_PERMISSIONS );
  
  if (fd < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Access State File '%s': %s",
               gdata.statefile, strerror(errno));
      return ++save;
    }
  
  if ((fstat(fd, &st) < 0) || (st.st_size < ((off_t)(sizeof(ir_uint32) * 2) + (off_t)(sizeof(MD5Digest)))))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "State File: Too small, Skipping");
      close(fd);
      return ++save;
    }
  
  buffer_len = st.st_size;
  buffer_begin = buffer = mycalloc(buffer_len);
  
  calluval = read(fd, buffer, buffer_len);
  close(fd);
  if (calluval != buffer_len)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD, "Cant Read State File (%u != %u) %s",
               calluval, buffer_len, strerror(errno));
      goto error_out;
    }
  
  /* verify md5sum */
  buffer_len -= sizeof(MD5Digest);
  
  MD5Init(&md5sum);
  MD5Update(&md5sum, (md5byte*)buffer, buffer_len);
  MD5Final(digest, &md5sum);
  
  if (memcmp(digest, buffer+(buffer_len/sizeof(ir_uint32)), sizeof(MD5Digest)))
    {
      outerror(OUTERROR_TYPE_CRASH,
               "\"%s\" Appears corrupt or is not an iroffer state file",
               gdata.statefile);
      goto error_out;
    }
  
  /* read */
  
  if (ntohl(*buffer) != STATEFILE_MAGIC)
    {
      outerror(OUTERROR_TYPE_CRASH,
               "\"%s\" Does not appear to be an iroffer state file",
               gdata.statefile);
      goto error_out;
    }
  buffer++;
  buffer_len -= sizeof(ir_uint32);
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
              "  [Version %u State File]", ntohl(*buffer));
    }
  buffer++;
  buffer_len -= sizeof(ir_uint32);
  
  while ((hdr = read_statefile_item(&buffer, &buffer_len)))
    {
      switch (hdr->tag)
        {
        case STATEFILE_TAG_TIMESTAMP:
          read_statefile_time(hdr, "Timestamp", &timestamp, "Written on");
          break;
          
        case STATEFILE_TAG_XFR_RECORD:
          read_statefile_float(hdr, "xfr Record", &(gdata.record), "Record");
          break;
          
        case STATEFILE_TAG_SENT_RECORD:
          read_statefile_float(hdr, "sent Record", &(gdata.sentrecord), "Bandwidth Record");
          break;
          
        case STATEFILE_TAG_TOTAL_SENT:
          read_statefile_llint(hdr, "Total Transferred", &(gdata.totalsent), "Total Transferred");
          break;
          
        case STATEFILE_TAG_TOTAL_UPTIME:
          read_statefile_long(hdr, "Total Runtime", &(gdata.totaluptime));
              if (gdata.debug > 0)
                {
                  char *tempstr;
                  tempstr = mymalloc(maxtextlength);
                  getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlength);
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "  [Total Runtime %s]", tempstr);
                  mydelete(tempstr);
                }
          break;
          
        case STATEFILE_TAG_LAST_LOGROTATE:
          read_statefile_time(hdr, "Last Log Rotate", &(gdata.last_logrotate), "Last Log Rotate");
          break;
          
        case STATEFILE_TAG_IROFFER_VERSION:
          if (hdr->length > sizeof(statefile_hdr_t))
            {
              char *iroffer_version = (char*)(&hdr[1]);
              char *iroffer_now;
              iroffer_version[hdr->length-sizeof(statefile_hdr_t)-1] = '\0';
              
              if (gdata.debug > 0)
                {
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "  [Written by %s]", iroffer_version);
                }
              iroffer_now = mycalloc(maxtextlength);
              snprintf(iroffer_now, maxtextlength, "iroffer-dinoex " VERSIONLONG ", %s", gdata.osstring);
              if (strcmp(iroffer_version, iroffer_now) != 0)
                {
                  ++save;
                  backup_statefile();
                }
              mydelete(iroffer_now);
            }
          else
            {
              read_statefile_bad_tag(hdr, "Iroffer Version");
            }
          break;
          
        case STATEFILE_TAG_IGNORE:
          {
            igninfo *ignore;
            statefile_hdr_t *ihdr;

            ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
            
            hdr->length -= sizeof(*hdr);
            ihdr = &hdr[1];
            
            while (hdr->length >= sizeof(*hdr))
              {
                ihdr->tag = ntohl(ihdr->tag);
                ihdr->length = ntohl(ihdr->length);
                switch (ihdr->tag)
                  {
                  case STATEFILE_TAG_IGNORE_FLAGS:
                    read_statefile_int(ihdr, "Ignore Flags", &(ignore->flags));
                    break;
                    
                  case STATEFILE_TAG_IGNORE_BUCKET:
                    read_statefile_long(ihdr, "Ignore Bucket", &(ignore->bucket));
                    break;
                    
                  case STATEFILE_TAG_IGNORE_LASTCONTACT:
                    read_statefile_time(ihdr, "Ignore Lastcontact", &(ignore->lastcontact), NULL);
                    break;
                    
                  case STATEFILE_TAG_IGNORE_HOSTMASK:
                    read_statefile_string(ihdr, "Ignore Hostmask", &(ignore->hostmask));
                    break;
                    
                  default:
                    read_statefile_unknown_tag(ihdr, "Ignore" );
                  }
                hdr->length -= ceiling(ihdr->length, 4);
                ihdr = (statefile_hdr_t*)(((char*)ihdr) + ceiling(ihdr->length, 4));
              }
            
            if ((!ignore->lastcontact) || (!ignore->hostmask))
              {
                read_statefile_incomplete_tag("Ignore" );
                mydelete(ignore->hostmask);
                irlist_delete(&gdata.ignorelist, ignore);
              }
            else
              {
                ignore->bucket -= (gdata.curtime - timestamp)/gdata.autoignore_threshold;
              }
          }
          
          break;
          
        case STATEFILE_TAG_MSGLOG:
          {
            msglog_t *msglog;
            statefile_hdr_t *ihdr;

            msglog = irlist_add(&gdata.msglog, sizeof(msglog_t));
            
            hdr->length -= sizeof(*hdr);
            ihdr = &hdr[1];
            
            while (hdr->length >= sizeof(*hdr))
              {
                ihdr->tag = ntohl(ihdr->tag);
                ihdr->length = ntohl(ihdr->length);
                switch (ihdr->tag)
                  {
                  case STATEFILE_TAG_MSGLOG_WHEN:
                    read_statefile_time(ihdr, "Msglog When", &(msglog->when), NULL);
                    break;
                    
                  case STATEFILE_TAG_MSGLOG_HOSTMASK:
                    read_statefile_string(ihdr, "Msglog Hostmask", &(msglog->hostmask));
                    break;
                    
                  case STATEFILE_TAG_MSGLOG_MESSAGE:
                    read_statefile_string(ihdr, "Msglog Message", &(msglog->message));
                    break;
                    
                  default:
                    read_statefile_unknown_tag(ihdr, "Msglog" );
                  }
                hdr->length -= ceiling(ihdr->length, 4);
                ihdr = (statefile_hdr_t*)(((char*)ihdr) + ceiling(ihdr->length, 4));
              }
            
            if ((!msglog->when) || (!msglog->hostmask) || (!msglog->message))
              {
                read_statefile_incomplete_tag("Msglog" );
                mydelete(msglog->hostmask);
                mydelete(msglog->message);
                irlist_delete(&gdata.msglog, msglog);
              }
          }
          
          break;
          
        case STATEFILE_TAG_XDCCS:
          {
            xdcc *xd;
            statefile_hdr_t *ihdr;
            
            xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
            xd->file_fd = FD_UNUSED;
            
            xd->minspeed = gdata.transferminspeed;
            xd->maxspeed = gdata.transfermaxspeed;
            
            hdr->length -= sizeof(*hdr);
            ihdr = &hdr[1];
            
            while (hdr->length >= sizeof(*hdr))
              {
                ihdr->tag = ntohl(ihdr->tag);
                ihdr->length = ntohl(ihdr->length);
                switch (ihdr->tag)
                  {
                  case STATEFILE_TAG_XDCCS_FILE:
                    read_statefile_string(ihdr, "XDCC File", &(xd->file));
                    xd->desc = mystrdup(getfilename(xd->file));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_DESC:
                    mydelete(xd->desc);
                    read_statefile_string(ihdr, "XDCC Desc", &(xd->desc));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_NOTE:
                    read_statefile_string(ihdr, "XDCC Note", &(xd->note));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_GETS:
                    read_statefile_int(ihdr, "XDCC Gets", &(xd->gets));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_MINSPEED:
                    read_statefile_float_set(ihdr, "XDCC Minspeed", &(xd->minspeed));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_MAXSPEED:
                    read_statefile_float_set(ihdr, "XDCC Maxspeed", &(xd->maxspeed));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_MD5SUM_INFO:
                    read_statefile_md5info(ihdr, "XDCC md5sum", xd);
                    break;
                    
                  case STATEFILE_TAG_XDCCS_CRC32:
                    read_statefile_int(ihdr, "XDCC CRC32", &(xd->crc32));
                    xd->has_crc32 = 1;
                    break;
                    
                  case STATEFILE_TAG_XDCCS_GROUP:
                    read_statefile_string(ihdr, "XDCC Group", &(xd->group));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_GROUP_DESC:
                    read_statefile_string(ihdr, "XDCC Group Desc", &(xd->group_desc));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_LOCK:
                    read_statefile_string(ihdr, "XDCC Lock", &(xd->lock));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_DLIMIT_MAX:
                    read_statefile_int(ihdr, "XDCC Limit Max", &(xd->dlimit_max));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_DLIMIT_USED:
                    read_statefile_int(ihdr, "XDCC Limit Used", &(xd->dlimit_used));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_DLIMIT_DESC:
                    read_statefile_string(ihdr, "XDCC Limit Desc", &(xd->dlimit_desc));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_TRIGGER:
                    read_statefile_string(ihdr, "XDCC Trigger", &(xd->trigger));
                    break;
                    
                  case STATEFILE_TAG_XDCCS_XTIME:
                    read_statefile_time(ihdr, "XDCC Time", &(xd->xtime), NULL);
                    break;

                  case STATEFILE_TAG_XDCCS_COLOR:
                    read_statefile_int(ihdr, "XDCC Color", &(xd->color));
                    break;
                    
                  default:
                    read_statefile_unknown_tag(ihdr, "XDCC" );
                  }
                hdr->length -= ceiling(ihdr->length, 4);
                ihdr = (statefile_hdr_t*)(((char*)ihdr) + ceiling(ihdr->length, 4));
              }
            
            if ((!xd->file) || (!xd->desc))
              {
                read_statefile_incomplete_tag("XDCC" );
                mydelete(xd->file);
                mydelete(xd->desc);
                mydelete(xd->note);
                mydelete(xd->group);
                mydelete(xd->group_desc);
                mydelete(xd->lock);
                mydelete(xd->trigger);
                irlist_delete(&gdata.xdccs, xd);
              }
            else
              {
                if (stat(xd->file, &st) < 0)
                  {
                    outerror(OUTERROR_TYPE_WARN, "Pack %u: Cant Access Offered File '%s': %s",
                             number_of_pack(xd),
                             xd->file, strerror(errno));
                    memset(&st, 0, sizeof(st));
                    break;
                  }
                if (!xd->has_md5sum ||
                    (xd->st_ino   != st.st_ino) ||
                    (xd->mtime    != st.st_mtime) ||
                    (xd->st_size  != st.st_size))
                  {
                    xd->st_size     = st.st_size;
                    xd->st_dev      = st.st_dev;
                    xd->st_ino      = st.st_ino;
                    xd->mtime       = st.st_mtime;
                    xd->has_md5sum  = 0;
                    memset(xd->md5sum, 0, sizeof(MD5Digest));
                  }
                if (xd->st_dev != st.st_dev)
                  {
                    /* only mountpoint has changed */
                    xd->st_dev = st.st_dev;
                  }
                
                if (xd->st_size == 0)
                  {
                    outerror(OUTERROR_TYPE_WARN, "Pack %u: The file \"%s\" has size of 0 byte!",
                             number_of_pack(xd),
                             xd->file);
                  }
                
                if (xd->st_size > gdata.max_file_size)
                  {
                    outerror(OUTERROR_TYPE_CRASH, "Pack %u: The file \"%s\" is too large!",
                             number_of_pack(xd),
                             xd->file);
                  }
              }
          }
          
          break;
          
        case STATEFILE_TAG_TLIMIT_DAILY_USED:
          read_statefile_llint(hdr, "Daily Transfer Limit Used", &(gdata.transferlimits[TRANSFERLIMIT_DAILY].used), "Daily Transfer Limit Used");
          break;
          
        case STATEFILE_TAG_TLIMIT_DAILY_ENDS:
          read_statefile_time(hdr, "Daily Transfer Limit Ends", &(gdata.transferlimits[TRANSFERLIMIT_DAILY].ends), "Daily Transfer Limit Ends");
          break;
          
        case STATEFILE_TAG_TLIMIT_WEEKLY_USED:
          read_statefile_llint(hdr, "Weekly Transfer Limit Used", &(gdata.transferlimits[TRANSFERLIMIT_WEEKLY].used), "Weekly Transfer Limit Used");
          break;
          
        case STATEFILE_TAG_TLIMIT_WEEKLY_ENDS:
          read_statefile_time(hdr, "Weekly Transfer Limit Ends", &(gdata.transferlimits[TRANSFERLIMIT_WEEKLY].ends), "Weekly Transfer Limit Ends");
          break;
          
        case STATEFILE_TAG_TLIMIT_MONTHLY_USED:
          read_statefile_llint(hdr, "Monthly Transfer Limit Used", &(gdata.transferlimits[TRANSFERLIMIT_MONTHLY].used), "Monthly Transfer Limit Used");
          break;
          
        case STATEFILE_TAG_TLIMIT_MONTHLY_ENDS:
          read_statefile_time(hdr, "Monthly Transfer Limit Ends", &(gdata.transferlimits[TRANSFERLIMIT_MONTHLY].ends), "Monthly Transfer Limit Ends");
          break;
          
        case STATEFILE_TAG_QUEUE:
          read_statefile_queue(hdr);
          break;
          
        default:
          read_statefile_unknown_tag(hdr, "Main" );
          break;
        }
    }
  
  if (buffer_len)
    {
      outerror(OUTERROR_TYPE_WARN, "Extra data at end of state file!? %u left",
               buffer_len);
    }
  
  /* end read */
  
  if ((gdata.debug > 0) || irlist_size(&gdata.ignorelist))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Found %u %s]",
              irlist_size(&gdata.ignorelist),
              (irlist_size(&gdata.ignorelist) == 1) ? "ignore" : "ignores");
    }
  
  if ((gdata.debug > 0) || irlist_size(&gdata.msglog))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Found %u %s]",
              irlist_size(&gdata.msglog),
              (irlist_size(&gdata.msglog) == 1) ? "message" : "messages");
    }
  
  if ((gdata.debug > 0) || irlist_size(&gdata.xdccs))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Found %u %s]",
              irlist_size(&gdata.xdccs),
              (irlist_size(&gdata.xdccs) == 1) ? "pack" : "packs");
    }
  
  if ((gdata.debug > 0) || irlist_size(&gdata.mainqueue))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Found %u %s]",
              irlist_size(&gdata.mainqueue),
              (irlist_size(&gdata.mainqueue) == 1) ? "queue" : "queues");
    }
  if ((gdata.debug > 0) || irlist_size(&gdata.idlequeue))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Found %u %s]",
              irlist_size(&gdata.idlequeue),
              (irlist_size(&gdata.idlequeue) == 1) ? "queue" : "queues");
    }
  
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "  [Done]");
  
 error_out:
  
  mydelete(buffer_begin);
  
  return save;
}


/* End of File */
