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
 * @(#) iroffer_upload.c 1.49@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_upload.c|20050116225131|24495
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"


void l_initvalues (upload * const l) {
   updatecontext();

      l->ul_status = UPLOAD_STATUS_UNUSED;
      l->clientsocket=FD_UNUSED;
      l->filedescriptor=FD_UNUSED;
      l->lastcontact = gdata.curtime;
   }

void l_establishcon (upload * const l)
{
  struct sockaddr_in remoteaddr;
  struct sockaddr_in localaddr;
  SIGNEDSOCK int addrlen;
  int retval;
  char *fullfile;
  struct stat s;
  
  updatecontext();
  
  /* local file already exists? */
  fullfile = mycalloc(strlen(gdata.uploaddir) + strlen(l->file) + 2);
  sprintf(fullfile, "%s/%s", gdata.uploaddir, l->file);
  
  l->filedescriptor = open(fullfile,
                           O_WRONLY | O_CREAT | O_EXCL | ADDED_OPEN_FLAGS,
                           CREAT_PERMISSIONS );
  
  if ((l->filedescriptor < 0) && (errno == EEXIST))
    {
      retval = stat(fullfile, &s);
      if (retval < 0)
        {
          outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Stat Upload File '%s': %s",
                   fullfile,strerror(errno));
          l_closeconn(l,"File Error, File couldn't be opened for writing",errno);
          mydelete(fullfile);
          return;
        }
#if 1
      if (!S_ISREG(s.st_mode) || (s.st_size >= l->totalsize))
        {
          l_closeconn(l,"File Error, That filename already exists",0);
          mydelete(fullfile);
          return;
        }
      else
#endif
        {
          l->filedescriptor = open(fullfile, O_WRONLY | O_APPEND | ADDED_OPEN_FLAGS);
          
          if (l->filedescriptor >= 0)
            {
              l->resumesize = l->bytesgot = s.st_size;
              
              if (l->resumed <= 0)
                {
                  close(l->filedescriptor);
                  privmsg_fast(l->nick, "\1DCC RESUME %s %i %" LLPRINTFMT "u\1",
                          l->file, l->remoteport, (unsigned long long)s.st_size);
                  mydelete(fullfile);
                  return;
                }
            }
        }
    }
  
  if (l->filedescriptor < 0)
    {
      outerror(OUTERROR_TYPE_WARN_LOUD,"Cant Access Upload File '%s': %s",
               fullfile,strerror(errno));
      l_closeconn(l,"File Error, File couldn't be opened for writing",errno);
      mydelete(fullfile);
      return;
    }
  
  mydelete(fullfile);
  
  bzero ((char *) &remoteaddr, sizeof (remoteaddr));
  
  l->clientsocket = socket( AF_INET, SOCK_STREAM, 0);
  if (l->clientsocket < 0)
    {
      l_closeconn(l,"Socket Error",errno);
      return;
    }
  
  remoteaddr.sin_family = AF_INET;
  remoteaddr.sin_port = htons(l->remoteport);
  remoteaddr.sin_addr.s_addr = htonl(l->remoteip);
  
  if (gdata.local_vhost)
    {
      bzero((char*)&localaddr, sizeof(struct sockaddr_in));
      localaddr.sin_family = AF_INET;
      localaddr.sin_port = 0;
      localaddr.sin_addr.s_addr = htonl(gdata.local_vhost);
      
      if (bind(l->clientsocket, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0)
        {
          l_closeconn(l,"Couldn't Bind Virtual Host, Sorry",errno);
          return;
        }
    }
  
  if (set_socket_nonblocking(l->clientsocket,1) < 0 )
    {
      outerror(OUTERROR_TYPE_WARN,"Couldn't Set Non-Blocking");
    }
  
  alarm(CTIMEOUT);
  retval = connect(l->clientsocket, (struct sockaddr *) &remoteaddr, sizeof(remoteaddr));
  if ( (retval < 0) && !((errno == EINPROGRESS) || (errno == EAGAIN)) )
    {
      l_closeconn(l,"Couldn't Connect",errno);
      alarm(0);
      return;
    }
  alarm(0);
  
  addrlen = sizeof (remoteaddr);
  if (getsockname(l->clientsocket,(struct sockaddr *) &remoteaddr, &addrlen) < 0)
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
   unsigned long g;
   off_t mysize;
   
   updatecontext();

   howmuch = BUFFERSIZE;
   howmuch2 = 1;
   for (i=0; i<MAXTXPERLOOP; i++) {
      if ((howmuch == BUFFERSIZE && howmuch2 > 0) && is_fd_readable(l->clientsocket)) {
         
         howmuch  = read(l->clientsocket, gdata.sendbuff, BUFFERSIZE);
         if (howmuch < 0)
           {
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
         
         if (howmuch > 0) l->lastcontact = gdata.curtime;
         
         l->bytesgot += howmuch2;
         gdata.xdccsent[gdata.curtime%XDCC_SENT_SIZE] += howmuch2;
         
         if (gdata.debug > 4) {
            ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_BLUE,"Read %d File %d",howmuch,howmuch2);
            }
         
         }
      
      }
   
   g = htonl((unsigned long)l->bytesgot);
   write(l->clientsocket,(unsigned char*)&g,4);
   
   if (l->bytesgot >= l->totalsize) {
      long timetook;
      char *tempstr;
      l->ul_status = UPLOAD_STATUS_WAITING;
      
      timetook = gdata.curtime - l->connecttime - 1;
      if (timetook < 1)
         timetook = 1;
      
      if (l->resumesize)
         mysize = l->bytesgot - l->resumesize;
      else
         mysize = l->totalsize;
      
      if (mysize <= 0)
         mysize = 1;
         
      tempstr = mycalloc(maxtextlength);
      
      if (timetook > (60*60))
        {
          snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
                   " %li hr", timetook/60/60);
        }
      
      if ((timetook%(60*60)) > 60)
        {
          snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
                   " %li min", (timetook%(60*60))/60);
        }
      
      snprintf(tempstr+strlen(tempstr), maxtextlength-strlen(tempstr)-1,
               " %li sec", timetook%60);
      
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
              "DCC Upload: Transfer Completed (%" LLPRINTFMT "i KB,%s, %0.1f KB/sec)",
              (long long)((mysize)/1024),
              tempstr,
              ((float)mysize)/1024.0/((float)timetook));
      
      notice(l->nick,"** Upload Completed (%li KB,%s, %0.1f KB/sec)",
             (long)((mysize)/1024),
             tempstr,
             ((float)mysize)/1024.0/((float)timetook));
      
      mydelete(tempstr);
   }
   
}


void l_istimeout (upload * const l)
{
  updatecontext();
  
  if ((l->ul_status == UPLOAD_STATUS_WAITING) && (gdata.curtime - l->lastcontact > 1))
    {
      if (gdata.debug > 0)
        {
          ioutput(CALLTYPE_MULTI_FIRST,OUT_S,COLOR_YELLOW,"clientsock = %d",l->clientsocket);
        }
      FD_CLR(l->clientsocket, &gdata.readset);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(l->clientsocket, SHUT_RDWR);
      close(l->clientsocket);
      close(l->filedescriptor);
      l->ul_status = UPLOAD_STATUS_DONE;
    }
  
  if ((gdata.curtime - l->lastcontact) > 180)
    {
      if (!gdata.attop) gototop();
      l_closeconn(l,"DCC Timeout (180 Sec Timeout)",0);
    }
}


void l_closeconn(upload * const l, const char *msg, int errno1)
{
  updatecontext();
  
  if (errno1)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
              "Upload: Connection closed: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,
              "Upload: Connection closed: %s", msg);
    }
  
  if (gdata.debug > 0)
    {
      ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_YELLOW,"clientsock = %d",l->clientsocket);
    }
  
  if (l->clientsocket != FD_UNUSED && l->clientsocket > 2)
    {
      FD_CLR(l->clientsocket, &gdata.writeset);
      FD_CLR(l->clientsocket, &gdata.readset);
      /*
       * cygwin close() is broke, if outstanding data is present
       * it will block until the TCP connection is dead, sometimes
       * upto 10-20 minutes, calling shutdown() first seems to help
       */
      shutdown(l->clientsocket, SHUT_RDWR);
      close(l->clientsocket);
    }
  
  if (l->filedescriptor != FD_UNUSED && l->filedescriptor > 2)
    {
      close(l->filedescriptor);
    }
  
  l->ul_status = UPLOAD_STATUS_DONE;
  
  if (errno1)
    {
      notice(l->nick, "** Closing Upload Connection: %s (%s)", msg, strerror(errno1));
    }
  else
    {
      notice(l->nick, "** Closing Upload Connection: %s", msg);
    }
}



/* End of File */
