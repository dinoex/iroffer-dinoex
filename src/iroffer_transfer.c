/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the README file.
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


void t_initvalues (transfer * const t) {

   updatecontext();

      t->tr_status = TRANSFER_STATUS_UNUSED;
      t->listensocket=FD_UNUSED;
      t->clientsocket=FD_UNUSED;
      t->lastcontact = gdata.curtime;
      t->id = 200;
      t->overlimit = 0;
   }

void t_setuplisten (transfer * const t)
{
  int tempc;
  
  updatecontext();
  
  if ((t->listensocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Could Not Create Socket, Aborting");
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
    }
  
  if (gdata.tcprangestart)
    {
      tempc = 1;
      setsockopt(t->listensocket, SOL_SOCKET, SO_REUSEADDR, &tempc, sizeof(int));
    }
  
  bzero ((char *) &t->serveraddress, sizeof (struct sockaddr_in));
  
  t->serveraddress.sin_family = AF_INET;
  t->serveraddress.sin_addr.s_addr = INADDR_ANY;
  
  if (ir_bind_listen_socket(t->listensocket, &t->serveraddress) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Bind to Socket, Aborting");
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
    }
  
  t->listenport = ntohs (t->serveraddress.sin_port);
  
  if (listen (t->listensocket, 1) < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Couldn't Listen, Aborting");
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
    }
  
  t->tr_status = TRANSFER_STATUS_LISTENING;
  
}

void t_establishcon (transfer * const t) {
   struct sockaddr_in temp1;
   SIGNEDSOCK int addrlen;
   
#if !defined(_OS_SunOS)
   SIGNEDSOCK int tempi;
#endif
#if !defined(_OS_SunOS) || defined(_OS_BSD_ANY) || !defined(CANT_SET_TOS)
   int tempc;
#endif
   
   updatecontext();
   
   addrlen = sizeof (struct sockaddr_in);
   
   if (!gdata.attop) gototop();
   
   if ((t->clientsocket = accept(t->listensocket, (struct sockaddr *) &t->serveraddress, &addrlen)) < 0) {
      outerror(OUTERROR_TYPE_WARN,"Accept Error, Aborting");
      t_closeconn(t,"Connection Error, Try Again",errno);
      return;
      }
   
   ir_listen_port_connected(t->listenport);
   
   FD_CLR(t->listensocket, &gdata.readset);
   t->tr_status = TRANSFER_STATUS_SENDING;
   close(t->listensocket);
   t->listensocket = FD_UNUSED;
   
   if (gdata.debug > 0) {
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"clientsock = %d",t->clientsocket);
      }
      
   if (t->xpack->file_fd == FD_UNUSED)
     {
       t->xpack->file_fd = open(t->xpack->file, O_RDONLY | ADDED_OPEN_FLAGS);
       if (t->xpack->file_fd < 0) {
         t->xpack->file_fd = FD_UNUSED;
         outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Access Offered File '%s': %s",t->xpack->file,strerror(errno));
         t_closeconn(t,"File Error, Report the Problem to the Owner",errno);
         return;
       }
       t->xpack->file_fd_location = 0;
     }
   
   t->bytessent = t->startresume;
   
#if !defined(_OS_SunOS)
   tempi = sizeof(int);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_YELLOW,"SO_SNDBUF ");
   getsockopt(t->clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," a %li",(long)tempc);
   
   tempc = 65535;
   setsockopt(t->clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, sizeof(int));

   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," b %li",(long)tempc);
   getsockopt(t->clientsocket, SOL_SOCKET, SO_SNDBUF, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_YELLOW," c %li",(long)tempc);
#endif
   
#if defined(_OS_BSD_ANY)
   /* #define SO_SNDLOWAT     0x1003     */
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_YELLOW,"SO_SNDLOWAT ");
   getsockopt(t->clientsocket, SOL_SOCKET, 0x1003, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," %i",tempc);
      
   tempc = 24577;
   setsockopt(t->clientsocket, SOL_SOCKET, 0x1003, &tempc, sizeof(int));
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_MIDDLE,OUT_S,COLOR_YELLOW," %i",tempc);

   getsockopt(t->clientsocket, SOL_SOCKET, 0x1003, &tempc, &tempi);
   if (gdata.debug > 0) ioutput(CALLTYPE_MULTI_END,OUT_S,COLOR_YELLOW," %i\n",tempc);
#endif
   
#if !defined(CANT_SET_TOS)
   /* Set TOS socket option to max throughput */
   tempc = 0x8; /* IPTOS_THROUGHPUT */
   setsockopt(t->clientsocket, SOL_IP, IP_TOS, &tempc, sizeof(int));
#endif
   
   if (set_socket_nonblocking(t->clientsocket,1) < 0 )
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
   
   
   t->lastcontact = gdata.curtime;
   t->connecttime = gdata.curtime;
   t->connecttimems = gdata.curtimems;
   t->lastspeed = t->xpack->minspeed;
   t->lastspeedamt = t->startresume;
   
   if ((getpeername(t->clientsocket,(struct sockaddr *) &temp1,&(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN,"Couldn't get Remote IP");
   else {
      t->remoteip = ntohl(temp1.sin_addr.s_addr);
      t->remoteport = ntohs(temp1.sin_port);
      }
   
   if ((getsockname(t->clientsocket,(struct sockaddr *) &temp1,&(addrlen))) < 0)
      outerror(OUTERROR_TYPE_WARN,"Couldn't get Local IP");
   else
      t->localip = ntohl(temp1.sin_addr.s_addr);
   
   ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,"XDCC [%02i:%s]: Connection established (%ld.%ld.%ld.%ld:%d -> %ld.%ld.%ld.%ld:%d)",
            t->id,t->nick,
            t->remoteip>>24, (t->remoteip>>16) & 0xFF, (t->remoteip>>8) & 0xFF, t->remoteip & 0xFF, t->remoteport,
            t->localip>>24, (t->localip>>16) & 0xFF, (t->localip>>8) & 0xFF, t->localip & 0xFF, t->listenport
            );
   
   
   }

void t_transfersome (transfer * const t)
{
  int j;
  int ii;
  ssize_t howmuch, howmuch2;
  size_t attempt;
  unsigned char *dataptr;
  off_t offset;
#if defined(HAVE_FREEBSD_SENDFILE)
  struct sf_hdtr sendfile_header = {};
#endif
  
  updatecontext();
  
  /* max bandwidth start.... */
  
  if (!t->nomax && (t->xpack->maxspeed > 0))
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
          
          howmuch = sendfile(t->clientsocket,
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
          
          j = sendfile(t->xpack->file_fd,
                       t->clientsocket,
                       offset,
                       attempt,
                       &sendfile_header,
                       &offset,
                       0);
          
          if ((j < 0) && (errno != EAGAIN))
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
                  outerror(OUTERROR_TYPE_WARN,"Can't seek location in file '%s': %s",
                           t->xpack->file, strerror(errno));
                  t_closeconn(t,"Unable to locate data in file",errno);
                  return;
                }
              t->xpack->file_fd_location = t->bytessent;
            }
          
          howmuch = read(t->xpack->file_fd, dataptr, attempt);
          
          if (howmuch < 0 && errno != EAGAIN)
            {
              outerror(OUTERROR_TYPE_WARN,"Can't read data from file '%s': %s",
                       t->xpack->file, strerror(errno));
              t_closeconn(t,"Unable to read data from file",errno);
              return;
            }
          else if (howmuch <= 0)
            {
              goto done;
            }
          
          t->xpack->file_fd_location += howmuch;
          
          howmuch2 = write(t->clientsocket, dataptr, howmuch);
          
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
                              (unsigned long long)mm->mmap_offset,
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
          
          howmuch2 = write(t->clientsocket, dataptr, howmuch);
          
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
          t->lastcontact = gdata.curtime;
        }
      
      t->bytessent += howmuch2;
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
          ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_BLUE,"File %d Write %d",howmuch,howmuch2);
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
  
  i = read(t->clientsocket, gdata.sendbuff, BUFFERSIZE);
  
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
      if (!gdata.attop)
        {
          gototop();
        }
      t_closeconn(t,"Connection Lost",errno);
    }
  else if (i < 1)
    {
      if (!gdata.attop)
        {
          gototop();
        }
      t_closeconn(t,"Connection Lost",0);
    }
  else
    {
      t->lastcontact = gdata.curtime;
      
      for (j=0; j<i; j++)
        {
          int byte = 3-((t->bytesgot+j)%4);
          t->curack &= ~(0xFFUL << (byte*8));
          t->curack |= gdata.sendbuff[j] << (byte*8);
          
          if (byte == 0)
            {
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
  
  if ((gdata.curtime - t->lastcontact) > 180)
    {
      if (!gdata.attop) gototop();
      t_closeconn(t,"DCC Timeout (180 Sec Timeout)",0);
    }
  else if ((gdata.curtime - t->lastcontact) > 150)
    {
      if (!t->close_to_timeout)
        {
          if (!gdata.attop) gototop();
          ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
                  "XDCC [%02i:%s]: Connection close to timeout 30 sec remain",
                  t->id, t->nick);
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
  
  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
          "XDCC [%02i:%s]: Transfer Completed (%" LLPRINTFMT "i KB,%s, %0.1f KB/sec)",
          t->id,t->nick,
          (long long )(t->xpack->st_size-t->startresume)/1024,
          tempstr,
          ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0));
  
  if (!gdata.quietmode)
    {
      if (t->xpack->has_md5sum)
        {
          notice(t->nick,"** Transfer Completed (%" LLPRINTFMT "i KB,%s, %0.1f KB/sec, md5sum: " MD5_PRINT_FMT ")",
                 (long long)(t->xpack->st_size-t->startresume)/1024,
                 tempstr,
                 ((float)(t->xpack->st_size-t->startresume))/1024.0/((float)timetookms/1000.0),
                 MD5_PRINT_DATA(t->xpack->md5sum));
        }
      else
        {
          notice(t->nick,"** Transfer Completed (%" LLPRINTFMT "i KB,%s, %0.1f KB/sec)",
                 (long long)(t->xpack->st_size-t->startresume)/1024,
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
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"clientsock = %d",t->clientsocket);
    }
  FD_CLR(t->clientsocket, &gdata.writeset);
  FD_CLR(t->clientsocket, &gdata.readset);
  /*
   * cygwin close() is broke, if outstanding data is present
   * it will block until the TCP connection is dead, sometimes
   * upto 10-20 minutes, calling shutdown() first seems to help
   */
  shutdown(t->clientsocket, SHUT_RDWR);
  close(t->clientsocket);
  t->xpack->file_fd_count--;
  if (!t->xpack->file_fd_count && (t->xpack->file_fd != FD_UNUSED))
    {
      close(t->xpack->file_fd);
      t->xpack->file_fd = FD_UNUSED;
      t->xpack->file_fd_location = 0;
    }
  t->tr_status = TRANSFER_STATUS_DONE;
  t->xpack->gets++;
  
  mydelete(tempstr);
}

void t_closeconn(transfer * const t, const char *msg, int errno1)
{
  updatecontext();
  
  if (errno1)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "XDCC [%02i:%s]: Connection closed: %s (%s)",
              t->id, t->nick,msg, strerror(errno1));
    }
  else
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_YELLOW,
              "XDCC [%02i:%s]: Connection closed: %s",
              t->id, t->nick,msg);
    }
  
  if (t->tr_status == TRANSFER_STATUS_DONE)
    {
      return;
    }
  
  if (gdata.debug > 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,
              "clientsock = %d",t->clientsocket);
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
  
  if (t->listensocket != FD_UNUSED && t->listensocket > 2)
    {
      FD_CLR(t->listensocket, &gdata.readset);
      close (t->listensocket);
      t->listensocket = FD_UNUSED;
    }
  if (t->clientsocket != FD_UNUSED && t->clientsocket > 2)
    {
      FD_CLR(t->clientsocket, &gdata.writeset);
      FD_CLR(t->clientsocket, &gdata.readset);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(t->clientsocket, SHUT_RDWR);
      close(t->clientsocket);
      t->clientsocket = FD_UNUSED;
    }
  t->xpack->file_fd_count--;
  if (!t->xpack->file_fd_count && (t->xpack->file_fd != FD_UNUSED))
    {
      close(t->xpack->file_fd);
      t->xpack->file_fd = FD_UNUSED;
      t->xpack->file_fd_location = 0;
    }
  
  t->tr_status = TRANSFER_STATUS_DONE;
  
  if (errno1)
    {
      notice(t->nick, "** Closing Connection: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      notice(t->nick, "** Closing Connection: %s", msg);
    }
}

void t_setresume(transfer * const t, const char *amt) {
   updatecontext();

   t->startresume = (off_t)atoull(amt);
   }

void t_remind(transfer * const t) {
   updatecontext();
   
   if (gdata.serverstatus == SERVERSTATUS_CONNECTED)
     {
       notice(t->nick,"** You have a DCC pending, Set your client to receive the transfer. "
	      "(%li seconds remaining until timeout)",(long)(t->lastcontact+180-gdata.curtime));
     }
   
   t->reminded++;
   }

void t_checkminspeed(transfer * const t) {
   char *tempstr2;

   updatecontext();

   if (t->tr_status != TRANSFER_STATUS_SENDING)      return; /* no checking unless we're sending */
   if (t->connecttime+MIN_TL > gdata.curtime)        return; /* no checking until time has passed */
   if (t->nomin || (t->xpack->minspeed) == 0.0)      return; /* no minspeed for this transfer */
   if ( t->lastspeed+0.11 > t->xpack->minspeed )     return; /* over minspeed */
   
   tempstr2 = mycalloc(maxtextlength);
   snprintf(tempstr2,maxtextlength-1,
        "Under Min Speed Requirement, %2.1fK/sec is less than %2.1fK/sec",
         t->lastspeed,t->xpack->minspeed);
   if (!gdata.attop) gototop();
   t_closeconn(t,tempstr2,0);
   mydelete(tempstr2);
   
   if (gdata.punishslowusers)
     {
       char *tempstr;
       igninfo *ignore;
       pqueue *pq;
       transfer *tr;
       
       for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
         {
           if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
               (strcasecmp(tr->nick,t->nick) == 0))
             {
               t_closeconn(tr, "You are being punished for your slowness", 0);
             }
         }
       
       pq = irlist_get_head(&gdata.mainqueue);
       while(pq)
         {
           if (strcasecmp(pq->nick,t->nick) == 0)
             {
               notice(pq->nick,"** Removed From Queue: You are being punished for your slowness");
               mydelete(pq->nick);
               mydelete(pq->hostname);
               pq = irlist_delete(&gdata.mainqueue, pq);
             }
           else
             {
               pq = irlist_get_next(pq);
             }
         }
       
       ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
       ignore->regexp = mycalloc(sizeof(regex_t));
       
       ignore->hostmask = mycalloc(strlen(t->hostname)+5);
       sprintf(ignore->hostmask,"*!*@%s",t->hostname);
       
       tempstr = hostmasktoregex(ignore->hostmask);
       if (regcomp(ignore->regexp,tempstr,REG_ICASE|REG_NOSUB))
         {
           ignore->regexp = NULL;
         }
       mydelete(tempstr);
       
       ignore->flags |= IGN_IGNORING;
       ignore->lastcontact = gdata.curtime;
       ignore->bucket = (gdata.punishslowusers*60)/gdata.autoignore_threshold;
       
       ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_NO_COLOR,
               "Punish-ignore activated for %s (%s) %d minutes",
               t->nick,
               ignore->hostmask,
               gdata.punishslowusers);
       
       notice(t->nick,"Punish-ignore activated for %s (%s) %d minutes",
              t->nick,
              ignore->hostmask,
              gdata.punishslowusers);
       
       write_statefile();
     }
   
   }

/* End of File */

