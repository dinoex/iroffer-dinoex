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
 * @(#) iroffer_admin.c 1.215@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_admin.c|20051123201143|48668
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "iroffer_dinoex.h"

#ifdef USE_CURL
#include <curl/curl.h>
#endif /* USE_CURL */

/* local functions */
static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
u_respond(const userinput * const u, const char *format, ...);

static void u_help(const userinput * const u);
static int u_xdl_space(void);
static void u_xdl_pack(const userinput * const u, char *tempstr, int i, int s, const xdcc *xd);
static void u_xdl_head(const userinput * const u);
static void u_xdl_foot(const userinput * const u);
static void u_xdl_full(const userinput * const u);
static void u_xdl_group(const userinput * const u);
static void u_xdl(const userinput * const u);
static void u_xdlock(const userinput * const u);
static void u_xds(const userinput * const u);
static void u_dcl(const userinput * const u);
static void u_dcld(const userinput * const u);
static void u_qul(const userinput * const u);
static void u_close(const userinput * const u);
static void u_closeu(const userinput * const u);
static void u_nomin(const userinput * const u);
static void u_nomax(const userinput * const u);
static void u_unlimited(const userinput * const u);
static void u_rmq(const userinput * const u);
static void u_raw(const userinput * const u);
#ifdef MULTINET
static void u_rawnet(const userinput * const u);
#endif /* MULTINET */
static void u_redraw(const userinput * const u);
static void u_delhist(const userinput * const u);
static void u_info(const userinput * const u);
static void u_remove(const userinput * const u);
static void u_removedir(const userinput * const u);
static void u_removegroup(const userinput * const u);
static void u_send(const userinput * const u);
static void u_queue(const userinput * const u);
static void u_psend(const userinput * const u);
static void u_msg(const userinput * const u);
#ifdef MULTINET
static void u_msgnet(const userinput * const u);
#endif /* MULTINET */
static void u_mesg(const userinput * const u);
static void u_mesq(const userinput * const u);
static void u_quit(const userinput * const u);
static void u_status(const userinput * const u);
static void u_chfile(const userinput * const u);
static void u_chdesc(const userinput * const u);
static void u_chnote(const userinput * const u);
static void u_chmins(const userinput * const u);
static void u_chmaxs(const userinput * const u);
static void u_chlimit(const userinput * const u);
static void u_chlimitinfo(const userinput * const u);
static void u_chgets(const userinput * const u);
static void u_lock(const userinput * const u);
static void u_unlock(const userinput * const u);
static void u_lockgroup(const userinput * const u);
static void u_unlockgroup(const userinput * const u);
static void u_groupdesc(const userinput * const u);
static void u_group(const userinput * const u);
static void u_movegroup(const userinput * const u);
static void u_regroup(const userinput * const u);
static void u_add(const userinput * const u);
static void u_adddir(const userinput * const u);
static void u_addnew(const userinput * const u);
static void u_addgroup(const userinput * const u);
static void u_chatme(const userinput * const u);
static void u_chatl(const userinput * const u);
static void u_closec(const userinput * const u);
static void u_rehash(const userinput * const u);
static void u_botinfo(const userinput * const u);
static void u_ignl(const userinput * const u);
static void u_ignore(const userinput * const u);
static void u_unignore(const userinput * const u);
static void u_nosave(const userinput * const u);
static void u_nosend(const userinput * const u);
static void u_nolist(const userinput * const u);
static void u_renumber(const userinput * const u);
static void u_msgread(const userinput * const u);
static void u_msgdel(const userinput * const u);
static void u_memstat(const userinput * const u);
static void u_qsend(const userinput * const u);
static void u_slotsmax(const userinput * const u);
static void u_queuesize(const userinput * const u);
static void u_holdqueue(const userinput * const u);
static void u_shutdown(const userinput * const u);
static void u_debug(const userinput * const u);
static void u_hop(const userinput * const u);
static void u_jump(const userinput * const u);
static void u_servqc(const userinput * const u);
static void u_servers(const userinput * const u);
static void u_trinfo(const userinput * const u);
static void u_listdir(const userinput * const u, const char *dir);
static void u_listul(const userinput * const u);
static void u_clearrecords(const userinput * const u);
static void u_rmul(const userinput * const u);
static void u_crash(const userinput * const u);
static void u_chanl(const userinput * const u);
static void u_identify(const userinput * const u);
static void u_announce(const userinput * const u);
static void u_addann(const userinput * const u);
static void u_crc(const userinput * const u);
static void u_filemove(const userinput * const u);
static void u_filedel(const userinput * const u);
static void u_showdir(const userinput * const u);
#ifdef USE_CURL
static void u_fetch(const userinput * const u);
#endif /* USE_CURL */


typedef struct
{
  const short help_section;
  const userinput_method_e methods_allowed;
  void (*handler)(const userinput * const);
  const char *command;
  const char *args;
  const char *desc;
} userinput_parse_t;


/* local info */
static const userinput_parse_t userinput_parse[] = {
{1,method_allow_all,u_help,     "HELP",NULL,"Shows Help"},
{1,method_allow_all_xdl,u_xdl_full,   "XDLFULL",NULL,"Lists All Offered Files"},
{1,method_allow_all_xdl,u_xdl_group,  "XDLGROUP","<g>","Show group <g>"},
{1,method_allow_all_xdl,u_xdl,  "XDL",NULL,"Lists Offered Groups and Files without group"},
{1,method_allow_all,u_xdlock,   "XDLOCK",NULL,"Show all locked Files"},
{1,method_allow_all,u_xds,      "XDS",NULL,"Save State File"},
{1,method_allow_all,u_dcl,      "DCL",NULL,"Lists Current Transfers"},
{1,method_allow_all,u_dcld,     "DCLD",NULL,"Lists Current Transfers with Details"},
{1,method_allow_all,u_trinfo,   "TRINFO","n","Lists Information About Transfer n"},
{1,method_allow_all,u_qul,      "QUL",NULL,"Lists Current Queue"},
{1,method_allow_all,u_ignl,     "IGNL",NULL,"Show Ignored List"},
{1,method_allow_all,u_listul,   "LISTUL",NULL,"Shows contents of upload directory"},
#ifdef MULTINET
{1,method_allow_all,u_chanl,    "CHANL","[<net>]","Shows channel list with member list"},
#else /* MULTINET */
{1,method_allow_all,u_chanl,    "CHANL",NULL,"Shows channel list with member list"},
#endif /* MULTINET */

{2,method_allow_all,u_close,    "CLOSE","n","Cancels Transfer with ID = n"},
{2,method_allow_all,u_closeu,   "CLOSEU","n","Cancels Upload with ID = n"},
{2,method_allow_all,u_rmq,      "RMQ","n","Removes Queue Number n"},
{2,method_allow_all,u_nomin,    "NOMIN","n","Disables Minspeed For Transfer ID n"},
{2,method_allow_all,u_nomax,    "NOMAX","n","Disables Maxspeed For Transfer ID n"},
{2,method_allow_all,u_unlimited, "UNLIMITED","n","Disables Bandwidth Limits For Transfer ID n"},
#ifdef MULTINET
{2,method_allow_all,u_send,     "SEND","nick n [<net>]","Sends Pack n to nick"},
{2,method_allow_all,u_queue,    "QUEUE","nick n [<net>]","Queues Pack n for nick"},
{2,method_allow_all,u_psend,    "PSEND","<channel> <style> [<net>]","Sends <style> (full|minimal|summary) XDCC LIST to <channel>"},
#else /* MULTINET */
{2,method_allow_all,u_send,     "SEND","nick n","Sends Pack n to nick"},
{2,method_allow_all,u_queue,    "QUEUE","nick n","Queues Pack n for nick"},
{2,method_allow_all,u_psend,    "PSEND","<channel> <style>","Sends <style> (full|minimal|summary) XDCC LIST to <channel>"},
#endif /* MULTINET */
{2,method_allow_all,u_qsend,    "QSEND",NULL,"Sends Out The First Queued Pack"},
{2,method_allow_all,u_slotsmax, "SLOTSMAX","[n]","temporary change slotsmax"},
{2,method_allow_all,u_queuesize, "QUEUESIZE","[n]","temporary change queuesize"},

{3,method_allow_all,u_info,     "INFO","n","Show Info for Pack n"},
{3,method_allow_all,u_remove,   "REMOVE","n","Removes Pack n"},
{3,method_allow_all,u_removedir,"REMOVEDIR","<dir>","Remove Every File in <dir>"},
{3,method_allow_all,u_removegroup, "REMOVEGROUP","<group>","Remove Every File within <group>"},
{3,method_allow_all,u_renumber, "RENUMBER","x y","Moves Pack x to y"},
{3,method_allow_all,u_add,      "ADD","<filename>","Add New Pack With <filename>"},
{3,method_allow_all,u_adddir,   "ADDDIR","<dir>","Add Every File in <dir>"},
{3,method_allow_all,u_addnew,   "ADDNEW","<dir>","Add any new files in <dir>"},
{3,method_allow_all,u_addgroup, "ADDGROUP","<g> <dir>","Add any new files in <dir> to group <g>"},
{3,method_allow_all,u_chfile,   "CHFILE","n <filename>","Change File of pack n to <filename>"},
{3,method_allow_all,u_chdesc,   "CHDESC","n <msg>","Change Description of pack n to <msg>"},
{3,method_allow_all,u_chnote,   "CHNOTE","n <msg>","Change Note of pack n to <msg>"},
{3,method_allow_all,u_chmins,   "CHMINS","n x","Change min speed of pack n to x KB"},
{3,method_allow_all,u_chmaxs,   "CHMAXS","n x","Change max speed of pack n to x KB"},
{3,method_allow_all,u_chlimit,  "CHLIMIT","n x","Change Download limit of pack n to x transfers per day"},
{3,method_allow_all,u_chlimitinfo, "CHLIMITINFO","n <msg>","Change Over Limit Info of pack n to <msg>"},
{3,method_allow_all,u_chgets,   "CHGETS","n x","Change the get count of a pack"},
{3,method_allow_all,u_lock,     "LOCK","n <msg>","Lock the pack n with password <msg>"},
{3,method_allow_all,u_unlock,   "UNLOCK","n","Unlock the pack n"},
{3,method_allow_all,u_lockgroup,   "LOCKGROUP","g x","Lock all packs in group <g> with password x"},
{3,method_allow_all,u_unlockgroup, "UNLOCKGROUP","g","Unlock all packs in group <g>"},
{3,method_allow_all,u_groupdesc,  "GROUPDESC","<g> <msg>","Change Desc of group <g> to <msg>"},
{3,method_allow_all,u_group,      "GROUP","n <g>","Change Group of pack n to <g>"},
{3,method_allow_all,u_movegroup,  "MOVEGROUP","n m <g>","Change Group of pack n to m, set to <g>"},
{3,method_allow_all,u_regroup,    "REGROUP","<g> <new>","Change all packs of group <g> to <new>"},
{3,method_allow_all,u_announce, "ANNOUNCE","n <msg>","ANNOUNCE <msg> for pack n in all joined channels"},
{3,method_allow_all,u_addann,   "ADDANN","<filename>","Add and Announce New Pack"},
{3,method_allow_all,u_crc,      "CRC","n","Check CRC of Pack n"},
{3,method_allow_all,u_filemove, "FILEMOVE","<filename> <filename>","rename file on disk"},
{3,method_allow_all,u_filedel,  "FILEDEL","<filename>","remove file from disk"},
{3,method_allow_all,u_showdir,  "SHOWDIR","<dir>","list directory on disk"},
#ifdef USE_CURL
{3,method_allow_all,u_fetch,    "FETCH","<file> <url>","download url and save file in upload dir"},
#endif /* USE_CURL */

{4,method_allow_all,u_msg,      "MSG","<nick> <message>","Send a message to a user"},
#ifdef MULTINET
{4,method_allow_all,u_msgnet,   "MSGNET","<net> <nick> <message>","Send a message to a user"},
#endif /* MULTINET */
{4,method_allow_all,u_mesg,     "MESG","<message>","Sends msg to all users who are transferring"},
{4,method_allow_all,u_mesq,     "MESQ","<message>","Sends msg to all users in a queue"},
{4,method_allow_all,u_ignore,   "IGNORE","n <hostmask>","Ignore hostmask (nick!user@host) for n minutes, wildcards allowed"},
{4,method_allow_all,u_unignore, "UNIGNORE","<hostmask>","Un-Ignore hostmask"},
{4,method_allow_all,u_nosave,   "NOSAVE","n","Disables XDCC AutoSave for next n minutes"},
{4,method_allow_all,u_nosend,   "NOSEND","n","Disables XDCC Send for next n minutes"},
{4,method_allow_all,u_nolist,   "NOLIST","n","Disables XDCC List and Plist for next n mins"},
{4,method_allow_all,u_msgread,  "MSGREAD",NULL,"Show MSG log"},
{4,method_allow_all,u_msgdel,   "MSGDEL",NULL,"Delete MSG log"},
{4,method_allow_all,u_rmul,     "RMUL","<file>","Delete a file in the Upload Dir"},
{4,method_allow_all,u_raw,      "RAW","<command>","Send <command> to server (RAW IRC)"},
#ifdef MULTINET
{4,method_allow_all,u_rawnet,   "RAWNET","<net> <command>","Send <command> to server (RAW IRC)"},
#endif /* MULTINET */

#ifdef MULTINET
{5,method_allow_all,u_servers,  "SERVERS","[<net>]","Shows the server list"},
#else /* MULTINET */
{5,method_allow_all,u_servers,  "SERVERS",NULL,"Shows the server list"},
#endif /* MULTINET */
{5,method_allow_all,u_hop,      "HOP","<channel>","leave and rejoin a channel to get status"},
#ifdef MULTINET
{5,method_allow_all,u_jump,     "JUMP","<num> [<net>]","Switches to a random server or server <num>"},
#else /* MULTINET */
{5,method_allow_all,u_jump,     "JUMP","<num>","Switches to a random server or server <num>"},
#endif /* MULTINET */
{5,method_allow_all,u_servqc,   "SERVQC",NULL,"Clears the server send queue"},
{5,method_allow_all,u_status,   "STATUS",NULL,"Show Useful Information"},
{5,method_allow_all,u_rehash,   "REHASH",NULL,"Re-reads config file(s) and reconfigures"},
{5,method_allow_all,u_botinfo,  "BOTINFO",NULL,"Show Information about the bot status"},
{5,method_allow_all,u_memstat,  "MEMSTAT",NULL,"Show Information about memory usage"},
{5,method_allow_all,u_clearrecords, "CLEARRECORDS",NULL,"Clears transfer, bandwidth, uptime, total sent, and transfer limits"},
{5,method_console,  u_redraw,   "REDRAW",NULL,"Redraws the Screen"},
{5,method_console,  u_delhist,  "DELHIST",NULL,"Deletes console history"},
{5,method_dcc,      u_quit,     "QUIT",NULL,"Close this DCC chat"},
{5,method_msg,      u_chatme,   "CHATME",NULL,"Sends you a DCC Chat Request"},
{5,method_allow_all,u_chatl,    "CHATL",NULL,"Lists DCC Chat Information"},
{5,method_allow_all,u_closec,   "CLOSEC","n","Closes DCC Chat with ID = n"},
{5,method_console,  u_debug,    "DEBUG","n","Set Debugging level to n"},
{5,method_allow_all,u_holdqueue, "HOLDQUEUE","<num>","set or toggles holdqueue"},
#ifdef MULTINET
{5,method_allow_all,u_identify, "IDENTIFY","[<net>]","Send stored password again to nickserv"},
#else /* MULTINET */
{5,method_allow_all,u_identify, "IDENTIFY",NULL,"Send stored password again to nickserv"},
#endif /* MULTINET */
{5,method_allow_all,u_shutdown, "SHUTDOWN","<act>","Shutdown iroffer, <act> is \"now\", \"delayed\", or \"cancel\""},

{6,method_console,  u_crash, "CRASH",NULL,"Cause a segmentation fault"},
};



void u_fillwith_console (userinput * const u, char *line)
{
  
  updatecontext();
  
  if (line[0] && (line[strlen(line)-1] == '\n'))
    {
      line[strlen(line)-1] = '\0';
    }
  
  u->method = method_console;
  u->snick = NULL;
  u->chat = NULL;
  
  u->cmd = caps(getpart(line,1));
  u->arg1 = getpart(line,2);
  u->arg2 = getpart(line,3);
  u->arg3 = getpart(line,4);
  
  if (u->arg1)
    {
      u->arg1e = mymalloc(strlen(line) - strlen(u->cmd) - 1 + 1);
      strcpy(u->arg1e, line + strlen(u->cmd) + 1);
    }
  else
    {
      u->arg1e = NULL;
    }
  
  if (u->arg2)
    {
      u->arg2e = mymalloc(strlen(line) - strlen(u->cmd) - strlen(u->arg1) - 2 + 1);
      strcpy(u->arg2e, line + strlen(u->cmd) + strlen(u->arg1) + 2);
    }
  else
    {
      u->arg2e = NULL;
    }
  
#ifdef MULTINET
  if (u->arg3)
    {
      u->arg3e = mymalloc(strlen(line) - strlen(u->cmd) - strlen(u->arg1) - strlen(u->arg2) - 3 + 1);
      strcpy(u->arg3e, line + strlen(u->cmd) + strlen(u->arg1) + strlen(u->arg2) + 3);
    }
  else
    {
      u->arg3e = NULL;
    }
#endif /* MULTINET */
  
  return;
}

void u_fillwith_dcc (userinput * const u, dccchat_t *chat, char *line)
{
  
  updatecontext();
  
  if (line[strlen(line)-1] == '\n')
    {
      line[strlen(line)-1] = '\0';
    }
  
  u->method = method_dcc;
  u->snick = NULL;
  u->chat = chat;
  
  u->cmd = caps(getpart(line,1));
  u->arg1 = getpart(line,2);
  u->arg2 = getpart(line,3);
  u->arg3 = getpart(line,4);
  
  if (u->arg1)
    {
      u->arg1e = mymalloc(strlen(line) - strlen(u->cmd) - 1 + 1);
      strcpy(u->arg1e, line + strlen(u->cmd) + 1);
    }
  else
    {
      u->arg1e = NULL;
    }
  
  if (u->arg2)
    {
      u->arg2e = mymalloc(strlen(line) - strlen(u->cmd) - strlen(u->arg1) - 2 + 1);
      strcpy(u->arg2e, line + strlen(u->cmd) + strlen(u->arg1) + 2);
    }
  else
    {
      u->arg2e = NULL;
    }
  
#ifdef MULTINET
  if (u->arg3)
    {
      u->arg3e = mymalloc(strlen(line) - strlen(u->cmd) - strlen(u->arg1) - strlen(u->arg2) - 3 + 1);
      strcpy(u->arg3e, line + strlen(u->cmd) + strlen(u->arg1) + strlen(u->arg2) + 3);
    }
  else
    {
      u->arg3e = NULL;
    }
#endif /* MULTINET */
  
  return;
}

void u_fillwith_msg (userinput * const u, const char* n, const char *line)
{
  char *t1,*t2,*t3,*t4,*t5;
  int len;
  
  updatecontext();
  
  u->method = method_msg;
  if (n)
    {
      u->snick = mymalloc(strlen(n)+1);
      strcpy(u->snick, n);
    }
  else
    {
      u->snick = NULL;
    }
  u->chat = NULL;
  t1 = getpart(line,1);
  t2 = getpart(line,2);
  t3 = getpart(line,3);
  t4 = getpart(line,4);
  t5 = getpart(line,5);
  
  u->cmd = caps(getpart(line,6));
  u->arg1 = getpart(line,7);
  u->arg2 = getpart(line,8);
  u->arg3 = getpart(line,9);
  
  len = strlen(t1) + strlen(t2) + strlen(t3) + strlen(t4) + strlen(t5) + 5;
  
  if (u->arg1)
    {
      u->arg1e = mymalloc(strlen(line) - len - strlen(u->cmd) - 1 + 1);
      strcpy(u->arg1e, line + len + strlen(u->cmd) + 1);
    }
  else
    {
      u->arg1e = NULL;
    }
  
  if (u->arg2)
    {
      u->arg2e = mymalloc(strlen(line) - len - strlen(u->cmd) - strlen(u->arg1) - 2 + 1);
      strcpy(u->arg2e, line + len + strlen(u->cmd) + strlen(u->arg1) + 2);
    }
  else
    {
      u->arg2e = NULL;
    }
  
#ifdef MULTINET
  if (u->arg3)
    {
      u->arg3e = mymalloc(strlen(line) - len - strlen(u->cmd) - strlen(u->arg1) - strlen(u->arg2) - 3 + 1);
      strcpy(u->arg3e, line + len + strlen(u->cmd) + strlen(u->arg1) + strlen(u->arg2) + 3);
    }
  else
    {
      u->arg3e = NULL;
    }
#endif /* MULTINET */
  
  mydelete(t1);
  mydelete(t2);
  mydelete(t3);
  mydelete(t4);
  mydelete(t5);
  
  return;
}

void u_fillwith_clean (userinput * const u)
{
  
  updatecontext();
  
  mydelete(u->snick);
  u->chat = NULL;
  mydelete(u->cmd);
  mydelete(u->arg1e);
  mydelete(u->arg2e);
  mydelete(u->arg1);
  mydelete(u->arg2);
  mydelete(u->arg3);
#ifdef MULTINET
  mydelete(u->arg3e);
#endif /* MULTINET */
}

static void u_respond(const userinput * const u, const char *format, ...)
{
  va_list args;
  channel_t *ch;
  char *tempnick;
  char *chan;

  updatecontext();
  
  va_start(args, format);
  
  switch (u->method)
    {
    case method_console:
      vioutput(CALLTYPE_NORMAL, OUT_S, COLOR_NO_COLOR, format, args);
      break;
    case method_dcc:
      vwritedccchat(u->chat, 1, format, args);
      break;
    case method_out_all:
      vioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_NO_COLOR, format, args);
      break;
    case method_fd:
      {
        ssize_t retval;
        char tempstr[maxtextlength];
        int llen;
        
        llen = vsnprintf(tempstr,maxtextlength-3,format,args);
        if ((llen < 0) || (llen >= maxtextlength-3))
          {
            outerror(OUTERROR_TYPE_WARN,"string too long!");
            tempstr[0] = '\0';
            llen = 0;
          }
        
        if (!gdata.xdcclistfileraw)
          {
            removenonprintablectrl(tempstr);
          }
        
#if defined(_OS_CYGWIN)
        tempstr[llen++] = '\r';
#endif
        tempstr[llen++] = '\n';
        tempstr[llen] = '\0';
        
        retval = write(u->fd, tempstr, strlen(tempstr));
        if (retval < 0)
          {
            outerror(OUTERROR_TYPE_WARN_LOUD,"Write failed: %s", strerror(errno));
          }
      }
      break;
    case method_msg:
      vprivmsg(u->snick, format, args);
      break;
    case method_xdl_channel:
    case method_xdl_channel_min:
    case method_xdl_channel_sum:
#ifdef MULTINET
      ch = irlist_get_head(&(gnetwork->channels));
#else /* MULTINET */
      ch = irlist_get_head(&gdata.channels);
#endif /* MULTINET */
      while(ch)
        {
          tempnick = mycalloc(strlen(u->snick)+1);
          strcpy(tempnick,u->snick);
          for (chan = strtok(tempnick, ","); chan != NULL; chan = strtok(NULL, ",") )
            {
              if (!strcasecmp(ch->name,chan))
                {
                  vprivmsg_chan(ch, format, args);
                }
            }
          mydelete(tempnick);
          ch = irlist_get_next(ch);
        }
      break;
    case method_xdl_user_privmsg:
      vprivmsg_slow(u->snick, format, args);
      break;
    case method_xdl_user_notice:
      vnotice_slow(u->snick, format, args);
      break;
    case method_allow_all:
    default:
      break;
    }
  
  va_end(args);
}

void u_parseit(userinput * const u) {
   int i,found = 0;
   
   updatecontext();
   
   if (!u->cmd || !strlen(u->cmd)) {
      u_respond(u,"** Missing Command, try again");
      u_fillwith_clean(u);
      return;
      }
   
   for (i=0; !found && i<(sizeof(userinput_parse)/sizeof(userinput_parse_t)); i++)
      if ( (!strcmp(userinput_parse[i].command,u->cmd)) &&
           (userinput_parse[i].methods_allowed & u->method) ) {
         found=1;
         userinput_parse[i].handler(u);
         }
   
   if (!found)
      u_respond(u,"** User Command Not Recognized, try \"HELP\"");
   
   if (found && u->method==method_console)
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"ADMIN %s Requested (console)",u->cmd);
   if (found && u->method==method_dcc)
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"ADMIN %s Requested (DCC Chat: %s)", u->cmd, u->chat->nick ? u->chat->nick : "???");
   if (found && u->method==method_msg)
      ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L|OUT_D,COLOR_MAGENTA,"ADMIN %s Requested (MSG: %s)",u->cmd,u->snick);
   
   u_fillwith_clean(u);
   
   }

static void u_help(const userinput * const u)
{
  int i,which=0;
  
  updatecontext();
  
  for (which=1; which<6; which++)
    {
      
      u_respond(u,"-- %s Commands --",
                which==1?"Info":
                which==2?"Transfer":
                which==3?"Pack":
                which==4?"Misc":
                which==5?"Bot":"<Unknown>");
      
      for (i=0; i<(sizeof(userinput_parse)/sizeof(userinput_parse_t)); i++)
        {
          if (userinput_parse[i].methods_allowed & u->method &&
              userinput_parse[i].help_section == which)
            {
              if (userinput_parse[i].args)
                {
                  int spaces;
                  
                  spaces = 20 - 1;
                  spaces -= sstrlen(userinput_parse[i].command);
                  spaces -= sstrlen(userinput_parse[i].args);
                  spaces = max2(0,spaces);
                  
                  u_respond(u,"  %s %s %.*s: %s",
                            userinput_parse[i].command,
                            userinput_parse[i].args,
                            spaces, "                       ", 
                            userinput_parse[i].desc);
                }
              else
                {
                  u_respond(u,"  %-20s : %s",
                            userinput_parse[i].command,
                            userinput_parse[i].desc);
                }
            }
        }
    }
  u_respond(u,"For additional help, see the complete documentation at http://iroffer.org/");
  
}

static int u_is_locked(const userinput * const u, const xdcc *xd) {
  if (gdata.hidelockedpacks == 0)
    return 0;
   
  if (xd->lock == NULL)
    return 0;
   
  switch (u->method)
    {
    case method_xdl_channel:
    case method_xdl_user_privmsg:
    case method_xdl_user_notice:
      return 1;
    default:
      break;
    }
  return 0;
}

static int u_xdl_space(void) {
   int i,s;
   xdcc *xd;

   i = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       i = max2(i,xd->gets);
       xd = irlist_get_next(xd);
     }
   s = 5;
   if (i < 10000) s = 4;
   if (i < 1000) s = 3;
   if (i < 100) s = 2;
   if (i < 10) s = 1;
   return s;
}
   
static const char *spaces[] = { ""," ","  ","   ","    ","     ","      " };

static void u_xdl_pack(const userinput * const u, char *tempstr, int i, int s, const xdcc *xd) {
   char *sizestrstr;
   int len;
   
   sizestrstr = sizestr(1, xd->st_size);
   snprintf(tempstr, maxtextlength - 1,
           "\2#%-2i\2 %*ix [%s] %s",
            i,
            s, xd->gets,
            sizestrstr,
            xd->desc);
   len = strlen(tempstr);
   mydelete(sizestrstr);
   
   if (xd->minspeed > 0 && xd->minspeed != gdata.transferminspeed)
     {
        snprintf(tempstr + len, maxtextlength - 1 - len,
                 " [%1.1fK Min]",
                 xd->minspeed);
        len = strlen(tempstr);
     }
   
   if (xd->maxspeed > 0 && xd->maxspeed != gdata.transfermaxspeed)
     {
        snprintf(tempstr + len, maxtextlength - 1 - len,
                 " [%1.1fK Max]",
                 xd->maxspeed);
        len = strlen(tempstr);
     }
   
   if (xd->dlimit_max != 0)
     {
        snprintf(tempstr + len, maxtextlength - 1 - len,
                 " [%d of %d DL left]",
                 xd->dlimit_used - xd->gets, xd->dlimit_max);
        len = strlen(tempstr);
     }
   
   u_respond(u,"%s",tempstr);
   
   if (xd->note && strlen(xd->note))
     {
       u_respond(u," \2^-\2%s%s",spaces[s],xd->note);
     }
}

static void u_xdl_head(const userinput * const u) {
   char *tempstr;
   char *tempnick;
   char *chan;
   int a,i,m,m1;
   int len;
   int head;
   xdcc *xd;
   channel_t *ch;
   ir_uint64 xdccsent;
   
   tempstr  = mycalloc(maxtextlength);

   if (u->method==method_xdl_channel_min) m = 1; else m = 0;
   if (u->method==method_xdl_channel_sum) m1 = 1; else m1 = 0;
   
   head = 0;
   switch (u->method)
    {
    case method_xdl_channel:
    case method_xdl_channel_min:
    case method_xdl_channel_sum:
#ifdef MULTINET
      ch = irlist_get_head(&(gnetwork->channels));
#else /* MULTINET */
      ch = irlist_get_head(&gdata.channels);
#endif /* MULTINET */
      while(ch)
        {
          if (ch->headline != NULL )
            {
              tempnick = mycalloc(strlen(u->snick)+1);
              strcpy(tempnick,u->snick);
              for (chan = strtok(tempnick, ","); chan != NULL; chan = strtok(NULL, ",") )
                {
                  if (!strcasecmp(ch->name,chan))
                    {
                      userinput u2;
                      
                      u2 = *u;
                      u2.snick = chan;
                      u_respond(&u2,"\2**\2 %s \2**\2", ch->headline);
                      head ++;
                    }
                }
              mydelete(tempnick);
            }
          ch = irlist_get_next(ch);
        }
      break;
    default:
      u_respond(u,"\2**\2 To stop this listing, type /MSG %s XDCC STOP \2**\2",
                  (gdata.user_nick ? gdata.user_nick : "??"));
      break;
    }
   
   if (gdata.headline)
     {
       if (head == 0)
          u_respond(u,"\2**\2 %s \2**\2",gdata.headline);
     }
   
   if (!m && !m1)
     {
       if (gdata.slotsmax - irlist_size(&gdata.trans) < 0)
         {
           a = irlist_size(&gdata.trans);
         }
       else
         {
           a = gdata.slotsmax;
         }
       
       snprintf(tempstr, maxtextlength - 1,
                "\2**\2 %i pack%s \2**\2  %i of %i slot%s open",
                irlist_size(&gdata.xdccs),
                irlist_size(&gdata.xdccs) != 1 ? "s" : "",
                a - irlist_size(&gdata.trans),
                a,
                a != 1 ? "s" : "");
       len = strlen(tempstr);
       
       if (gdata.slotsmax <= irlist_size(&gdata.trans))
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", Queue: %i/%i",
                    irlist_size(&gdata.mainqueue),
                    gdata.queuesize);
           len = strlen(tempstr);
         }
       
       if (gdata.transferminspeed > 0)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", Min: %1.1fKB/s",
                    gdata.transferminspeed);
           len = strlen(tempstr);
         }
       
       if (gdata.transfermaxspeed > 0)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", Max: %1.1fKB/s",
                    gdata.transfermaxspeed);
           len = strlen(tempstr);
         }
       
       if (gdata.record > 0.5)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", Record: %1.1fKB/s",
                    gdata.record);
           len = strlen(tempstr);
         }
       
       u_respond(u,"%s",tempstr);
       
       
       for (i=0,xdccsent=0; i<XDCC_SENT_SIZE; i++)
         {
           xdccsent += (ir_uint64)gdata.xdccsent[i];
         }
       
       snprintf(tempstr, maxtextlength - 1,
                "\2**\2 Bandwidth Usage \2**\2 Current: %1.1fKB/s,",
                ((float)xdccsent) / XDCC_SENT_SIZE / 1024.0);
       len = strlen(tempstr);
       
       if (gdata.maxb)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    " Cap: %i.0KB/s,",
                    gdata.maxb / 4);
           len = strlen(tempstr);
         }
       
       if (gdata.sentrecord > 0.5)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    " Record: %1.1fKB/s",
                    gdata.sentrecord);
           len = strlen(tempstr);
         }
       
       u_respond(u,"%s",tempstr);
       
       u_respond(u,"\2**\2 To request a file, type \"/msg %s xdcc send x\" \2**\2",
                 (gdata.user_nick ? gdata.user_nick : "??"));
       
       if ((gdata.hide_list_info == 0) && (gdata.disablexdccinfo == 0))
          u_respond(u,"\2**\2 To request details, type \"/msg %s xdcc info x\" \2**\2",
                    (gdata.user_nick ? gdata.user_nick : "??"));
       
       i = 0;
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           if (xd->group != NULL)
             {
               i++;
             }
           xd = irlist_get_next(xd);
         }
       if (i > 0)
          u_respond(u,"\2**\2 To list a group, type \"/msg %s xdcc list GROUP\" \2**\2",
                    (gdata.user_nick ? gdata.user_nick : "??"));
     }
   
   if (m1)
     {
       if (!gdata.restrictprivlist)
         {
           u_respond(u,"\2**\2 For a listing type: \"/msg %s xdcc list\" \2**\2",
                     (gdata.user_nick ? gdata.user_nick : "??"));
         }
       if (gdata.creditline)
         {
           u_respond(u,"\2**\2 %s \2**\2",gdata.creditline);
         }
     }
   
   mydelete(tempstr);
}

static void u_xdl_foot(const userinput * const u) {
   xdcc *xd;
   float toffered;

   if (gdata.creditline)
     {
       u_respond(u,"\2**\2 %s \2**\2",gdata.creditline);
     }
   
   if (u->method != method_xdl_channel_min)
     {
       toffered = 0;
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           toffered += (float)xd->st_size;
           xd = irlist_get_next(xd);
         }
   
       u_respond(u,
                 "Total Offered: %1.1f MB  Total Transferred: %1.2f %cB",
                 toffered/1024.0/1024.0,
                 (gdata.totalsent/1024/1024) > 1024 ? ( (gdata.totalsent/1024/1024/1024) > 1024 ? ((float)gdata.totalsent)/1024/1024/1024/1024 : ((float)gdata.totalsent)/1024/1024/1024 ) : ((float)gdata.totalsent)/1024/1024 ,
                 (gdata.totalsent/1024/1024) > 1024 ? ( (gdata.totalsent/1024/1024/1024) > 1024 ? 'T' : 'G' ) : 'M'
         );
     }
}

static void u_xdl_full(const userinput * const u) {
   char *tempstr;
   xdcc *xd;
   int i,s;
   
   updatecontext();
   
   u_xdl_head(u);
   
   tempstr = mycalloc(maxtextlength);
   i = 1;
   s = u_xdl_space();
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (u_is_locked(u,xd) == 0)
         u_xdl_pack(u,tempstr,i,s,xd);
       i++;
       xd = irlist_get_next(xd);
     }
         
   mydelete(tempstr);

   u_xdl_foot(u);
}

static void u_xdl_group(const userinput * const u) {
   char *tempstr;
   char *msg3;
   xdcc *xd;
   int i,k,s;

   updatecontext();

   u_xdl_head(u);

   msg3 = u->arg1;
   if (msg3 == NULL)
     return;

   tempstr = mycalloc(maxtextlength);
   i = 1;
   k = 0;
   s = u_xdl_space();
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,msg3) == 0 )
             {
               if (xd->group_desc != NULL)
                 {
                   u_respond(u,"group: %s %s",msg3,xd->group_desc);
                 }

               if (u_is_locked(u,xd) == 0)
                 u_xdl_pack(u,tempstr,i,s,xd);
               k++;
             }
         }
       i++;
       xd = irlist_get_next(xd);
     }
         
   mydelete(tempstr);
   
   u_xdl_foot(u);
   
   if (!k)
     {
       u_respond(u,"Sorry, nothing was found, try a XDCC LIST");
     }
}

static void u_xdl(const userinput * const u) {
   char *tempstr;
   char *inlist;
   int i,m,s;
   xdcc *xd;
   irlist_t grplist = {};

   updatecontext();
   
   u_xdl_head(u);

   if (u->method==method_xdl_channel_sum) return;
   if (u->method==method_xdl_channel_min) m = 1; else m = 0;

   tempstr  = mycalloc(maxtextlength);
   
   s = u_xdl_space();
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       /* skip is group is set */
       if (xd->group == NULL)
         {
           if (u_is_locked(u,xd) == 0)
             u_xdl_pack(u,tempstr,i,s,xd);
         }
       i++;
       xd = irlist_get_next(xd);
     }
   
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       /* groupe entry and entry is visible */
       if ((xd->group != NULL) && (xd->group_desc != NULL))
          {
            snprintf(tempstr,maxtextlength-1,
                     "%s %s",xd->group,xd->group_desc);
            inlist = irlist_add(&grplist, strlen(tempstr) + 1);
            strcpy(inlist, tempstr);
          }
       xd = irlist_get_next(xd);
     }

     irlist_sort(&grplist, irlist_sort_cmpfunc_string, NULL);
     inlist = irlist_get_head(&grplist);
     while (inlist)
       {
         u_respond(u,"group: %s",inlist);
         inlist = irlist_delete(&grplist, inlist);
       }
   
   u_xdl_foot(u);
   
   mydelete(tempstr);
}

static void u_xdlock(const userinput * const u)
{
   char *tempstr;
   int i,s;
   xdcc *xd;

   updatecontext();
   
   tempstr  = mycalloc(maxtextlength);
   
   s = u_xdl_space();
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (xd->lock != NULL)
         {
           u_xdl_pack(u,tempstr,i,s,xd);
           u_respond(u," \2^-\2%sPassword: %s",spaces[s],xd->lock);
         }
       i++;
       xd = irlist_get_next(xd);
     }
   
   mydelete(tempstr);
}

static void u_xds(const userinput * const u)
{
  updatecontext();
  write_statefile();
  xdccsavetext();
}

static void u_dcl(const userinput * const u)
{
  const char *y;
  int i;
  upload *ul;
  transfer *tr;

  updatecontext();
  
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads))
    {
      u_respond(u,"No Active Transfers");
      return;
    }
  
  if (irlist_size(&gdata.trans))
    {
      u_respond(u,"Current Transfer%s",irlist_size(&gdata.trans)!=1?"s":"");
      u_respond(u,"   ID  User        File                               Status");
    }
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      switch (tr->tr_status)
        {
        case TRANSFER_STATUS_LISTENING:
          y = "Listening";
          break;
          
        case TRANSFER_STATUS_SENDING:
          y = "Sending";
          break;
          
        case TRANSFER_STATUS_WAITING:
          y = "Finishing";
          break;
          
        case TRANSFER_STATUS_DONE:
          y = "Closing";
          break;
            
        default:
          y = "Unknown!";
          break;
        }
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING)
        {
          u_respond(u,"  %3i  %-9s   %-32s   %s %2.0f%%",
                    tr->id,tr->nick,getfilename(tr->xpack->file),y,
                    ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size));
        }
      else
        {
          u_respond(u,"  %3i  %-9s   %-32s   %s",
                    tr->id,tr->nick,getfilename(tr->xpack->file),y);
        }
      tr = irlist_get_next(tr);
    }
  
  if (irlist_size(&gdata.uploads))
    {
      u_respond(u,"Current Upload%s",irlist_size(&gdata.uploads)!=1?"s":"");
      u_respond(u,"   ID  User        File                               Status");
    }
  
  i = 0;
  ul = irlist_get_head(&gdata.uploads);
  while (ul)
    {
      switch (ul->ul_status)
        {
        case UPLOAD_STATUS_CONNECTING:
          y = "Connecting";
          break;
          
        case UPLOAD_STATUS_GETTING:
          y = "Getting";
          break;
          
        case UPLOAD_STATUS_WAITING:
          y = "Finishing";
          break;
          
        case UPLOAD_STATUS_DONE:
          y = "Done";
          break;
          
        default:
          y = "Unknown!";
          break;
        }
      
      if (ul->ul_status == UPLOAD_STATUS_GETTING)
        {
          u_respond(u,"   %2i  %-9s   %-32s   %s %2.0f%%",
                    i,ul->nick,getfilename(ul->file),y,
                    ((float)ul->bytesgot)*100.0/((float)ul->totalsize));
        }
      else
        {
          u_respond(u,"   %2i  %-9s   %-32s   %s",
                    i,ul->nick,getfilename(ul->file),y);
        }
      ul = irlist_get_next(ul);
      i++;
    }
  
}

static void u_dcld(const userinput * const u)
{
  char *tempstr2, *tempstr3, *tempstr4;
  const char *y;
  int i,left,started;
  upload *ul;
  transfer *tr;
  
  updatecontext();
  
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads))
    {
      u_respond(u,"No Active Transfers");
      return;
    }
  
   tempstr2 = mycalloc(maxtextlengthshort);
   tempstr3 = mycalloc(maxtextlengthshort);
   tempstr4 = mycalloc(maxtextlengthshort);
   
   if (irlist_size(&gdata.trans))
     {
       u_respond(u,"Current Transfer%s",irlist_size(&gdata.trans)!=1?"s":"");
       u_respond(u," ID  User        File                               Status");
       u_respond(u,"  ^-    Speed    Current/    End   Start/Remain    Min/  Max  Resumed");
       u_respond(u," --------------------------------------------------------------------");
     }
   
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      switch (tr->tr_status)
        {
        case TRANSFER_STATUS_LISTENING:
          y = "Listening";
          break;
          
        case TRANSFER_STATUS_SENDING:
          y = "Sending";
          break;
          
        case TRANSFER_STATUS_WAITING:
          y = "Finishing";
          break;
          
        case TRANSFER_STATUS_DONE:
          y = "Closing";
          break;
          
        default:
          y = "Unknown!";
          break;
        }
      
#ifdef MULTINET
      if (gdata.networks[tr->net].name == NULL)
        u_respond(u,"network: %d", tr->net);
      else
        u_respond(u,"network: %d: %s", tr->net, gdata.networks[tr->net].name);
#endif /* MULTINET */
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING)
        {
          u_respond(u,"%3i  %-9s   %-32s   %s %2.0f%%",
                    tr->id,tr->nick,getfilename(tr->xpack->file),y,
                    ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size));
        }
      else
        {
          u_respond(u,"%3i  %-9s   %-32s   %s",
                    tr->id,tr->nick,getfilename(tr->xpack->file),y);
        }
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING || tr->tr_status == TRANSFER_STATUS_WAITING)
        {
          left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          started = min2(359999,gdata.curtime-tr->connecttime);
          snprintf(tempstr2, maxtextlengthshort - 1,
                   "%1.1fK", tr->xpack->minspeed);
          snprintf(tempstr3, maxtextlengthshort - 1,
                   "%6liK", (long)(tr->startresume)/1024);
          snprintf(tempstr4, maxtextlengthshort - 1,
                   "%1.1fK", tr->xpack->maxspeed);
          
          u_respond(u,
                    "  ^- %5.1fK/s    %6" LLPRINTFMT "iK/%6" LLPRINTFMT "iK  %2i%c%02i%c/%2i%c%02i%c  %5s/%5s  %7s",
                    tr->lastspeed,
                    (long long)((tr->bytessent)/1024),
                    (long long)((tr->xpack->st_size)/1024),
                    started < 3600 ? started/60 : started/60/60 ,
                    started < 3600 ? 'm' : 'h',
                    started < 3600 ? started%60 : (started/60)%60 ,
                    started < 3600 ? 's' : 'm',
                    left < 3600 ? left/60 : left/60/60 ,
                    left < 3600 ? 'm' : 'h',
                    left < 3600 ? left%60 : (left/60)%60 ,
                    left < 3600 ? 's' : 'm',
                    (tr->nomin || (tr->xpack->minspeed == 0.0)) ? "no" : tempstr2 ,
                    (tr->nomax || (tr->xpack->maxspeed == 0.0)) ? "no" : tempstr4 ,
                    tr->startresume ? tempstr3 : "no");
        }
      else
        {
          u_respond(u,"  ^-    -----    -------/-------   -----/------  -----/-----      ---");
        }
      tr = irlist_get_next(tr);
    }
  
  if (irlist_size(&gdata.uploads))
    {
      u_respond(u,"Current Upload%s",irlist_size(&gdata.uploads)!=1?"s":"");
      u_respond(u," ID  User        File                               Status");
      u_respond(u,"  ^-    Speed    Current/    End   Start/Remain");
      u_respond(u," --------------------------------------------------------------");
    }
  
  i = 0;
  ul = irlist_get_head(&gdata.uploads);
  while(ul)
    {
      switch (ul->ul_status)
        {
        case UPLOAD_STATUS_CONNECTING:
          y = "Connecting";
          break;
          
        case UPLOAD_STATUS_GETTING:
          y = "Getting";
          break;
          
        case UPLOAD_STATUS_WAITING:
          y = "Finishing";
          break;
          
        case UPLOAD_STATUS_DONE:
          y = "Done";
          break;
          
        default:
          y = "Unknown!";
          break;
        }
         
      if (ul->ul_status == UPLOAD_STATUS_GETTING)
        {
          u_respond(u," %2i  %-9s   %-32s   %s %2.0f%%",
                    i,ul->nick,getfilename(ul->file),y,
                    ((float)ul->bytesgot)*100.0/((float)ul->totalsize));
        }
      else
        {
          u_respond(u," %2i  %-9s   %-32s   %s",
                    i,ul->nick,getfilename(ul->file),y);
        }
      
      if (ul->ul_status == UPLOAD_STATUS_GETTING || ul->ul_status == UPLOAD_STATUS_WAITING)
        {
          left = min2(359999,(ul->totalsize-ul->bytesgot)/((int)(max2(ul->lastspeed,0.001)*1024)));
          started = min2(359999,gdata.curtime-ul->connecttime);
          
          u_respond(u,
                    "  ^- %5.1fK/s    %6" LLPRINTFMT "iK/%6" LLPRINTFMT "iK  %2i%c%02i%c/%2i%c%02i%c",
                    ul->lastspeed,
                    (long long)((ul->bytesgot)/1024),
                    (long long)((ul->totalsize)/1024),
                    started < 3600 ? started/60 : started/60/60 ,
                    started < 3600 ? 'm' : 'h',
                    started < 3600 ? started%60 : (started/60)%60 ,
                    started < 3600 ? 's' : 'm',
                    left < 3600 ? left/60 : left/60/60 ,
                    left < 3600 ? 'm' : 'h',
                    left < 3600 ? left%60 : (left/60)%60 ,
                    left < 3600 ? 's' : 'm'
                    );
        }
      else
        {
          u_respond(u, "  ^-    -----    -------/-------   -----/------  -----/-----      ---");
        }
      ul = irlist_get_next(ul);
      i++;
    }
  
  u_respond(u," --------------------------------------------------------------------");
  
  mydelete(tempstr2);
  mydelete(tempstr3);
  mydelete(tempstr4);
}

static void u_qul(const userinput * const u)
{
  int i;
  unsigned long rtime, lastrtime; 
  pqueue *pq;
  transfer *tr;
  
  updatecontext();
  
  if (!irlist_size(&gdata.mainqueue))
    {
      u_respond(u,"No Users Queued");
      return;
    }
  
  u_respond(u,"Current Queue:");
  u_respond(u,"    #  User        File                              Waiting     Left");
  
  lastrtime=0;
  
  /* if we are sending more than allowed, we need to skip the difference */
  for (i=0; i<irlist_size(&gdata.trans)-gdata.slotsmax; i++)
    {
      rtime=-1;
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          int left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          if (left > lastrtime && left < rtime)
            {
              rtime = left;
            }
          tr = irlist_get_next(tr);
        }
      if (rtime < 359999)
        {
          lastrtime=rtime;
        }
    }
  
  i=1;
  pq = irlist_get_head(&gdata.mainqueue);
  while(pq)
    {
      rtime=-1;
      tr = irlist_get_head(&gdata.trans);
      while(tr)
        {
          int left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          if (left > lastrtime && left < rtime)
            {
              rtime = left;
            }
          tr = irlist_get_next(tr);
        }
      lastrtime=rtime;
      
      if (rtime < 359999)
        {
          u_respond(u,"   %2i  %-9s   %-32s   %2lih%2lim   %2lih%2lim",
                    i,
                    pq->nick,
                    getfilename(pq->xpack->file),
                    (long)((gdata.curtime-pq->queuedtime)/60/60),
                    (long)(((gdata.curtime-pq->queuedtime)/60)%60),
                    (long)(rtime/60/60),
                    (long)(rtime/60)%60);
        }
      else
        {
          u_respond(u,"   %2i  %-9s   %-32s   %2lih%2lim  Unknown",
                    i,
                    pq->nick,
                    getfilename(pq->xpack->file),
                    (long)((gdata.curtime-pq->queuedtime)/60/60),
                    (long)(((gdata.curtime-pq->queuedtime)/60)%60));
        }
      pq = irlist_get_next(pq);
      i++;
    }
  
}

static void u_close(const userinput * const u)
{
  int num = -1;
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || !does_tr_id_exist(num))
    {
      u_respond(u,"Invalid ID number, Try \"DCL\" for a list");
    }
  else
    {
      t_closeconn(does_tr_id_exist(num),"Owner Requested Close",0);
    }
}

static void u_closeu(const userinput * const u)
{
  int num = -1;
  upload *ul;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || (num >= irlist_size(&gdata.uploads)))
    {
      u_respond(u,"Invalid ID number, Try \"DCL\" for a list");
    }
  else
    {
      ul = irlist_get_nth(&gdata.uploads, num);
      
      l_closeconn(ul,"Owner Requested Close",0);
    }
}

static void u_nomin(const userinput * const u)
{
  int num = -1;
  transfer *tr;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || !does_tr_id_exist(num))
    {
      u_respond(u,"Invalid ID number, Try \"DCL\" for a list");
    }
  else
    {
      tr = does_tr_id_exist(num);
      tr->nomin = 1;
    }
}

static void u_nomax(const userinput * const u)
{
  int num = -1;
  transfer *tr;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || !does_tr_id_exist(num))
    {
      u_respond(u,"Invalid ID number, Try \"DCL\" for a list");
    }
  else
    {
      tr = does_tr_id_exist(num);
      tr->nomax = 1;
    }
}

static void u_unlimited(const userinput * const u)
{
  int num = -1;
  transfer *tr;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || !does_tr_id_exist(num))
    {
      u_respond(u,"Invalid ID number, Try \"DCL\" for a list");
    }
  else
    {
      tr = does_tr_id_exist(num);
      tr->nomax = 1;
      tr->unlimited = 1;
    }
}

static void u_rmq(const userinput * const u)
{
  int num = 0;
  pqueue *pq;
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1)
    {
      u_respond(u,"Invalid ID number, Try \"QUL\" for a list");
      return;
    }
  
  pq = irlist_get_nth(&gdata.mainqueue, num-1);
  
  if (!pq)
    {
      u_respond(u,"Invalid ID number, Try \"QUL\" for a list");
    }
  else
    {
#ifdef MULTINET
      backup = gnetwork;
      gnetwork = &(gdata.networks[pq->net]);
#endif /* MULTINET */
      notice(pq->nick,"** Removed From Queue: Owner Requested Remove");
      mydelete(pq->nick);
      mydelete(pq->hostname);
      irlist_delete(&gdata.mainqueue, pq);
#ifdef MULTINET
      gnetwork = backup;
#endif /* MULTINET */
    }
}

static void u_raw(const userinput * const u)
{
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */
  
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Command");
      return;
    }

#ifdef MULTINET
  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
#endif /* MULTINET */
  writeserver(WRITESERVER_NOW, "%s", u->arg1e);
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
}

#ifdef MULTINET
static void u_rawnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;
  
  updatecontext();
  
  net = get_network(u->arg1);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
  
  if (!u->arg2e || !strlen(u->arg2e))
    {
      u_respond(u,"Try Specifying a Command");
      return;
    }

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  writeserver(WRITESERVER_NOW, "%s", u->arg2e);
  gnetwork = backup;
}
#endif /* MULTINET */

static void u_info(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  char *sizestrstr;
  char *sendnamestr;
  char tempstr[maxtextlengthshort];
  
  updatecontext();
  
  if (u->arg1) { num = atoi(u->arg1); }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  u_respond(u, "Pack Info for Pack #%i:", num);
  
  u_respond(u, " Filename       %s", xd->file);

  sendnamestr = getsendname(xd->file);
  u_respond(u, " Sendname       %s", sendnamestr);
  mydelete(sendnamestr);

  u_respond(u, " Description    %s", xd->desc);
  if (xd->note[0])
    {
      u_respond(u, " Note           %s", xd->note);
    }
  
  sizestrstr = sizestr(1, xd->st_size);
  u_respond(u, " Filesize       %" LLPRINTFMT "i [%sB]",
            (long long)xd->st_size, sizestrstr);
  mydelete(sizestrstr);
  
  getdatestr(tempstr, xd->mtime, maxtextlengthshort);
  u_respond(u, " Last Modified  %s", tempstr);
  
  u_respond(u, " Device/Inode   %" LLPRINTFMT "u/%" LLPRINTFMT "u",
            (unsigned long long)xd->st_dev, (unsigned long long)xd->st_ino);
  
  u_respond(u, " Gets           %d", xd->gets);
  if (xd->minspeed)
    {
      u_respond(u, " Minspeed       %1.1fKB/sec", xd->minspeed);
    }
  if (xd->maxspeed)
    {
      u_respond(u, " Maxspeed       %1.1fKB/sec", xd->maxspeed);
    }
  
  if (xd->has_md5sum)
    {
      u_respond(u, " md5sum         " MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
    }
  if (xd->has_crc32)
    {
      u_respond(u, " crc32          %.8lX", xd->crc32);
    }
  
  return;
}

static void u_remove(const userinput * const u) {
   int num = 0;
   char *tmpdesc;
   char *tmpgroup;
   pqueue *pq;
   transfer *tr;
   xdcc *xd;
#ifdef MULTINET
   gnetwork_t *backup;
#endif /* MULTINET */
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   
   if ( num < 1 || num > irlist_size(&gdata.xdccs) ) {
      u_respond(u,"Try a valid pack number");
      return;
      }
   
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
           (tr->xpack == xd))
         {
           t_closeconn(tr,"Pack removed",0);
         }
       tr = irlist_get_next(tr);
     }
   
   pq = irlist_get_head(&gdata.mainqueue);
   while(pq)
     {
       if (pq->xpack == xd)
         {
#ifdef MULTINET
           backup = gnetwork;
           gnetwork = &(gdata.networks[pq->net]);
#endif /* MULTINET */
           notice(pq->nick,"** Removed From Queue: Pack removed");
#ifdef MULTINET
           gnetwork = backup;
#endif /* MULTINET */
           mydelete(pq->nick);
           mydelete(pq->hostname);
           pq = irlist_delete(&gdata.mainqueue, pq);
         }
       else
         {
           pq = irlist_get_next(pq);
         }
     }
   
   u_respond(u,"Removed Pack %i [%s]", num, xd->desc);
   
   if (gdata.md5build.xpack == xd)
     {
       outerror(OUTERROR_TYPE_WARN,"[MD5]: Canceled (remove)");
       
       FD_CLR(gdata.md5build.file_fd, &gdata.readset);
       close(gdata.md5build.file_fd);
       gdata.md5build.file_fd = FD_UNUSED;
       gdata.md5build.xpack = NULL;
     }
   
   assert(xd->file_fd == FD_UNUSED);
   assert(xd->file_fd_count == 0);
#ifdef HAVE_MMAP
   assert(!irlist_size(&xd->mmaps));
#endif
   
   mydelete(xd->file);
   mydelete(xd->desc);
   mydelete(xd->note);
   /* keep group info for later work */
   tmpgroup = xd->group;
   xd->group = NULL;
   tmpdesc = xd->group_desc;
   xd->group_desc = NULL;
   irlist_delete(&gdata.xdccs, xd);
   
   if (tmpdesc != NULL)
     {
       if (tmpgroup != NULL)
         reorder_new_groupdesc(tmpgroup,tmpdesc);
       mydelete(tmpdesc);
     }
   if (tmpgroup != NULL)
     mydelete(tmpgroup);
   
   write_statefile();
   xdccsavetext();
   }

static void u_removedir(const userinput * const u)
{
  DIR *d;
  struct dirent *f;
  char *tempstr, *thedir;
  int thedirlen;
  xdcc *xd;
  
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Directory");
      return;
    }
  
  convert_to_unix_slash(u->arg1e);
   
  if (u->arg1e[strlen(u->arg1e)-1] == '/')
    {
      u->arg1e[strlen(u->arg1e)-1] = '\0';
    }
  
  thedirlen = strlen(u->arg1e);
  if (gdata.filedir)
    {
      thedirlen += strlen(gdata.filedir) + 1;
    }
  
  thedir = mycalloc(thedirlen+1);
  strcpy(thedir, u->arg1e);
  
  d = opendir(thedir);
  
  if (!d && (errno == ENOENT) && gdata.filedir)
    {
      snprintf(thedir, thedirlen+1, "%s/%s",
               gdata.filedir, u->arg1e);
      d = opendir(thedir);
    }
  
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  
  while ((f = readdir(d)))
    {
      struct stat st;
      int len = strlen(f->d_name);
      int n;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          u_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
          mydelete(tempstr);
          continue;
        }
      if (!S_ISREG(st.st_mode))
        {
          mydelete(tempstr);
          continue;
        }
      
      n = 0;
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
        {
          n++;
          if ((xd->st_dev == st.st_dev) &&
              (xd->st_ino == st.st_ino))
            {
              userinput u2;
              char tempstr2[8];
              
              snprintf(tempstr2, 8, "%d", n);
              
              u2 = *u;
              u2.arg1 = tempstr2;
              u_remove(&u2);
              
              /* start over, the list has changed */
              n = 0;
              xd = irlist_get_head(&gdata.xdccs);
            }
          else
            {
              xd = irlist_get_next(xd);
            }
        }
      
      mydelete(tempstr);
    }
  
  closedir(d);
  
  mydelete(thedir);
  return;
}

static void u_removegroup(const userinput * const u)
{
   xdcc *xd;
   userinput u2;
   int n;
   char tempstr2[8];
   
   updatecontext();
   
   if (!u->arg1 || !strlen(u->arg1))
     {
       u_respond(u,"Try Specifying a Group");
       return;
     }
   
   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                snprintf(tempstr2, 8, "%d", n);
                
                u2 = *u;
                u2.arg1 = tempstr2;
                u_remove(&u2);
                
                /* start over, the list has changed */
                n = 0;
                xd = irlist_get_head(&gdata.xdccs);
                continue;
             }
         }
       xd = irlist_get_next(xd);
     }
}

static void u_redraw(const userinput * const u) {
   updatecontext();
   
   initscreen(0);
   gotobot();
   }

static void u_delhist(const userinput * const u)
{
  updatecontext();
  
  u_respond(u, "Deleted all %d lines of console history",
            irlist_size(&gdata.console_history));
  
  irlist_delete_all(&gdata.console_history);
  gdata.console_history_offset = 0;
  
  return;
}

static void u_send(const userinput * const u) {
   int num = 0;
#ifdef MULTINET
   gnetwork_t *backup;
   int net;
#endif /* MULTINET */
   
   updatecontext();
   
#ifdef MULTINET
   net = get_network(u->arg3);
   if (net < 0)
     {
       u_respond(u,"Try specifying a valid network number");
       return;
     }
#endif /* MULTINET */

   if (u->arg2) num = atoi(u->arg2);
   
   if (!u->arg1 || !strlen(u->arg1)) {
      u_respond(u,"Try Specifying a Nick");
      return;
      }
   
   if (num > irlist_size(&gdata.xdccs) || num < 1) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   u_respond(u,"Sending %s pack %i",u->arg1,num);
   
#ifdef MULTINET
   backup = gnetwork;
   gnetwork = &(gdata.networks[net]);
#endif /* MULTINET */
   sendxdccfile(u->arg1,"man","man",num,NULL,NULL);
#ifdef MULTINET
   gnetwork = backup;
#endif /* MULTINET */
   
   }

/* this function imported from iroffer-lamm */
static void u_queue(const userinput * const u) {
   int num = 0;
   int alreadytrans;
   pqueue *pq;
   xdcc *xd;
   char *tempstr;
#ifdef MULTINET
   gnetwork_t *backup;
   int net;
#endif /* MULTINET */
   
   updatecontext();
   
#ifdef MULTINET
   net = get_network(u->arg3);
   if (net < 0)
     {
       u_respond(u,"Try specifying a valid network number");
       return;
     }
#endif /* MULTINET */

   if (u->arg2) num = atoi(u->arg2);
   
   if (!u->arg1 || !strlen(u->arg1)) {
      u_respond(u,"Try Specifying a Nick");
      return;
      }
   
   if (num > irlist_size(&gdata.xdccs) || num < 1) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);

   alreadytrans = 0;
   pq = irlist_get_head(&gdata.mainqueue);
   while(pq)
     {
       if (!strcasecmp(pq->nick,u->arg1))
         {
           if (pq->xpack == xd)
             {
               alreadytrans++;
               break;
             }
         }
       pq = irlist_get_next(pq);
     }

   if (alreadytrans > 0) {
      u_respond(u,"Already Queued %s for Pack %i!", u->arg1,num);
      return;
      }
   
   u_respond(u,"Queueing %s for Pack %i", u->arg1,num);
   
#ifdef MULTINET
   backup = gnetwork;
   gnetwork = &(gdata.networks[net]);
#endif /* MULTINET */
   tempstr = addtoqueue(u->arg1, "man", num);
   notice(u->arg1, "** %s", tempstr);
   mydelete(tempstr);
#ifdef MULTINET
   gnetwork = backup;
#endif /* MULTINET */
   
   if (!gdata.exiting &&
       irlist_size(&gdata.mainqueue) &&
       (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
     {
       sendaqueue(0);
     }
   }

static void u_psend(const userinput * const u)
{
  userinput manplist;
  userinput_method_e method;
#ifdef MULTINET
  gnetwork_t *backup;
  int net;
#endif /* MULTINET */
  
  updatecontext();
  
#ifdef MULTINET
  net = get_network(u->arg3);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
#endif /* MULTINET */

  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a Channel");
      return;
    }
  
  if (!u->arg2 || !strlen(u->arg2))
    {
      method = method_xdl_channel;
      u_respond(u, "No PLIST style specified. Using style full");
    }
  else
    {
      if (strcmp(u->arg2,"full") == 0)
        {
          method = method_xdl_channel;
        }
      else if (strcmp(u->arg2,"minimal") == 0)
        {
          method = method_xdl_channel_min;
        }
      else if (strcmp(u->arg2,"summary") == 0)
        {
          if (gdata.restrictprivlist && !gdata.creditline && !gdata.headline)
            {
              u_respond(u,"Summary Plist makes no sense with restrictprivlist set and no creditline or headline");
              return;
            }
          else
            {
              method = method_xdl_channel_sum;
            }
        }
      else
        {
          u_respond(u, "PLIST format is not (full|minimal|summary)");
          return;
        }
    }
  
#ifdef MULTINET
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
#endif /* MULTINET */
  u_fillwith_msg(&manplist,u->arg1,"A A A A A xdl");
  manplist.method = method;
  u_parseit(&manplist);
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
  
  u_respond(u,"Sending PLIST with style %s to %s",
            u->arg2 ? u->arg2 : "full",
            u->arg1);
  
}

/* iroffer-lamm: add-ons */
static void u_announce (const userinput * const u) {
  int num = 0;
  xdcc *xd;
  channel_t *ch;
  char *tempstr;
  char *tempstr2;
#ifdef MULTINET
  int ss;
  gnetwork_t *backup;
#endif /* MULTINET */
  
  updatecontext ();
  
  if (u->arg1) num = atoi (u->arg1);
  
  if (num > irlist_size(&gdata.xdccs) || num < 1) {
    u_respond (u, "Try Specifying a Valid Pack Number");
    return;
    }
  if (!u->arg2e || !strlen (u->arg2e)) {
    u_respond (u, "Try Specifying a Message (e.g. NEW)");
    return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  u_respond(u,"Pack Info for Pack #%i:",num);
  
  tempstr = mycalloc(maxtextlength);
  tempstr2 = mycalloc(maxtextlength);
  snprintf(tempstr2,maxtextlength-2,"[\2%s\2] %s",u->arg2e,xd->desc);
  snprintf(tempstr,maxtextlength-2,"%s - /msg %s xdcc send %i",tempstr2,gdata.user_nick,num);
  
#ifdef MULTINET
  backup = gnetwork;
  for (ss=0; ss<gdata.networks_online; ss++) {
      gnetwork = &(gdata.networks[ss]);
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch) {
        if (ch->flags & CHAN_ONCHAN)
          privmsg_chan(ch, tempstr);
        ch = irlist_get_next(ch);
        }
    }
  gnetwork = backup;
#else /* MULTINET */
  ch = irlist_get_head(&gdata.channels);
  while(ch) {
    if (ch->flags & CHAN_ONCHAN)
      privmsg_chan(ch, tempstr);
    ch = irlist_get_next(ch);
    }
#endif /* MULTINET */
  u_respond(u,"Announced [%s] - %s",u->arg2e,xd->desc);
  mydelete(tempstr2);
  mydelete(tempstr);
}

/* iroffer-lamm: add-ons */
static void u_addann (const userinput * const u) {
  char *tempstr;
  userinput *ui;
  int i;
  
  updatecontext ();
  i = irlist_size(&gdata.xdccs);
  u_add(u);
  if (irlist_size(&gdata.xdccs) > i) {
    tempstr = mycalloc (maxtextlength);
    ui = mycalloc(sizeof(userinput));
    snprintf(tempstr, maxtextlength - 2, "A A A A A announce %i added",irlist_size(&gdata.xdccs));
    u_fillwith_msg(ui,NULL,tempstr);
    ui->method = method_out_all;       /* just OUT_S|OUT_L|OUT_D it */
    u_parseit(ui);
    mydelete(ui);
    mydelete(tempstr);
    }
}

static void u_crc(const userinput * const u) {
  int num = 0;
  xdcc *xd;
  const char *crcmsg;
  
  updatecontext ();
  
  if (u->arg1) {
    num = atoi (u->arg1);
    if (num > irlist_size(&gdata.xdccs) || num < 1) {
      u_respond (u, "Try Specifying a Valid Pack Number");
      return;
      }
  
    xd = irlist_get_nth(&gdata.xdccs, num-1);
    u_respond(u,"Validating CRC for Pack #%i:",num);
    crcmsg = validate_crc32(xd, 0);
    if (crcmsg != NULL)
      u_respond(u,"File '%s' %s.", xd->file, crcmsg);
  }
  else {
   num = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       num ++;
       crcmsg = validate_crc32(xd, 1);
       if (crcmsg != NULL)
         u_respond(u,"Pack %d, File '%s' %s.", num, xd->file, crcmsg);
       xd = irlist_get_next(xd);
     }
  }
}

static void u_filemove(const userinput * const u)
{
   int xfiledescriptor;
   struct stat st;
   char *file1;
   char *file2;
   
   updatecontext();
   
   if (gdata.direct_file_access == 0) {
      u_respond(u,"Disabled in Config");
      return;
      }

   if (!u->arg1 || !strlen(u->arg1)) {
      u_respond(u,"Try Specifying a Filename");
      return;
      }
   
   if (!u->arg2e || !strlen(u->arg2e)) {
      u_respond(u,"Try Specifying a new Filename");
      return;
      }
   
   file1 = mymalloc(strlen(u->arg1)+1);
   strcpy(file1,u->arg1);
   convert_to_unix_slash(file1);
   
   xfiledescriptor=open(file1, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0 && (errno == ENOENT) && gdata.filedir)
     {
       mydelete(file1);
       file1 = mymalloc(strlen(gdata.filedir)+1+strlen(u->arg1)+1);
       sprintf(file1,"%s/%s",gdata.filedir,u->arg1);
       convert_to_unix_slash(file1);
       xfiledescriptor=open(file1, O_RDONLY | ADDED_OPEN_FLAGS);
     }
   
   if (xfiledescriptor < 0) {
      u_respond(u,"Cant Access File: %s: %s", file1, strerror(errno));
      mydelete(file1);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      u_respond(u,"Cant Access File Details: %s: %s", file1, strerror(errno));
      close(xfiledescriptor);
      mydelete(file1);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      u_respond(u,"%s is not a file", file1);
      mydelete(file1);
      return;
     }
   
   file2 = mymalloc(strlen(u->arg2e)+1);
   strcpy(file2,u->arg2e);
   convert_to_unix_slash(file2);
   
   if (strchr(file2, '/') == NULL)
     {
       mydelete(file2);
       file2 = mymalloc(strlen(gdata.filedir)+1+strlen(u->arg2e)+1);
       sprintf(file2,"%s/%s",gdata.filedir,u->arg2e);
       convert_to_unix_slash(file2);
     }
   
   if (rename(file1,file2) < 0)
     {
       u_respond(u,"File %s could not be moved to %s: %s", file1, file2, strerror(errno));
     }
   else
     {
       u_respond(u,"File %s was moved to %s.",file1, file2);
     }
   mydelete(file1);
   mydelete(file2);
}

static void u_filedel(const userinput * const u)
{
   int xfiledescriptor;
   struct stat st;
   char *file;
   
   updatecontext();
   
   if (gdata.direct_file_access == 0) {
      u_respond(u,"Disabled in Config");
      return;
      }

   if (!u->arg1e || !strlen(u->arg1e)) {
      u_respond(u,"Try Specifying a Filename");
      return;
      }
   
   file = mymalloc(strlen(u->arg1e)+1);
   strcpy(file,u->arg1e);
   convert_to_unix_slash(file);
   
   xfiledescriptor=open(file, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0 && (errno == ENOENT) && gdata.filedir)
     {
       mydelete(file);
       file = mymalloc(strlen(gdata.filedir)+1+strlen(u->arg1e)+1);
       sprintf(file,"%s/%s",gdata.filedir,u->arg1e);
       convert_to_unix_slash(file);
       xfiledescriptor=open(file, O_RDONLY | ADDED_OPEN_FLAGS);
     }
   
   if (xfiledescriptor < 0) {
      u_respond(u,"Cant Access File: %s: %s", file, strerror(errno));
      mydelete(file);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      u_respond(u,"Cant Access File Details: %s: %s", file, strerror(errno));
      close(xfiledescriptor);
      mydelete(file);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      u_respond(u,"%s is not a file", file);
      mydelete(file);
      return;
     }
   
   if (unlink(file) < 0)
     {
       u_respond(u,"File %s could not be deleted: %s", file, strerror(errno));
     }
   else
     {
       u_respond(u,"File %s was deleted.", file);
     }
   mydelete(file);
}

static void u_showdir(const userinput * const u)
{
  char *thedir;
  int thedirlen;
   
  updatecontext();
   
  if (gdata.direct_file_access == 0)
    {
      u_respond(u,"Disabled in Config");
      return;
    }
   
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Directory");
      return;
    }
  
  convert_to_unix_slash(u->arg1e);
   
  if (u->arg1e[strlen(u->arg1e)-1] == '/')
    {
      u->arg1e[strlen(u->arg1e)-1] = '\0';
    }
  
  thedirlen = strlen(u->arg1e);
  if (gdata.filedir)
    {
      thedirlen += strlen(gdata.filedir) + 1;
    }
  
  thedir = mymalloc(thedirlen+1);
  strcpy(thedir, u->arg1e);
  
  u_listdir(u, thedir);
  mydelete(thedir);
  return;
}

#ifdef USE_CURL
static void u_fetch(const userinput * const u)
{
  updatecontext();
  
  if (gdata.direct_file_access == 0)
    {
      u_respond(u,"Disabled in Config");
      return;
    }
   
  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a File");
      return;
    }
  
  if (!u->arg2e || !strlen(u->arg2e))
    {
      u_respond(u,"Try Specifying a URL");
      return;
    }
  
  start_fetch_url(u);
}
#endif /* USE_CURL */

static void u_msg(const userinput * const u)
{
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */
  
  updatecontext();
  
  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a Nick");
      return;
    }
  
  if (!u->arg2e || !strlen(u->arg2e))
    {
      u_respond(u,"Try Specifying a Message");
      return;
    }
  
#ifdef MULTINET
  backup = gnetwork;
  gnetwork = &(gdata.networks[0]);
#endif /* MULTINET */
  privmsg_fast(u->arg1,"%s",u->arg2e);
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
  
}

#ifdef MULTINET
static void u_msgnet(const userinput * const u)
{
  gnetwork_t *backup;
  int net;
  
  updatecontext();
  
  net = get_network(u->arg1);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
  
  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Nick");
      return;
    }
  
  if (!u->arg3e || !strlen(u->arg3e))
    {
      u_respond(u,"Try Specifying a Message");
      return;
    }

  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  privmsg_fast(u->arg2,"%s",u->arg3e);
  gnetwork = backup;
}
#endif /* MULTINET */

static void u_mesg(const userinput * const u)
{
  transfer *tr;
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */
 
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Message");
      return;
    }
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
#ifdef MULTINET
      backup = gnetwork;
      gnetwork = &(gdata.networks[tr->net]);
#endif /* MULTINET */
      notice(tr->nick,"MESSAGE FROM OWNER: %s",u->arg1e);
      tr = irlist_get_next(tr);
#ifdef MULTINET
      gnetwork = backup;
#endif /* MULTINET */
    }
  
  u_respond(u,"Sent message to %i user%s",irlist_size(&gdata.trans),irlist_size(&gdata.trans)!=1?"s":"");
  
}

static void u_mesq(const userinput * const u)
{
  int count;
  pqueue *pq;
#ifdef MULTINET
  gnetwork_t *backup;
#endif /* MULTINET */
  
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Message");
      return;
    }
  
  count=0;
  pq = irlist_get_head(&gdata.mainqueue);
  while(pq)
    {
#ifdef MULTINET
      backup = gnetwork;
      gnetwork = &(gdata.networks[pq->net]);
#endif /* MULTINET */
      notice(pq->nick,"MESSAGE FROM OWNER: %s",u->arg1e);
      count++;
      pq = irlist_get_next(pq);
#ifdef MULTINET
      gnetwork = backup;
#endif /* MULTINET */
    }
  
  u_respond(u,"Sent message to %i user%s",count,count!=1?"s":"");
  
}

static void u_quit(const userinput * const u)
{
  updatecontext();
  
  ioutput(CALLTYPE_NORMAL,OUT_S|OUT_L,COLOR_MAGENTA,"DCC CHAT: QUIT");
  u_respond(u,"Bye.");
  
  shutdowndccchat(u->chat,1);
  /* caller deletes */
}

static void u_status(const userinput * const u) {
   char *tempstr = mycalloc(maxtextlength);
   
   updatecontext();
   
   getstatusline(tempstr,maxtextlength);
   u_respond(u,"%s",tempstr);
   
   mydelete(tempstr);
   }

static void u_chfile(const userinput * const u) {
   int num = 0;
   int xfiledescriptor;
   struct stat st;
   char tempstr[maxtextlength];
   transfer *tr;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   if (!u->arg2e || !strlen(u->arg2e)) {
      u_respond(u,"Try Specifying a Filename");
      return;
      }
   
   /* verify file is ok first */
   tempstr[0] = '\0';
   
   convert_to_unix_slash(u->arg2e);
   
   xfiledescriptor=open(u->arg2e, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0 && (errno == ENOENT) && gdata.filedir) {
      snprintf(tempstr, maxtextlength - 1,
               "%s/%s", gdata.filedir, u->arg2e);
      convert_to_unix_slash(tempstr);
      xfiledescriptor=open(tempstr, O_RDONLY | ADDED_OPEN_FLAGS);
      }
   
   if (xfiledescriptor < 0) {
      u_respond(u,"Cant Access File: %s",strerror(errno));
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      u_respond(u,"Cant Access File Details: %s",strerror(errno));
      close(xfiledescriptor);
      return;
     }
      
   close(xfiledescriptor);
   
   if ( st.st_size == 0 ) {
      u_respond(u,"File has size of 0 bytes!");
      return;
      }
   
   if ((st.st_size > gdata.max_file_size) || (st.st_size < 0)) {
      u_respond(u,"File is too large.");
      return;
      }
   
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   tr = irlist_get_head(&gdata.trans);
   while(tr)
     {
       if ((tr->tr_status != TRANSFER_STATUS_DONE) &&
           (tr->xpack == xd))
         {
           t_closeconn(tr,"Pack file changed",0);
         }
       tr = irlist_get_next(tr);
     }
   
   u_respond(u, "CHFILE: [Pack %i] Old: %s New: %s",
             num,xd->file,tempstr[0] ? tempstr : u->arg2e);
   
   mydelete(xd->file);
   xd->file = mycalloc(strlen(tempstr[0] ? tempstr : u->arg2e) + 1);
   
   strcpy(xd->file, tempstr[0] ? tempstr : u->arg2e);
   xd->st_size  = st.st_size;
   xd->st_dev   = st.st_dev;
   xd->st_ino   = st.st_ino;
   xd->mtime    = st.st_mtime;
   
   if (gdata.md5build.xpack == xd)
     {
       outerror(OUTERROR_TYPE_WARN,"[MD5]: Canceled (chfile)");
       
       FD_CLR(gdata.md5build.file_fd, &gdata.readset);
       close(gdata.md5build.file_fd);
       gdata.md5build.file_fd = FD_UNUSED;
       gdata.md5build.xpack = NULL;
     }
   xd->has_md5sum = 0;
   xd->has_crc32 = 0;
   memset(xd->md5sum,0,sizeof(MD5Digest));
   
   assert(xd->file_fd == FD_UNUSED);
   assert(xd->file_fd_count == 0);
#ifdef HAVE_MMAP
   assert(!irlist_size(&xd->mmaps));
#endif
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_add(const userinput * const u) {
   int xfiledescriptor;
   struct stat st;
   xdcc *xd;
   char *group;
   char *file;
   char *a1;
   char *a2;
   
   updatecontext();
   
   if (!u->arg1e || !strlen(u->arg1e)) {
      u_respond(u,"Try Specifying a Filename");
      return;
      }
   
   file = mymalloc(strlen(u->arg1e)+1);
   strcpy(file,u->arg1e);
   convert_to_unix_slash(file);
   
   xfiledescriptor=open(file, O_RDONLY | ADDED_OPEN_FLAGS);
   
   if (xfiledescriptor < 0 && (errno == ENOENT) && gdata.filedir)
     {
       mydelete(file);
       file = mymalloc(strlen(gdata.filedir)+1+strlen(u->arg1e)+1);
       sprintf(file,"%s/%s",gdata.filedir,u->arg1e);
       convert_to_unix_slash(file);
       xfiledescriptor=open(file, O_RDONLY | ADDED_OPEN_FLAGS);
     }
   
   if (xfiledescriptor < 0) {
      u_respond(u,"Cant Access File: %s",strerror(errno));
      mydelete(file);
      return;
      }
   
   if (fstat(xfiledescriptor,&st) < 0)
     {
      u_respond(u,"Cant Access File Details: %s",strerror(errno));
      close(xfiledescriptor);
      mydelete(file);
      return;
     }
   close(xfiledescriptor);
   
   if (!S_ISREG(st.st_mode))
     {
      u_respond(u,"%s is not a file",file);
      mydelete(file);
      return;
     }
   
   if ( st.st_size == 0 ) {
      u_respond(u,"File has size of 0 bytes!");
      mydelete(file);
      return;
      }
   
   if ((st.st_size > gdata.max_file_size) || (st.st_size < 0)) {
      u_respond(u,"File is too large.");
      mydelete(file);
      return;
      }
   
   if (gdata.noduplicatefiles) {
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           if ((xd->st_dev == st.st_dev) &&
              (xd->st_ino == st.st_ino))
             {
               u_respond(u,"File '%s' is already added.", u->arg1e);
               return;
             }
           xd = irlist_get_next(xd);
         }
      }
   
   group = NULL;
   if (gdata.auto_default_group) {
      a1 = mycalloc(strlen(u->arg1e) + 1);
      strtextcpy(a1, u->arg1e);
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           a2 = mycalloc(strlen(xd->file) + 1);
           strtextcpy(a2, xd->file);
           if (!strcmp(a1, a2))
             {
               group = xd->group;
               mydelete(a2);
               break;
             }
           mydelete(a2);
           xd = irlist_get_next(xd);
         }
      mydelete(a1);
      }
   if ((gdata.auto_path_group) && (group == NULL)) {
      a1 = mycalloc(strlen(u->arg1e) + 1);
      strpathcpy(a1, u->arg1e);
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           a2 = mycalloc(strlen(xd->file) + 1);
           strpathcpy(a2, xd->file);
           if (!strcmp(a1, a2))
             {
               group = xd->group;
               mydelete(a2);
               break;
             }
           mydelete(a2);
           xd = irlist_get_next(xd);
         }
      mydelete(a1);
      }
   
   xd = irlist_add(&gdata.xdccs, sizeof(xdcc));
   
   xd->file = file;
   
   xd->note = mymalloc(1);
   strcpy(xd->note,"");
   
   xd->desc = mymalloc(strlen(getfilename(u->arg1e)) + 1);
   strcpy(xd->desc,getfilename(u->arg1e));
   
   xd->gets = 0;
   xd->minspeed = gdata.transferminspeed;
   xd->maxspeed = gdata.transfermaxspeed;
   
   xd->st_size  = st.st_size;
   xd->st_dev   = st.st_dev;
   xd->st_ino   = st.st_ino;
   xd->mtime    = st.st_mtime;
   
   xd->file_fd = FD_UNUSED;
   xd->file_fd_count = 0;
   xd->file_fd_location = 0;
   
   u_respond(u, "ADD PACK: [Pack: %i] [File: %s] Use CHDESC to change description",
             irlist_size(&gdata.xdccs),xd->file);
   
   if ((gdata.auto_default_group) && (group != NULL)) {
         xd->group = mycalloc(strlen(group)+1);
         strcpy(xd->group,group);
         u_respond(u,"GROUP: [Pack: %i] New: %s",
                   irlist_size(&gdata.xdccs), xd->group);
      }
   
   write_statefile();
   xdccsavetext();
   
   /* iroffer-lamm: autoaddann */
   if (gdata.autoaddann) {
      userinput *ui;
      char *tempstr;
      
      tempstr = mycalloc (maxtextlength);
      ui = mycalloc(sizeof(userinput));
      snprintf(tempstr, maxtextlength - 2, "A A A A A announce %i %s",irlist_size(&gdata.xdccs),gdata.autoaddann);
      u_fillwith_msg(ui,NULL,tempstr);
      ui->method = method_out_all;  /* just OUT_S|OUT_L|OUT_D it */
      u_parseit(ui);
      mydelete(ui);
      mydelete(tempstr);
      } 
   }

static void u_adddir(const userinput * const u)
{
  DIR *d;
  struct dirent *f;
  char *thefile, *tempstr, *thedir;
  irlist_t dirlist = {};
  int thedirlen;
  
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Directory");
      return;
    }
  
  convert_to_unix_slash(u->arg1e);
   
  if (u->arg1e[strlen(u->arg1e)-1] == '/')
    {
      u->arg1e[strlen(u->arg1e)-1] = '\0';
    }
  
  thedirlen = strlen(u->arg1e);
  if (gdata.filedir)
    {
      thedirlen += strlen(gdata.filedir) + 1;
    }
  
  thedir = mymalloc(thedirlen+1);
  strcpy(thedir, u->arg1e);
  
  d = opendir(thedir);
  
  if (!d && (errno == ENOENT) && gdata.filedir)
    {
      snprintf(thedir, thedirlen+1, "%s/%s",
               gdata.filedir, u->arg1e);
      d = opendir(thedir);
    }
  
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  
  while ((f = readdir(d)))
    {
      struct stat st;
      int len = strlen(f->d_name);
      
      if (verifyshell(&gdata.adddir_exclude, f->d_name))
        continue;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          u_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
        }
      else if (strcmp(f->d_name,".") &&
               strcmp(f->d_name,"..") &&
               S_ISDIR(st.st_mode))
        {
          u_respond(u,"  Ignoring directory: %s", tempstr);
        }
      else if (S_ISREG(st.st_mode))
        {
          thefile = irlist_add(&dirlist, len + thedirlen + 2);
          strcpy(thefile, tempstr);
        }
      mydelete(tempstr);
    }
  
  closedir(d);
  
  irlist_sort(&dirlist, irlist_sort_cmpfunc_string, NULL);
  
  u_respond(u,"Adding %d files...",
            irlist_size(&dirlist));
  
  thefile = irlist_get_head(&dirlist);
  while (thefile)
    {
      userinput u2;
      u_respond(u,"  Adding %s:",thefile);
      
      u2 = *u;
      u2.arg1e = thefile;
      u_add(&u2);
      
      thefile = irlist_delete(&dirlist, thefile);
    }
  
  mydelete(thedir);
  return;
}

static void u_addnew(const userinput * const u)
{
  DIR *d;
  struct dirent *f;
  char *thefile, *tempstr, *thedir;
  irlist_t dirlist = {};
  int thedirlen, foundit;
  xdcc *xd;
  
  updatecontext();
  
  if (!u->arg1e || !strlen(u->arg1e))
    {
      u_respond(u,"Try Specifying a Directory");
      return;
    }
  
  convert_to_unix_slash(u->arg1e);
   
  if (u->arg1e[strlen(u->arg1e)-1] == '/')
    {
      u->arg1e[strlen(u->arg1e)-1] = '\0';
    }
  
  thedirlen = strlen(u->arg1e);
  if (gdata.filedir)
    {
      thedirlen += strlen(gdata.filedir) + 1;
    }
  
  thedir = mymalloc(thedirlen+1);
  strcpy(thedir, u->arg1e);
  
  d = opendir(thedir);
  
  if (!d && (errno == ENOENT) && gdata.filedir)
    {
      snprintf(thedir, thedirlen+1, "%s/%s",
               gdata.filedir, u->arg1e);
      d = opendir(thedir);
    }
  
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  
  while ((f = readdir(d)))
    {
      struct stat st;
      int len = strlen(f->d_name);
      
      if (verifyshell(&gdata.adddir_exclude, f->d_name))
        continue;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          u_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
        }
      else if (strcmp(f->d_name,".") &&
               strcmp(f->d_name,"..") &&
               S_ISDIR(st.st_mode))
        {
          u_respond(u,"  Ignoring directory: %s", tempstr);
        }
      else if (S_ISREG(st.st_mode))
        {
          foundit = 0;
          xd = irlist_get_head(&gdata.xdccs);
          while(xd)
            {
              if ((xd->st_dev == st.st_dev) &&
                  (xd->st_ino == st.st_ino))
                {
                  foundit = 1;
                  break;
                }
              
              xd = irlist_get_next(xd);
            }
          
          if (foundit == 0)
            {
              thefile = irlist_add(&dirlist, len + thedirlen + 2);
              strcpy(thefile, tempstr);
            }
        }
      mydelete(tempstr);
    }
  
  closedir(d);
  
  irlist_sort(&dirlist, irlist_sort_cmpfunc_string, NULL);
  
  u_respond(u,"Adding %d new files...",
            irlist_size(&dirlist));
  
  thefile = irlist_get_head(&dirlist);
  while (thefile)
    {
      userinput u2;
      u_respond(u,"  Adding %s:",thefile);
      
      u2 = *u;
      u2.arg1e = thefile;
      u_add(&u2);
      
      thefile = irlist_delete(&dirlist, thefile);
    }
  
  mydelete(thedir);
  return;
}

static void u_addgroup(const userinput * const u)
{
  DIR *d;
  struct dirent *f;
  char *thefile, *tempstr, *thedir;
  irlist_t dirlist = {};
  int thedirlen, foundit;
  int newgroup;
  int num;
  int rc;
  xdcc *xd;
  
  updatecontext();
  
  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a Group");
      return;
    }
  
  if (!u->arg2e || !strlen(u->arg2e))
    {
      u_respond(u,"Try Specifying a Directory");
      return;
    }
  
  convert_to_unix_slash(u->arg2e);
  if (gdata.groupsincaps)
    caps(u->arg1);
   
  if (u->arg2e[strlen(u->arg2e)-1] == '/')
    {
      u->arg2e[strlen(u->arg2e)-1] = '\0';
    }
  
  thedirlen = strlen(u->arg2e);
  if (gdata.filedir)
    {
      thedirlen += strlen(gdata.filedir) + 1;
    }
  
  thedir = mycalloc(thedirlen+1);
  strcpy(thedir, u->arg2e);
  
  d = opendir(thedir);
  
  if (!d && (errno == ENOENT) && gdata.filedir)
    {
      snprintf(thedir, thedirlen+1, "%s/%s",
               gdata.filedir, u->arg2e);
      d = opendir(thedir);
    }
  
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  
  while ((f = readdir(d)))
    {
      struct stat st;
      int len = strlen(f->d_name);
      
      if (verifyshell(&gdata.adddir_exclude, f->d_name))
        continue;
      
      tempstr = mycalloc(len + thedirlen + 2);
      
      snprintf(tempstr, len + thedirlen + 2,
               "%s/%s", thedir, f->d_name);
      
      if (stat(tempstr,&st) < 0)
        {
          u_respond(u,"cannot access %s, ignoring: %s",
                    tempstr, strerror(errno));
        }
      else if (S_ISREG(st.st_mode))
        {
          foundit = 0;
          xd = irlist_get_head(&gdata.xdccs);
          while(xd)
            {
              if (!strcmp(tempstr, xd->file))
                {
                  foundit = 1;
                  break;
                }
              
              xd = irlist_get_next(xd);
            }
          
          if (foundit == 0)
            {
              thefile = irlist_add(&dirlist, len + thedirlen + 2);
              strcpy(thefile, tempstr);
            }
        }
      mydelete(tempstr);
    }
  
  closedir(d);
  
  irlist_sort(&dirlist, irlist_sort_cmpfunc_string, NULL);
  
  u_respond(u,"Adding %d new files...",
            irlist_size(&dirlist));

  newgroup = 0;
  thefile = irlist_get_head(&dirlist);
  while (thefile)
    {
      userinput u2;
      u_respond(u,"  Adding %s:",thefile);
      
      u2 = *u;
      u2.arg1e = thefile;
      u_add(&u2);

      num = 0;
      xd = irlist_get_head(&gdata.xdccs);
      while(xd)
         {
           num++;
           if (!strcmp(thefile,xd->file))
             {
               if (xd->group != NULL)
                  {
                     if (strcmp(xd->group, u->arg1) == 0 )
                       break;
                     mydelete(xd->group);
                  }
               xd->group = mycalloc(strlen(u->arg1)+1);
               strcpy(xd->group,u->arg1);
               u_respond(u, "GROUP: [Pack %i] New: %s",
                 num,u->arg1);
               break;
             }
           xd = irlist_get_next(xd);
         }
      
      if ((++newgroup) == 1)
         {
           rc = add_default_groupdesc(u->arg1);
           if (rc == 1)
             u_respond(u, "New GROUPDESC: %s",u->arg1);
         }
      
      thefile = irlist_delete(&dirlist, thefile);
    }
  
  mydelete(thedir);
  return;
}

static void u_chdesc(const userinput * const u) {
   int num = 0;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   if (!u->arg2e || !strlen(u->arg2e)) {
      u_respond(u,"Try Specifying a Description");
      return;
      }
   
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   u_respond(u, "CHDESC: [Pack %i] Old: %s New: %s",
      num,xd->desc,u->arg2e);
   
   mydelete(xd->desc);
   xd->desc = mymalloc(strlen(u->arg2e) + 1);
   
   strcpy(xd->desc,u->arg2e);
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_chnote(const userinput * const u) {
   int num = 0;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   u_respond(u, "CHNOTE: [Pack %i] Old: %s New: %s",
             num,xd->note,
             u->arg2e ? u->arg2e : "");
   
   mydelete(xd->note);
   
   if (!u->arg2e)
     {
       xd->note = mymalloc(1);
       strcpy(xd->note,"");
     }
   else
     {
       xd->note = mymalloc(strlen(u->arg2e) + 1);
       strcpy(xd->note,u->arg2e);
     }
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_chmins(const userinput * const u) {
   int num = 0;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   if (!u->arg2 || !strlen(u->arg2)) {
      u_respond(u,"Try Specifying a Minspeed");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   u_respond(u, "CHMINS: [Pack %i] Old: %1.1f New: %1.1f",
             num,xd->minspeed,atof(u->arg2));
   
   xd->minspeed = gdata.transferminspeed;
   if ( atof(u->arg2) != gdata.transferminspeed )
      xd->minspeed = atof(u->arg2);
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_chmaxs(const userinput * const u) {
   int num = 0;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   if (!u->arg2 || !strlen(u->arg2)) {
      u_respond(u,"Try Specifying a Maxspeed");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   
   u_respond(u, "CHMAXS: [Pack %i] Old: %1.1f New: %1.1f",
             num,xd->maxspeed,atof(u->arg2));
   
   xd->maxspeed = gdata.transfermaxspeed;
   if ( atof(u->arg2) != gdata.transfermaxspeed )
      xd->maxspeed = atof(u->arg2);
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_chlimit(const userinput * const u) {
   int num = 0;
   int val = 0;
   xdcc *xd;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   if (num < 1 || num > irlist_size(&gdata.xdccs)) {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
      }
   
   if (!u->arg2 || !strlen(u->arg2)) {
      u_respond(u,"Try Specifying a daily Downloadlimit");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   val = atoi(u->arg2);

   u_respond(u, "CHLIMIT: [Pack %i] Old: %d New: %d",
             num,xd->dlimit_max,val);
   
   xd->dlimit_max = val;
   if (val == 0)
     xd->dlimit_used = 0;
   else
     xd->dlimit_used = xd->gets + xd->dlimit_max;
   
   write_statefile();
   xdccsavetext();
   
   }

static void u_chlimitinfo(const userinput * const u) {
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  if (!u->arg2 || !strlen(u->arg2))
    {
       u_respond(u, "DLIMIT: [Pack %i] descr removed", num);
       mydelete(xd->dlimit_desc);
       xd->dlimit_desc = NULL;
    }
  else
    {
       u_respond(u, "DLIMIT: [Pack %i] descr: %s", num, u->arg2e);
       xd->dlimit_desc = mycalloc(strlen(u->arg2e)+1);
       strcpy(xd->dlimit_desc,u->arg2e);
    }
   
  write_statefile();
  xdccsavetext();
  }

static void u_chgets(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Count");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  u_respond(u, "CHGETS: [Pack %i] Old: %d New: %d",
            num,xd->gets,atoi(u->arg2));
  
  xd->gets = atoi(u->arg2);
  
  write_statefile();
  xdccsavetext();
}

static void u_lock(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Password");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  
  u_respond(u, "LOCK: [Pack %i] Password: %s", num, u->arg2e);
  xd->lock = mycalloc(strlen(u->arg2e)+1);
  strcpy(xd->lock,u->arg2e);

  write_statefile();
  xdccsavetext();
}

static void u_unlock(const userinput * const u)
{
  int num = 0;
  xdcc *xd;
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  u_respond(u, "UNLOCK: [Pack %i]", num);
  
  mydelete(xd->lock);
  xd->lock = NULL;
  
  write_statefile();
  xdccsavetext();
}

static void u_lockgroup(const userinput * const u)
{
   xdcc *xd;
   int n;
   
   updatecontext();
   
   if (!u->arg1 || !strlen(u->arg1))
     {
       u_respond(u,"Try Specifying a Group");
       return;
     }
   
  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Password");
      return;
    }
  
   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                u_respond(u, "LOCK: [Pack %i] Password: %s", n, u->arg2e);
                xd->lock = mycalloc(strlen(u->arg2e)+1);
                strcpy(xd->lock,u->arg2e);
             }
         }
       xd = irlist_get_next(xd);
     }
  write_statefile();
  xdccsavetext();
}

static void u_unlockgroup(const userinput * const u)
{
   xdcc *xd;
   int n;
   
   updatecontext();
   
   if (!u->arg1 || !strlen(u->arg1))
     {
       u_respond(u,"Try Specifying a Group");
       return;
     }
   
   n = 0;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       n++;
       if (xd->group != NULL)
         {
           if (strcasecmp(xd->group,u->arg1) == 0)
             {
                u_respond(u, "UNLOCK: [Pack %i]", n);
                mydelete(xd->lock);
                xd->lock = NULL;
             }
         }
       xd = irlist_get_next(xd);
     }
  write_statefile();
  xdccsavetext();
}

static void u_groupdesc(const userinput * const u) {
  int k;

  updatecontext();

  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a Valid Group");
      return;
    }
   
  k = reorder_new_groupdesc(u->arg1,u->arg2e);
  if (k == 0)
    return;

  if (u->arg2e && strlen(u->arg2e))
    {
      u_respond(u, "New GROUPDESC: %s",u->arg2e);
    }
  else
    {
      u_respond(u, "Removed GROUPDESC");
    }
  write_statefile();
  xdccsavetext();
}

static void u_group(const userinput * const u) {
  xdcc *xd;
  const char *new;
  char *tmpdesc;
  char *tmpgroup;
  int rc;
  int num = 0;
  
  updatecontext();
  
  if (u->arg1)
    {
      num = atoi(u->arg1);
    }
  
  if (num < 1 || num > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);

  new = u->arg2;
  if (!u->arg2 || !strlen(u->arg2))
    {
      if (xd->group == NULL)
      {
        u_respond(u,"Try Specifying a Group");
        return;
      }
      new = "MAIN";
    }
  else
    {
       if (gdata.groupsincaps)
         caps(u->arg2);
    }
  
  if (xd->group != NULL)
    {
      u_respond(u, "GROUP: [Pack %i] Old: %s New: %s",
                num,xd->group,new);
      /* keep group info for later work */
      tmpgroup = xd->group;
      xd->group = NULL;
      tmpdesc = xd->group_desc;
      xd->group_desc = NULL;
      if (tmpdesc != NULL)
        {
          if (tmpgroup != NULL)
            reorder_new_groupdesc(tmpgroup,tmpdesc);
          mydelete(tmpdesc);
        }
      if (tmpgroup != NULL)
        mydelete(tmpgroup);
    }
  else
    {
      u_respond(u, "GROUP: [Pack %i] New: %s",
                num,u->arg2);
    }
  
  if (new == u->arg2)
    {
      xd->group = mycalloc(strlen(u->arg2)+1);
      strcpy(xd->group,u->arg2);
      reorder_groupdesc(u->arg2);
      rc = add_default_groupdesc(u->arg2);
      if (rc == 1)
        u_respond(u, "New GROUPDESC: %s",u->arg2);
    }
  
  write_statefile();
  xdccsavetext();
}
static void u_movegroup(const userinput * const u) {
  xdcc *xd;
  const char *new;
  char *tmpdesc;
  char *tmpgroup;
  int rc;
  int num;
  int num1 = 0;
  int num2 = 0;
  
  updatecontext();
  
  if (u->arg1)
    {
      num1 = atoi(u->arg1);
    }
  
  if (num1 < 1 || num1 > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  if (u->arg2)
    {
      num2 = atoi(u->arg2);
    }
  
  if (num2 < 1 || num2 > irlist_size(&gdata.xdccs))
    {
      u_respond(u,"Try Specifying a Valid Pack Number");
      return;
    }
  
  new = u->arg3;
  if (!u->arg3 || !strlen(u->arg3))
    {
      new = "MAIN";
    }
  else
    {
       if (gdata.groupsincaps)
         caps(u->arg3);
    }
  
  for (num = num1; num <= num2; num ++)
    {
       xd = irlist_get_nth(&gdata.xdccs, num-1);
       if (xd->group != NULL)
         {
           u_respond(u, "GROUP: [Pack %i] Old: %s New: %s",
                     num,xd->group,new);
           /* keep group info for later work */
           tmpgroup = xd->group;
           xd->group = NULL;
           tmpdesc = xd->group_desc;
           xd->group_desc = NULL;
           if (tmpdesc != NULL)
             {
               if (tmpgroup != NULL)
                 reorder_new_groupdesc(tmpgroup,tmpdesc);
               mydelete(tmpdesc);
             }
           if (tmpgroup != NULL)
             mydelete(tmpgroup);
         }
       else
         {
           u_respond(u, "GROUP: [Pack %i] New: %s",
                     num,u->arg3);
         }
       
       if (new == u->arg3)
         {
           xd->group = mycalloc(strlen(u->arg3)+1);
           strcpy(xd->group,u->arg3);
           reorder_groupdesc(u->arg3);
           rc = add_default_groupdesc(u->arg3);
           if (rc == 1)
             u_respond(u, "New GROUPDESC: %s",u->arg3);
         }
      
     }
  
  write_statefile();
  xdccsavetext();
}


static void u_regroup(const userinput * const u) {
  xdcc *xd;
  const char *g;
  int k;

  updatecontext();

  if (!u->arg1 || !strlen(u->arg1))
    {
      u_respond(u,"Try Specifying a Valid Group");
      return;
    }
   
  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Valid Group");
      return;
    }
   
  if (gdata.groupsincaps)
    caps(u->arg1);
   
  k = 0;
  xd = irlist_get_head(&gdata.xdccs);
  while(xd)
    {
      if (xd->group != NULL)
        g = xd->group;
      else
        g = "main";
      if (strcasecmp(g,u->arg1) == 0)
        {
          k++;
          if (xd->group != NULL)
            mydelete(xd->group);
          xd->group = mycalloc(strlen(u->arg2)+1);
          strcpy(xd->group,u->arg2);
        }
      xd = irlist_get_next(xd);
    }

  if (k == 0)
    return;

  u_respond(u, "GROUP: Old: %s New: %s", u->arg1, u->arg2);
  if (strcasecmp(u->arg1,"main") == 0)
    add_default_groupdesc(u->arg2);
  write_statefile();
  xdccsavetext();
}

static void u_chatme(const userinput * const u) {
   
   updatecontext();
   
   u_respond(u,"Sending You A DCC Chat Request");
   
   if (setupdccchatout(u->snick))
      u_respond(u,"[Failed to listen, try again]");
   
   
   }

static void u_chatl(const userinput * const u)
{
  char *tempstr;
  dccchat_t *chat;
  int count;
  
  updatecontext();
  
  if (!gdata.num_dccchats)
    {
      u_respond(u,"No Active DCC Chats");
      return;
    }
  
  for (chat = irlist_get_head(&gdata.dccchats), count = 1;
       chat;
       chat = irlist_get_next(chat), count++)
    {
      if (chat->status == DCCCHAT_UNUSED)
        {
          continue;
        }
      
      u_respond(u,"DCC CHAT %d:",count);
      switch (chat->status)
        {
        case DCCCHAT_LISTENING:
          u_respond(u,"  Chat sent to %s. Waiting for inbound connection.",
                    chat->nick);
          break;
          
        case DCCCHAT_CONNECTING:
          u_respond(u,"  Chat received from %s. Waiting for outbound connection.",
                    chat->nick);
          break;
          
        case DCCCHAT_AUTHENTICATING:
          u_respond(u,"  Chat established with %s. Waiting for password.",
                    chat->nick);
          break;
          
        case DCCCHAT_CONNECTED:
          u_respond(u,"  Chat established with %s.",
                    chat->nick);
          break;
          
        case DCCCHAT_UNUSED:
        default:
          outerror(OUTERROR_TYPE_CRASH,
                   "Unexpected dccchat state %d", chat->status);
          break;
        }
      
      tempstr = mycalloc(maxtextlengthshort);
      
      getdatestr(tempstr,chat->connecttime,maxtextlengthshort);
      u_respond(u,"  Connected at %s",tempstr);
      
      getdatestr(tempstr,chat->connecttime,maxtextlengthshort);
      u_respond(u,"  Last contact %s",tempstr);
      
      mydelete(tempstr);
      
      u_respond(u,"  Local: %lu.%lu.%lu.%lu:%d, Remote: %lu.%lu.%lu.%lu:%d",
                (chat->localip >> 24) & 0xFF,
                (chat->localip >> 16) & 0xFF,
                (chat->localip >>  8) & 0xFF,
                (chat->localip      ) & 0xFF,
                chat->localport,
                (chat->remoteip >> 24) & 0xFF,
                (chat->remoteip >> 16) & 0xFF,
                (chat->remoteip >>  8) & 0xFF,
                (chat->remoteip      ) & 0xFF,
                chat->remoteport);
    }
  
  return;
}

static void u_closec(const userinput * const u)
{
  int num = -1;
  dccchat_t *chat;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 1) || (num > irlist_size(&gdata.dccchats)))
    {
      u_respond(u,"Invalid ID number, Try \"CHATL\" for a list");
    }
  else
    {
      chat = irlist_get_nth(&gdata.dccchats, num-1);
      
      if (chat == u->chat)
        {
          u_respond(u, "Disconnecting yourself.");
          shutdowndccchat(chat,1);
        }
      else
        {
          writedccchat(chat, 0, "Disconnected due to CLOSEC\n");
          shutdowndccchat(chat,1);
        }
    }
  
  return;
}


static void u_rehash(const userinput * const u) {
   
   /* other variables */
   char *templine = mycalloc(maxtextlength);
   int h,i,filedescriptor,needtojump;
#ifdef MULTINET
   int ss;
   gnetwork_t *backup;
#endif /* MULTINET */
   channel_t *ch, *rch;
   xdcc *xd;
   
   updatecontext();
   
   if (u->method==method_out_all) u_respond(u,"Caught SIGUSR2, Rehashing...");
   
   gdata.r_transferminspeed = gdata.transferminspeed;
   gdata.r_transfermaxspeed = gdata.transfermaxspeed;
   gdata.r_ourip = gdata.getipfromserver ? gdata.ourip : 0;
   
   if (gdata.logfd != FD_UNUSED)
     {
       close(gdata.logfd);
       gdata.logfd = FD_UNUSED;
     }
   
   reinit_config_vars();
   
   set_loginname();
   
   /* go */
   for (h=0; h<MAXCONFIG && gdata.configfile[h]; h++) {
      u_respond(u,"Reloading %s ...",gdata.configfile[h]);
      
      filedescriptor=open(gdata.configfile[h], O_RDONLY | ADDED_OPEN_FLAGS);
      if (filedescriptor < 0) {
         u_respond(u,"Cant Access File, Aborting rehash: %s",strerror(errno));
         u_respond(u,"**WARNING** missing vital information, fix and re-rehash ASAP");
         mydelete(templine);
         return;
         }
   
      while (getfline(templine,maxtextlength,filedescriptor,1))
        {
          if ((templine[0] != '#') && templine[0])
            {
              getconfig_set(templine,1);
            }
        }
      
      close(filedescriptor);
     }
#ifdef MULTINET
   gdata.networks_online ++;
#endif /* MULTINET */
   
   /* see what needs to be redone */
   
   u_respond(u,"Reconfiguring...");
   rehash_dinoex();
   
   /* keep dynamic IP */
   if (gdata.getipfromserver)
     gdata.ourip = gdata.r_ourip;
   gdata.r_ourip = 0;
   
   needtojump=0;
   if (gdata.local_vhost != gdata.r_local_vhost)
     {
       needtojump=1;
     }

#ifdef MULTINET
   backup = gnetwork;
   for (ss=0; ss<gdata.networks_online; ss++)
     {
	gnetwork = &(gdata.networks[ss]);
#endif /* MULTINET */
   
   /* part deleted channels, add common channels */
#ifdef MULTINET
   ch = irlist_get_head(&(gnetwork->channels));
#else /* MULTINET */
   ch = irlist_get_head(&gdata.channels);
#endif /* MULTINET */
   while(ch)
     {
#ifdef MULTINET
       rch = irlist_get_head(&(gnetwork->r_channels));
#else /* MULTINET */
       rch = irlist_get_head(&gdata.r_channels);
#endif /* MULTINET */
       while(rch)
         {
           if (!strcmp(ch->name,rch->name))
             {
               break;
             }
           rch = irlist_get_next(rch);
         }
       
       if (!rch)
         {
           if (!needtojump && (ch->flags & CHAN_ONCHAN))
             {
               writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
             }
           if (gdata.debug > 2)
             {
               ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,
                       "1 = %s parted\n",ch->name);
             }
           clearmemberlist(ch);
           mydelete(ch->name);
           mydelete(ch->key);
#ifdef MULTINET
           ch = irlist_delete(&(gnetwork->channels), ch);
#else /* MULTINET */
           ch = irlist_delete(&gdata.channels, ch);
#endif /* MULTINET */
         }
       else
         {
           rch->flags |= ch->flags & CHAN_ONCHAN;
           rch->members = ch->members;
           mydelete(ch->name);
           mydelete(ch->key);
           *ch = *rch;
           if (gdata.debug > 2)
             {
               ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,
                       "2  = %s common\n", ch->name);
             }
#ifdef MULTINET
           irlist_delete(&(gnetwork->r_channels), rch);
#else /* MULTINET */
           irlist_delete(&gdata.r_channels, rch);
#endif /* MULTINET */
           ch = irlist_get_next(ch);
         }
      }
   
   /* join/add new channels */
   
#ifdef MULTINET
   rch = irlist_get_head(&(gnetwork->r_channels));
#else /* MULTINET */
   rch = irlist_get_head(&gdata.r_channels);
#endif /* MULTINET */
   while(rch)
     {
#ifdef MULTINET
       ch = irlist_get_head(&(gnetwork->channels));
#else /* MULTINET */
       ch = irlist_get_head(&gdata.channels);
#endif /* MULTINET */
       while(ch)
         {
           if (!strcmp(ch->name,rch->name))
             {
               break;
             }
           ch = irlist_get_next(ch);
         }
       
       if (!ch)
         {
#ifdef MULTINET
           ch = irlist_add(&(gnetwork->channels), sizeof(channel_t));
#else /* MULTINET */
           ch = irlist_add(&gdata.channels, sizeof(channel_t));
#endif /* MULTINET */
           *ch = *rch;
           ch->flags &= ~CHAN_ONCHAN;
           if (!needtojump)
             {
               joinchannel(ch);
             }
           if (gdata.debug > 2)
             {
               ioutput(CALLTYPE_NORMAL,OUT_S,COLOR_NO_COLOR,
                       "3 = %s new\n",ch->name);
             }
#ifdef MULTINET
           rch = irlist_delete(&(gnetwork->r_channels), rch);
#else /* MULTINET */
           rch = irlist_delete(&gdata.r_channels, rch);
#endif /* MULTINET */
         }
       else
         {
           outerror(OUTERROR_TYPE_CRASH,"channel found!");
         }
     }
#ifdef MULTINET
       
     } /* networks */
   gnetwork = backup;
#endif /* MULTINET */
   
   gdata.local_vhost = gdata.r_local_vhost;
   gdata.r_local_vhost = 0;
   
   if (needtojump) {
      u_respond(u,"vhost changed, reconnecting");
      switchserver(-1);
      /* switchserver takes care of joining channels */
      }
   
   if (((!gdata.pidfile) && (gdata.r_pidfile)) ||
       ((gdata.pidfile) && (!gdata.r_pidfile)) ||
       ((gdata.pidfile) && (gdata.r_pidfile) && strcmp(gdata.pidfile,gdata.r_pidfile)))
     {
       u_respond(u,"pidfile changed, switching");
       if (gdata.pidfile)
         {
           unlink(gdata.pidfile);
         }
       if (gdata.r_pidfile)
         {
           writepidfile(gdata.r_pidfile);
         }
     }
   mydelete(gdata.pidfile);
   gdata.pidfile = gdata.r_pidfile;
   gdata.r_pidfile = NULL;
   
   if (!gdata.r_config_nick)
     {
       u_respond(u,"user_nick missing! keeping old nick!");
     }
   else
     {
       if (strcmp(gdata.config_nick,gdata.r_config_nick))
         {
#ifdef MULTINET
           backup = gnetwork;
           for (ss=0; ss<gdata.networks_online; ss++)
             {
	        gnetwork = &(gdata.networks[ss]);
                u_respond(u,"user_nick changed, renaming nick to %s", gdata.r_config_nick);
                writeserver(WRITESERVER_NOW, "NICK %s", gdata.r_config_nick);
             }
           gnetwork = backup;
#else /* MULTINET */
           u_respond(u,"user_nick changed, renaming nick to %s", gdata.r_config_nick);
           writeserver(WRITESERVER_NOW, "NICK %s", gdata.r_config_nick);
#endif /* MULTINET */
         }
       mydelete(gdata.config_nick);
       gdata.config_nick = gdata.r_config_nick;
       gdata.r_config_nick = NULL;
     }
   
   gdata.maxb = gdata.overallmaxspeed;
   if (gdata.overallmaxspeeddayspeed != gdata.overallmaxspeed) {
      struct tm *localt;
      localt = localtime(&gdata.curtime);

      if (localt->tm_hour >= gdata.overallmaxspeeddaytimestart
          && localt->tm_hour < gdata.overallmaxspeeddaytimeend
          && ( gdata.overallmaxspeeddaydays & (1 << localt->tm_wday)) )
         gdata.maxb = gdata.overallmaxspeeddayspeed;
      }
   
   if ( gdata.r_transferminspeed != gdata.transferminspeed)
     {
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           if (xd->minspeed == gdata.r_transferminspeed)
             {             
               xd->minspeed = gdata.transferminspeed;
             }
           xd = irlist_get_next(xd);
         }
     }
   
   if ( gdata.r_transfermaxspeed != gdata.transfermaxspeed)
     {
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           if (xd->maxspeed == gdata.r_transfermaxspeed)
             {
               xd->maxspeed = gdata.transfermaxspeed;
             }
           xd = irlist_get_next(xd);
         }
     }
   
   /* check for completeness */
   u_respond(u,"Checking for completeness of config file ...");
   
#ifdef MULTINET
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       if ( !irlist_size(&(gdata.networks[ss].servers))
            || gdata.config_nick == NULL || gdata.user_realname == NULL
            || gdata.user_modes == NULL
            || gdata.slotsmax == 0)
          u_respond(u,"**WARNING** missing vital information, fix and re-rehash ASAP");
     }
#else /* MULTINET */
   if ( !irlist_size(&gdata.servers)
        || gdata.config_nick == NULL || gdata.user_realname == NULL
        || gdata.user_modes == NULL
        || gdata.slotsmax == 0)
      u_respond(u,"**WARNING** missing vital information, fix and re-rehash ASAP");
#endif /* MULTINET */
   
   if ( irlist_size(&gdata.uploadhost) && ( gdata.uploaddir == NULL || strlen(gdata.uploaddir) < 2 ) )
      u_respond(u,"**WARNING** incomplete upload information, fix and re-rehash ASAP");
      
   if ( !irlist_size(&gdata.downloadhost) )
     {
       u_respond(u,"**WARNING** no download hosts defined");
     }
   
   if ( !gdata.statefile )
     {
       u_respond(u,"**WARNING** no state file defined");
     }
   
   u_respond(u,"Done.");
   
   reverify_restrictsend();
   
   for (i=0; i<100; i++)
     {
       if (!gdata.exiting &&
           irlist_size(&gdata.mainqueue) &&
           (irlist_size(&gdata.trans) < min2(MAXTRANS,gdata.slotsmax)))
         {
           sendaqueue(0);
         }
     }
   
   mydelete(templine);

   }

static void u_botinfo(const userinput * const u) {
   char *tempstr = mycalloc(maxtextlength);
   struct rusage r;
   int len;
   int ii;
#ifdef MULTINET
   int ss;
#endif /* MULTINET */
   channel_t *ch;

   updatecontext();
   
   u_respond(u,"BotInfo:");
   
   u_respond(u,"iroffer v" VERSIONLONG ", http://iroffer.dinoex.net/%s%s",
             gdata.hideos ? "" : " - ",
             gdata.hideos ? "" : gdata.osstring);

   getuptime(tempstr, 0, gdata.startuptime, maxtextlength);
   u_respond(u,"iroffer started up %s ago",tempstr);

   getuptime(tempstr, 0, gdata.curtime-gdata.totaluptime, maxtextlength);
   u_respond(u,"total running time of %s",tempstr);

   getrusage(RUSAGE_SELF,&r);
   
   u_respond(u,"cpu usage: %2.2fs user (%2.5f%%), %2.2fs system (%2.5f%%)",
                  ((float)r.ru_utime.tv_sec+(((float)r.ru_utime.tv_usec)/1000000.0)),
            100.0*((float)r.ru_utime.tv_sec+(((float)r.ru_utime.tv_usec)/1000000.0))/((float)(max2(1,gdata.curtime-gdata.startuptime))),
                  ((float)r.ru_stime.tv_sec+(((float)r.ru_stime.tv_usec)/1000000.0)),
            100.0*((float)r.ru_stime.tv_sec+(((float)r.ru_stime.tv_usec)/1000000.0))/((float)(max2(1,gdata.curtime-gdata.startuptime)))
            );

   u_respond(u,"configured nick: %s, actual nick: %s, realname: %s, modes: %s",
             gdata.config_nick,
             (gdata.user_nick ? gdata.user_nick : "??"),
             gdata.user_realname,
             gdata.user_modes);
   
#ifdef MULTINET
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       if (gdata.networks[ss].name == NULL)
         u_respond(u,"network: %d", ss);
       else
         u_respond(u,"network: %d: %s", ss, gdata.networks[ss].name);
       
       switch (gdata.connectionmethod.how)
         {
         case how_direct:
             {
                u_respond(u,"current server: %s:%u (direct)",
                          gdata.networks[ss].curserver.hostname,
                          gdata.networks[ss].curserver.port);
             }
           break;
         case how_bnc:
           if (gdata.connectionmethod.vhost)
             {
               u_respond(u,"current server: %s:%u (bnc at %s:%i with %s)",
                         gdata.networks[ss].curserver.hostname,
                         gdata.networks[ss].curserver.port,
                         gdata.connectionmethod.host,gdata.connectionmethod.port,
                         gdata.connectionmethod.vhost);
             }
           else
             {
               u_respond(u,"current server: %s:%u (bnc at %s:%i)",
                         gdata.networks[ss].curserver.hostname,
                         gdata.networks[ss].curserver.port,
                         gdata.connectionmethod.host,
                         gdata.connectionmethod.port);
             }
           break;
         case how_wingate:
           u_respond(u,"current server: %s:%u (wingate at %s:%i)",
                     gdata.networks[ss].curserver.hostname,
                     gdata.networks[ss].curserver.port,
                     gdata.connectionmethod.host,
                     gdata.connectionmethod.port);
           break;
         case how_custom:
           u_respond(u,"current server: %s:%u (custom at %s:%i)",
                     gdata.networks[ss].curserver.hostname,
                     gdata.networks[ss].curserver.port,
                     gdata.connectionmethod.host,
                     gdata.connectionmethod.port);
           break;
         }
       
       u_respond(u,"current server actual name: %s ",
                 gdata.networks[ss].curserveractualname ? gdata.networks[ss].curserveractualname : "<unknown>");
       
       ch = irlist_get_head(&gdata.networks[ss].channels);
       while(ch)
         {
           snprintf(tempstr, maxtextlength - 1,
                    "channel %10s: joined: %3s",
                    ch->name,
                    ch->flags & CHAN_ONCHAN ? "yes" : "no ");
           len = strlen(tempstr);
           
           if (ch->key)
             {
               snprintf(tempstr + len, maxtextlength - 1 - len,
                        ", key: %s",
                        ch->key);
               len = strlen(tempstr);
             }
           
           if (ch->plisttime)
             {
               snprintf(tempstr + len, maxtextlength - 1 - len,
                        ", plist every %2i min (%s)",
                        ch->plisttime,
                        ch->flags & CHAN_MINIMAL ? "minimal" : (ch->flags & CHAN_SUMMARY ? "summary" : "full"));
             }
           
           u_respond(u,"%s",tempstr);
           
           ch = irlist_get_next(ch);
         }
     }
#else /* MULTINET */
   switch (gdata.connectionmethod.how)
     {
     case how_direct:
       u_respond(u,"current server: %s:%u (direct)",
                 gdata.curserver.hostname,gdata.curserver.port);
       break;
     case how_bnc:
       if (gdata.connectionmethod.vhost)
         {
           u_respond(u,"current server: %s:%u (bnc at %s:%i with %s)",
                     gdata.curserver.hostname,gdata.curserver.port,gdata.connectionmethod.host,gdata.connectionmethod.port,gdata.connectionmethod.vhost);
         }
       else
         {
           u_respond(u,"current server: %s:%u (bnc at %s:%i)",
                     gdata.curserver.hostname,gdata.curserver.port,gdata.connectionmethod.host,gdata.connectionmethod.port);
         }
       break;
     case how_wingate:
       u_respond(u,"current server: %s:%u (wingate at %s:%i)",
                 gdata.curserver.hostname,gdata.curserver.port,gdata.connectionmethod.host,gdata.connectionmethod.port);
       break;
     case how_custom:
       u_respond(u,"current server: %s:%u (custom at %s:%i)",
                 gdata.curserver.hostname,gdata.curserver.port,gdata.connectionmethod.host,gdata.connectionmethod.port);
       break;
     }
   
   u_respond(u,"current server actual name: %s ",
             gdata.curserveractualname ? gdata.curserveractualname : "<unknown>");
   
   ch = irlist_get_head(&gdata.channels);
   while(ch)
     {
       snprintf(tempstr, maxtextlength - 1,
                "channel %10s: joined: %3s",
                ch->name,
                ch->flags & CHAN_ONCHAN ? "yes" : "no ");
       len = strlen(tempstr);
       
       if (ch->key)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", key: %s",
                    ch->key);
           len = strlen(tempstr);
         }
       
       if (ch->plisttime)
         {
           snprintf(tempstr + len, maxtextlength - 1 - len,
                    ", plist every %2i min (%s)",
                    ch->plisttime,
                    ch->flags & CHAN_MINIMAL ? "minimal" : (ch->flags & CHAN_SUMMARY ? "summary" : "full"));
         }
       
       u_respond(u,"%s",tempstr);
       
       ch = irlist_get_next(ch);
     }
#endif /* MULTINET */
   
   u_respond(u, "bandwidth: lowsend: %i, minspeed: %1.1f, maxspeed: %1.1f, overallmaxspeed: %i",
             gdata.lowbdwth,gdata.transferminspeed,gdata.transfermaxspeed,gdata.maxb/4);
   
   if (gdata.overallmaxspeed != gdata.overallmaxspeeddayspeed)
     {
       u_respond(u, "           default max: %i, day max: %i ( %i:00 -> %i:59, days=\"%s%s%s%s%s%s%s\" )",
                 gdata.overallmaxspeed/4,gdata.overallmaxspeeddayspeed/4,
                 gdata.overallmaxspeeddaytimestart,gdata.overallmaxspeeddaytimeend-1,
                 (gdata.overallmaxspeeddaydays & (1 << 1)) ? "M":"",
                 (gdata.overallmaxspeeddaydays & (1 << 2)) ? "T":"",
                 (gdata.overallmaxspeeddaydays & (1 << 3)) ? "W":"",
                 (gdata.overallmaxspeeddaydays & (1 << 4)) ? "R":"",
                 (gdata.overallmaxspeeddaydays & (1 << 5)) ? "F":"",
                 (gdata.overallmaxspeeddaydays & (1 << 6)) ? "S":"",
                 (gdata.overallmaxspeeddaydays & (1 << 0)) ? "U":"");
     }
   else
     {
       u_respond(u, "           default max: %i, day max: (same)",
                 gdata.overallmaxspeed/4);
     }
   
   for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
     {
       char *tempstr2 = mycalloc(maxtextlength);
       
       getdatestr(tempstr2, gdata.transferlimits[ii].ends, maxtextlength);
       
       if (gdata.transferlimits[ii].limit)
         {
           u_respond(u, "transferlimit: %7s (ends %s): used %" LLPRINTFMT "uMB, limit %" LLPRINTFMT "uMB",
                     transferlimit_type_to_string(ii), tempstr2,
                     gdata.transferlimits[ii].used / 1024 / 1024,
                     gdata.transferlimits[ii].limit / 1024 / 1024);
         }
       else
         {
           u_respond(u, "transferlimit: %7s (ends %s): used %" LLPRINTFMT "uMB, limit unlimited",
                     transferlimit_type_to_string(ii),tempstr2 ,
                     gdata.transferlimits[ii].used / 1024 / 1024);
         }
     }
   
   u_respond(u, "files: pid: %s, log: %s, state: %s, xdcclist: %s",
             (gdata.pidfile?gdata.pidfile:"(none)"),
             (gdata.logfile?gdata.logfile:"(none)"),
             (gdata.statefile?gdata.statefile:"(none)"),
             (gdata.xdcclistfile?gdata.xdcclistfile:"(none)"));
   
   u_respond(u, "config file%s: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             gdata.configfile[1]?"s":"",
             gdata.configfile[0],
             gdata.configfile[1]?", ":"",gdata.configfile[1]?gdata.configfile[1]:"",
             gdata.configfile[2]?", ":"",gdata.configfile[2]?gdata.configfile[2]:"",
             gdata.configfile[3]?", ":"",gdata.configfile[3]?gdata.configfile[3]:"",
             gdata.configfile[4]?", ":"",gdata.configfile[4]?gdata.configfile[4]:"",
             gdata.configfile[5]?", ":"",gdata.configfile[5]?gdata.configfile[5]:"",
             gdata.configfile[6]?", ":"",gdata.configfile[6]?gdata.configfile[6]:"",
             gdata.configfile[7]?", ":"",gdata.configfile[7]?gdata.configfile[7]:"",
             gdata.configfile[8]?", ":"",gdata.configfile[8]?gdata.configfile[8]:"",
             gdata.configfile[9]?", ":"",gdata.configfile[9]?gdata.configfile[9]:""
             );
   
   if (irlist_size(&gdata.uploadhost)) {
      snprintf(tempstr,maxtextlength - 1,
               "%" LLPRINTFMT "iMB",
               (long long)(gdata.uploadmaxsize/1024/1024));
      u_respond(u, "upload allowed, dir: %s, max size: %s",
                gdata.uploaddir,
                gdata.uploadmaxsize?tempstr:"none");
      }
   
   if (gdata.stdout_buffer_init)
     {
       u_respond(u,"console buffering: %u written, %u flushed, %u dropped (%u queued)",
                 gdata.stdout_buffer.count_written,
                 gdata.stdout_buffer.count_flushed,
                 gdata.stdout_buffer.count_dropped,
                 gdata.stdout_buffer.count_written - gdata.stdout_buffer.count_flushed);
     }
   
   u_respond(u,"transfer method: %s (blocksize %d)",
#if defined(HAVE_LINUX_SENDFILE)
             (gdata.transfermethod == TRANSFERMETHOD_LINUX_SENDFILE) ? "linux-sendfile" :
#endif
#if defined(HAVE_FREEBSD_SENDFILE)
             (gdata.transfermethod == TRANSFERMETHOD_FREEBSD_SENDFILE) ? "freebsd-sendfile" :
#endif
#if defined(HAVE_MMAP)
             (gdata.transfermethod == TRANSFERMETHOD_MMAP) ? "mmap/write" :
#endif
             (gdata.transfermethod == TRANSFERMETHOD_READ_WRITE) ? "read/write" : "unknown",
             BUFFERSIZE);
   
   if (gdata.delayedshutdown) {
      u_respond(u,"NOTICE: Delayed shutdown activated, iroffer will shutdown once there are no active transfers");
      u_respond(u,"NOTICE: To cancel the delayed shutdown, issue \"SHUTDOWN CANCEL\"");
      }
   
   mydelete(tempstr);
   }


static void u_ignl(const userinput * const u)
{
  int first;
  int left, ago;
  igninfo *ignore;
  
  updatecontext();
  
  if (!irlist_size(&gdata.ignorelist))
    {
      u_respond(u,"No Hosts Ignored or Watched");
      return;
    }
  
  first = 1;
  ignore = irlist_get_head(&gdata.ignorelist);
  while(ignore)
    {
      if (ignore->flags & IGN_IGNORING)
        {
          if (first)
            {
              u_respond(u,"Current Ignore List:");
              u_respond(u,"  Last Request  Un-Ignore    Type  Hostmask");
              first = 0;
            }
          
          ago = gdata.curtime-ignore->lastcontact;
          left = gdata.autoignore_threshold*(ignore->bucket+1);
          
          u_respond(u,"  %4i%c%02i%c ago   %4i%c%02i%c  %6s  %-32s",
                    ago < 3600 ? ago/60 : ago/60/60 ,
                    ago < 3600 ? 'm' : 'h',
                    ago < 3600 ? ago%60 : (ago/60)%60 ,
                    ago < 3600 ? 's' : 'm',
                    left < 3600 ? left/60 : left/60/60 ,
                    left < 3600 ? 'm' : 'h',
                    left < 3600 ? left%60 : (left/60)%60 ,
                    left < 3600 ? 's' : 'm',
                    ignore->flags & IGN_MANUAL ? "manual" : "auto",
                    ignore->hostmask);
        }
      ignore = irlist_get_next(ignore);
    }

  first = 1;
  ignore = irlist_get_head(&gdata.ignorelist);
  while(ignore)
    {
      if (!(ignore->flags & IGN_IGNORING))
        {
          if (first)
            {
              u_respond(u,"Current Watch List:");
              u_respond(u,"  Last Request   Un-Watch          Hostmask");
              first = 0;
            }
          
          ago = gdata.curtime-ignore->lastcontact;
          left = gdata.autoignore_threshold*(ignore->bucket+1);
          
          u_respond(u,"  %4i%c%02i%c ago   %4i%c%02i%c          %-32s",
                    ago < 3600 ? ago/60 : ago/60/60 ,
                    ago < 3600 ? 'm' : 'h',
                    ago < 3600 ? ago%60 : (ago/60)%60 ,
                    ago < 3600 ? 's' : 'm',
                    left < 3600 ? left/60 : left/60/60 ,
                    left < 3600 ? 'm' : 'h',
                    left < 3600 ? left%60 : (left/60)%60 ,
                    left < 3600 ? 's' : 'm',
                    ignore->hostmask);
        }
      ignore = irlist_get_next(ignore);
    }

  return;
}

static void u_ignore(const userinput * const u)
{
  int num=0;
  igninfo *ignore;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if (!u->arg1)
    {
      u_respond(u,"Try specifying an amount of time to ignore");        
      return;
    }
  
  if (!u->arg2 || strlen(u->arg2) < 4)
    {
      u_respond(u,"Try specifying a hostmask longer than 4 characters");
      return;
    }
  
  ignore = irlist_get_head(&gdata.ignorelist);
  while(ignore)
    {
      if (ignore->regexp && !regexec(ignore->regexp,u->arg2,0,NULL,0))
        {
          break;
        }
      ignore = irlist_get_next(ignore);
    }
  
  if (!ignore)
    {
      char *tempstr;
      
      ignore = irlist_add(&gdata.ignorelist, sizeof(igninfo));
      ignore->regexp = mycalloc(sizeof(regex_t));
      
      ignore->hostmask = mymalloc(strlen(u->arg2)+1);
      strcpy(ignore->hostmask,u->arg2);
      
      tempstr = hostmasktoregex(u->arg2);
      if (regcomp(ignore->regexp,tempstr,REG_ICASE|REG_NOSUB))
        {
          ignore->regexp = NULL;
        }
      
      ignore->flags |= IGN_IGNORING;
      ignore->lastcontact = gdata.curtime;
      
      mydelete(tempstr);
    }
  
  ignore->flags |= IGN_MANUAL;
  ignore->bucket = (num*60)/gdata.autoignore_threshold;
  
  u_respond(u, "Ignore activated for %s which will last %i min",
            u->arg2,num);
  write_statefile();
  
}


static void u_unignore(const userinput * const u)
{
  igninfo *ignore;
  
  updatecontext();
  
  if (!u->arg1)
    {
      u_respond(u,"Try specifying a hostmask to un-ignore");
      return;
    }
  
  ignore = irlist_get_head(&gdata.ignorelist);
  while(ignore)
    {
      if (strcmp(ignore->hostmask,u->arg1) == 0)
        {
          mydelete(ignore->hostmask);
          if (ignore->regexp)
            {
              regfree(ignore->regexp);
            }
          mydelete(ignore->regexp);
          irlist_delete(&gdata.ignorelist, ignore);
          
          u_respond(u, "Ignore removed for %s",u->arg1);
          write_statefile();
          
          break;
        }
      else
        {
          ignore = irlist_get_next(ignore);
        }
    }
  
  if (!ignore)
    {
      u_respond(u,"Hostmask not found");
    }
  
  return;
}


static void u_nosave(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   gdata.noautosave=gdata.curtime + 60*num - 1;
   u_respond(u,"** XDCC AutoSave has been disabled for the next %i minute%s",num,num!=1?"s":"");
   
   }


static void u_nosend(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   gdata.nonewcons=gdata.curtime + 60*num - 1;
   u_respond(u,"** XDCC Send has been disabled for the next %i minute%s",num,num!=1?"s":"");
   
   }


static void u_nolist(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   gdata.nolisting=gdata.curtime + 60*num - 1;
   u_respond(u,"** XDCC List and PLIST have been disabled for the next %i minute%s",num,num!=1?"s":"");
   
   }


static void u_renumber(const userinput * const u)
{
  int oldp = 0, newp = 0;
  xdcc *xdo, *xdn;
  
  updatecontext();
  
  if (u->arg1) oldp = atoi(u->arg1);
  if (u->arg2) newp = atoi(u->arg2);
  
  if ((oldp < 1) ||
      (oldp > irlist_size(&gdata.xdccs)) ||
      (newp < 1) ||
      (newp > irlist_size(&gdata.xdccs)) ||
      (newp == oldp))
    {
      u_respond(u,"Invalid pack number");
      return;
    }
  
  u_respond(u,"** Moved pack %i to %i",oldp,newp);
  
  /* get pack we are renumbering */
  xdo = irlist_get_nth(&gdata.xdccs, oldp-1);
  irlist_remove(&gdata.xdccs, xdo);

  if (newp == 1)
    {
      irlist_insert_head(&gdata.xdccs, xdo);
    }
  else
    {
      xdn = irlist_get_nth(&gdata.xdccs, newp-2);
      irlist_insert_after(&gdata.xdccs, xdo, xdn);
    }
  
  write_statefile();
  xdccsavetext();
}


static void u_msgread(const userinput * const u)
{
  int count;
  char *tempstr;
  msglog_t *ml;
  
  updatecontext();
  
  tempstr = mycalloc(maxtextlength);
   
  for (ml = irlist_get_head(&gdata.msglog); ml; ml = irlist_get_next(ml))
    {
      getdatestr(tempstr, ml->when, maxtextlength);
      
      u_respond(u, "%s: %s", tempstr, ml->hostmask);
      u_respond(u, " ^- %s", ml->message);
    }
  
  mydelete(tempstr);
  
  count = irlist_size(&gdata.msglog);
  u_respond(u, "msglog: %i message%s in log%s%s",
            count,
            count != 1 ? "s" : "",
            count ? ", use MSGDEL to remove " : "",
            count > 1 ? "them" : (count == 1 ? "it" : ""));
  return;
}


static void u_msgdel(const userinput * const u)
{
  msglog_t *ml;
  
  updatecontext();
  
  u_respond(u,"msglog: deleted %d messages",
            irlist_size(&gdata.msglog));
  
  while ((ml = irlist_get_head(&gdata.msglog)))
    {
      mydelete(ml->hostmask);
      mydelete(ml->message);
      irlist_delete(&gdata.msglog, ml);
    }
  
  write_statefile();
  return;
}


static void u_memstat(const userinput * const u)
{
  int i;
  long numcountrecent, sizecount;
  struct rusage r;
#ifdef HAVE_MMAP
  xdcc *xd;
  int mmap_count;
#endif
  
  updatecontext();
  
  u_respond(u,"iroffer memory usage:");
  
  getrusage(RUSAGE_SELF,&r);
  
  u_respond(u,"rusage: maxrss %li, ixrss %li, idrss %li, "
            "isrss %li, minflt %li, majflt %li, nswap %li",
            r.ru_maxrss, r.ru_ixrss, r.ru_idrss,
            r.ru_isrss, r.ru_minflt, r.ru_majflt, r.ru_nswap);
  u_respond(u,"        inbloc %li, oublock %li, msgsnd %li, "
            "msgrcv %li, nsignals %li, nvcsw %li, nivcsw %li",
            r.ru_inblock, r.ru_oublock, r.ru_msgsnd,
            r.ru_msgrcv, r.ru_nsignals, r.ru_nvcsw, r.ru_nivcsw);
  
  u_respond(u, "gdata:  %d bytes", sizeof(gdata_t));
  
  numcountrecent = sizecount = 0;
  for (i=0; i<(MEMINFOHASHSIZE * gdata.meminfo_depth); i++)
    {
      if (gdata.meminfo[i].ptr != NULL)
        {
          sizecount += gdata.meminfo[i].size;
          if (gdata.meminfo[i].alloctime > gdata.curtime-600)
            {
              numcountrecent++;
            }
        }
    }
  
  u_respond(u, "heap:   %li bytes, %i allocations (%li created in past 10 min) (depth %d)",
            sizecount,
            gdata.meminfo_count,
            numcountrecent,
            gdata.meminfo_depth);
  
#ifdef HAVE_MMAP
  mmap_count = 0;
  for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd))
    {
      mmap_count += irlist_size(&xd->mmaps);
    }
  
  u_respond(u,"mmaps:  %i kbytes, %d file mappings",
            mmap_count * IR_MMAP_SIZE / 1024, mmap_count);
#endif
  
  if (u->arg1 && !strcmp(u->arg1,"list"))
    {
      meminfo_t *meminfo;
      meminfo_t *meminfo2 = NULL;
      int meminfo_depth;
      
      /*
       * we need to copy the entire table so we dont walk it
       * while it while it can be modified
       */
    again:
      meminfo_depth = gdata.meminfo_depth;
      meminfo = mycalloc(sizeof(meminfo_t) * MEMINFOHASHSIZE * meminfo_depth);
      if (meminfo_depth != gdata.meminfo_depth)
        {
          meminfo2 = meminfo;
          goto again;
        }
      memcpy(meminfo, gdata.meminfo,
             sizeof(meminfo_t) * MEMINFOHASHSIZE * meminfo_depth);
      
      u_respond(u,"iroffer heap details:");
      u_respond(u,"     id |    address |    size |     when | where");
      
      for (i=0; i<(MEMINFOHASHSIZE * meminfo_depth); i++)
        {
          if (meminfo[i].ptr != NULL)
            {
              u_respond(u, "%3i %3i | 0x%8.8lX | %6iB | %7lis | %s:%d %s()",
                        i / meminfo_depth,
                        i % meminfo_depth,
                        (long)meminfo[i].ptr,
                        meminfo[i].size,
                        meminfo[i].alloctime-gdata.startuptime,
                        meminfo[i].src_file,
                        meminfo[i].src_line,
                        meminfo[i].src_func);
            }
        }
      mydelete(meminfo);
      mydelete(meminfo2);
    }
  
#ifdef HAVE_MMAP
  if (u->arg1 && !strcmp(u->arg1,"list") && mmap_count)
    {
      int pack_count = 1;
      
      u_respond(u,"iroffer memmap details:");
      u_respond(u," pack |                 location |    address | references");
      
      for (xd = irlist_get_head(&gdata.xdccs); xd; xd = irlist_get_next(xd))
        {
          mmap_info_t *mm;
          
          irlist_sort(&xd->mmaps, irlist_sort_cmpfunc_off_t, NULL);
          
          for (mm = irlist_get_head(&xd->mmaps); mm; mm = irlist_get_next(mm))
            {
              u_respond(u," %4i | 0x%8.8" LLPRINTFMT "X .. 0x%8.8" LLPRINTFMT "X | %p | %10d",
                        pack_count,
                        (unsigned long long)mm->mmap_offset,
                        (unsigned long long)mm->mmap_offset + (unsigned long long)mm->mmap_size - 1,
                        mm->mmap_ptr,
                        mm->ref_count);
            }
          pack_count++;
        }
    }
#endif
  
  if (!u->arg1 || strcmp(u->arg1,"list"))
    {
      u_respond(u,"for a detailed listing use \"memstat list\"");
    }
}


static void u_qsend(const userinput * const u)
{
  
  updatecontext();
  
  if (!irlist_size(&gdata.mainqueue))
    {
      u_respond(u,"No Users Queued");
      return;
    }
  
  if (irlist_size(&gdata.trans) >= MAXTRANS)
    {
      u_respond(u,"Too many transfers");
      return;
    }
  
  sendaqueue(2);
  return;
}

static void u_slotsmax(const userinput * const u) {
   int val;
   
   updatecontext();
    
   if (u->arg1)
     {
       val = atoi(u->arg1);
       gdata.slotsmax = between(1,val,MAXTRANS);
     }
   u_respond(u,"SLOTSMAX now %d", gdata.slotsmax);
}

static void u_queuesize(const userinput * const u) {
   int val;
   
   updatecontext();
    
   if (u->arg1)
     {
       val = atoi(u->arg1);
       gdata.queuesize = between(0,val,1000000);
     }
   u_respond(u,"QUEUESIZE now %d", gdata.queuesize);
}

static void u_holdqueue(const userinput * const u) {
   int val;
   
   updatecontext();
    
   if (gdata.holdqueue)
     {
       val = 0;
     }
   else
     {
       val = 1;
     }
   
   if (u->arg1)
     {
       val = atoi(u->arg1);
     }
   
   gdata.holdqueue = val;
   u_respond(u,"HOLDQUEUE now %d", val);
}

static void u_identify(const userinput * const u)
{
#ifdef MULTINET
  gnetwork_t *backup;
  int net;
  
  updatecontext();
  
  net = get_network(u->arg1);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
#endif /* MULTINET */
  
  if (!gdata.nickserv_pass)
    {
       u_respond(u,"No nickserv_pass set!");
       return;
    } 
  
#ifdef MULTINET
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
#endif /* MULTINET */
  privmsg("nickserv","IDENTIFY %s",gdata.nickserv_pass);
#ifdef MULTINET
  gnetwork = backup;
#endif /* MULTINET */
  u_respond(u,"nickserv identify send.");
}

static void u_shutdown(const userinput * const u) {
   updatecontext();
   
   if (u->arg1)
     {
       caps(u->arg1);
     }
   
   if (!u->arg1 || (strcmp(u->arg1,"NOW") && strcmp(u->arg1,"DELAYED") && strcmp(u->arg1,"CANCEL")) ) {
      u_respond(u,"Usage: SHUTDOWN <now|delayed|cancel>");
      return;
      }
   
   if (!strcmp(u->arg1,"NOW")) {
      shutdowniroffer();
      }
   else if (!strcmp(u->arg1,"DELAYED")) {
      u_respond(u,"Delayed shutdown activated, iroffer will shutdown once there are no active transfers");
      u_respond(u,"To cancel the delayed shutdown, issue \"SHUTDOWN CANCEL\"");
      gdata.delayedshutdown=1;
      }
   else if (!strcmp(u->arg1,"CANCEL")) {
      u_respond(u,"Delayed shutdown canceled");
      gdata.delayedshutdown=0;
      }
   
   }

static void u_debug(const userinput * const u) {
   updatecontext();
   
   if (!u->arg1) return;
   
   gdata.debug = atoi(u->arg1);
   }

static void u_servqc(const userinput * const u)
{
#ifdef MULTINET
  int ss;
#endif /* MULTINET */
  updatecontext();
  
#ifdef MULTINET
  for (ss=0; ss<gdata.networks_online; ss++)
    {
      u_respond(u,"Cleared server queue of %d lines",
                irlist_size(&gdata.networks[ss].serverq_fast) +
                irlist_size(&gdata.networks[ss].serverq_normal) +
                irlist_size(&gdata.networks[ss].serverq_slow));
      
      irlist_delete_all(&gdata.networks[ss].serverq_fast);
      irlist_delete_all(&gdata.networks[ss].serverq_normal);
      irlist_delete_all(&gdata.networks[ss].serverq_slow);
    }
#else /* MULTINET */
  u_respond(u,"Cleared server queue of %d lines",
            irlist_size(&gdata.serverq_fast) +
            irlist_size(&gdata.serverq_normal) +
            irlist_size(&gdata.serverq_slow));
  
  irlist_delete_all(&gdata.serverq_fast);
  irlist_delete_all(&gdata.serverq_normal);
  irlist_delete_all(&gdata.serverq_slow);
#endif /* MULTINET */
  return;
}

static void u_hop(const userinput * const u)
{
   channel_t *ch;
#ifdef MULTINET
   gnetwork_t *backup;
   int ss;
#endif /* MULTINET */
   
   updatecontext();
   
#ifdef MULTINET
   backup = gnetwork;
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       gnetwork = &(gdata.networks[ss]);
       /* part & join channels */
       ch = irlist_get_head(&(gnetwork->channels));
       while(ch)
         {
           if ((!u->arg1) || (!strcasecmp(u->arg1,ch->name)))
             {
               writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
               clearmemberlist(ch);
               ch->flags &= ~CHAN_ONCHAN;
               ch->flags &= ~CHAN_KICKED;
               joinchannel(ch);
             }
           ch = irlist_get_next(ch);
         }
     }
   gnetwork = backup;
#else /* MULTINET */
   /* part & join channels */
   ch = irlist_get_head(&gdata.channels);
   while(ch)
     {
       if ((!u->arg1) || (!strcasecmp(u->arg1,ch->name)))
         {
           writeserver(WRITESERVER_NORMAL, "PART %s", ch->name);
           clearmemberlist(ch);
           ch->flags &= ~CHAN_ONCHAN;
           ch->flags &= ~CHAN_KICKED;
           joinchannel(ch);
         }
       ch = irlist_get_next(ch);
     }
#endif /* MULTINET */
}

static void u_jump(const userinput * const u)
{
#ifdef MULTINET
  gnetwork_t *backup;
  int net;
#endif /* MULTINET */
  
  updatecontext();
  
#ifdef MULTINET
  net = get_network(u->arg2);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
#endif /* MULTINET */

  if (u->arg1)
    {
      int num;
      num = atoi(u->arg1);
      
#ifdef MULTINET
      if ((num < 0) || (num > irlist_size(&gdata.networks[net].servers)))
#else /* MULTINET */
      if ((num < 1) || (num > irlist_size(&gdata.servers)))
#endif /* MULTINET */
        {
          u_respond(u,"Try specifying a valid server number, use \"servers\" for a list");
        }
      else
        {
#ifdef MULTINET
          backup = gnetwork;
	  gnetwork = &(gdata.networks[net]);
          gnetwork->serverconnectbackoff = 0;
#else /* MULTINET */
          gdata.serverconnectbackoff = 0;
#endif /* MULTINET */
          switchserver(num-1);
#ifdef MULTINET
          gnetwork = backup;
#endif /* MULTINET */
        }
    }
  else
    {
#ifdef MULTINET
      backup = gnetwork;
      gnetwork = &(gdata.networks[net]);
      gnetwork->serverconnectbackoff = 0;
#else /* MULTINET */
      gdata.serverconnectbackoff = 0;
#endif /* MULTINET */
      switchserver(-1);
#ifdef MULTINET
      gnetwork = backup;
#endif /* MULTINET */
    }
}

static void u_servers(const userinput * const u)
{
  int i;
  server_t *ss;
#ifdef MULTINET
  int net;
#endif /* MULTINET */
  
  updatecontext();
  
#ifdef MULTINET
  net = get_network(u->arg1);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
#endif /* MULTINET */
  
  u_respond(u,"Server List:");
  u_respond(u,"  Num  Server                         Port  Password");
  
#ifdef MULTINET
  ss = irlist_get_head(&gdata.networks[net].servers);
#else /* MULTINET */
  ss = irlist_get_head(&gdata.servers);
#endif /* MULTINET */
  i = 1;
  while(ss)
    {
      u_respond(u,"  %3i  %-27s  %6u  %s",
                i,
                ss->hostname,
                ss->port,
                ss->password ? "(hidden)" : "(none)");
      ss = irlist_get_next(ss);
      i++;
    }
  
  u_respond(u,"Current Server: %s:%u (%s)",
#ifdef MULTINET
            gdata.networks[net].curserver.hostname,
            gdata.networks[net].curserver.port,
            gdata.networks[net].curserveractualname ? gdata.networks[net].curserveractualname : "<unknown>");
#else /* MULTINET */
            gdata.curserver.hostname,
            gdata.curserver.port,
            gdata.curserveractualname ? gdata.curserveractualname : "<unknown>");
#endif /* MULTINET */
  
}

static void u_trinfo(const userinput * const u)
{
  int num = -1;
  char *tempstr2, *tempstr3;
  const char *y;
  int left,started,lcontact;
  transfer *tr;
  
  updatecontext();
  
  if (u->arg1) num = atoi(u->arg1);
  
  if ((num < 0) || !does_tr_id_exist(num))
    {
      u_respond(u,"Try Specifying a Valid Transfer Number");
      return;
    }
  
  u_respond(u,"Transfer Info for ID %i:",num);
  
  tr = does_tr_id_exist(num);
  
  switch (tr->tr_status)
    {
    case TRANSFER_STATUS_LISTENING:
      y = "Listening";
      break;
      
    case TRANSFER_STATUS_SENDING:
      y = "Sending";
      break;
      
    case TRANSFER_STATUS_WAITING:
      y = "Finishing";
      break;
      
    case TRANSFER_STATUS_DONE:
      y = "Closing";
      break;
      
    default:
      y = "Unknown!";
      break;
    }
  
  u_respond(u,"User %s, Hostname %s, Status %s",
            tr->nick,tr->hostname,y);
  
  u_respond(u,"File: %s",getfilename(tr->xpack->file));
  
  u_respond(u,"Start %" LLPRINTFMT "iK, Current %" LLPRINTFMT "iK, End %" LLPRINTFMT "iK (%2.0f%% File, %2.0f%% Xfer)",
            (long long)((tr->startresume)/1024),
            (long long)((tr->bytessent)/1024),
            (long long)((tr->xpack->st_size)/1024),
            ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size),
            ((float)(tr->bytessent-tr->startresume))*100.0/((float)max2(1,(tr->xpack->st_size-tr->startresume))));
  
  tempstr2 = mycalloc(maxtextlengthshort);
  tempstr3 = mycalloc(maxtextlengthshort);
  
  snprintf(tempstr2,maxtextlengthshort-1,"%1.1fK/s",tr->xpack->minspeed);
  snprintf(tempstr3,maxtextlengthshort-1,"%1.1fK/s",tr->xpack->maxspeed);
  
  u_respond(u,"Min %s, Current %1.1fK/s, Max %s, In Transit %" LLPRINTFMT "iK",
            (tr->nomin || (tr->xpack->minspeed == 0.0)) ? "no" : tempstr2 ,
            tr->lastspeed,
            (tr->nomax || (tr->xpack->maxspeed == 0.0)) ? "no" : tempstr3 ,
            (long long)(tr->bytessent-tr->lastack)/1024);
  
  mydelete(tempstr2);
  mydelete(tempstr3);
  
  left     = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
  started  = min2(359999,gdata.curtime-tr->connecttime);
  lcontact = min2(359999,gdata.curtime-tr->lastcontact);
  u_respond(u,"Transfer started %i%c %i%c ago, Finish in %i%c %i%c, Last contact %i%c %i%c ago.",
            started < 3600 ? started/60 : started/60/60 ,
            started < 3600 ? 'm' : 'h',
            started < 3600 ? started%60 : (started/60)%60 ,
            started < 3600 ? 's' : 'm',
            left < 3600 ? left/60 : left/60/60 ,
            left < 3600 ? 'm' : 'h',
            left < 3600 ? left%60 : (left/60)%60 ,
            left < 3600 ? 's' : 'm',
            lcontact < 3600 ? lcontact/60 : lcontact/60/60 ,
            lcontact < 3600 ? 'm' : 'h',
            lcontact < 3600 ? lcontact%60 : (lcontact/60)%60 ,
            lcontact < 3600 ? 's' : 'm');
  
  u_respond(u,"Local: %ld.%ld.%ld.%ld:%d, Remote: %ld.%ld.%ld.%ld:%d",
            tr->localip>>24, (tr->localip>>16) & 0xFF, (tr->localip>>8) & 0xFF, tr->localip & 0xFF, tr->listenport,
            tr->remoteip>>24, (tr->remoteip>>16) & 0xFF, (tr->remoteip>>8) & 0xFF, tr->remoteip & 0xFF, tr->remoteport);
  
  u_respond(u,"Sockets: Listen %i, Transfer %i, File %i",
            (tr->listensocket == FD_UNUSED) ? 0 : tr->listensocket,
            (tr->clientsocket == FD_UNUSED) ? 0 : tr->clientsocket,
            (tr->xpack->file_fd == FD_UNUSED) ? 0 : tr->xpack->file_fd);
  
#ifdef HAVE_MMAP
  if (tr->mmap_info)
    {
      u_respond(u,"MMAP: [%p] 0x%.8" LLPRINTFMT "X .. 0x%.8" LLPRINTFMT "X .. 0x%.8" LLPRINTFMT "X",
                tr->mmap_info->mmap_ptr,
                (unsigned long long)tr->mmap_info->mmap_offset,
                (unsigned long long)tr->bytessent,
                (unsigned long long)tr->mmap_info->mmap_offset + (unsigned long long)tr->mmap_info->mmap_size - 1);
    }
#endif
  
}


static void u_listdir(const userinput * const u, const char *dir)
{
  DIR *d;
  struct dirent *f;
  char *thefile, *tempstr;
  irlist_t dirlist = {};
  int thedirlen;
#ifndef NO_STATVFS
  struct statvfs stf;
#else
#ifndef NO_STATFS
  struct statfs stf;
#endif
#endif
  
  updatecontext();
  
  thedirlen = strlen(dir);
  
  d = opendir(dir);
  
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  while ((f = readdir(d)))
    {
      if (strcmp(f->d_name,".") && strcmp(f->d_name,".."))
        {
          thefile = irlist_add(&dirlist, strlen(f->d_name) + 1);
          strcpy(thefile, f->d_name);
        }
    }
  
  closedir(d);
  
  irlist_sort(&dirlist, irlist_sort_cmpfunc_string, NULL);
  
  if (!irlist_size(&dirlist))
    {
      u_respond(u,"Upload directory is empty");
    }
  else
    {
      u_respond(u,"Listing '%s':", dir);
      
      thefile = irlist_get_head(&dirlist);
      while (thefile)
        {
          struct stat st;
          int len = strlen(thefile);
          
          tempstr = mycalloc(len + thedirlen + 2);
          
          snprintf(tempstr, len + thedirlen + 2,
                   "%s/%s", dir, thefile);
          
          if (lstat(tempstr,&st) < 0)
            {
              u_respond(u,"cannot access '%s', ignoring: %s",
                        tempstr, strerror(errno));
            }
          else if (S_ISREG(st.st_mode))
            {
              char *sizestrstr;
              sizestrstr = sizestr(1, st.st_size);
              
              u_respond(u,"%9s  %s",
                        sizestrstr,
                        thefile);
              mydelete(sizestrstr);
            }
          else if (S_ISDIR (st.st_mode) ||
                   S_ISCHR (st.st_mode) ||
                   S_ISBLK (st.st_mode) ||
                   S_ISFIFO(st.st_mode) ||
                   S_ISLNK (st.st_mode) ||
                   S_ISSOCK(st.st_mode))
            {
              u_respond(u,"%9s  %s",
                        S_ISDIR (st.st_mode) ? "=DIR=" :
                        S_ISCHR (st.st_mode) ? "=CHAR=" :
                        S_ISBLK (st.st_mode) ? "=BLOCK=" :
                        S_ISFIFO(st.st_mode) ? "=FIFO=" :
                        S_ISLNK (st.st_mode) ? "=SYMLINK=" :
                        S_ISSOCK(st.st_mode) ? "=SOCKET=" :
                        "???",
                        thefile);
            }
          
          mydelete(tempstr);
          
          thefile = irlist_delete(&dirlist, thefile);
        }
    }
  
#ifndef NO_STATVFS
  if (statvfs(dir, &stf) < 0)
    {
      u_respond(u,"Unable to determine device sizes: %s",
                strerror(errno));
    }
  else
    {
      char *d_size, *d_used, *d_free, *d_resv;
      
      d_size = sizestr(0, (off_t)stf.f_blocks * (off_t)stf.f_frsize);
      d_used = sizestr(0, (off_t)(stf.f_blocks - stf.f_bavail) * (off_t)stf.f_frsize);
      d_free = sizestr(0, (off_t)stf.f_bavail * (off_t)stf.f_frsize);
      d_resv = sizestr(0, (off_t)(stf.f_bfree - stf.f_bavail) * (off_t)stf.f_frsize);
      
      u_respond(u,"Device size: %s, used %s, free %s, reserved %s",
                d_size, d_used, d_free, d_resv);
      
      mydelete(d_size);
      mydelete(d_used);
      mydelete(d_free);
      mydelete(d_resv);
    }
#else
#ifndef NO_STATFS
  if (statfs(dir, &stf) < 0)
    {
      u_respond(u,"Unable to determine device sizes: %s",
                strerror(errno));
    }
  else
    {
      char *d_size, *d_used, *d_free, *d_resv;
      
      d_size = sizestr(0, (off_t)stf.f_blocks * (off_t)stf.f_bsize);
      d_used = sizestr(0, (off_t)(stf.f_blocks - stf.f_bavail) * (off_t)stf.f_bsize);
      d_free = sizestr(0, (off_t)stf.f_bavail * (off_t)stf.f_bsize);
      d_resv = sizestr(0, (off_t)(stf.f_bfree - stf.f_bavail) * (off_t)stf.f_bsize);
      
      u_respond(u,"Device size: %s, used %s, free %s, reserved %s",
                d_size, d_used, d_free, d_resv);
      
      mydelete(d_size);
      mydelete(d_used);
      mydelete(d_free);
      mydelete(d_resv);
    }
#endif
#endif
  
  return;
  
}

static void u_listul(const userinput * const u)
{
  updatecontext();
  
  if (!irlist_size(&gdata.uploadhost) || !gdata.uploaddir)
    {
      u_respond(u,"No upload hosts or no uploaddir defined.");
      return;
    }
  
  u_listdir(u, gdata.uploaddir);
  return;
  
}

static void u_clearrecords(const userinput * const u)
{
  int ii;
  updatecontext();
  
  gdata.record = 0;
  gdata.sentrecord = 0;
  gdata.totalsent = 0;
  gdata.totaluptime = 0;
  
  for (ii=0; ii<NUMBER_TRANSFERLIMITS; ii++)
    {
      gdata.transferlimits[ii].ends = 0;
    }
  
  u_respond(u,"Cleared transfer record, bandwidth record, total sent, total uptime, and transfer limits");
  
}

static void u_rmul(const userinput * const u) {
   char *tempstr;
   
   updatecontext();
   
   if (!irlist_size(&gdata.uploadhost) || !gdata.uploaddir) {
      u_respond(u,"No upload hosts or no uploaddir defined.");
      return;
      }
   
   if (!u->arg1e || !strlen(u->arg1e)) {
      u_respond(u,"Try Specifying a Filename");
      return;
      }
   
   convert_to_unix_slash(u->arg1e);
   
   if (strstr(u->arg1e,"/")) {
      u_respond(u,"Filename contains invalid characters");
      return;
      }
   
   tempstr = mymalloc(strlen(gdata.uploaddir) + 1 + strlen(u->arg1e) + 1);
   sprintf(tempstr,"%s/%s",gdata.uploaddir,u->arg1e);
   
   if (doesfileexist(tempstr)) {
      if (unlink(tempstr) < 0)
         u_respond(u,"Unable to remove the file");
      else
         u_respond(u,"Deleted");
      }
   else
      u_respond(u,"That filename doesn't exist");
   
   mydelete(tempstr);
   
   }

static void u_crash(const userinput * const u) {
   
   updatecontext();
   
   *((int*)(0)) = 0;
   
}

#define USERS_PER_CHAN_LINE  6

static void u_chanl(const userinput * const u)
{
  int j;
  member_t *member;
  char *tempstr = mycalloc(maxtextlength);
  channel_t *ch;
#ifdef MULTINET
  int net;
#endif /* MULTINET */
  
  updatecontext();
  
#ifdef MULTINET
  net = get_network(u->arg1);
  if (net < 0)
    {
      u_respond(u,"Try specifying a valid network number");
      return;
    }
#endif /* MULTINET */
  
  u_respond(u,"Channel Members:");
  
#ifdef MULTINET
  ch = irlist_get_head(&gdata.networks[net].channels);
#else /* MULTINET */
  ch = irlist_get_head(&gdata.channels);
#endif /* MULTINET */
  while(ch)
    {
      j = 0;
      member = irlist_get_head(&ch->members);
      while(member)
        {
          if (!(j%USERS_PER_CHAN_LINE))
            {
              snprintf(tempstr,maxtextlength-1,"%s: ",ch->name);
            }
          snprintf(tempstr + strlen(tempstr),
                   maxtextlength - 1 - strlen(tempstr),
                   "%s%s ", member->prefixes, member->nick);
          if (!((j+1)%USERS_PER_CHAN_LINE))
            {
              u_respond(u,"%s",tempstr);
              tempstr[0] = '\0';
            }
          
          member = irlist_get_next(member);
          j++;
        }
      
      if (j%USERS_PER_CHAN_LINE)
        {
          u_respond(u,"%s",tempstr);
        }
      
      u_respond(u,"%s: %i user%s",ch->name,j,j!=1?"s":"");
      
      ch = irlist_get_next(ch);
    }
  
  mydelete(tempstr);
  
}
/* End of File */
