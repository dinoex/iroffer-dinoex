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
 * @(#) iroffer_transfer.c 1.105@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_transfer.c|20050116225153|54665
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_kqueue.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"
#include "dinoex_transfer.h"
#include "dinoex_misc.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

void t_initvalues (transfer * const t) {

   updatecontext();

      t->tr_status = TRANSFER_STATUS_UNUSED;
      t->con.listensocket = FD_UNUSED;
      t->con.clientsocket = FD_UNUSED;
      t->con.lastcontact = gdata.curtime;
      t->id = 200;
      t->overlimit = 0;
   }

void t_setuplisten (transfer * const t)
{
  int rc;
  
  updatecontext();

  rc = irc_open_listen(&(t->con));
  if (rc != 0)
    {
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
    }
  
  t->tr_status = TRANSFER_STATUS_LISTENING;
}

void t_establishcon (transfer * const t)
{
   SIGNEDSOCK int addrlen;
   
   updatecontext();
   
   addrlen = (t->con.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
   
   if ((t->con.clientsocket = accept(t->con.listensocket, &t->con.local.sa, &addrlen)) < 0) {
      int errno2 = errno;
      outerror(OUTERROR_TYPE_WARN, "Accept Error, Aborting: %s", strerror(errno));
      t_closeconn(t, "Connection Error, Try Again", errno2);
      return;
      }
   
   ir_listen_port_connected(t->con.localport);
   
   event_close(t->con.listensocket);
   t->con.listensocket = FD_UNUSED;

   t_setup_send(t);
}

void t_setup_send(transfer * const t)
{
   char *msg;
   SIGNEDSOCK int addrlen;
   
   updatecontext();
   
   t->tr_status = TRANSFER_STATUS_SENDING;
   if (gdata.debug > 0) {
      ioutput(OUT_S, COLOR_YELLOW, "clientsock = %d", t->con.clientsocket);
      }
      
   if (t->xpack->file_fd == FD_UNUSED)
     {
       t->xpack->file_fd = open(t->xpack->file, O_RDONLY | ADDED_OPEN_FLAGS);
       if (t->xpack->file_fd < 0) {
         int errno2 = errno;
         t->xpack->file_fd = FD_UNUSED;
         outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Access Offered File '%s': %s",t->xpack->file,strerror(errno));
         t_closeconn(t, "File Error, Report the Problem to the Owner", errno2);
         return;
       }
       t->xpack->file_fd_location = 0;
       if (gdata.mirc_dcc64)
         if (t->xpack->st_size > 0xFFFFFFFFL)
           t->mirc_dcc64 = 1;
     }
   
   t->bytessent = t->startresume;
   
   ir_setsockopt(t->con.clientsocket);
   
   t->con.lastcontact = gdata.curtime;
   t->con.connecttime = gdata.curtime;
   t->connecttimems = gdata.curtimems;
   t->lastspeed = t->xpack->minspeed;
   t->lastspeedamt = t->startresume;
   addrlen = (t->con.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
   
   if ((getpeername(t->con.clientsocket, &(t->con.remote.sa), &(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN, "Couldn't get Remote IP: %s", strerror(errno));
   
   if (t_check_ip_access(t))
      return;
   
   if (t->con.family == AF_INET) {
      t->remoteip = ntohl(t->con.remote.sin.sin_addr.s_addr);
      }
   
   if ((getsockname(t->con.clientsocket, &(t->con.local.sa), &(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN, "Couldn't get Local IP: %s", strerror(errno));
   
   msg = mymalloc(maxtextlength);
   my_getnameinfo(msg, maxtextlength -1, &(t->con.remote.sa));
   mydelete(t->con.remoteaddr);
   t->con.remoteaddr = mystrdup(msg);
   my_getnameinfo(msg, maxtextlength -1, &(t->con.local.sa));
   t->con.localaddr = mystrdup(msg);
   mydelete(msg);

   ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "XDCC [%02i:%s on %s]: Connection established (%s -> %s)",
            t->id, t->nick, gdata.networks[ t->net ].name,
            t->con.remoteaddr, t->con.localaddr);
}

void t_transfersome (transfer * const t)
{
  unsigned int j;
  int ii;
  ssize_t howmuch, howmuch2;
  size_t attempt;
  unsigned char *dataptr;
  off_t offset;
#if defined(HAVE_FREEBSD_SENDFILE)
  struct sf_hdtr sendfile_header = {0, 0, 0, 0};
  int jj;
#endif
  
  updatecontext();
  
  /* max bandwidth start.... */
  
  if (!t->nomax && (t->maxspeed > 0))
    {
      if (t->tx_bucket < TXSIZE)
        {
          t->overlimit = 1;
          return; /* over transfer limit */
        }
    }
  else
    {
      t->tx_bucket = TXSIZE * MAXTXPERLOOP;
    }
  
  j = gdata.xdccsent[(gdata.curtime)%XDCC_SENT_SIZE] 
    + gdata.xdccsent[(gdata.curtime-1)%XDCC_SENT_SIZE]
    + gdata.xdccsent[(gdata.curtime-2)%XDCC_SENT_SIZE]
    + gdata.xdccsent[(gdata.curtime-3)%XDCC_SENT_SIZE];
  
  if ( gdata.maxb && (j >= gdata.maxb*1024))
    {
      if (t->unlimited == 0)
        return; /* over overall limit */
    }
  
  t->overlimit = 0;
  
  /* max bandwidth end.... */
  
  do
    {
      attempt = min2(t->tx_bucket - (t->tx_bucket % TXSIZE), BUFFERSIZE);
      
      switch (gdata.transfermethod)
        {
#if defined(HAVE_LINUX_SENDFILE)
        case TRANSFERMETHOD_LINUX_SENDFILE:
          
          offset = t->bytessent;
          
          howmuch = sendfile(t->con.clientsocket,
                             t->xpack->file_fd,
                             &offset,
                             attempt);
          
          if (howmuch < 0 && ((errno == ENOSYS) || (errno == EOVERFLOW)))
            {
              /* sendfile doesn't work on this system, fall back */
              outerror(OUTERROR_TYPE_WARN, "%s transfer method does not work on this system, falling back to next available method", "linux-sendfile");
              gdata.transfermethod++;
              return;
            }
          else if (howmuch < 0 && errno != EAGAIN)
            {
              t_closeconn(t,"Unable to transfer data",errno);
              return;
            }
          else if (howmuch <= 0)
            {
              goto done;
            }
          
          howmuch2 = max2(0,howmuch);
          break;
          
#endif

#if defined(HAVE_FREEBSD_SENDFILE)
        case TRANSFERMETHOD_FREEBSD_SENDFILE:
          
          offset = t->bytessent;
          
          jj = sendfile(t->xpack->file_fd,
                       t->con.clientsocket,
                       offset,
                       attempt,
                       &sendfile_header,
                       &offset,
                       0);
          
          if (jj < 0 && ((errno == ENOSYS) || (errno == EOPNOTSUPP)))
            {
              /* sendfile doesn't work on this system, fall back */
              outerror(OUTERROR_TYPE_WARN, "%s transfer method does not work on this system, falling back to next available method", "freebsd-sendfile");
              gdata.transfermethod++;
              return;
            }
          else if ((jj < 0) && (errno != EAGAIN))
            {
              t_closeconn(t,"Unable to transfer data",errno);
              return;
            }
          
          /*
           * NOTE: according to freebsd man page,
           * sendfile can set EAGAIN and _STILL_ send data!!
           */
          howmuch = (ssize_t)offset;
          if (howmuch == 0)
            {
              goto done;
            }
          howmuch2 = max2(0,howmuch);
          break;
#endif
          
        case TRANSFERMETHOD_READ_WRITE:
          dataptr = gdata.sendbuff;
          
          if (t->xpack->file_fd_location != t->bytessent)
            {
              offset = lseek(t->xpack->file_fd, t->bytessent, SEEK_SET);
              
              if (offset != t->bytessent)
                {
                  int errno2 = errno;
                  outerror(OUTERROR_TYPE_WARN,"Can't seek location in file '%s': %s",
                           t->xpack->file, strerror(errno));
                  t_closeconn(t, "Unable to locate data in file", errno2);
                  return;
                }
              t->xpack->file_fd_location = t->bytessent;
            }
          
          howmuch = read(t->xpack->file_fd, dataptr, attempt);
          
          if (howmuch < 0 && errno != EAGAIN)
            {
              int errno2 = errno;
              outerror(OUTERROR_TYPE_WARN,"Can't read data from file '%s': %s",
                       t->xpack->file, strerror(errno));
              t_closeconn(t, "Unable to read data from file", errno2);
              return;
            }
          else if (howmuch <= 0)
            {
              goto done;
            }
          
          t->xpack->file_fd_location += howmuch;
          
          howmuch2 = send(t->con.clientsocket, dataptr, howmuch, MSG_NOSIGNAL);
          
          if (howmuch2 < 0 && errno != EAGAIN)
            {
              t_closeconn(t,"Connection Lost",errno);
              return;
            }
          
          howmuch2 = max2(0,howmuch2);
          break;
          
#if defined(HAVE_MMAP)
        case TRANSFERMETHOD_MMAP:
          if (t->bytessent == t->xpack->st_size)
            {
              /* EOF */
              goto done;
            }
          if (!t->mmap_info ||
              ((ir_uint64)(t->bytessent) >= (t->mmap_info->mmap_offset + (ir_uint64)(t->mmap_info->mmap_size))))
            {
              int callval_i;
              mmap_info_t *mm;
              
              if (t->mmap_info)
                {
                  t->mmap_info->ref_count--;
                  if (!t->mmap_info->ref_count)
                    {
                      callval_i = munmap(t->mmap_info->mmap_ptr, t->mmap_info->mmap_size);
                      if (callval_i < 0)
                        {
                          outerror(OUTERROR_TYPE_WARN, "Couldn't munmap(): %s",
                                   strerror(errno));
                        }
                      irlist_delete(&t->xpack->mmaps, t->mmap_info);
                    }
                  t->mmap_info = NULL;
                }
              
              /* see if what we want is already mapped */
              for (mm = irlist_get_head(&t->xpack->mmaps); mm; mm = irlist_get_next(mm))
                {
                  if (mm->mmap_offset == (t->bytessent & ~(IR_MMAP_SIZE-1)))
                    {
                      t->mmap_info = mm;
                      t->mmap_info->ref_count++;
                      break;
                    }
                }
              
              if (!t->mmap_info)
                {
                  /* nope, add one */
                  mm = irlist_add(&t->xpack->mmaps, sizeof(mmap_info_t));
                  t->mmap_info = mm;
                  
                  mm->ref_count++;
                  mm->mmap_offset = t->bytessent & ~(IR_MMAP_SIZE-1);
                  mm->mmap_size = IR_MMAP_SIZE;
                  if ((mm->mmap_offset + (ir_uint64)(mm->mmap_size)) > (ir_uint64)(t->xpack->st_size))
                    {
                      mm->mmap_size = t->xpack->st_size - mm->mmap_offset;
                    }
                  
                  mm->mmap_ptr = mmap(NULL,
                                      mm->mmap_size,
                                      PROT_READ,
                                      MAP_SHARED,
                                      t->xpack->file_fd,
                                      mm->mmap_offset);
                  if ((mm->mmap_ptr == (unsigned char *)MAP_FAILED) || (!mm->mmap_ptr))
                    {
                      irlist_delete(&t->xpack->mmaps, mm);
                      t->mmap_info = NULL;
                      if (errno == ENOMEM)
                        {
                          /* mmap doesn't work on this system, fall back */
                          outerror(OUTERROR_TYPE_WARN, "%s transfer method does not work on this system, falling back to next available method", "mmap");
                          gdata.transfermethod++;
                        }
                      else
                        {
                          t_closeconn(t,"Unable to access file",errno);
                        }
                      return;
                    }
                  if (gdata.debug > 53)
                    {
                      ioutput(OUT_S, COLOR_BLUE,
                              "mmap() [%p] offset=0x%.8" LLPRINTFMT "X size=0x%.8" LLPRINTFMT "X",
                              mm->mmap_ptr,
                              (ir_uint64)(mm->mmap_offset),
                              (ir_uint64)(mm->mmap_size));
                    }
                }
            }
          
          dataptr = t->mmap_info->mmap_ptr + t->bytessent - t->mmap_info->mmap_offset;
          
          if ((t->bytessent + attempt) > (t->mmap_info->mmap_offset + t->mmap_info->mmap_size))
            {
              howmuch = (t->mmap_info->mmap_offset + t->mmap_info->mmap_size) - t->bytessent;
            }
          else
            {
              howmuch = attempt;
            }
          
          if (howmuch == 0)
            {
              /* EOF */
              goto done;
            }
          
          howmuch2 = send(t->con.clientsocket, dataptr, howmuch, MSG_NOSIGNAL);
          
          if (howmuch2 < 0 && errno != EAGAIN)
            {
              t_closeconn(t,"Connection Lost",errno);
              return;
            }
          
          howmuch2 = max2(0,howmuch2);
          break;
#endif
          
        default:
          t_closeconn(t,"Transfer Method unknown!", 0);
          return;
        }
      
      if (howmuch2 > 0)
        {
          t->con.lastcontact = gdata.curtime;
        }
      
      t->bytessent += howmuch2;
      gdata.xdccsum[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
      if (t->unlimited == 0)
        gdata.xdccsent[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
      t->tx_bucket -= howmuch2;
      j += howmuch2;
      gdata.totalsent += howmuch2;
      
      for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
        {
          gdata.transferlimits[ii].used += (ir_uint64)howmuch2;
        }
      
      if (gdata.debug > 51)
        {
          ioutput(OUT_S, COLOR_BLUE, "File %ld Write %ld", (long)howmuch, (long)howmuch2);
        }
      
      /* if over 50% send one to be fair */
      if ( gdata.maxb && ((j*100) > (gdata.maxb*1024*50)) )
        {
          goto done;
        }
      
    } while ((t->tx_bucket >= TXSIZE) && (howmuch2 > 0));
   
 done:
  
   if (t->bytessent >= t->xpack->st_size)
     {
#ifdef HAVE_MMAP
       if (t->mmap_info)
         {
           t->mmap_info->ref_count--;
           if (!t->mmap_info->ref_count)
             {
               int callval_i;
               callval_i = munmap(t->mmap_info->mmap_ptr, t->mmap_info->mmap_size);
               if (callval_i < 0)
                 {
                   outerror(OUTERROR_TYPE_WARN, "Couldn't munmap(): %s",
                            strerror(errno));
                 }
               irlist_delete(&t->xpack->mmaps, t->mmap_info);
             }
           t->mmap_info = NULL;
         }
#endif
       
       t->tr_status = TRANSFER_STATUS_WAITING;
     }
   
   return;
}

void t_readjunk (transfer * const t)
{
  int i,j;
  int show;
  
  updatecontext();
  
  i = recv(t->con.clientsocket, gdata.sendbuff, BUFFERSIZE, MSG_DONTWAIT);
  
  if (gdata.debug > 52)
    {
      ioutput(OUT_S, COLOR_BLUE, "Read %d:", i);
      hexdump(OUT_S, COLOR_BLUE, "", gdata.sendbuff, i);
    }
  
  if (i < 0)
    {
      t_closeconn(t,"Connection Lost",errno);
    }
  else if (i < 1)
    {
      t_closeconn(t,"Connection Lost",0);
    }
  else
    {
      t->con.lastcontact = gdata.curtime;
      
      for (j=0; j<i; j++)
        {
          int byte;

          if (t->mirc_dcc64)
            {
              byte = 7-((t->bytesgot+j)%8);
              t->curack &= ~(0xFFULL << (byte*8));
              t->curack |= (ir_uint64)(gdata.sendbuff[j]) << (byte*8);
            }
          else
            {
              byte = 3-((t->bytesgot+j)%4);
              t->curack &= ~(0xFFUL << (byte*8));
              t->curack |= (ir_uint64)gdata.sendbuff[j] << (byte*8);
            }
          
          if (byte == 0)
            {
              show = verify_acknowlede(t);
              t->lastack = t->curack;
            
              if (t->tr_status == TRANSFER_STATUS_WAITING)
                show ++;

              if ((gdata.debug >54) && (show > 0))
                {
                  ioutput(OUT_S, COLOR_BLUE,
                          "XDCC [%02i:%s on %s]: Acknowleged %" LLPRINTFMT "d Bytes",
                          t->id, t->nick, gdata.networks[ t->net ].name,
                          t->lastack );
                }
            }
        }
      
      t->bytesgot += i;
    }
  
  return;
}

void t_istimeout (transfer * const t)
{
  updatecontext();
  
  if ((gdata.curtime - t->con.lastcontact) > 180)
    {
      t_closeconn(t,"DCC Timeout (180 Sec Timeout)",0);
    }
  else if ((gdata.curtime - t->con.lastcontact) > 150)
    {
      if (!t->close_to_timeout)
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
                  "XDCC [%02i:%s on %s]: Connection close to timeout 30 sec remain",
                  t->id, t->nick, gdata.networks[ t->net ].name);
        }
      t->close_to_timeout = 1;
    }
  else
    {
      t->close_to_timeout = 0;
    }
}

void t_flushed (transfer * const t)
{
  ir_uint64 timetookms;
  char *tempstr;
  
  updatecontext();
  
  if (t->lastack < t->xpack->st_size)
    {
      return;
    }
  
  tempstr = mymalloc(maxtextlength);
  tempstr[0] = 0;
  
  timetookms = gdata.curtimems - t->connecttimems;
  if (timetookms < 1U)
    {
      timetookms = 1;
    }
  
  if (timetookms > (60*60*1000U))
    {
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
               " %" LLPRINTFMT "u hr", timetookms/60/60/1000);
    }
  
  if ((timetookms%(60*60*1000U)) > (60*1000U))
    {
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
               " %" LLPRINTFMT "u min", (timetookms%(60*60*1000))/60/1000);
    }
  
  snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
           " %" LLPRINTFMT "u.%03" LLPRINTFMT "u sec", (timetookms%(60*1000))/1000, (timetookms%1000));
  
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC [%02i:%s on %s]: Transfer Completed (%" LLPRINTFMT "d kB,%s, %0.1f kB/sec)",
          t->id, t->nick, gdata.networks[ t->net ].name,
          (t->xpack->st_size-t->startresume)/1024,
          tempstr,
          ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0));
  
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Log: Pack %u, Nick %s" ", Network %s" ", Sent %" LLPRINTFMT "d kB" ", Recv %" LLPRINTFMT "d kB" ", File %s",
          number_of_pack(t->xpack),
          t->nick,
          gdata.networks[ t->net ].name,
          (t->xpack->st_size-t->startresume)/1024, t->bytesgot/1024,
          t->xpack->desc );
  
  if (t->quietmode == 0)
    {
      if (t->xpack->has_md5sum)
        {
          notice(t->nick, "** Transfer Completed (%" LLPRINTFMT "d kB,%s, %0.1f kB/sec, md5sum: " MD5_PRINT_FMT ")",
                 (t->xpack->st_size-t->startresume)/1024,
                 tempstr,
                 ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0),
                 MD5_PRINT_DATA(t->xpack->md5sum));
        }
      else
        {
          notice(t->nick, "** Transfer Completed (%" LLPRINTFMT "d kB,%s, %0.1f kB/sec)",
                 (t->xpack->st_size-t->startresume)/1024,
                 tempstr,
                 ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0));
        }
    }
  if (gdata.download_completed_msg)
    {
      notice_slow(t->nick, "%s", gdata.download_completed_msg);
    }
  
  if ( ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0) > gdata.record )
    {
      gdata.record = ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0);
    }
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S, COLOR_YELLOW, "clientsock = %d", t->con.clientsocket);
    }
  shutdown_close(t->con.clientsocket);
  t->xpack->file_fd_count--;
  if (!t->xpack->file_fd_count && (t->xpack->file_fd != FD_UNUSED))
    {
      close(t->xpack->file_fd);
      t->xpack->file_fd = FD_UNUSED;
      t->xpack->file_fd_location = 0;
    }
  t->tr_status = TRANSFER_STATUS_DONE;
  t->xpack->gets++;
  
  if ((t->xpack->dlimit_max != 0) && (t->xpack->gets >= t->xpack->dlimit_used))
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW, "Reached Pack Download Limit %u for %s",
              t->xpack->dlimit_max, t->xpack->desc);
       
      /* remove queued users */
      queue_pack_limit(&gdata.mainqueue, t->xpack);
      queue_pack_limit(&gdata.idlequeue, t->xpack);
    }
  
  mydelete(tempstr);
}

void t_closeconn(transfer * const t, const char *msg, int errno1)
{
  gnetwork_t *backup;
  
  updatecontext();
  
  if (errno1)
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "XDCC [%02i:%s on %s]: Connection closed: %s (%s)",
              t->id, t->nick, gdata.networks[ t->net ].name, msg, strerror(errno1));
    }
  else
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "XDCC [%02i:%s on %s]: Connection closed: %s",
              t->id, t->nick, gdata.networks[ t->net ].name, msg);
    }
  
  if (t->tr_status == TRANSFER_STATUS_DONE)
    {
      return;
    }
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S, COLOR_YELLOW,
              "clientsock = %d", t->con.clientsocket);
    }
  
#ifdef HAVE_MMAP
  if (t->mmap_info)
    {
      t->mmap_info->ref_count--;
      if (!t->mmap_info->ref_count)
        {
          int callval_i;
          callval_i = munmap(t->mmap_info->mmap_ptr, t->mmap_info->mmap_size);
          if (callval_i < 0)
            {
              outerror(OUTERROR_TYPE_WARN, "Couldn't munmap(): %s",
                       strerror(errno));
            }
          irlist_delete(&t->xpack->mmaps, t->mmap_info);
        }
      t->mmap_info = NULL;
    }
#endif
  
  if (t->con.listensocket != FD_UNUSED && t->con.listensocket > 2)
    {
      event_close(t->con.listensocket);
      t->con.listensocket = FD_UNUSED;
    }
  if (t->con.clientsocket != FD_UNUSED && t->con.clientsocket > 2)
    {
      shutdown_close(t->con.clientsocket);
      t->con.clientsocket = FD_UNUSED;
    }
  t->xpack->file_fd_count--;
  if (!t->xpack->file_fd_count && (t->xpack->file_fd != FD_UNUSED))
    {
      close(t->xpack->file_fd);
      t->xpack->file_fd = FD_UNUSED;
      t->xpack->file_fd_location = 0;
    }
  
  if (t->tr_status == TRANSFER_STATUS_LISTENING)
    ir_listen_port_connected(t->con.localport);

#ifdef USE_UPNP
  if (gdata.upnp_router && (t->con.family == AF_INET))
    upnp_rem_redir(t->con.localport);
#endif /* USE_UPNP */

  t->tr_status = TRANSFER_STATUS_DONE;
  
  backup = gnetwork;
  gnetwork = &(gdata.networks[t->net]);
  if (errno1)
    {
      notice(t->nick, "** Closing Connection: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      notice(t->nick, "** Closing Connection: %s", msg);
    }
  gnetwork = backup;
}

void t_setresume(transfer * const t, const char *amt) {
   updatecontext();

   t->startresume = (off_t)atoull(amt);
   t->lastack = t->startresume;
   }

void t_remind(transfer * const t) {
   gnetwork_t *backup;
   
   updatecontext();
   
   backup = gnetwork;
   gnetwork = &(gdata.networks[t->net]);
   if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
     {
       notice(t->nick,"** You have a DCC pending, Set your client to receive the transfer. "
              "Type \"/MSG %s XDCC CANCEL\" to abort the transfer. "
              "(%li seconds remaining until timeout)",
              get_user_nick(),
              (long)(t->con.lastcontact+180-gdata.curtime));
     }
   
   if ( gdata.reminder_send_retry > t->reminded ) {
     t_start_dcc_send(t);
     }
   gnetwork = backup;
   t->reminded++;
   }

void t_checkminspeed(transfer * const t) {
   char *hostmask;
   char *tempstr2;

   updatecontext();

   if (t->tr_status != TRANSFER_STATUS_SENDING)      return; /* no checking unless we're sending */
   if (t->con.connecttime+MIN_TL > gdata.curtime)    return; /* no checking until time has passed */
   if (t->nomin || (t->xpack->minspeed) == 0.0)      return; /* no minspeed for this transfer */
   if ( t->lastspeed+0.11 > t->xpack->minspeed )     return; /* over minspeed */
   
   if (gdata.no_minspeed_on_free)
     {
        if (irlist_size(&gdata.trans) < gdata.slotsmax) return; /* free slots */
     }
   
   tempstr2 = mymalloc(maxtextlength);
   snprintf(tempstr2, maxtextlength,
        "Under Min Speed Requirement, %2.1fK/sec is less than %2.1fK/sec",
         t->lastspeed,t->xpack->minspeed);
   t_closeconn(t,tempstr2,0);
   mydelete(tempstr2);
   
   if (gdata.punishslowusers)
     {
       igninfo *ignore;
       transfer *tr;
       gnetwork_t *backup;
       
       for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
         {
           if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
               (strcasecmp(tr->nick,t->nick) == 0))
             {
               t_closeconn(tr, "You are being punished for your slowness", 0);
             }
         }
       
       queue_punish_abuse("You are being punished for your slowness", t->net, t->nick);
       
       hostmask = to_hostmask( "*", t->hostname);
       ignore = get_ignore(hostmask);
       mydelete(hostmask);
       ignore->flags |= IGN_IGNORING;
       ignore->bucket = (gdata.punishslowusers*60)/gdata.autoignore_threshold;
       
       backup = gnetwork;
       gnetwork = &(gdata.networks[t->net]);
       ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
               "Punish-ignore activated for (%s on %s) (%s) %u minutes",
               t->nick, gdata.networks[ t->net ].name,
               ignore->hostmask,
               gdata.punishslowusers);
       
       notice(t->nick, "Punish-ignore activated for %s (%s) %u minutes",
              t->nick,
              ignore->hostmask,
              gdata.punishslowusers);
       
       gnetwork = backup;
       write_statefile();
     }
   
   }

/* End of File */

