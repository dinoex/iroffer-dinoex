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
#include "dinoex_irc.h"
#include "dinoex_queue.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

void t_initvalues (transfer * const t) {

   updatecontext();

      t->con.family = gnetwork->myip.sa.sa_family;
      t->tr_status = TRANSFER_STATUS_UNUSED;
      t->con.listensocket = FD_UNUSED;
      t->con.clientsocket = FD_UNUSED;
      t->con.lastcontact = gdata.curtime;
      t->con.localport = 0;
      t->id = 200;
      t->overlimit = 0;
   }

void t_setuplisten (transfer * const t)
{
  int rc;
  
  updatecontext();

  rc = open_listen(t->con.family, &(t->con.local), &(t->con.listensocket), 0, gdata.tcprangestart, 1, NULL);
  if (rc != 0)
    {
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
    }
  
  t->con.localport = get_port(&(t->con.local));
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
   
   FD_CLR(t->con.listensocket, &gdata.readset);
   close(t->con.listensocket);
   t->con.listensocket = FD_UNUSED;

   t_setup_send(t);
}

void t_setup_send(transfer * const t)
{
   char *msg;
   SIGNEDSOCK int addrlen;
   
#if !defined(_OS_SunOS)
   SIGNEDSOCK int tempi;
#endif
#if !defined(_OS_SunOS) || defined(_OS_BSD_ANY) || !defined(CANT_SET_TOS)
   int tempc;
#endif
   
   t->tr_status = TRANSFER_STATUS_SENDING;
   if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "clientsock = %d", t->con.clientsocket);
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
     }
   
   t->bytessent = t->startresume;
   
#if !defined(_OS_SunOS)
   tempi = sizeof(int);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_YELLOW,"SO_SNDBUF ");
   getsockopt(t->con.clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," a %li",(long)tempc);
   
   tempc = 65535;
   setsockopt(t->con.clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, sizeof(int));

   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," b %li",(long)tempc);
   getsockopt(t->con.clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_YELLOW," c %li",(long)tempc);
#endif
   
#if defined(_OS_BSD_ANY)
   /* #define SO_SNDLOWAT     0x1003     */
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_YELLOW,"SO_SNDLOWAT ");
   getsockopt(t->con.clientsocket, SOL_SOCKET, 0x1003, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," %i",tempc);
      
   tempc = 24577;
   setsockopt(t->con.clientsocket, SOL_SOCKET, 0x1003, &tempc, sizeof(int));
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," %i",tempc);

   getsockopt(t->con.clientsocket, SOL_SOCKET, 0x1003, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_YELLOW," %i\n", tempc);
#endif
   
#if !defined(CANT_SET_TOS)
   /* Set TOS socket option to max throughput */
   tempc = 0x8; /* IPTOS_THROUGHPUT */
   setsockopt(t->con.clientsocket, IPPROTO_IP, IP_TOS, &tempc, sizeof(int));
#endif
   
   if (set_socket_nonblocking(t->con.clientsocket, 1) < 0 )
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
   
   
   t->con.lastcontact = gdata.curtime;
   t->con.connecttime = gdata.curtime;
   t->connecttimems = gdata.curtimems;
   t->lastspeed = t->xpack->minspeed;
   t->lastspeedamt = t->startresume;
   addrlen = (t->con.family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
   
   if ((getpeername(t->con.clientsocket, &(t->con.remote.sa), &(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN, "Couldn't get Remote IP: %s", strerror(errno));

   else {
      t->remoteip = ntohl(t->con.remote.sin.sin_addr.s_addr);
      }
   
   if ((getsockname(t->con.clientsocket, &(t->con.local.sa), &(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN, "Couldn't get Local IP: %s", strerror(errno));
   
   msg = mycalloc(maxtextlength);
   my_getnameinfo(msg, maxtextlength -1, &(t->con.remote.sa), 0);
   mydelete(t->con.remoteaddr);
   t->con.remoteaddr = mystrdup(msg);
   my_getnameinfo(msg, maxtextlength -1, &(t->con.local.sa), 0);
   t->con.localaddr = mystrdup(msg);
   mydelete(msg);

   ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
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
          
          if (howmuch < 0 && errno == ENOSYS)
            {
              /* sendfile doesn't work on this system, fall back */
              outerror(OUTERROR_TYPE_WARN, "linux-sendfile transfer method does not work on this system, falling back to next available method");
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
          
          if ((jj < 0) && (errno != EAGAIN))
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
          
          howmuch2 = write(t->con.clientsocket, dataptr, howmuch);
          
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
              (t->bytessent >= (t->mmap_info->mmap_offset + t->mmap_info->mmap_size)))
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
                  if ((mm->mmap_offset + mm->mmap_size) > t->xpack->st_size)
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
                          outerror(OUTERROR_TYPE_WARN, "mmap transfer method does not work on this system, falling back to next available method");
                          gdata.transfermethod++;
                        }
                      else
                        {
                          t_closeconn(t,"Unable to access file",errno);
                        }
                      return;
                    }
                  if (gdata.debug > 4)
                    {
                      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_BLUE,
                              "mmap() [%p] offset=0x%.8" LLPRINTFMT "X size=0x%.8" LLPRINTFMT "X",
                              mm->mmap_ptr,
                              mm->mmap_offset,
                              (unsigned long long)mm->mmap_size);
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
          
          howmuch2 = write(t->con.clientsocket, dataptr, howmuch);
          
          if (howmuch2 < 0 && errno != EAGAIN)
            {
              t_closeconn(t,"Connection Lost",errno);
              return;
            }
          
          howmuch2 = max2(0,howmuch2);
          break;
#endif
          
        default:
          t_closeconn(t,"Transfer Method unknown! %d", gdata.transfermethod);
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
      gdata.totalsent += (unsigned long long)howmuch2;
      
      for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
        {
          gdata.transferlimits[ii].used += (ir_uint64)howmuch2;
        }
      
      if (gdata.debug > 4)
        {
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_BLUE,"File %ld Write %ld",(long)howmuch,(long)howmuch2);
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
  
  updatecontext();
  
  i = read(t->con.clientsocket, gdata.sendbuff, BUFFERSIZE);
  
  if (gdata.debug > 4)
    {
      ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_BLUE,"Read %d: ",i);
      for (j=0; j<i; j++)
        {
          ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_BLUE,
                  "%2.2X ",gdata.sendbuff[j]);
        }
      ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_BLUE,"%s","");
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
          int byte = 3-((t->bytesgot+j)%4);
          t->curack &= ~(0xFFUL << (byte*8));
          t->curack |= gdata.sendbuff[j] << (byte*8);
          
          if (byte == 0)
            {
              if (t->xpack->st_size > 0xFFFFFFFF)
                {
                  while (t->curack < t->lastack)
                    t->curack += 0x100000000ULL;
                }
              t->lastack = t->curack;
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
          ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
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
  unsigned long long timetookms;
  char *tempstr;
  
  updatecontext();
  
  if (t->lastack < t->xpack->st_size)
    {
      return;
    }
  
  tempstr = mycalloc(maxtextlength);
  
  timetookms = gdata.curtimems - t->connecttimems;
  if (timetookms < 1)
    {
      timetookms = 1;
    }
  
  if (timetookms > (60*60*1000))
    {
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
               " %" LLPRINTFMT "u hr", timetookms/60/60/1000);
    }
  
  if ((timetookms%(60*60*1000)) > (60*1000))
    {
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
               " %" LLPRINTFMT "u min", (timetookms%(60*60*1000))/60/1000);
    }
  
  snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
           " %" LLPRINTFMT "u.%03" LLPRINTFMT "u sec", (timetookms%(60*1000))/1000, (timetookms%1000));
  
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "XDCC [%02i:%s on %s]: Transfer Completed (%" LLPRINTFMT "u KB,%s, %0.1f KB/sec)",
          t->id, t->nick, gdata.networks[ t->net ].name,
          (t->xpack->st_size-t->startresume/1024),
          tempstr,
          ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0));
  
  ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "Log: Pack %d, Nick %s, Network %s, File %s",
          number_of_pack(t->xpack),
          t->nick,
          gdata.networks[ t->net ].name,
          t->xpack->desc );
  
  if (!gdata.quietmode)
    {
      if (t->xpack->has_md5sum)
        {
          notice(t->nick,"** Transfer Completed (%" LLPRINTFMT "u KB,%s, %0.1f KB/sec, md5sum: " MD5_PRINT_FMT ")",
                 (t->xpack->st_size-t->startresume/1024),
                 tempstr,
                 ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0),
                 MD5_PRINT_DATA(t->xpack->md5sum));
        }
      else
        {
          notice(t->nick,"** Transfer Completed (%" LLPRINTFMT "u KB,%s, %0.1f KB/sec)",
                 (t->xpack->st_size-t->startresume/1024),
                 tempstr,
                 ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0));
        }
    }
  
  if ( ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0) > gdata.record )
    {
      gdata.record = ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0);
    }
  
  if (gdata.debug > 0)
    {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW, "clientsock = %d", t->con.clientsocket);
    }
  FD_CLR(t->con.clientsocket, &gdata.writeset);
  FD_CLR(t->con.clientsocket, &gdata.readset);
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
      ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"Reached Pack Download Limit %d for %s",
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
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "XDCC [%02i:%s on %s]: Connection closed: %s (%s)",
              t->id, t->nick, gdata.networks[ t->net ].name, msg, strerror(errno1));
    }
  else
    {
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "XDCC [%02i:%s on %s]: Connection closed: %s",
              t->id, t->nick, gdata.networks[ t->net ].name, msg);
    }
  
  if (t->tr_status == TRANSFER_STATUS_DONE)
    {
      return;
    }
  
  if (gdata.debug > 0)
    {
      ioutput(CALLTYPE_NORMAL, OUT_S, COLOR_YELLOW,
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
      FD_CLR(t->con.listensocket, &gdata.readset);
      close (t->con.listensocket);
      t->con.listensocket = FD_UNUSED;
    }
  if (t->con.clientsocket != FD_UNUSED && t->con.clientsocket > 2)
    {
      FD_CLR(t->con.clientsocket, &gdata.writeset);
      FD_CLR(t->con.clientsocket, &gdata.readset);
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
              save_nick(gnetwork->user_nick),
              (long)(t->con.lastcontact+180-gdata.curtime));
     }
   
   gnetwork = backup;
   t->reminded++;
   }

void t_checkminspeed(transfer * const t) {
   char *tempstr2;

   updatecontext();

   if (t->tr_status != TRANSFER_STATUS_SENDING)      return; /* no checking unless we're sending */
   if (t->con.connecttime+MIN_TL > gdata.curtime)    return; /* no checking until time has passed */
   if (t->nomin || (t->xpack->minspeed) == 0.0)      return; /* no minspeed for this transfer */
   if ( t->lastspeed+0.11 > t->xpack->minspeed )     return; /* over minspeed */
   
   tempstr2 = mycalloc(maxtextlength);
   snprintf(tempstr2,maxtextlength-1,
        "Under Min Speed Requirement, %2.1fK/sec is less than %2.1fK/sec",
         t->lastspeed,t->xpack->minspeed);
   t_closeconn(t,tempstr2,0);
   mydelete(tempstr2);
   
   if (gdata.punishslowusers)
     {
       char *tempstr;
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
       
       queue_punishslowusers(&gdata.mainqueue, t->net, t->nick);
       queue_punishslowusers(&gdata.idlequeue, t->net, t->nick);
       
       ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
       ignore->regexp = mycalloc(sizeof(regex_t));
       
       ignore->hostmask = to_hostmask( "*", t->hostname);
       
       tempstr = hostmasktoregex(ignore->hostmask);
       if (regcomp(ignore->regexp,tempstr,REG_ICASE|REG_NOSUB))
         {
           ignore->regexp = NULL;
         }
       mydelete(tempstr);
       
       ignore->flags |= IGN_IGNORING;
       ignore->lastcontact = gdata.curtime;
       ignore->bucket = (gdata.punishslowusers*60)/gdata.autoignore_threshold;
       
       backup = gnetwork;
       gnetwork = &(gdata.networks[t->net]);
       ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
               "Punish-ignore activated for (%s on %s) (%s) %d minutes",
               t->nick, gdata.networks[ t->net ].name,
               ignore->hostmask,
               gdata.punishslowusers);
       
       notice(t->nick,"Punish-ignore activated for %s (%s) %d minutes",
              t->nick,
              ignore->hostmask,
              gdata.punishslowusers);
       
       gnetwork = backup;
       write_statefile();
     }
   
   }

/* End of File */

