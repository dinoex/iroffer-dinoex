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
 * @(#) iroffer_upload.c 1.50@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_upload.c|20050313183435|24505
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_kqueue.h"
#include "dinoex_upload.h"
#include "dinoex_irc.h"
#include "dinoex_jobs.h"
#include "dinoex_ruby.h"

#ifdef USE_UPNP
#include "upnp.h"
#endif /* USE_UPNP */

void l_initvalues (upload * const l) {
   updatecontext();

      l->ul_status = UPLOAD_STATUS_UNUSED;
      l->con.listensocket = FD_UNUSED;
      l->con.clientsocket = FD_UNUSED;
      l->filedescriptor=FD_UNUSED;
      l->con.localport = 0;
      l->con.lastcontact = gdata.curtime;
   }

void l_establishcon (upload * const l)
{
  char *tempstr;
  SIGNEDSOCK int addrlen;
  int retval;
  struct stat s;
  
  updatecontext();
  
  retval = l_setup_file(l, &s);
  if (retval == 2)
    {
      tempstr = getsendname(l->file);
      privmsg_fast(l->nick, IRC_CTCP "DCC RESUME %s %d %" LLPRINTFMT "d" IRC_CTCP,
                   tempstr, l->con.remoteport, s.st_size);
      mydelete(tempstr);
      return;
    }
  if (retval != 0)
    {
      return;
    }
  
  bzero ((char *) &(l->con.remote), sizeof(l->con.remote));
  
  l->con.clientsocket = socket(l->con.family, SOCK_STREAM, 0);
  if (l->con.clientsocket < 0)
    {
      l_closeconn(l,"Socket Error",errno);
      return;
    }
  
  if (l->con.family == AF_INET )
    {
      addrlen = sizeof(struct sockaddr_in);
      l->con.remote.sin.sin_family = AF_INET;
      l->con.remote.sin.sin_port = htons(l->con.remoteport);
      l->con.remote.sin.sin_addr.s_addr = htonl(atoul(l->con.remoteaddr));
    }
  else
    {
      addrlen = sizeof(struct sockaddr_in6);
      l->con.remote.sin6.sin6_family = AF_INET6;
      l->con.remote.sin6.sin6_port = htons(l->con.remoteport);
      retval = inet_pton(AF_INET6, l->con.remoteaddr, &(l->con.remote.sin6.sin6_addr));
      if (retval < 0)
        outerror(OUTERROR_TYPE_WARN_LOUD, "Invalid IP: %s", l->con.remoteaddr);
    }
  
  if (bind_irc_vhost(l->con.family, l->con.clientsocket) != 0)
    {
      l_closeconn(l, "Couldn't Bind Virtual Host, Sorry", errno);
      return;
    }
  
  ir_setsockopt(l->con.clientsocket);
  
  alarm(CTIMEOUT);
  retval = connect(l->con.clientsocket, &(l->con.remote.sa), addrlen);
  if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) )
    {
      l_closeconn(l,"Couldn't Connect",errno);
      alarm(0);
      return;
    }
  alarm(0);
  
  addrlen = sizeof(l->con.remote);
  if (getsockname(l->con.clientsocket, &(l->con.remote.sa), &addrlen) < 0)
    {
      l_closeconn(l,"Couldn't getsockname",errno);
      return;
    }
  
  l->ul_status = UPLOAD_STATUS_CONNECTING;
  notice(l->nick,"DCC Send Accepted, Connecting...");
  
  return;
}


void l_transfersome (upload * const l) {
   int i, howmuch, howmuch2;
   unsigned int j;
   unsigned long g;
   off_t mysize;
   
   updatecontext();

   howmuch = BUFFERSIZE;
   howmuch2 = 1;
   for (i=0; i<MAXTXPERLOOP; i++) {
      if ((howmuch == BUFFERSIZE && howmuch2 > 0) && is_fd_readable(l->con.clientsocket)) {
         
         l->overlimit = 0;
         if ( gdata.max_upspeed )
           {
             j = gdata.xdccrecv[(gdata.curtime)%XDCC_SENT_SIZE]
               + gdata.xdccrecv[(gdata.curtime-1)%XDCC_SENT_SIZE]
               + gdata.xdccrecv[(gdata.curtime-2)%XDCC_SENT_SIZE]
               + gdata.xdccrecv[(gdata.curtime-3)%XDCC_SENT_SIZE];
             if ( j >= gdata.max_upspeed*1024 )
               {
                 l->overlimit = 1;
                 return;
               }
           }
         
         /* read more later */
         if ( time(NULL) > gdata.curtime + 30 )
           return;
         
         howmuch  = recv(l->con.clientsocket, gdata.sendbuff, BUFFERSIZE, MSG_NOSIGNAL);
         if (howmuch < 0)
           {
             if (errno == EAGAIN)
               return;
             l_closeconn(l,"Connection Lost",errno);
             return;
           }
         else if (howmuch < 1)
           {
             l_closeconn(l,"Connection Lost",0);
             return;
           }
         
         howmuch2 = write(l->filedescriptor, gdata.sendbuff, howmuch);
         if (howmuch2 != howmuch)
           {
             l_closeconn(l,"Unable to write data to file",errno);
             return;
           }
         
         if (howmuch > 0) l->con.lastcontact = gdata.curtime;
         
         l->bytesgot += howmuch2;
         gdata.xdccrecv[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
         if (gdata.ignoreuploadbandwidth == 0)
           {
             gdata.xdccsent[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
             gdata.xdccsum[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
           }
         
         if (gdata.debug > 50) {
            ioutput(OUT_S, COLOR_BLUE, "Read %d File %d", howmuch, howmuch2);
            }
         
         }
      
      }
   
   /* don't block if max window is reached */
   if (is_fd_writeable(l->con.clientsocket))
     {
       if (l->mirc_dcc64)
         {
           g = htonl((ir_uint32)((l->bytesgot >> 32) & 0xFFFFFFFFL));
           send(l->con.clientsocket, (unsigned char*)&g, 4, MSG_NOSIGNAL);
           l->bytessent += 4;
         }
       g = htonl((ir_uint32)(l->bytesgot & 0xFFFFFFFF));
       send(l->con.clientsocket, (unsigned char*)&g, 4, MSG_NOSIGNAL);
       l->bytessent += 4;
     }
   
   if (l->bytesgot >= l->totalsize) {
      long timetook;
      char *tempstr;
      l->ul_status = UPLOAD_STATUS_WAITING;
      
      timetook = gdata.curtime - l->con.connecttime - 1;
      if (timetook < 1)
         timetook = 1;
      
      if (l->resumesize)
         mysize = l->bytesgot - l->resumesize;
      else
         mysize = l->totalsize;
      
      if (mysize <= 0)
         mysize = 1;
         
      tempstr = mymalloc(maxtextlength);
      tempstr[0] = 0;
      
      if (timetook > (60*60))
        {
          snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
                   " %li hr", timetook/60/60);
        }
      
      if ((timetook%(60*60)) > 60)
        {
          snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
                   " %li min", (timetook%(60*60))/60);
        }
      
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr),
               " %li sec", timetook%60);
      
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "DCC Upload: Transfer Completed (%" LLPRINTFMT "d kB,%s, %0.1f kB/sec)",
              mysize/1024,
              tempstr,
              ((float)mysize)/1024.0/((float)timetook));
      
      notice(l->nick, "** Upload Completed (%li kB,%s, %0.1f kB/sec)",
             (long)((mysize)/1024),
             tempstr,
             ((float)mysize)/1024.0/((float)timetook));
      
      mydelete(tempstr);
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
              "Upload: Nick %s" ", Network %s" ", Recv %" LLPRINTFMT "d kB" ", Sent %" LLPRINTFMT "d kB" ", File %s",
              l->nick,
              gdata.networks[ l->net ].name,
              mysize/1024, l->bytessent/1024,
              l->file);
   }
   
}


void l_istimeout (upload * const l)
{
  updatecontext();
  
  if ((l->ul_status == UPLOAD_STATUS_WAITING) && (gdata.curtime - l->con.lastcontact > 1))
    {
      if (gdata.debug > 0)
        {
          ioutput(OUT_S, COLOR_YELLOW, "clientsock = %d", l->con.clientsocket);
        }
      shutdown_close(l->con.clientsocket);
      close(l->filedescriptor);
      l->ul_status = UPLOAD_STATUS_DONE;
#ifdef USE_RUBY
      do_myruby_upload_done( l->file );
#endif /* USE_RUBY */
    }
  
  if ((gdata.curtime - l->con.lastcontact) > 180)
    {
      l_closeconn(l,"DCC Timeout (180 Sec Timeout)",0);
    }
}


void l_closeconn(upload * const l, const char *msg, int errno1)
{
  gnetwork_t *backup;
  
  updatecontext();
  
  if (errno1)
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "Upload: Connection closed: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
              "Upload: Connection closed: %s", msg);
    }
  
  if (gdata.debug > 0)
    {
      ioutput(OUT_S, COLOR_YELLOW, "clientsock = %d", l->con.clientsocket);
    }
  
  if (l->con.listensocket != FD_UNUSED && l->con.listensocket > 2)
    {
      event_close(l->con.listensocket);
    }
  
  if (l->con.clientsocket != FD_UNUSED && l->con.clientsocket > 2)
    {
      shutdown_close(l->con.clientsocket);
    }
  
  if (l->filedescriptor != FD_UNUSED && l->filedescriptor > 2)
    {
      close(l->filedescriptor);
    }
  
  if (l->ul_status == UPLOAD_STATUS_LISTENING)
    ir_listen_port_connected(l->con.localport);

#ifdef USE_UPNP
  if (gdata.upnp_router && (l->con.family == AF_INET))
    upnp_rem_redir(l->con.localport);
#endif /* USE_UPNP */
  l->ul_status = UPLOAD_STATUS_DONE;
  
  backup = gnetwork;
  gnetwork = &(gdata.networks[l->net]);
  if (errno1)
    {
      notice(l->nick, "** Closing Upload Connection: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      notice(l->nick, "** Closing Upload Connection: %s", msg);
    }
  gnetwork = backup;
}



/* End of File */
