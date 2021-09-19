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
 * SPDX-FileCopyrightText: 1998-2005 David Johnson
 * SPDX-FileCopyrightText: 2004-2021 Dirk Meyer
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * @(#) iroffer_dccchat.c 1.77@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_dccchat.c|20050313183434|02319
 *
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_jobs.h"
#include "dinoex_chat.h"
#include "dinoex_badip.h"
#include "dinoex_ssl.h"

void parsedccchat(dccchat_t *chat,
                  char* line)
{
  char *linec;
  userinput ui;
  unsigned int count;
  unsigned int found;
  
  updatecontext();
  
  linec = mystrdup(line);
  caps(linec);
  
  chat->con.lastcontact = gdata.curtime;
  
  switch (chat->status)
    {
    case DCCCHAT_AUTHENTICATING:
      found = dcc_host_password(chat, line);
      if (found == 1)
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT Correct password");
          
          chat->status = DCCCHAT_CONNECTED;
          
          writedccchat(chat,0," \n");
          writedccchat(chat,0,"Entering DCC Chat Admin Interface\n");
          writedccchat(chat,0,"For Help type \"help\"\n");
          
          if (chat->level >= ADMIN_LEVEL_FULL)
            {
          count = irlist_size(&gdata.msglog);
          writedccchat(chat, 0, "** You have %u %s in the message log%s\n",
                       count,
                       count != 1 ? "messages" : "message",
                       count ? ", use MSGREAD to read them" : "");
          writedccchat(chat,0," \n");
            }
        }
      else
        {
          ioutput(OUT_S|OUT_L|OUT_D, COLOR_MAGENTA,
                  "DCC CHAT: Incorrect password");
          
          writedccchat(chat,0,"Incorrect Password\n");
          count_badip(&(chat->con.remote));
          chat_shutdown_ssl(chat, 1);
          /* caller deletes */
        }
      break;
      
    case DCCCHAT_CONNECTED:
      if (gdata.debug > 11)
        {
          ioutput(OUT_S|OUT_L, COLOR_CYAN, ">DCC>: %s", line);
        }
      u_fillwith_dcc(&ui,chat,line);
      u_parseit(&ui);
      break;
      
    case DCCCHAT_UNUSED:
    case DCCCHAT_LISTENING:
    case DCCCHAT_CONNECTING:
    case DCCCHAT_SSL_ACCEPT:
    case DCCCHAT_SSL_CONNECT:
    default:
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "Unexpected dccchat state %u", chat->status);
      break;
    }
  
  mydelete(linec);
}

void writedccchat(dccchat_t *chat, int add_return, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   vwritedccchat(chat, add_return, format, args);
   va_end(args);
}

void vwritedccchat(dccchat_t *chat, int add_return, const char *format, va_list ap)
{
  char tempstr[maxtextlength];
  int len;
  
  updatecontext();
  
  if ((chat->status != DCCCHAT_AUTHENTICATING) &&
      (chat->status != DCCCHAT_CONNECTED))
    {
      return;
    }
  
  len = vsnprintf(tempstr, maxtextlength, format, ap);
  
  if ((len < 0) || (len >= (int)maxtextlength))
    {
      outerror(OUTERROR_TYPE_WARN_LOUD | OUTERROR_TYPE_NOLOG,
               "DCCCHAT: Output too large, ignoring!");
      return;
    }
  
  ir_boutput_write(&chat->boutput, tempstr, len);
  
  if (add_return)
    {
      ir_boutput_write(&chat->boutput, "\n", 1);
    }
  
  if (gdata.debug > 11)
    {
      if (tempstr[len-1] == '\n')
        {
          tempstr[len-1] = '\0';
        }
      ioutput(OUT_S|OUT_L, COLOR_CYAN, "<DCC<: %s", tempstr);
    }
}

void flushdccchat(dccchat_t *chat)
{
  if ((chat->status == DCCCHAT_AUTHENTICATING) ||
      (chat->status == DCCCHAT_CONNECTED))
    {
      ir_boutput_attempt_flush(&chat->boutput);
    }
  return;
}

void writestatus(dccchat_t *chat) {
   char *tempstr;
   
   updatecontext();
   
   tempstr = mymalloc(maxtextlength);
   
   getstatusline(tempstr,maxtextlength);
   writedccchat(chat,0,"%s\n",tempstr);
   
   mydelete(tempstr);
   }

/* End of File */
