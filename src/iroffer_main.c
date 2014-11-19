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
 * @(#) iroffer_main.c 1.245@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_main.c|20050313225819|23935
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_http.h"
#include "dinoex_upload.h"
#include "dinoex_transfer.h"
#include "dinoex_ssl.h"
#include "dinoex_curl.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"
#include "dinoex_telnet.h"
#include "dinoex_badip.h"
#include "dinoex_jobs.h"
#include "dinoex_ruby.h"
#include "dinoex_main.h"
#include "dinoex_chat.h"
#include "dinoex_kqueue.h"
#include "dinoex_misc.h"

/* local functions */
static void mainloop(void);

/* main */
int
#ifdef __GNUC__
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ != 199901L
__attribute__ ((noreturn))
#endif
#endif
main(int argc, char *const *argv) {
   
   initvars();
#ifdef USE_KQUEUE
   ir_kqueue_init();
#endif /* USE_KQUEUE */
   
   command_options(argc, argv);
   
#if defined(_OS_CYGWIN)
   gdata.noscreen = 1;
#endif
   
   startupiroffer();
   
   while ( 1 )
     {
       mainloop();
     }
   }

static void select_dump(const char *desc, int highests)
{
  char buffer[maxtextlength];
  size_t len;
  int ii;
  
  len = 0;
  len += add_snprintf(buffer + len, maxtextlength - len, "select %s: [read", desc);
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.readset))
        {
          len += add_snprintf(buffer + len, maxtextlength - len, " %d", ii);
        }
    }
  len += add_snprintf(buffer + len, maxtextlength - len, "] [write");
  for (ii=0; ii<highests+1; ii++)
    {
      if (FD_ISSET(ii, &gdata.writeset))
        {
          len += add_snprintf(buffer + len, maxtextlength - len, " %d", ii);
        }
    }
  len += add_snprintf(buffer + len, maxtextlength - len, "]");
  ioutput(OUT_S, COLOR_CYAN, "%s", buffer);
  
}

static void mainloop (void) {
   /* data is persistant across calls */
   static struct timeval timestruct;
   static int changequartersec, changesec, changemin, changehour;
   static time_t lasttime, lastmin, lasthour, last4sec, last5sec, last20sec;
   static time_t lastautoadd;
   static time_t last3min, last2min, lastignoredec;
   static int first_loop = 1;
   static ir_uint64 last250ms;

   userinput *pubplist;
   userinput *urehash;
   ir_uint64 xdccsent;
   unsigned int i;
   int highests;
   unsigned int ss;
   upload *ul;
   transfer *tr;
   channel_t *ch;
   xdcc *xd;
   dccchat_t *chat;
   
   updatecontext();
   gnetwork = NULL;
   
   if (first_loop)
     {
       /* init if first time called */
       FD_ZERO(&gdata.readset);
       FD_ZERO(&gdata.writeset);
       changehour=changemin=changesec=changequartersec=0;
       gettimeofday(&timestruct, NULL);
       last250ms = gdata.curtimems;
       gdata.curtimems = timeval_to_ms(&timestruct);
       ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, "Startup"
              " running: %ld ms", (long)(gdata.curtimems - last250ms));
       gdata.curtime = timestruct.tv_sec;
       lasttime=gdata.curtime;
       last250ms = gdata.curtimems;
       lastmin=(lasttime/60)-1;
       lasthour=(lasttime/60/60)-1;
       last4sec = last5sec = last20sec = last2min = last3min = lasttime;
       lastignoredec = lasttime;
       for (ss=0; ss<gdata.networks_online; ss++)
         {
           gdata.networks[ss].lastnotify = lasttime;
           gdata.networks[ss].lastslow = lasttime;
           gdata.networks[ss].server_input_line[0] = '\0';
         }
       
       gdata.cursendptr = 0;
       lastautoadd = gdata.curtime + 60;
       
       first_loop = 0;
     }
   
      updatecontext();
   
      FD_ZERO(&gdata.readset);
      FD_ZERO(&gdata.writeset);
      FD_ZERO(&gdata.execset);
      highests = 0;
#ifdef USE_CURL
      fetch_multi_fdset(&gdata.readset, &gdata.writeset, &gdata.execset, &highests);
#endif /* USE_CURL */
      
      highests = irc_select(highests);
      
      if (!gdata.background)
        {
          FD_SET(fileno(stdin), &gdata.readset);
          highests = max2(highests, fileno(stdin));
        }
      
      highests = chat_select_fdset(highests);
      highests = t_select_fdset(highests, changequartersec);
      highests = l_select_fdset(highests, changequartersec);
#ifndef WITHOUT_TELNET
      highests = telnet_select_fdset(highests);
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
      highests = h_select_fdset(highests, changequartersec);
#endif /* WITHOUT_HTTP */
      
      if (gdata.md5build.file_fd != FD_UNUSED)
        {
          assert(gdata.md5build.xpack);
          FD_SET(gdata.md5build.file_fd, &gdata.readset);
          highests = max2(highests, gdata.md5build.file_fd);
        }
      
      updatecontext();
   
      if (gdata.debug > 81)
        {
          select_dump("try", highests);
        }
      
      if (gdata.attop) gotobot();
      
      tostdout_write();
      
      gettimeofday(&timestruct, NULL);
      gdata.selecttimems = timeval_to_ms(&timestruct);
      if (ir_kqueue_select(highests+1, &gdata.readset, &gdata.writeset, &gdata.execset) < 0)
        {
          if (errno != EINTR)
            {
              outerror(OUTERROR_TYPE_WARN,"Select returned an error: %s",strerror(errno));
              usleep(10000); /* prevent fast spinning */
            }
          
          /* data is undefined on error, zero and continue */
          FD_ZERO(&gdata.readset);
          FD_ZERO(&gdata.writeset);
          FD_ZERO(&gdata.execset);
        }
      
      if (gdata.debug > 81)
        {
          select_dump("got", highests);
        }
      
      /*----- one second check ----- */
      
      updatecontext();
      
      if (gettimeofday(&timestruct, NULL) < 0)
        {
          outerror(OUTERROR_TYPE_CRASH,"gettimeofday() failed! %s\n",strerror(errno));
        }
      
      gdata.curtimems = timeval_to_ms(&timestruct);
      gdata.curtime = timestruct.tv_sec;
      if (gdata.curtimems > gdata.selecttimems + 1000)
        outerror(OUTERROR_TYPE_WARN, "Iroffer was blocked for %lims",
                 (long)(gdata.curtimems - gdata.selecttimems));
      
      /* adjust for drift and cpu usage */
      if ((gdata.curtimems > (last250ms+1000)) ||
          (gdata.curtimems < last250ms))
        {
          /* skipped forward or backwards, correct */
          last250ms = gdata.curtimems-250;
        }
      
      if (gdata.curtimems >= (last250ms+250))
        {
          changequartersec = 1;
          /* note bandwidth limiting requires no drift! */
          last250ms += 250;
        }
      else
        {
          changequartersec = 0;
        }
      
      changesec = 0;
      if (gdata.curtime != lasttime) {
         
         if (gdata.curtime < lasttime - MAX_WAKEUP_WARN) {
            outerror(OUTERROR_TYPE_WARN, "System Time Changed Backwards %lim %lis!!\n",
                     (long)(lasttime-gdata.curtime)/60, (long)(lasttime-gdata.curtime)%60);
            }
         
         if (gdata.curtime > lasttime + MAX_WAKEUP_WARN) {
            outerror(OUTERROR_TYPE_WARN, "System Time Changed Forward or Mainloop Skipped %lim %lis!!\n",
                     (long)(gdata.curtime-lasttime)/60, (long)(gdata.curtime-lasttime)%60);
              if (gdata.debug > 0) {
                dump_slow_context();
                }
            }

         if (gdata.curtime > lasttime + MAX_WAKEUP_ERR) {
            outerror(OUTERROR_TYPE_WARN, "System Time Changed Forward or Mainloop Skipped %lim %lis!!\n",
               (long)(gdata.curtime-lasttime)/60, (long)(gdata.curtime-lasttime)%60);
            if (gdata.debug > 0)
              {
                dumpcontext();
              }
            }
         
         lasttime = gdata.curtime;
         changesec = 1;
         
         }
      
      if (changesec && lasttime/60/60 != lasthour) {
         lasthour = lasttime/60/60;
         changehour = 1;
         }
      
      if (changesec && lasttime/60 != lastmin) {
         lastmin = lasttime/60;
         changemin = 1;
         }
      
      if (gdata.needsshutdown)
        {
          gdata.needsshutdown = 0;
          shutdowniroffer();
        }
      
      if (gdata.needsreap)
        {
          gdata.needsreap = 0;
          irc_resolved();
        }
      
#ifdef USE_CURL
      fetch_perform();
#endif /* USE_CURL */
      
      updatecontext();
      
      if (changesec) {
         gdata.totaluptime++;
         gdata.xdccsent[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;
         gdata.xdccrecv[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;
         
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsum[i];
         if (((float)xdccsent)/XDCC_SENT_SIZE/1024.0 > gdata.sentrecord)
            gdata.sentrecord = ((float)xdccsent)/XDCC_SENT_SIZE/1024.0;
         gdata.xdccsum[(gdata.curtime+1)%XDCC_SENT_SIZE] = 0;

         run_delayed_jobs();
      }

      updatecontext();
      
      /*----- see if anything waiting on console ----- */
      gdata.needsclear = 0;
      if (!gdata.background && FD_ISSET(fileno(stdin), &gdata.readset))
         parseconsole();
      
      irc_perform(changesec);
      l_perform(changesec);
      chat_perform();
      t_perform(changesec, changequartersec);
#ifndef WITHOUT_TELNET
      telnet_perform();
#endif /* WITHOUT_TELNET */
#ifndef WITHOUT_HTTP
      h_perform(changesec, changequartersec);
#endif /* WITHOUT_HTTP */
      
      /*----- time for a delayed shutdown? ----- */
      if (changesec && gdata.delayedshutdown)
        {
          if (!irlist_size(&gdata.trans))
            {
              ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                      "Delayed Shutdown Activated, No Transfers Remaining");
              shutdowniroffer();
            }
        }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
        /*----- send server stuff ----- */
        if (changesec) {
           sendserver();
           if (gdata.curtime%INAMNT_SIZE == (INAMNT_SIZE-1))
              gnetwork->inamnt[0] = 0;
           else
              gnetwork->inamnt[gdata.curtime%INAMNT_SIZE+1] = 0;
           }
        /*----- see if we can send out some xdcc lists */
        if (changesec && gnetwork->serverstatus == SERVERSTATUS_CONNECTED) {
          if (!irlist_size((&gnetwork->serverq_normal)) && !irlist_size(&(gnetwork->serverq_slow)))
            sendxdlqueue();
          }
        }
      gnetwork = NULL;
      
      /*----- see if its time to change maxb */
      if (changehour) {
         gdata.maxb = gdata.overallmaxspeed;
         if (gdata.overallmaxspeeddayspeed != gdata.overallmaxspeed) {
            struct tm *localt;
            localt = localtime(&gdata.curtime);

            if ((unsigned int)localt->tm_hour >= gdata.overallmaxspeeddaytimestart
                && (unsigned int)localt->tm_hour < gdata.overallmaxspeeddaytimeend
                && ( gdata.overallmaxspeeddaydays & (1 << (unsigned int)localt->tm_wday)) )
               gdata.maxb = gdata.overallmaxspeeddayspeed;
            }
         isrotatelog();
         expire_options();
         }
      
      /*----- see if we've hit a transferlimit or need to reset counters */
      if (changesec)
        {
          unsigned int ii;
          unsigned int transferlimits_over = 0;
          for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
            {
              /* reset counters? */
              if ((!gdata.transferlimits[ii].ends) ||
                  (gdata.transferlimits[ii].ends < gdata.curtime))
                {
                  struct tm *localt;
                  if (gdata.transferlimits[ii].limit && gdata.transferlimits[ii].ends)
                    {
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "Resetting %s transfer limit, used %" LLPRINTFMT "uMB of the %" LLPRINTFMT "uMB limit",
                              transferlimit_type_to_string(ii),
                              gdata.transferlimits[ii].used / 1024 / 1024,
                              gdata.transferlimits[ii].limit / 1024 / 1024);
                    }
                  
                  /* find our next end time */
                  localt = localtime(&gdata.curtime);
                  localt->tm_sec = localt->tm_min = localt->tm_hour = 0; /* midnight */
                  localt->tm_isdst = -1; /* check for daylight saving time */
                  switch (ii)
                    {
                    case TRANSFERLIMIT_DAILY:
                      /* tomorrow */
                      localt->tm_mday++;
                      break;
                      
                    case TRANSFERLIMIT_WEEKLY:
                      /* next sunday morning */
                      localt->tm_mday += 7 - localt->tm_wday;
                      break;
                      
                    case TRANSFERLIMIT_MONTHLY:
                      /* next month */
                      localt->tm_mday = gdata.start_of_month;
                      localt->tm_mon++;
                      break;
                      
                    default:
                      outerror(OUTERROR_TYPE_CRASH, "unknown type %u", ii);
                    }
                  /* tm_wday and tm_yday are ignored in mktime() */
                  gdata.transferlimits[ii].ends = mktime(localt);
                  gdata.transferlimits[ii].used = 0;
                  if ( ii == TRANSFERLIMIT_DAILY )
                    reset_download_limits();
                }
              
              if (!transferlimits_over &&
                  gdata.transferlimits[ii].limit &&
                  (gdata.transferlimits[ii].used >= gdata.transferlimits[ii].limit))
                {
                  transferlimits_over = 1 + ii;
                  
                  if (!gdata.transferlimits_over)
                    {
                      char *tempstr = transfer_limit_exceeded_msg(ii);
                      
                      ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                              "All %" LLPRINTFMT "uMB of the %s transfer limit used. Stopping transfers.",
                              gdata.transferlimits[ii].limit / 1024 / 1024,
                              transferlimit_type_to_string(ii));
                      
                      /* remove queued users */
                      queue_all_remove(&gdata.mainqueue, tempstr);
                      queue_all_remove(&gdata.idlequeue, tempstr);
                      
                      /* stop transfers */
                      for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr))
                        {
                          if (tr->tr_status != TRANSFER_STATUS_DONE)
                            {
                              gnetwork = &(gdata.networks[tr->net]);
                              t_closeconn(tr,tempstr,0);
                            }
                        }
                      
                      gnetwork = NULL;
                      mydelete(tempstr);
                    }
                }
            }
          
          if (gdata.transferlimits_over != transferlimits_over)
            {
              if (!transferlimits_over)
                {
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "No longer over any transfer limits. Transfers are now allowed.");
                }
              gdata.transferlimits_over = transferlimits_over;
            }
        }
      
      /*----- gdata.autoignore_threshold seconds ----- */
      if (changesec && ((unsigned)gdata.curtime > (lastignoredec + gdata.autoignore_threshold)))
        {
          igninfo *ignore;
          
          lastignoredec += gdata.autoignore_threshold;
          
          ignore = irlist_get_head(&gdata.ignorelist);
          
          while(ignore)
            {
              ignore->bucket--;
              if ((ignore->flags & IGN_IGNORING) && (ignore->bucket == 0))
                {
                  ignore->flags &= ~IGN_IGNORING;
                  ioutput(OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR,
                          "Ignore removed for %s",ignore->hostmask);
                  write_statefile();
                }
              if (ignore->bucket == 0)
                {
                  mydelete(ignore->hostmask);
                  ignore = irlist_delete(&gdata.ignorelist, ignore);
                }
              else
                {
                  ignore = irlist_get_next(ignore);
                }
            }
        }
      
      /*----- periodicmsg_time seconds ----- */
      if (changesec) {
         send_periodicmsg();
         }
      
      updatecontext();
      
      /*----- 5 seconds ----- */
      if (changesec && (gdata.curtime - last5sec > 4)) {
         last5sec = gdata.curtime;
         
         updatecontext();
         /*----- server timeout ----- */
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
           if (gdata.needsshutdown)
             continue;
         if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
             (gdata.curtime > gnetwork->lastservercontact + SRVRTOUT)) {
            if (gnetwork->servertime < 3)
              {
                const char *servname = gnetwork->curserveractualname ? gnetwork->curserveractualname : gnetwork->curserver.hostname;
                size_t     len       = 6 + strlen(servname);
                char       *tempstr3 = mymalloc(len + 1);
                snprintf(tempstr3, len + 1, "PING %s\n", servname);
                writeserver_ssl(tempstr3, len);
                if (gdata.debug > 0)
                  {
                    tempstr3[len-1] = '\0';
                    len--;
                    ioutput(OUT_S|OUT_L, COLOR_MAGENTA, "<NORES<: %s", tempstr3);
                  }
                mydelete(tempstr3);
                gnetwork->servertime++;
              }
            else if (gnetwork->servertime == 3) {
               ioutput(OUT_S|OUT_L|OUT_D, COLOR_RED,
                       "Closing Server Connection on %s: No Response for %u minutes.",
                       gnetwork->name, SRVRTOUT/60);
               close_server();
               gnetwork->servertime = 0;
               }
            }
         
         
         /*----- ping server ----- */
         if (gnetwork->recentsent) {
            pingserver();
            gnetwork->recentsent--;
            }
         
         }
         } /* networks */
      gnetwork = NULL;
      
      /*----- 4 seconds ----- */
      if (changesec && (gdata.curtime - last4sec > 3))
        {
         
         /*----- update lastspeed, check minspeed ----- */
          tr = irlist_get_head(&gdata.trans);
          while(tr)
            {
              if ( tr->con.connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
                {
                  tr->lastspeed = 
                    (tr->lastspeed)*DCL_SPDW_I + 
                    (((float)(tr->bytessent-tr->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_I)/((float)(gdata.curtime-last4sec)*1.0);
                }
              else                                              /* ongoing */
                {
                  tr->lastspeed = 
                    (tr->lastspeed)*DCL_SPDW_O + 
                    (((float)(tr->bytessent-tr->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_O)/((float)(gdata.curtime-last4sec)*1.0);
                  }
              
              tr->lastspeedamt = tr->bytessent;
              
              t_checkminspeed(tr);
              
              tr = irlist_get_next(tr);
            }
         
          ul = irlist_get_head(&gdata.uploads);
          while(ul)
            {
              if ( ul->con.connecttime+(MIN_TL/2) > gdata.curtime ) /* initial */
                {
                  ul->lastspeed = 
                    (ul->lastspeed)*DCL_SPDW_I + 
                    (((float)(ul->bytesgot-ul->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_I)/((float)(gdata.curtime-last4sec)*1.0);
                }
              else                                                /* ongoing */
                {
                  ul->lastspeed = 
                    (ul->lastspeed)*DCL_SPDW_O + 
                    (((float)(ul->bytesgot-ul->lastspeedamt))/1024.0)*(1.0-DCL_SPDW_O)/((float)(gdata.curtime-last4sec)*1.0);
                }
              ul->lastspeedamt = ul->bytesgot;
              
              ul = irlist_get_next(ul);
            }
          
          last4sec = gdata.curtime;
        }

      updatecontext();
      /*----- check for size change ----- */
      if (changesec)
         checktermsize();
      
      updatecontext();
      
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      /*----- plist stuff ----- */
      if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
          changemin &&
          irlist_size(&gdata.xdccs) &&
          !gdata.transferlimits_over &&
          (irlist_size(&(gnetwork->serverq_channel)) < irlist_size(&gdata.xdccs)) &&
          (!gdata.queuesize || irlist_size(&gdata.mainqueue) < gdata.queuesize) &&
          (gdata.nolisting <= gdata.curtime))
        {
            char *tchanf = NULL, *tchanm = NULL, *tchans = NULL;
            
            for(ch = irlist_get_head(&(gnetwork->channels));
                ch;
                ch = irlist_get_next(ch))
              {
                if ((ch->flags & CHAN_ONCHAN) &&
                    (ch->nextann < gdata.curtime) &&
                    ch->plisttime &&
                    (((gdata.curtime / 60) % ch->plisttime) == ch->plistoffset))
                  {
                    ch->nextmsg = gdata.curtime + ch->delay;
                    if (ch->pgroup != NULL)
                      {
                        ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (pgroup)", ch->name);
                        pubplist = mycalloc(sizeof(userinput));
                        pubplist->method = method_xdl_channel;
                        pubplist->net = gnetwork->net;
                        pubplist->level = ADMIN_LEVEL_PUBLIC;
                        a_fillwith_plist(pubplist, ch->name, ch);
                        u_parseit(pubplist);
                        mydelete(pubplist);
                        continue;
                      }
                    if (ch->flags & CHAN_MINIMAL)
                      {
                        if (tchanm)
                          {
                            strncat(tchanm,",",maxtextlength-strlen(tchanm)-1);
                            strncat(tchanm,ch->name,maxtextlength-strlen(tchanm)-1);
                          }
                        else
                          {
                            tchanm = mymalloc(maxtextlength);
                            strncpy(tchanm,ch->name,maxtextlength-1);
                          }
                      }
                    else if (ch->flags & CHAN_SUMMARY)
                      {
                        if (tchans)
                          {
                            strncat(tchans,",",maxtextlength-strlen(tchans)-1);
                            strncat(tchans,ch->name,maxtextlength-strlen(tchans)-1);
                          }
                        else
                          {
                            tchans = mymalloc(maxtextlength);
                            strncpy(tchans,ch->name,maxtextlength-1);
                          }
                      }
                    else
                      {
                        if (tchanf)
                          {
                            strncat(tchanf,",",maxtextlength-strlen(tchanf)-1);
                            strncat(tchanf,ch->name,maxtextlength-strlen(tchanf)-1);
                          }
                        else
                          {
                            tchanf = mymalloc(maxtextlength);
                            strncpy(tchanf,ch->name,maxtextlength-1);
                          }
                      }
                  }
              }
            
            if (tchans)
              {
                if (gdata.restrictprivlist && !gdata.creditline && !irlist_size(&gdata.headline))
                  {
                    ioutput(OUT_S|OUT_D, COLOR_NO_COLOR,
                            "Can't send Summary Plist to %s (restrictprivlist is set and no creditline or headline, summary makes no sense!)", tchans);
                  }
                else
                  {
                    ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (summary)", tchans);
                    pubplist = mycalloc(sizeof(userinput));
                    a_fillwith_msg2(pubplist, tchans, "XDL");
                    pubplist->method = method_xdl_channel_sum;
                    u_parseit(pubplist);
                    mydelete(pubplist);
                  }
                mydelete(tchans);
              }
            if (tchanf) {
               ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (full)", tchanf);
               pubplist = mycalloc(sizeof(userinput));
               a_fillwith_plist(pubplist, tchanf, NULL);
               pubplist->method = method_xdl_channel;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanf);
               }
            if (tchanm) {
               ioutput(OUT_S|OUT_D, COLOR_NO_COLOR, "Plist sent to %s (minimal)", tchanm);
               pubplist = mycalloc(sizeof(userinput));
               a_fillwith_msg2(pubplist, tchanm, "XDL");
               pubplist->method = method_xdl_channel_min;
               u_parseit(pubplist);
               mydelete(pubplist);
               mydelete(tchanm);
               }
            
         }
         } /* networks */
      gnetwork = NULL;
      
      updatecontext();
      /*----- low bandwidth send, save state file ----- */
      if (changesec && (gdata.curtime - last3min > 180)) {
         last3min = gdata.curtime;
         
         xdccsent = 0;
         for (i=0; i<XDCC_SENT_SIZE; i++)
            xdccsent += (ir_uint64)gdata.xdccsent[i];
         xdccsent /= XDCC_SENT_SIZE*1024;
         
         if ((xdccsent < (unsigned)gdata.lowbdwth)) {
           if ( check_main_queue( gdata.maxtrans ) ) {
               send_from_queue(1, 0, NULL);
             }
           }
         else {
           start_one_send();
           }
         write_files();
         }
      
      updatecontext();
      for (ss=0; ss<gdata.networks_online; ss++) {
        gnetwork = &(gdata.networks[ss]);
      /*----- queue notify ----- */
      if (changesec && gdata.notifytime && (!gdata.quietmode) &&
          ((unsigned)gdata.curtime > (gnetwork->lastnotify + (gdata.notifytime*60))))
        {
         gnetwork->lastnotify = gdata.curtime;
         
         if (gnetwork->serverstatus == SERVERSTATUS_CONNECTED)
           {
             if ((irlist_size(&(gnetwork->serverq_fast)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_normal)) >= 10) ||
                 (irlist_size(&(gnetwork->serverq_slow)) >= 50))
               {
                 ioutput(OUT_S|OUT_D|OUT_L, COLOR_NO_COLOR,
                         "notifications skipped on %s, server queue is rather large",
                         gnetwork->name);
               }
             else
               {
                 notifyqueued();
                 notifybandwidth();
                 notifybandwidthtrans();
               }
           }
         
         }
         } /* networks */
      gnetwork = NULL;
      
      updatecontext();
      /*----- log stats / remote admin stats ----- */
      if ( changesec &&
           ((unsigned)gdata.curtime >= (last2min + gdata.status_time_dcc_chat)))
        {
          last2min = gdata.curtime;
          if (gdata.logstats)
            {
          logstat();
          
               chat_writestatus();
            }
        }
      
      updatecontext();

      /* look to see if any files changed */
      if (changesec)
        look_for_file_remove();

      updatecontext();
      
      /*----- 20 seconds ----- */
      if (changesec && (gdata.curtime - last20sec > 19)) {
         expire_badip();
         
         if (gdata.logfd != FD_UNUSED)
           {
             /* cycle */
             close(gdata.logfd);
             gdata.logfd = FD_UNUSED;
           }
         
         updatecontext();
         
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
         /* try rejoining channels not on */
         ch = irlist_get_head(&(gnetwork->channels));
         while(ch)
           {
             if ((gnetwork->serverstatus == SERVERSTATUS_CONNECTED) &&
                 !(ch->flags & CHAN_ONCHAN))
               {
                 joinchannel(ch);
               }
             ch = irlist_get_next(ch);
           }
         } /* networks */
         gnetwork = NULL;
         
         last20sec = gdata.curtime;
         
         updatecontext();
         
         for (ss=0; ss<gdata.networks_online; ss++) {
           gnetwork = &(gdata.networks[ss]);
           /* try to regain nick */
           if (!gnetwork->user_nick || strcmp(get_config_nick(), gnetwork->user_nick))
             {
               writeserver(WRITESERVER_NORMAL, "NICK %s", get_config_nick());
             }
           } /* networks */
         gnetwork = NULL;
         
         updatecontext();
         
         /* update status line */
         if (!gdata.background && !gdata.noscreen) {
            char tempstr[maxtextlength];
            char tempstr2[maxtextlengthshort];
            
            if (gdata.attop) gotobot();
            
            tostdout(IRVT_SAVE_CURSOR);
            
            getstatusline(tempstr,maxtextlength);
            tempstr[min2(maxtextlength-2,gdata.termcols-4)] = '\0';
            snprintf(tempstr2, maxtextlengthshort, IRVT_CURSOR_HOME1 "[ %%-%us ]", gdata.termlines - 1, gdata.termcols - 4);
            tostdout(tempstr2,tempstr);
            
            tostdout(IRVT_CURSOR_HOME2 IRVT_UNSAVE_CURSOR, gdata.termlines, gdata.termcols);
            }
         
         admin_jobs();
#ifdef USE_RUBY
	 rehash_myruby(1);
#endif /* USE_RUBY */
         delayed_announce();
         }
      
      updatecontext();
      
      if (changemin)
        {
          reverify_restrictsend();
          update_hour_dinoex(lastmin);
          check_idle_queue(0);
          clean_uploadhost();
          auto_rehash();
        }

      updatecontext();
      
      if ((gdata.md5build.file_fd != FD_UNUSED) &&
          FD_ISSET(gdata.md5build.file_fd, &gdata.readset))
        {
          ssize_t howmuch;
#if defined(_OS_CYGWIN)
          int reads_per_loop = 32;
#else /* _OS_CYGWIN */
          int reads_per_loop = 64;
#endif /* _OS_CYGWIN */
          
          assert(gdata.md5build.xpack);
          
          while (reads_per_loop--)
            {
              howmuch = read(gdata.md5build.file_fd, gdata.sendbuff, BUFFERSIZE);
              
              if (gdata.debug >30)
                {
                  ioutput(OUT_S, COLOR_YELLOW, "MD5: [Pack %u] read %ld",
                          number_of_pack(gdata.md5build.xpack), (long)howmuch);
                }
              
              if ((howmuch < 0) && (errno != EAGAIN))
                {
                  outerror(OUTERROR_TYPE_WARN, "MD5: [Pack %u] Can't read data from file '%s': %s",
                           number_of_pack(gdata.md5build.xpack),
                           gdata.md5build.xpack->file, strerror(errno));
                  
                  event_close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  gdata.md5build.xpack = NULL;
                  break;
                }
              else if (howmuch < 0)
                {
                  break;
                }
              else if (howmuch == 0)
                {
                  /* EOF */
                  outerror(OUTERROR_TYPE_WARN, "MD5: [Pack %u] Can't read data from file '%s': %s",
                           number_of_pack(gdata.md5build.xpack),
                           gdata.md5build.xpack->file, "truncated");
                  event_close(gdata.md5build.file_fd);
                  gdata.md5build.file_fd = FD_UNUSED;
                  gdata.md5build.xpack = NULL;
                  break;
                }
              /* else got data */
              MD5Update(&gdata.md5build.md5sum, gdata.sendbuff, howmuch);
              if (!gdata.nocrc32)
                crc32_update((char *)gdata.sendbuff, howmuch);
              gdata.md5build.bytes += howmuch;
              if (gdata.md5build.bytes == gdata.md5build.xpack->st_size)
                {
                  complete_md5_hash();
                  break;
                }
            }
        }
      
      if (!gdata.nomd5sum && changesec && (!gdata.md5build.xpack))
        {
          unsigned int packnum = 1;
          /* see if any pack needs a md5sum calculated */
          if (gdata.nomd5_start <= gdata.curtime)
          for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd), packnum++)
            {
              if (!gdata.nocrc32)
                {
                  if (!xd->has_crc32)
                    xd->has_md5sum = 0; /* force recheck with crc */
                }
              if (!xd->has_md5sum)
                {
                  if (verifyshell(&gdata.md5sum_exclude, xd->file))
                    continue;
                  if (!gdata.attop) gototop();
                  start_md5_hash(xd, packnum);
                  break;
                }
            }
        }
      
      updatecontext();
      
      if (gdata.exiting && has_closed_servers()) {
         
         for (chat = irlist_get_head(&gdata.dccchats);
              chat;
              chat = irlist_delete(&gdata.dccchats,chat))
           {
             writedccchat(chat, 0, "iroffer exited, Closing DCC Chat\n");
             shutdowndccchat(chat,1);
           }
         
         mylog("iroffer exited\n\n");
         
         exit_iroffer(0);
         }
      
      updatecontext();
      
      if (gdata.needsrehash) {
         gdata.needsrehash = 0;
         urehash = mycalloc(sizeof(userinput));
         a_fillwith_msg2(urehash, NULL, "REHASH");
         urehash->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
         urehash->net = 0;
         urehash->level = ADMIN_LEVEL_FULL;
         u_parseit(urehash);
         mydelete(urehash);
         }
      
      updatecontext();
      
      chat = irlist_get_head(&gdata.dccchats);
      while (chat)
        {
          if (chat->status == DCCCHAT_UNUSED)
            {
              chat = irlist_delete(&gdata.dccchats,chat);
            }
          else
            {
              flushdccchat(chat);
              chat = irlist_get_next(chat);
            }
        }
      
      if (gdata.autoadd_time > 0)
        {
          if (changesec && ((unsigned)gdata.curtime > (lastautoadd + gdata.autoadd_time)))
            {
              lastautoadd = gdata.curtime;
              autoadd_all();
            }
        }
      
      /* END */
      updatecontext();
      if (gdata.needsclear) drawbot();
      
      changehour=changemin=0;
      
   }

/* End of File */
