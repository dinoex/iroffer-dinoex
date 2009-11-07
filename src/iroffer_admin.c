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
 * @(#) iroffer_admin.c 1.215@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_admin.c|20051123201143|48668
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_admin.h"
#include "dinoex_curl.h"
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_main.h"
#include "dinoex_transfer.h"
#include "dinoex_upload.h"
#include "dinoex_misc.h"

/* local functions */
static void u_help(const userinput * const u);
static void u_xdl_full(const userinput * const u);
static void u_xdl_group(const userinput * const u);
static void u_xdl(const userinput * const u);
static void u_xds(const userinput * const u);
static void u_dcl(const userinput * const u);
static void u_dcld(const userinput * const u);
static void u_close(const userinput * const u);
static void u_closeu(const userinput * const u);
static void u_nomin(const userinput * const u);
static void u_nomax(const userinput * const u);
static void u_redraw(const userinput * const u);
static void u_delhist(const userinput * const u);
static void u_info(const userinput * const u);
static void u_removedir(const userinput * const u);
static void u_send(const userinput * const u);
static void u_psend(const userinput * const u);
static void u_mesg(const userinput * const u);
static void u_mesq(const userinput * const u);
static void u_quit(const userinput * const u);
static void u_status(const userinput * const u);
static void u_chfile(const userinput * const u);
static void u_chdesc(const userinput * const u);
static void u_chnote(const userinput * const u);
static void u_chmins(const userinput * const u);
static void u_chmaxs(const userinput * const u);
static void u_chgets(const userinput * const u);
static void u_adddir(const userinput * const u);
static void u_addnew(const userinput * const u);
static void u_chatme(const userinput * const u);
static void u_chatl(const userinput * const u);
static void u_closec(const userinput * const u);
static void u_rehash(const userinput * const u);
static void u_botinfo(const userinput * const u);
static void u_ignl(const userinput * const u);
static void u_ignore(const userinput * const u);
static void u_unignore(const userinput * const u);
static void u_bannhost(const userinput * const u);
static void u_nosave(const userinput * const u);
static void u_nosend(const userinput * const u);
static void u_nolist(const userinput * const u);
static void u_msgread(const userinput * const u);
static void u_msgdel(const userinput * const u);
static void u_memstat(const userinput * const u);
static void u_qsend(const userinput * const u);
static void u_shutdown(const userinput * const u);
static void u_debug(const userinput * const u);
static void u_jump(const userinput * const u);
static void u_servers(const userinput * const u);
static void u_trinfo(const userinput * const u);
static void u_clearrecords(const userinput * const u);
static void u_rmul(const userinput * const u);
static void u_crash(const userinput * const u);
static void u_chanl(const userinput * const u);

typedef struct
{
  const short help_section;
  const short level;
  const userinput_method_e methods_allowed;
  void (*handler)(const userinput * const);
  const char *command;
  const char *args;
  const char *desc;
} userinput_parse_t;


/* local info */
static const userinput_parse_t userinput_parse[] = {
{1,1,method_allow_all,u_help,          "HELP",NULL,"Shows help"},
{1,0,method_allow_all_xdl,u_xdl_full,  "XDLFULL",NULL,"Lists all offered packs"},
{1,0,method_allow_all_xdl,u_xdl_group, "XDLGROUP","[group]","Show packs from <group>"},
{1,0,method_allow_all_xdl,u_xdl,       "XDL",NULL,"Lists offered groups and packs without group"},
{1,1,method_allow_all,a_xdlock,        "XDLOCK",NULL,"Show all locked packs"},
{1,1,method_allow_all,a_xdtrigger,     "XDTRIGGER",NULL,"Show all packs with dynamic triggers"},
{1,1,method_allow_all,a_find,          "FIND","pattern","List packs that matches <pattern>"},
{1,5,method_allow_all,u_xds,           "XDS",NULL,"Save state file"},
{1,1,method_allow_all,u_dcl,           "DCL",NULL,"Lists current transfers"},
{1,1,method_allow_all,u_dcld,          "DCLD",NULL,"Lists current transfers with details"},
{1,1,method_allow_all,u_trinfo,        "TRINFO","id","Lists information about transfer <id>"},
{1,1,method_allow_all,a_getl,          "GETL",NULL,"Lists current upload queue"},
{1,1,method_allow_all,a_qul,           "QUL",NULL,"Lists current queue"},
{1,1,method_allow_all,u_ignl,          "IGNL",NULL,"Show ignored list"},
{1,5,method_allow_all,a_listul,        "LISTUL","[dir]","Shows contents of upload directory"},
{1,5,method_allow_all,u_chanl,         "CHANL","[net]","Shows channel list with member list"},

{2,2,method_allow_all,u_close,         "CLOSE","id","Cancels transfer <id>"},
{2,2,method_allow_all,u_closeu,        "CLOSEU","id","Cancels upload <id>"},
{2,2,method_allow_all,a_acceptu,       "ACCEPTU","min [hostmask]","Accepting uploads from <hostmask> for <x> minutes"},
{2,2,method_allow_all,a_get,           "GET","net nick n [password]","Request pack <n> from bot <nick>"},
{2,2,method_allow_all,a_rmq,           "RMQ","position","Removes entry at <position> from main queue"},
{2,2,method_allow_all,a_rmiq,          "RMIQ","position","Removes entry at <position> from idle queue"},
{2,5,method_allow_all,u_nomin,         "NOMIN","id","Disables minspeed for transfer <id>"},
{2,5,method_allow_all,u_nomax,         "NOMAX","id","Disables maxspeed for transfer <id>"},
{2,5,method_allow_all,a_unlimited,     "UNLIMITED","id","Disables bandwidth limits for transfer <id>"},
{2,5,method_allow_all,a_maxspeed,      "MAXSPEED","id x","Set max bandwidth limit of <x> KB/s for transfer <id>"},
{2,2,method_allow_all,u_send,          "SEND","nick n [net]","Sends pack <n> to <nick>"},
{2,2,method_allow_all,a_queue,         "QUEUE","nick n [net]","Queues pack <n> for <nick> for main queue"},
{2,2,method_allow_all,a_iqueue,        "IQUEUE","nick n [net]","Queues pack <n> for <nick> for idle queue"},
{2,2,method_allow_all,u_psend,         "PSEND","channel style [net]","Sends <style> (full|minimal|summary) XDCC LIST to <channel>"},
{2,2,method_allow_all,u_qsend,         "QSEND","[id]","Start an extra transfer from queue"},
{2,5,method_allow_all,a_slotsmax,      "SLOTSMAX","[slots]","temporary change slotsmax to <slots>"},
{2,5,method_allow_all,a_queuesize,     "QUEUESIZE","[slots]","temporary change queuesize to <slots>"},
{2,5,method_allow_all,a_requeue,       "REQUEUE","x y","Moves main queue entry from position <x> to <y>"},
{2,5,method_allow_all,a_reiqueue,      "REIQUEUE","x y","Moves idle queue entry from position <x> to <y>"},

{3,0,method_allow_all_xdl,u_info,      "INFO","n","Show info for pack <n>"},
{3,4,method_allow_all,a_remove,        "REMOVE","n [m]","Removes pack <n> or <n> to <m>"},
{3,4,method_allow_all,u_removedir,     "REMOVEDIR","dir","Remove every pack found in <dir>"},
{3,4,method_allow_all,a_removegroup,   "REMOVEGROUP","group","Remove every pack found in <group>"},
{3,3,method_allow_all,a_renumber3,     "RENUMBER","x [y] z","Moves packs <x> to <y> to position <z>"},
{3,3,method_allow_all,a_sort,          "SORT","[field] [field]","Sort all packs by fields"},
{3,3,method_allow_all,a_add,           "ADD","filename","Add new pack with <filename>"},
{3,3,method_allow_all,u_adddir,        "ADDDIR","dir","Add every file in <dir>"},
{3,3,method_allow_all,u_addnew,        "ADDNEW","dir","Add new files in <dir>"},
{3,3,method_allow_all,a_addgroup,      "ADDGROUP","group dir","Add new files in <dir> to <group>"},
{3,3,method_allow_all,a_addmatch,      "ADDMATCH","pattern","Add new files matching this pattern"},
{3,3,method_allow_all,a_autoadd,       "AUTOADD",NULL,"scan autoadd_dirs for new files now"},
{3,3,method_allow_all,a_autogroup,     "AUTOGROUP",NULL,"Create a group for each directory with packs"},
{3,3,method_allow_all,u_chfile,        "CHFILE","n filename","Change file of pack <n> to <filename>"},
{3,3,method_allow_all,a_newdir,        "NEWDIR","dirname newdir","rename pathnames of all matching packs"},
{3,3,method_allow_all,u_chdesc,        "CHDESC","n [msg]","Change description of pack <n> to <msg>"},
{3,3,method_allow_all,u_chnote,        "CHNOTE","n [msg]","Change note of pack <n> to <msg>"},
{3,3,method_allow_all,a_chtime,        "CHTIME","n [msg]","Change add time of pack <n> to <msg>"},
{3,3,method_allow_all,u_chmins,        "CHMINS","n x","Change min speed of pack <n> to <x> KB/s"},
{3,3,method_allow_all,u_chmaxs,        "CHMAXS","n x","Change max speed of pack <n> to <x> KB/s"},
{3,3,method_allow_all,a_chlimit,       "CHLIMIT","n x","Change download limit of pack <n> to <x> transfers per day"},
{3,3,method_allow_all,a_chlimitinfo,   "CHLIMITINFO","n [msg]","Change over limit info of pack <n> to <msg>"},
{3,3,method_allow_all,a_chtrigger,     "CHTRIGGER","n [msg]","Change trigger for pack <n> to <msg>"},
{3,5,method_allow_all,u_chgets,        "CHGETS","n x","Set the get counter of pack <n> to <x>"},
{3,5,method_allow_all,a_chcolor,       "CHCOLOR","n [m] x","Set the pack <n> to <m> to color <x>"},
{3,2,method_allow_all,a_lock,          "LOCK","n [m] password","Lock the pack <n> to <m> with <password>"},
{3,2,method_allow_all,a_unlock,        "UNLOCK","n [m]","Unlock the pack <n> to <m>"},
{3,2,method_allow_all,a_lockgroup,     "LOCKGROUP","group password","Lock all packs in <group> with <password>"},
{3,2,method_allow_all,a_unlockgroup,   "UNLOCKGROUP","group","Unlock all packs in <group>"},
{3,2,method_allow_all,a_relock,        "RELOCK","old-password password","Lock all packs with <old-password> with <password>"},
{3,3,method_allow_all,a_groupdesc,     "GROUPDESC","group msg","Change description of <group> to <msg>"},
{3,3,method_allow_all,a_group,         "GROUP","n group","Change group of pack <n> to <group>"},
{3,3,method_allow_all,a_movegroup,     "MOVEGROUP","n m group","Change group to <group> for pack <n> to <m>"},
{3,3,method_allow_all,a_regroup,       "REGROUP","group new","Change all packs of <group> to group <new>"},
{3,3,method_allow_all,a_newgroup,      "NEWGROUP","group dir","Change any files in <dir> to <group>"},
{3,2,method_allow_all,a_announce,      "ANNOUNCE","n [msg]","ANNOUNCE <msg> for pack <n> in all joined channels"},
{3,2,method_allow_all,a_newann,        "NEWANN","n","ANNOUNCE for the last <n> packs in all joined channels"},
{3,2,method_allow_all,a_mannounce,     "MANNOUNCE","n m [msg]","ANNOUNCE <msg> for pack <n> to <m> in all joined channels"},
{3,2,method_allow_all,a_cannounce,     "CANNOUNCE","channel n [msg]","ANNOUNCE <msg> for pack <n> in <channel>"},
{3,2,method_allow_all,a_sannounce,     "SANNOUNCE","n [m]","Short ANNOUNCE pack <n> to <m> in all joined channels"},
{3,3,method_allow_all,a_addann,        "ADDANN","filename","Add and announce new pack"},
{3,2,method_allow_all,a_md5,           "MD5","[n] [m]","Calculate MD5 and CRC sums of pack <n> to <m>"},
{3,2,method_allow_all,a_crc,           "CRC","[n] [m]","Check CRC of pack <n> to <m>"},
{4,5,method_allow_all,a_filemove,      "FILEMOVE","filename newfile","rename file on disk"},
{4,5,method_allow_all,a_movefile,      "MOVEFILE","n filename","rename the file of pack <n> on disk to <filename>"},
{4,3,method_allow_all,a_movegroupdir,  "MOVEGROUPDIR","group dir","move any file in group <g> to dir <dir>"},
{4,5,method_allow_all,a_filedel,       "FILEDEL","filename","remove file from disk"},
{4,5,method_allow_all,a_fileremove,    "FILEREMOVE","n [m]","remove pack <n> or <n> to <m> and remove its file from disk"},
{4,5,method_allow_all,a_showdir,       "SHOWDIR","dir","list directory on disk"},
{4,5,method_allow_all,a_makedir,       "MAKEDIR","dir","create a new directory on disk"},
#ifdef USE_CURL
{4,3,method_allow_all,a_fetch,         "FETCH","file url","download url and save file in upload dir"},
{4,3,method_allow_all,a_fetchcancel,   "FETCHCANCEL","id","stop download of fetch with <id>"},
#endif /* USE_CURL */

{5,2,method_allow_all,a_msg,           "MSG","nick message","Send <message> to user or channel <nick>"},
{5,2,method_allow_all,a_amsg,          "AMSG","msg","Announce <msg> in all joined channels"},
{5,2,method_allow_all,a_msgnet,        "MSGNET","net nick message","Send <message> to user or channel <nick>"},
{5,2,method_allow_all,u_mesg,          "MESG","message","Sends <message> to all users who are transferring"},
{5,2,method_allow_all,u_mesq,          "MESQ","message","Sends <message> to all users in a queue"},
{5,2,method_allow_all,u_ignore,        "IGNORE","x hostmask","Ignore <hostmask> (nick!user@host) for <x> minutes, wildcards allowed"},
{5,2,method_allow_all,u_unignore,      "UNIGNORE","hostmask","Un-Ignore <hostmask>"},
{5,2,method_allow_all,u_bannhost,      "BANNHOST","x hostmask","Stop transfers and ignore <hostmask> (nick!user@host) for <x> minutes"},
{5,2,method_allow_all,a_bannnick,      "BANNNICK","nick [net]","Stop transfers and remove <nick> from queue"},
{5,5,method_allow_all,u_nosave,        "NOSAVE","x","Disables autosave for next <x> minutes"},
{5,2,method_allow_all,u_nosend,        "NOSEND","x [msg]","Disables XDCC SEND for next <x> minutes"},
{5,2,method_allow_all,u_nolist,        "NOLIST","x","Disables XDCC LIST and plist for next <x> minutes"},
{5,2,method_allow_all,a_nomd5,         "NOMD5","x","Disables MD5 and CRC calculation for next <x> minutes"},
{5,3,method_allow_all,u_msgread,       "MSGREAD",NULL,"Show MSG log"},
{5,5,method_allow_all,u_msgdel,        "MSGDEL",NULL,"Delete MSG log"},
{5,3,method_allow_all,u_rmul,          "RMUL","filename","Delete a file in the upload dir"},
{5,5,method_allow_all,a_raw,           "RAW","command","Send <command> to server (RAW IRC)"},
{5,5,method_allow_all,a_rawnet,        "RAWNET","net command","Send <command> to server (RAW IRC)"},
{5,5,method_allow_all,a_lag,           "LAG","[net]","Show lag on networks"},

{6,2,method_allow_all,u_servers,       "SERVERS","[net]","Shows the server list"},
{6,2,method_allow_all,a_hop,           "HOP","[channel] [net]","leave and rejoin a channel to get status"},
{6,2,method_allow_all,a_nochannel,     "NOCHANNEL","x [channel]","leave channel for <x> minutes"},
{6,2,method_allow_all,a_join,          "JOIN","channel [net] [key]","join channel till rehash"},
{6,2,method_allow_all,a_part,          "PART","channel [net]","part channel till rehash"},
{6,2,method_allow_all,u_jump,          "JUMP","server [net]","Switches to a random server or server <server>"},
{6,5,method_allow_all,a_servqc,        "SERVQC",NULL,"Clears the server send queue"},
{6,2,method_allow_all,u_status,        "STATUS",NULL,"Show Useful Information"},
{6,5,method_allow_all,u_rehash,        "REHASH",NULL,"Re-reads config file(s) and reconfigures"},
{6,2,method_allow_all,u_botinfo,       "BOTINFO",NULL,"Show Information about the bot status"},
{6,5,method_allow_all,u_memstat,       "MEMSTAT",NULL,"Show Information about memory usage"},
{6,5,method_allow_all,u_clearrecords,  "CLEARRECORDS",NULL,"Clears transfer, bandwidth, uptime, total sent, and transfer limits"},
{6,5,method_allow_all,a_cleargets,     "CLEARGETS",NULL,"Clears download counters for each pack"},
{6,5,method_console,  u_redraw,        "REDRAW",NULL,"Redraws the Screen"},
{6,5,method_console,  u_delhist,       "DELHIST",NULL,"Deletes console history"},
{6,1,method_dcc,      u_quit,          "QUIT",NULL,"Close this DCC chat"},
{6,1,method_dcc,      u_quit,          "EXIT",NULL,"Close this DCC chat"},
{6,1,method_dcc,      u_quit,          "LOGOUT",NULL,"Close this DCC chat"},
{6,1,method_msg,      u_chatme,        "CHATME",NULL,"Sends you a DCC Chat Request"},
{6,1,method_allow_all,u_chatl,         "CHATL",NULL,"Lists DCC chat information"},
{6,5,method_allow_all,u_closec,        "CLOSEC","id","Closes DCC chat <id>"},
{6,5,method_console,  u_debug,         "DEBUG","x","Set debugging level to <x>"},
{6,5,method_allow_all,a_config,        "CONFIG","key value","Set config variable <key> to <value>"},
{6,5,method_allow_all,a_print,         "PRINT","key","Print config variable <key>"},
{6,1,method_allow_all,a_identify,      "IDENTIFY","[net]","Send stored password again to nickserv"},
{6,2,method_allow_all,a_holdqueue,     "HOLDQUEUE","[x]","Change holdqueue flag, x=1 set, x=0 reset"},
{6,5,method_allow_all,u_shutdown,      "SHUTDOWN","act","Shutdown iroffer, <act> is \"now\", \"delayed\", or \"cancel\""},
{6,5,method_console,  a_backgroud,     "BACKGROUND",NULL,"Switch to background mode"},
{6,5,method_allow_all,a_dump,          "DUMP",NULL,"Write a dump into the logfile"},
{6,5,method_allow_all,a_restart,       "RESTART",NULL,"Shutdown and restart the bot"},
{6,5,method_console,  u_crash,         "CRASH",NULL,"Write a dump into the logfile and exit"},
};



void u_fillwith_console (userinput * const u, char *line)
{
  
  updatecontext();
  
  u->method = method_console;
  u->snick = NULL;
  u->chat = NULL;
  u->net = 0;
  u->level = ADMIN_LEVEL_CONSOLE;
  u->hostmask = NULL;
  
  a_parse_inputline(u, line);
}

void u_fillwith_dcc (userinput * const u, dccchat_t *chat, char *line)
{
  
  updatecontext();
  
  u->method = method_dcc;
  u->snick = NULL;
  u->chat = chat;
  u->net = chat->net;
  u->level = chat->level;
  u->hostmask = mystrdup(chat->hostmask);
  
  a_parse_inputline(u, line);
}

static void u_fillwith_clean (userinput * const u)
{
  
  updatecontext();
  
  mydelete(u->snick);
  u->chat = NULL;
  u->net = 0;
  u->level = ADMIN_LEVEL_PUBLIC;
  mydelete(u->hostmask);
  mydelete(u->cmd);
  mydelete(u->arg1e);
  mydelete(u->arg2e);
  mydelete(u->arg3e);
  mydelete(u->arg1);
  mydelete(u->arg2);
  mydelete(u->arg3);
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
u_respond(const userinput * const u, const char *format, ...)
{
  va_list args;
  channel_t *ch;
  char *tempnick;
  char *chan;
  char *fish;
  int delay;

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
        
        if (gdata.dos_text_files)
          tempstr[llen++] = '\r';
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
      delay = 0;
      fish = NULL;
      tempnick = mystrdup(u->snick);
      for (chan = strtok(tempnick, ","); chan != NULL; chan = strtok(NULL, ",") )
        {
          for (ch = irlist_get_head(&(gnetwork->channels));
               ch;
               ch = irlist_get_next(ch))
            {
              if (!strcasecmp(ch->name, chan))
                {
                  delay = ch->delay;
                  fish = ch->fish;
                  break;
                }
            }
          if (ch != NULL)
            break;
        }
      mydelete(tempnick);
      vprivmsg_chan(delay, u->snick, fish, format, args);
      break;
    case method_xdl_user_privmsg:
      vprivmsg_slow(u->snick, format, args);
      break;
    case method_xdl_user_notice:
      vnotice_slow(u->snick, format, args);
      break;
    case method_allow_all:
    case method_allow_all_xdl:
    default:
      break;
    }
  
  va_end(args);
}

void u_parseit(userinput * const u) {
   int i,found = 0;
   gnetwork_t *backup;
   
   updatecontext();
   
   backup = gnetwork;
   if ( gnetwork == NULL )
     gnetwork = &(gdata.networks[0]);
   if (!u->cmd || !strlen(u->cmd)) {
      u_respond(u,"** Missing Command, try again");
      u_fillwith_clean(u);
      gnetwork = backup;
      return;
      }
   
   for (i=0; !found && i<(int)((sizeof(userinput_parse)/sizeof(userinput_parse_t))); i++) {
      if ( (!strcmp(userinput_parse[i].command,u->cmd)) &&
           (userinput_parse[i].level <= u->level ) &&
           (userinput_parse[i].methods_allowed & u->method) ) {
         found=1;
         userinput_parse[i].handler(u);
         }
      }
   
   if (!found)
      u_respond(u,"** User Command Not Recognized, try \"HELP\"");
   
   if (found && u->method==method_console)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA, "ADMIN %s requested (console)", u->cmd);
   if (found && u->method==method_dcc)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA, "ADMIN %s requested (DCC Chat: %s) (network: %s)",
               u->cmd, save_nick(u->chat->nick), gnetwork->name);
   if (found && u->method==method_msg)
      ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_MAGENTA, "ADMIN %s requested (MSG: %s) (network: %s)",
              u->cmd, u->snick, gnetwork->name);
   
   u_fillwith_clean(u);
   gnetwork = backup;
   
   }

int u_expand_command(void)
{
  char *cmd;
  char *end;
  size_t j;
  int i;
  int found = 0;
  int first = -1;

  cmd = mystrdup(gdata.console_input_line);
  end = strchr(cmd, ' ');
  if (end != NULL) {
    *end = 0;
    for (i=0; i<(int)((sizeof(userinput_parse)/sizeof(userinput_parse_t))); i++) {
      if (strcmp(userinput_parse[i].command, cmd))
        continue;

      if ((userinput_parse[i].methods_allowed & method_console) == 0)
        continue;

      tostdout("** Command: %s %s, %s\n",
               cmd, userinput_parse[i].args ? userinput_parse[i].args : "(no args)",
               userinput_parse[i].desc);
      break;
    }
    mydelete(cmd);
    return found;
  }
  caps(cmd);
  j = strlen(cmd);
  for (i=0; i<(int)((sizeof(userinput_parse)/sizeof(userinput_parse_t))); i++) {
    if (strncmp(userinput_parse[i].command, cmd, j))
      continue;

    if ((userinput_parse[i].methods_allowed & method_console) == 0)
      continue;

    found++;
    if (first < 0) first = i;
  }
  if (found == 0) {
    tostdout("** User Command Not Recognized, try \"HELP\"");
    tostdout("\n");
    mydelete(cmd);
    return 0;
  }
  if (found == 1) {
    mydelete(cmd);
    strcpy(gdata.console_input_line, userinput_parse[first].command);
    return strlen(userinput_parse[first].command) - j;
  }
  tostdout("** Commands:");
  for (i=0; i<(int)((sizeof(userinput_parse)/sizeof(userinput_parse_t))); i++) {
    if (strncmp(userinput_parse[i].command, cmd, j))
      continue;

    if ((userinput_parse[i].methods_allowed & method_console) == 0)
      continue;

    if (i != first)
      tostdout(",");
    tostdout(" %s", userinput_parse[i].command);
  }
  tostdout("\n");
  mydelete(cmd);
  return 0;
}

static void u_help(const userinput * const u)
{
  int i,which=0;
  
  updatecontext();
  
  for (which=1; which<7; which++)
    {
      if ((which == 4) && (gdata.direct_file_access == 0))
        continue;
      
      u_respond(u,"-- %s Commands --",
                which==1?"Info":
                which==2?"Transfer":
                which==3?"Pack":
                which==4?"File":
                which==5?"Misc":
                which==6?"Bot":"<Unknown>");
      
      for (i=0; i<(int)((sizeof(userinput_parse)/sizeof(userinput_parse_t))); i++)
        {
          if (userinput_parse[i].methods_allowed & u->method &&
              (userinput_parse[i].level <= u->level ) &&
              userinput_parse[i].help_section == which)
            {
              if (userinput_parse[i].args)
                {
                  int spaces;
                  
                  spaces = 20 - 1;
                  spaces -= sstrlen(userinput_parse[i].command);
                  spaces = max2(0,spaces);
                  
                  u_respond(u, "  %s %-*s : %s",
                            userinput_parse[i].command,
                            spaces,
                            userinput_parse[i].args,
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
  u_respond(u, "For additional help, see the documentation at " "http://iroffer.dinoex.net/");
  
}

void u_xdl_pack(const userinput * const u, char *tempstr, int i, int l, int s, const xdcc *xd) {
   char datestr[maxtextlengthshort];
   char *sizestrstr;
   char *colordesc;
   int len;
   
   sizestrstr = sizestr(1, xd->st_size);
   datestr[0] = 0;
   if (gdata.show_date_added)
     {
       datestr[0] = ' ';
       datestr[1] = 0;
       user_getdatestr(datestr + 1, xd->xtime ? xd->xtime : xd->mtime, maxtextlengthshort - 1);
     }
   if (xd->color)
     {
       colordesc = mycalloc(maxtextlength);
       snprintf(colordesc, maxtextlength, "\003%d%s\003", xd->color, xd->desc);
     }
   else
     {
       colordesc = xd->desc;
     }
   snprintf(tempstr, maxtextlength,
           "\2#%-*i\2 %*ix [%s]%s %s",
            l,
            i,
            s, xd->gets,
            sizestrstr,
            datestr,
            colordesc);
   len = strlen(tempstr);
   mydelete(sizestrstr);
   if (colordesc != xd->desc)
     mydelete(colordesc);
   
   if (xd->minspeed > 0 && xd->minspeed != gdata.transferminspeed)
     {
        snprintf(tempstr + len, maxtextlength - len,
                 " [%1.1fK Min]",
                 xd->minspeed);
        len = strlen(tempstr);
     }
   
   if (xd->maxspeed > 0 && xd->maxspeed != gdata.transfermaxspeed)
     {
        snprintf(tempstr + len, maxtextlength - len,
                 " [%1.1fK Max]",
                 xd->maxspeed);
        len = strlen(tempstr);
     }
   
   if (xd->dlimit_max != 0)
     {
        snprintf(tempstr + len, maxtextlength - len,
                 " [%d of %d DL left]",
                 xd->dlimit_used - xd->gets, xd->dlimit_max);
        len = strlen(tempstr);
     }
   
   u_respond(u, "%s", tempstr);
   
   if (xd->note && xd->note[0])
     {
       u_respond(u, " \2^-\2%*s%s", s, "", xd->note);
     }
}

static void u_xdl_head(const userinput * const u) {
   char *tempstr;
   char *tempnick;
   char *chan;
   const char *mynick;
   int a, i, m, m1;
   int len;
   int head;
   xdcc *xd;
   channel_t *ch;
   ir_uint64 xdccsent;
   
   updatecontext();
   tempstr  = mycalloc(maxtextlength);

   if (u->method==method_xdl_channel_min) m = 1; else m = 0;
   if (u->method==method_xdl_channel_sum) m1 = 1; else m1 = 0;
   mynick = get_user_nick();
   
   head = 0;
   switch (u->method)
    {
    case method_xdl_channel:
    case method_xdl_channel_min:
    case method_xdl_channel_sum:
      ch = irlist_get_head(&(gnetwork->channels));
      while(ch)
        {
          if (ch->headline != NULL )
            {
              tempnick = mystrdup(u->snick);
              for (chan = strtok(tempnick, ","); chan != NULL; chan = strtok(NULL, ",") )
                {
                  if (!strcasecmp(ch->name, chan))
                    {
                      userinput u2;
                      
                      u2 = *u;
                      u2.snick = chan;
                      u_respond(&u2, "\2**\2 %s \2**\2", ch->headline);
                      head ++;
                    }
                }
              mydelete(tempnick);
            }
          ch = irlist_get_next(ch);
        }
      break;
    default:
      u_respond(u, "\2**\2 To stop this listing, type \"/MSG %s XDCC STOP\" \2**\2",
                mynick);
      break;
    }
   
   if (gdata.headline)
     {
       if (head == 0)
          u_respond(u, "\2**\2 %s \2**\2", gdata.headline);
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
       
       snprintf(tempstr, maxtextlength,
                "\2**\2 %i %s \2**\2  %i of %i %s open",
                irlist_size(&gdata.xdccs),
                irlist_size(&gdata.xdccs) != 1 ? "packs" : "pack",
                a - irlist_size(&gdata.trans),
                a,
                a != 1 ? "slots" : "slot");
       len = strlen(tempstr);
       
       if (gdata.slotsmax <= irlist_size(&gdata.trans))
         {
           snprintf(tempstr + len, maxtextlength - len,
                    ", Queue: %i/%i",
                    irlist_size(&gdata.mainqueue),
                    gdata.queuesize);
           len = strlen(tempstr);
         }
       
       if (gdata.transferminspeed > 0)
         {
           snprintf(tempstr + len, maxtextlength - len,
                    ", Min: %1.1fKB/s",
                    gdata.transferminspeed);
           len = strlen(tempstr);
         }
       
       if (gdata.transfermaxspeed > 0)
         {
           snprintf(tempstr + len, maxtextlength - len,
                    ", Max: %1.1fKB/s",
                    gdata.transfermaxspeed);
           len = strlen(tempstr);
         }
       
       if (gdata.record > 0.5)
         {
           snprintf(tempstr + len, maxtextlength - len,
                    ", Record: %1.1fKB/s",
                    gdata.record);
           len = strlen(tempstr);
         }
       
       u_respond(u,"%s",tempstr);
       
       
       for (i=0,xdccsent=0; i<XDCC_SENT_SIZE; i++)
         {
           xdccsent += (ir_uint64)gdata.xdccsent[i];
         }
       
       snprintf(tempstr, maxtextlength,
                "\2**\2 Bandwidth Usage \2**\2 Current: %1.1fKB/s,",
                ((float)xdccsent) / XDCC_SENT_SIZE / 1024.0);
       len = strlen(tempstr);
       
       if (gdata.maxb)
         {
           snprintf(tempstr + len, maxtextlength - len,
                    " Cap: %u.0KB/s,",
                    gdata.maxb / 4);
           len = strlen(tempstr);
         }
       
       if (gdata.sentrecord > 0.5)
         {
           snprintf(tempstr + len, maxtextlength - len,
                    " Record: %1.1fKB/s",
                    gdata.sentrecord);
           len = strlen(tempstr);
         }
       
       u_respond(u,"%s",tempstr);
       
       u_respond(u, "\2**\2 To request a file, type \"/MSG %s XDCC SEND x\" \2**\2",
                 mynick);
       
       if ((gdata.hide_list_info == 0) && (gdata.disablexdccinfo == 0))
          u_respond(u, "\2**\2 To request details, type \"/MSG %s XDCC INFO x\" \2**\2",
                    mynick);
       
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
         {
            u_respond(u, "\2**\2 To list a group, type \"/MSG %s XDCC LIST group\" \2**\2",
                      mynick);
            
            if (!gdata.restrictprivlistfull)
              u_respond(u, "\2**\2 To list all packs, type \"/MSG %s XDCC LIST ALL\" \2**\2",
                        mynick);
         }
     }
   
   if (m1)
     {
       if (!gdata.restrictprivlist)
         {
           u_respond(u, "\2**\2 For a listing type: \"/MSG %s XDCC LIST\" \2**\2",
                     mynick);
         }
       if (gdata.creditline)
         {
           u_respond(u, "\2**\2 %s \2**\2", gdata.creditline);
         }
     }
   
   mydelete(tempstr);
}

static void u_xdl_foot(const userinput * const u) {
   xdcc *xd;
   char *sizestrstr;
   char *totalstr;
   ir_uint64 toffered;

   if (gdata.creditline)
     {
       u_respond(u, "\2**\2 %s \2**\2", gdata.creditline);
     }
   
   if (u->method != method_xdl_channel_min)
     {
       toffered = 0;
       xd = irlist_get_head(&gdata.xdccs);
       while(xd)
         {
           toffered += xd->st_size;
           xd = irlist_get_next(xd);
         }
   
       sizestrstr = sizestr(0, toffered);
       totalstr = sizestr(0, gdata.totalsent);
       u_respond(u, "Total Offered: %sB  Total Transferred: %sB",
                 sizestrstr, totalstr);
       mydelete(totalstr);
       mydelete(sizestrstr);
     }
}

static void u_xdl_full(const userinput * const u) {
   char *tempstr;
   xdcc *xd;
   int i;
   int l;
   int s;
   
   updatecontext();
   
   u_xdl_head(u);
   
   tempstr = mycalloc(maxtextlength);
   i = 1;
   l = a_xdl_left();
   s = a_xdl_space();
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       if (hide_locked(u, xd) == 0)
         u_xdl_pack(u, tempstr, i, l, s, xd);
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
   int i;
   int k;
   int l;
   int s;

   updatecontext();

   u_xdl_head(u);

   msg3 = u->arg1;
   tempstr = mycalloc(maxtextlength);
   i = 1;
   k = 0;
   l = a_xdl_left();
   s = a_xdl_space();
   xd = irlist_get_head(&gdata.xdccs);
   for (xd = irlist_get_head(&gdata.xdccs);
        xd;
        xd = irlist_get_next(xd), i++)
     {
       if (msg3 == NULL)
         {
            if (xd->group != NULL)
               continue;
         }
       else
         {
            if (xd->group == NULL)
               continue;

            if (strcasecmp(xd->group, msg3) != 0 )
               continue;

            if (xd->group_desc != NULL)
               {
                  u_respond(u, "group: %s%s%s", msg3, gdata.group_seperator, xd->group_desc);
               }
         }
         if (hide_locked(u, xd) != 0)
            continue;

         u_xdl_pack(u, tempstr, i, l, s, xd);
         k++;
     }
         
   mydelete(tempstr);
   
   u_xdl_foot(u);
   
   if (!k)
     {
       u_respond(u, "Sorry, nothing was found, try a XDCC LIST");
     }
}

static void u_xdl(const userinput * const u) {
   char *tempstr;
   char *inlist;
   int i;
   int l;
   int s;
   xdcc *xd;
   irlist_t grplist = {0, 0, 0};

   updatecontext();
   
   u_xdl_head(u);

   if (u->method==method_xdl_channel_sum) return;

   tempstr  = mycalloc(maxtextlength);
   
   l = a_xdl_left();
   s = a_xdl_space();
   i = 1;
   xd = irlist_get_head(&gdata.xdccs);
   while(xd)
     {
       /* skip is group is set */
       if (xd->group == NULL)
         {
           if (hide_locked(u, xd) == 0)
             u_xdl_pack(u, tempstr, i, l, s, xd);
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
            snprintf(tempstr, maxtextlength,
                     "%s%s%s", xd->group, gdata.group_seperator, xd->group_desc);
            irlist_add_string(&grplist, tempstr);
          }
       xd = irlist_get_next(xd);
     }

     irlist_sort2(&grplist, irlist_sort_cmpfunc_string);
     inlist = irlist_get_head(&grplist);
     while (inlist)
       {
         u_respond(u, "group: %s", inlist);
         inlist = irlist_delete(&grplist, inlist);
       }
   
   u_xdl_foot(u);
   
   mydelete(tempstr);
}

static void u_xds(const userinput * const UNUSED(u))
{
  updatecontext();
  write_files();
}

static void u_dcl(const userinput * const u)
{
  const char *y;
  int i;
  upload *ul;
  transfer *tr;

  updatecontext();
  
#ifdef USE_CURL
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads) && !fetch_started)
#else /* USE_CURL */
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads))
#endif /* USE_CURL */
    {
      u_respond(u, "No active transfers");
      return;
    }
  
  if (irlist_size(&gdata.trans))
    {
      u_respond(u, "Current %s", irlist_size(&gdata.trans)!=1 ? "transfers" : "transfer");
      u_respond(u, "   ID  User        Pack File                               Status");
    }
  
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      y = t_print_state(tr);
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING)
        {
          u_respond(u, "  %3i  %-9s   %-4d %-32s   %s %2.0f%%",
                    tr->id, tr->nick, number_of_pack(tr->xpack), getfilename(tr->xpack->file), y,
                    ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size));
        }
      else
        {
          u_respond(u, "  %3i  %-9s   %-4d %-32s   %s",
                    tr->id, tr->nick, number_of_pack(tr->xpack), getfilename(tr->xpack->file), y);
        }
      tr = irlist_get_next(tr);
    }
  
#ifdef USE_CURL
  if (irlist_size(&gdata.uploads) || fetch_started)
#else /* USE_CURL */
  if (irlist_size(&gdata.uploads))
#endif /* USE_CURL */
    {
      u_respond(u, "Current %s", irlist_size(&gdata.uploads)!=1 ? "uploads" : "upload" );
      u_respond(u,"   ID  User        File                               Status");
    }
  
  i = 0;
  ul = irlist_get_head(&gdata.uploads);
  while (ul)
    {
      y = l_print_state(ul);
      
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
#ifdef USE_CURL
  dinoex_dcl(u);
#endif /* USE_CURL */
}

static void u_dcld(const userinput * const u)
{
  char *tempstr2, *tempstr3, *tempstr4;
  const char *y;
  int i,left,started;
  upload *ul;
  transfer *tr;
  
  updatecontext();
  
#ifdef USE_CURL
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads) && !fetch_started)
#else /* USE_CURL */
  if (!irlist_size(&gdata.trans) && !irlist_size(&gdata.uploads))
#endif /* USE_CURL */
    {
      u_respond(u, "No active transfers");
      return;
    }
  
   tempstr2 = mycalloc(maxtextlengthshort);
   tempstr3 = mycalloc(maxtextlengthshort);
   tempstr4 = mycalloc(maxtextlengthshort);
   
   if (irlist_size(&gdata.trans))
     {
       u_respond(u, "Current %s", irlist_size(&gdata.trans)!=1 ? "transfers" : "transfer");
       u_respond(u, " ID  User        Pack File                               Status");
       u_respond(u,"  ^-    Speed    Current/    End   Start/Remain    Min/  Max  Resumed");
       u_respond(u," --------------------------------------------------------------------");
     }
   
  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      y = t_print_state(tr);
      
      u_respond(u, "network: %d: %s", tr->net, gdata.networks[tr->net].name);
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING)
        {
          u_respond(u, "%3i  %-9s   %-4d %-32s   %s %2.0f%%",
                    tr->id, tr->nick, number_of_pack(tr->xpack), getfilename(tr->xpack->file), y,
                    ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size));
        }
      else
        {
          u_respond(u, "%3i  %-9s   %-4d %-32s   %s",
                    tr->id, tr->nick, number_of_pack(tr->xpack), getfilename(tr->xpack->file), y);
        }
      
      if (tr->tr_status == TRANSFER_STATUS_SENDING || tr->tr_status == TRANSFER_STATUS_WAITING)
        {
          left = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
          started = min2(359999, gdata.curtime-tr->con.connecttime);
          snprintf(tempstr2, maxtextlengthshort,
                   "%1.1fK", tr->xpack->minspeed);
          snprintf(tempstr3, maxtextlengthshort,
                   "%6liK", (long)(tr->startresume/1024));
          snprintf(tempstr4, maxtextlengthshort,
                   "%1.1fK", tr->maxspeed);
          
          u_respond(u,
                    "  ^- %5.1fK/s    %6" LLPRINTFMT "dK/%6" LLPRINTFMT "dK  %2i%c%02i%c/%2i%c%02i%c  %5s/%5s  %7s",
                    tr->lastspeed,
                    (tr->bytessent/1024),
                    (tr->xpack->st_size/1024),
                    started < 3600 ? started/60 : started/60/60 ,
                    started < 3600 ? 'm' : 'h',
                    started < 3600 ? started%60 : (started/60)%60 ,
                    started < 3600 ? 's' : 'm',
                    left < 3600 ? left/60 : left/60/60 ,
                    left < 3600 ? 'm' : 'h',
                    left < 3600 ? left%60 : (left/60)%60 ,
                    left < 3600 ? 's' : 'm',
                    (tr->nomin || (tr->xpack->minspeed == 0.0)) ? "no" : tempstr2 ,
                    (tr->nomax || (tr->maxspeed == 0.0)) ? "no" : tempstr4 ,
                    tr->startresume ? tempstr3 : "no");
        }
      else
        {
          u_respond(u,"  ^-    -----    -------/-------   -----/------  -----/-----      ---");
        }
      tr = irlist_get_next(tr);
    }
  
#ifdef USE_CURL
  if (irlist_size(&gdata.uploads) || fetch_started)
#else /* USE_CURL */
  if (irlist_size(&gdata.uploads))
#endif /* USE_CURL */
    {
      u_respond(u, "Current %s", irlist_size(&gdata.uploads)!=1 ? "uploads" : "upload");
      u_respond(u," ID  User        File                               Status");
      u_respond(u,"  ^-    Speed    Current/    End   Start/Remain");
      u_respond(u," --------------------------------------------------------------");
    }
  
  i = 0;
  ul = irlist_get_head(&gdata.uploads);
  while(ul)
    {
      y = l_print_state(ul);
      
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
          started = min2(359999, gdata.curtime-ul->con.connecttime);
          
          u_respond(u,
                    "  ^- %5.1fK/s    %6" LLPRINTFMT "dK/%6" LLPRINTFMT "dK  %2i%c%02i%c/%2i%c%02i%c",
                    ul->lastspeed,
                    (ul->bytesgot/1024),
                    (ul->totalsize/1024),
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
  
  mydelete(tempstr2);
  mydelete(tempstr3);
  mydelete(tempstr4);
#ifdef USE_CURL
  dinoex_dcld(u);
#endif /* USE_CURL */
  
  u_respond(u, " --------------------------------------------------------------------");
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

static void u_info(const userinput * const u)
{
  int num;
  xdcc *xd;
  char *sizestrstr;
  char *sendnamestr;
  char tempstr[maxtextlengthshort];
  
  updatecontext();
  
  num = get_pack_nr(u, u->arg1);
  if (num <= 0)
    return;
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;
  
  u_respond(u, "Pack Info for Pack #%i:", num);
  
  sendnamestr = getsendname(xd->file);
  if (u->level > 0) 
    {
  u_respond(u, " Filename       %s", xd->file);

  u_respond(u, " Sendname       %s", sendnamestr);
    }
  else
    {
  u_respond(u, " Filename       %s", sendnamestr);
    }
  mydelete(sendnamestr);

  u_respond(u, " Description    %s", xd->desc);
  if (xd->note && xd->note[0])
    {
      u_respond(u, " Note           %s", xd->note);
    }
  
  sizestrstr = sizestr(1, xd->st_size);
  u_respond(u, " Filesize       %" LLPRINTFMT "d [%sB]",
            xd->st_size, sizestrstr);
  mydelete(sizestrstr);
  
  getdatestr(tempstr, xd->mtime, maxtextlengthshort);
  u_respond(u, " Last Modified  %s", tempstr);
  
  if (u->level > 0) 
    {
  u_respond(u, " Device/Inode   %" LLPRINTFMT "u/%" LLPRINTFMT "u",
            (ir_uint64)xd->st_dev, (ir_uint64)xd->st_ino);
    }
  
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
  if ((u->level > 0) && (xd->xtime != 0))
    {
      user_getdatestr(tempstr, xd->xtime, maxtextlengthshort);
      u_respond(u, " Pack Added     %s", tempstr);
    }
  if (xd->lock)
    {
      u_respond(u, " is protected by password");
    }

  return;
}

static void u_removedir(const userinput * const u)
{
  DIR *d;
  char *thedir;
  
  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
     return;

  if (u->arg1[strlen(u->arg1)-1] == '/')
    {
      u->arg1[strlen(u->arg1)-1] = '\0';
    }
  
  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      return;
    }
  
  a_removedir_sub(u, thedir, d);
  mydelete(thedir);
  return;
}

static void u_redraw(const userinput * const UNUSED(u)) {
   updatecontext();
   
   initscreen(0, 1);
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
   int num;
   gnetwork_t *backup;
   int net;
   
   updatecontext();

   net = get_network_msg(u, u->arg3);
   if (net < 0)
     return;

   if (invalid_nick(u, u->arg1) != 0)
      return;

   num = get_pack_nr(u, u->arg2);
   if (num <= 0)
      return;

   u_respond(u,"Sending %s pack %i",u->arg1,num);
   
   backup = gnetwork;
   gnetwork = &(gdata.networks[net]);
   sendxdccfile(u->arg1, "man", "man", num, NULL, NULL);
   gnetwork = backup;
   
   }

static void u_psend(const userinput * const u)
{
  userinput manplist;
  userinput_method_e method;
  channel_t *ch;
  gnetwork_t *backup;
  int net;
  const char *nname;
  
  updatecontext();
  
  net = get_network_msg(u, u->arg3);
  if (net < 0)
    return;

  if (invalid_channel(u, u->arg1) != 0)
    return;

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
  
  ch = NULL;
  backup = gnetwork;
  gnetwork = &(gdata.networks[net]);
  if (u->arg1[0] == '#')
    {
      ch = is_not_joined_channel(u, u->arg1);
      if (ch == NULL)
        {
           gnetwork = backup;
           return;
        }
    }
  
  bzero((char *)&manplist, sizeof(userinput));
  manplist.method = method;
  manplist.net = gnetwork->net;
  manplist.level = ADMIN_LEVEL_PUBLIC;
  a_fillwith_plist(&manplist, u->arg1, ch);
  u_parseit(&manplist);
  nname = gnetwork->name;
  gnetwork = backup;
  
  u_respond(u, "Sending PLIST with style %s to %s on network %s",
            u->arg2 ? u->arg2 : "full",
            u->arg1, nname);
  
}

static void u_mesg(const userinput * const u)
{
  transfer *tr;
  gnetwork_t *backup;
 
  updatecontext();
  
  if (invalid_message(u, u->arg1e) != 0)
    return;

  tr = irlist_get_head(&gdata.trans);
  while(tr)
    {
      backup = gnetwork;
      gnetwork = &(gdata.networks[tr->net]);
      notice(tr->nick,"MESSAGE FROM OWNER: %s",u->arg1e);
      tr = irlist_get_next(tr);
      gnetwork = backup;
    }
  
  u_respond(u, "Sent message to %i %s", irlist_size(&gdata.trans), irlist_size(&gdata.trans)!=1 ? "users" : "user");
  
}

static void u_mesq(const userinput * const u)
{
  int count;
  ir_pqueue *pq;
  gnetwork_t *backup;
  
  updatecontext();
  
  if (invalid_message(u, u->arg1e) != 0)
    return;

  count=0;
  pq = irlist_get_head(&gdata.mainqueue);
  while(pq)
    {
      backup = gnetwork;
      gnetwork = &(gdata.networks[pq->net]);
      notice(pq->nick,"MESSAGE FROM OWNER: %s",u->arg1e);
      count++;
      pq = irlist_get_next(pq);
      gnetwork = backup;
    }
  
  u_respond(u, "Sent message to %i %s", count, count!=1 ? "users" : "user");
  
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
   int num;
   int xfiledescriptor;
   struct stat st;
   char *file;
   char *old;
   xdcc *xd;
   
   updatecontext();
   
   num = get_pack_nr(u, u->arg1);
   if (num <= 0)
      return;

   if (invalid_file(u, u->arg2) != 0)
      return;

   /* verify file is ok first */
   file = mystrdup(u->arg2);
   
   xfiledescriptor = a_open_file(&file, O_RDONLY | ADDED_OPEN_FLAGS);
   if (a_access_fstat(u, xfiledescriptor, &file, &st))
      return;
   
   xd = irlist_get_nth(&gdata.xdccs, num-1);
   if (group_restricted(u, xd))
     return;
   
   a_cancel_transfers(xd, "Pack file changed");
   
   u_respond(u, "CHFILE: [Pack %i] Old: %s New: %s",
             num, xd->file, file);
   
   old = xd->file;
   xd->file     = file;
   xd->st_size  = st.st_size;
   xd->st_dev   = st.st_dev;
   xd->st_ino   = st.st_ino;
   xd->mtime    = st.st_mtime;
   /* change default description */
   if (strcmp(xd->desc, getfilename(old)) == 0)
     {
       mydelete(xd->desc);
       xd->desc = mystrdup(getfilename(xd->file));
     }
   mydelete(old);
   
   cancel_md5_hash(xd, "CHFILE");
   
   write_files();
   }

static void u_adddir(const userinput * const u)
{
  DIR *d;
  char *thedir;
  
  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
     return;

  if (u->arg1[strlen(u->arg1)-1] == '/')
    {
      u->arg1[strlen(u->arg1)-1] = '\0';
    }
  
  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      mydelete(thedir);
      return;
    }
  
  a_adddir_sub(u, thedir, d, 0, NULL, NULL);
  mydelete(thedir);
  return;
}

static void u_addnew(const userinput * const u)
{
  DIR *d;
  char *thedir;
  
  updatecontext();

  if (invalid_dir(u, u->arg1) != 0)
     return;

  if (u->arg1[strlen(u->arg1)-1] == '/')
    {
      u->arg1[strlen(u->arg1)-1] = '\0';
    }
  
  thedir = mystrdup(u->arg1);
  d = a_open_dir(&thedir);
  if (!d)
    {
      u_respond(u,"Can't Access Directory: %s",strerror(errno));
      mydelete(thedir);
      return;
    }
  
  a_adddir_sub(u, thedir, d, 1, NULL, NULL);
  mydelete(thedir);
  return;
}

static void u_chdesc(const userinput * const u) {
   int num;
   xdcc *xd;
   const char *newdesc;
   
   updatecontext();
   
   num = get_pack_nr(u, u->arg1);
   if (num <= 0)
      return;

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   if (group_restricted(u, xd))
     return;
   
   newdesc = u->arg2e;
   if (!u->arg2e || !strlen(u->arg2e)) {
      newdesc = getfilename(xd->file);
      if (strcmp(newdesc, xd->desc) == 0) {
        u_respond(u, "Try Specifying a Description");
        return;
        }
      }
   else {
      clean_quotes(u->arg2e);
      }
   
   u_respond(u, "CHDESC: [Pack %i] Old: %s New: %s",
      num, xd->desc, newdesc);
   
   mydelete(xd->desc);
   xd->desc = mystrdup(newdesc);
   
   write_files();
   }

static void u_chnote(const userinput * const u) {
   int num;
   xdcc *xd;
   
   updatecontext();
   
   num = get_pack_nr(u, u->arg1);
   if (num <= 0)
      return;

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   if (group_restricted(u, xd))
     return;
   
   u_respond(u, "CHNOTE: [Pack %i] Old: %s New: %s",
             num,xd->note,
             u->arg2 ? u->arg2e : "");
   
   mydelete(xd->note);
   
   if (!u->arg2e)
     {
       xd->note = NULL;
     }
   else
     {
       clean_quotes(u->arg2e);
       xd->note = mystrdup(u->arg2e);
     }
   
   write_files();
   }

static void u_chmins(const userinput * const u) {
   int num;
   xdcc *xd;
   
   updatecontext();
   
   num = get_pack_nr(u, u->arg1);
   if (num <= 0)
      return;

   if (!u->arg2 || !strlen(u->arg2)) {
      u_respond(u,"Try Specifying a Minspeed");
      return;
      }

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   if (group_restricted(u, xd))
     return;
   
   u_respond(u, "CHMINS: [Pack %i] Old: %1.1f New: %1.1f",
             num,xd->minspeed,atof(u->arg2));
   
   xd->minspeed = gdata.transferminspeed;
   if ( atof(u->arg2) != gdata.transferminspeed )
      xd->minspeed = atof(u->arg2);
   
   write_files();
   }

static void u_chmaxs(const userinput * const u) {
   int num;
   xdcc *xd;
   
   updatecontext();
   
   num = get_pack_nr(u, u->arg1);
   if (num <= 0)
      return;

   if (invalid_maxspeed(u, u->arg2) != 0)
     return;

   xd = irlist_get_nth(&gdata.xdccs, num-1);
   if (group_restricted(u, xd))
     return;
   
   u_respond(u, "CHMAXS: [Pack %i] Old: %1.1f New: %1.1f",
             num,xd->maxspeed,atof(u->arg2));
   
   xd->maxspeed = gdata.transfermaxspeed;
   if ( atof(u->arg2) != gdata.transfermaxspeed )
      xd->maxspeed = atof(u->arg2);
   
   write_files();
   }

static void u_chgets(const userinput * const u)
{
  int num;
  xdcc *xd;
  
  updatecontext();
  
  num = get_pack_nr(u, u->arg1);
  if (num <= 0)
     return;

  if (!u->arg2 || !strlen(u->arg2))
    {
      u_respond(u,"Try Specifying a Count");
      return;
    }
  
  xd = irlist_get_nth(&gdata.xdccs, num-1);
  if (group_restricted(u, xd))
    return;
  
  u_respond(u, "CHGETS: [Pack %i] Old: %d New: %d",
            num,xd->gets,atoi(u->arg2));
  
  xd->gets = atoi(u->arg2);
  
  write_files();
}

static void u_chatme(const userinput * const u) {
   
   updatecontext();
   
   u_respond(u,"Sending You A DCC Chat Request");
   
   if (setupdccchatout(u->snick, u->hostmask))
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
          outerror(OUTERROR_TYPE_WARN_LOUD,
                   "Unexpected dccchat state %d", chat->status);
          break;
        }
      
      tempstr = mycalloc(maxtextlengthshort);
      
      getdatestr(tempstr, chat->con.connecttime, maxtextlengthshort);
      u_respond(u,"  Connected at %s",tempstr);
      
      getdatestr(tempstr, chat->con.connecttime, maxtextlengthshort);
      u_respond(u,"  Last contact %s",tempstr);
      
      mydelete(tempstr);
      
      u_respond(u, "  Local: %s, Remote: %s",
                chat->con.localaddr,
                chat->con.remoteaddr);
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
   int i;
   int ss;
   xdcc *xd;
   
   updatecontext();
   
   if (u->method==method_out_all) u_respond(u,"Caught SIGUSR2, Rehashing...");
   
   gdata.r_transferminspeed = gdata.transferminspeed;
   gdata.r_transfermaxspeed = gdata.transfermaxspeed;
   a_rehash_prepare();
   
   if (gdata.logfd != FD_UNUSED)
     {
       close(gdata.logfd);
       gdata.logfd = FD_UNUSED;
     }
   
   reinit_config_vars();
   
   set_loginname();
   
   /* go */
   a_read_config_files(u);
   
   /* see what needs to be redone */
   
   u_respond(u,"Reconfiguring...");
   rehash_dinoex();
   a_rehash_needtojump(u);
   a_rehash_channels();
   a_rehash_jump();
   
   if (((!gdata.pidfile) && (gdata.r_pidfile)) ||
       ((gdata.pidfile) && (!gdata.r_pidfile)) ||
       ((gdata.pidfile) && (gdata.r_pidfile) && strcmp(gdata.pidfile,gdata.r_pidfile)))
     {
       u_respond(u,"pidfile changed, switching");
       if (gdata.r_pidfile)
         {
           unlink(gdata.r_pidfile);
         }
       if (gdata.pidfile)
         {
           writepidfile(gdata.pidfile);
         }
     }
   a_rehash_cleanup(u);
   
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
   
   if ( gdata.config_nick == NULL )
     u_respond(u, "**WARNING** Missing vital information: %s, fix and re-rehash ASAP", "user_nick");
   
   if ( gdata.user_realname == NULL )
     u_respond(u, "**WARNING** Missing vital information: %s, fix and re-rehash ASAP", "user_realname");
   
   if ( gdata.user_modes == NULL )
     u_respond(u, "**WARNING** Missing vital information: %s, fix and re-rehash ASAP", "user_modes");
   
   if ( gdata.slotsmax == 0 )
     u_respond(u, "**WARNING** Missing vital information: %s, fix and re-rehash ASAP", "slotsmax");
   
   for (ss=0; ss<gdata.networks_online; ss++)
     {
       if ( !irlist_size(&gdata.networks[ss].servers) )
         u_respond(u, "**WARNING** Missing vital information: %s, fix and re-rehash ASAP", "server");

     }
   
   if ( irlist_size(&gdata.uploadhost) && ( gdata.uploaddir == NULL || strlen(gdata.uploaddir) < 2U ) )
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
           sendaqueue(0, 0, NULL);
         }
     }
   
   mydelete(templine);

   }

static void u_botinfo(const userinput * const u) {
   char *tempstr = mycalloc(maxtextlength);
   struct rusage r;
   int len;
   int ii;
   int ss;
   channel_t *ch;
   gnetwork_t *backup;

   updatecontext();
   
   u_respond(u,"BotInfo:");
   
   u_respond(u, "iroffer-dinoex " VERSIONLONG ", " "http://iroffer.dinoex.net/" "%s%s",
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

   for (ss=0; ss<gdata.networks_online; ss++)
     {
       char *msg;
       u_respond(u, "network: %d: %s", ss + 1, gdata.networks[ss].name);
       msg = mycalloc(maxtextlength);
       my_dcc_ip_show(msg, maxtextlength - 1, &(gdata.networks[ss].myip), ss);
       u_respond(u, "DCC IP: %s NAT=%d", msg, gdata.networks[ss].usenatip);
       mydelete(msg);
       
       backup = gnetwork;
       gnetwork = &(gdata.networks[ss]);
       u_respond(u, "configured nick: %s, actual nick: %s, realname: %s, modes: %s",
                 get_config_nick(),
                 get_user_nick(),
                 gdata.user_realname,
                 get_user_modes());
       gnetwork = backup;
       
       switch (gdata.networks[ss].connectionmethod.how)
         {
         case how_direct:
             {
                u_respond(u, "current server: %s:%u (direct)",
                          gdata.networks[ss].curserver.hostname,
                          gdata.networks[ss].curserver.port);
             }
           break;
         case how_ssl:
             {
                u_respond(u, "current server: %s:%u (ssl)",
                          gdata.networks[ss].curserver.hostname,
                          gdata.networks[ss].curserver.port);
             }
           break;
         case how_bnc:
           if (gdata.networks[ss].connectionmethod.vhost)
             {
               u_respond(u, "current server: %s:%u (bnc at %s:%i with %s)",
                         gdata.networks[ss].curserver.hostname,
                         gdata.networks[ss].curserver.port,
                         gdata.networks[ss].connectionmethod.host,
                         gdata.networks[ss].connectionmethod.port,
                         gdata.networks[ss].connectionmethod.vhost);
             }
           else
             {
               u_respond(u, "current server: %s:%u (bnc at %s:%i)",
                         gdata.networks[ss].curserver.hostname,
                         gdata.networks[ss].curserver.port,
                         gdata.networks[ss].connectionmethod.host,
                         gdata.networks[ss].connectionmethod.port);
             }
           break;
         case how_wingate:
           u_respond(u, "current server: %s:%u (wingate at %s:%i)",
                     gdata.networks[ss].curserver.hostname,
                     gdata.networks[ss].curserver.port,
                     gdata.networks[ss].connectionmethod.host,
                     gdata.networks[ss].connectionmethod.port);
           break;
         case how_custom:
           u_respond(u, "current server: %s:%u (custom at %s:%i)",
                     gdata.networks[ss].curserver.hostname,
                     gdata.networks[ss].curserver.port,
                     gdata.networks[ss].connectionmethod.host,
                     gdata.networks[ss].connectionmethod.port);
           break;
         }
       
       u_respond(u, "current server actual name: %s ",
                 gdata.networks[ss].curserveractualname ? gdata.networks[ss].curserveractualname : "<unknown>");
       if (gdata.networks[ss].serverstatus == SERVERSTATUS_CONNECTED)
         {
           getuptime(tempstr, 0, gdata.networks[ss].connecttime, maxtextlength - 1);
           u_respond(u, "current server connected for: %s ", tempstr);
         }
       
       ch = irlist_get_head(&gdata.networks[ss].channels);
       while(ch)
         {
           snprintf(tempstr, maxtextlength,
                    "channel %10s: joined: %3s",
                    ch->name,
                    ch->flags & CHAN_ONCHAN ? "for " : "no");
           len = strlen(tempstr);
           
           if (ch->flags & CHAN_ONCHAN )
             {
               getuptime(tempstr + len, 0, ch->lastjoin, maxtextlength - 1 - len);
               len = strlen(tempstr);
             }
           
           if (ch->key)
             {
               snprintf(tempstr + len, maxtextlength - len,
                        ", key: %s",
                        ch->key);
               len = strlen(tempstr);
             }
           
           if (ch->fish)
             {
               len += snprintf(tempstr + len, maxtextlength - len,
                               ", fish: %s",
                               ch->fish);
             }
           
           if (ch->plisttime)
             {
               snprintf(tempstr + len, maxtextlength - len,
                        ", plist every %2i min (%s)",
                        ch->plisttime,
                        ch->flags & CHAN_MINIMAL ? "minimal" : (ch->flags & CHAN_SUMMARY ? "summary" : "full"));
             }
           
           u_respond(u, "%s", tempstr);
           
           ch = irlist_get_next(ch);
         }
     }
   
   u_respond(u, "bandwidth: lowsend: %i, minspeed: %1.1f, maxspeed: %1.1f, overallmaxspeed: %i",
             gdata.lowbdwth,gdata.transferminspeed,gdata.transfermaxspeed,gdata.maxb/4);
   
   if (gdata.overallmaxspeed != gdata.overallmaxspeeddayspeed)
     {
       u_respond(u, "           default max: %i, day max: %i ( %i:00 -> %i:00, days=\"%s%s%s%s%s%s%s\" )",
                 gdata.overallmaxspeed/4,gdata.overallmaxspeeddayspeed/4,
                 gdata.overallmaxspeeddaytimestart, gdata.overallmaxspeeddaytimeend,
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
       mydelete(tempstr2);
     }
   
   u_respond(u, "files: pid: %s, log: %s, state: %s, xdcclist: %s",
             (gdata.pidfile?gdata.pidfile:"(none)"),
             (gdata.logfile?gdata.logfile:"(none)"),
             (gdata.statefile?gdata.statefile:"(none)"),
             (gdata.xdcclistfile?gdata.xdcclistfile:"(none)"));
   
   u_respond(u, "config %s: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             gdata.configfile[1]?"files":"file",
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
      snprintf(tempstr, maxtextlength,
               "%" LLPRINTFMT "dMB",
               (gdata.uploadmaxsize/1024/1024));
      u_respond(u, "upload allowed, dir: %s, max size: %s",
                gdata.uploaddir,
                gdata.uploadmaxsize?tempstr:"none");
      }
   
   if (gdata.md5build.xpack) {
      u_respond(u, "calculating MD5/CRC32 for pack %d",
                number_of_pack(gdata.md5build.xpack));
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
  
  if (!u->arg2 || strlen(u->arg2) < 4U)
    {
      u_respond(u,"Try specifying a hostmask longer than 4 characters");
      return;
    }
  
  ignore = get_ignore(u->arg2);
  ignore->flags |= IGN_IGNORING;
  ignore->flags |= IGN_MANUAL;
  ignore->bucket = (num*60)/gdata.autoignore_threshold;
  if (ignore->bucket < 0)
    ignore->bucket = 0x7FFFFFF;
  
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
      if (strcasecmp(ignore->hostmask, u->arg1) == 0)
        {
          mydelete(ignore->hostmask);
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

static void u_bannhost(const userinput * const u)
{
   u_ignore(u);

   if (!u->arg1) return;
   if (!u->arg2 || strlen(u->arg2) < 4U) return;

   a_bann_hostmask(u, u->arg2);
}

static void u_nosave(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   num = max_minutes_waits(&gdata.noautosave, num);
   u_respond(u, "** XDCC AutoSave has been disabled for the next %i %s", num, num!=1 ? "minutes" : "minute");
   
   }


static void u_nosend(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   mydelete(gdata.nosendmsg);
   if (u->arg2e)
      {
         clean_quotes(u->arg2e);
         gdata.nosendmsg=mystrdup(u->arg2e);
      }
   num = max_minutes_waits(&gdata.nonewcons, num);
   u_respond(u, "** XDCC Send has been disabled for the next %i %s", num, num!=1 ? "minutes" : "minute");
   
   }


static void u_nolist(const userinput * const u) {
   int num = 0;
   
   updatecontext();
   
   if (u->arg1) num = atoi(u->arg1);
   num = max_minutes_waits(&gdata.nolisting, num);
   u_respond(u, "** XDCC List and PLIST have been disabled for the next %i %s", num, num!=1 ? "minutes" : "minute");
   
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
  u_respond(u, "msglog: %i %s in log%s%s",
            count,
            count != 1 ? "messages" : "message",
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
  
  u_respond(u, "gdata:  %ld bytes", (long)sizeof(gdata_t));
  
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
      meminfo_depth = gdata.meminfo_depth;
      meminfo = mycalloc(sizeof(meminfo_t) * MEMINFOHASHSIZE * meminfo_depth);
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
          
          irlist_sort2(&xd->mmaps, irlist_sort_cmpfunc_off_t);
          
          for (mm = irlist_get_head(&xd->mmaps); mm; mm = irlist_get_next(mm))
            {
              u_respond(u," %4i | 0x%8.8" LLPRINTFMT "X .. 0x%8.8" LLPRINTFMT "X | %p | %10d",
                        pack_count,
                        (ir_uint64)(mm->mmap_offset),
                        mm->mmap_offset + mm->mmap_size - 1,
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
  int num = 0;
  
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
  
  if (u->arg1) num = atoi(u->arg1);
  sendaqueue(2, num, NULL);
  return;
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

static void u_jump(const userinput * const u)
{
  gnetwork_t *backup;
  int net;
  
  updatecontext();
  
  net = get_network_msg(u, u->arg2);
  if (net < 0)
    return;

  if (u->arg1)
    {
      int num;
      num = atoi(u->arg1);
      
      if ((num < 0) || (num > irlist_size(&gdata.networks[net].servers)))
        {
          u_respond(u,"Try specifying a valid server number, use \"servers\" for a list");
        }
      else
        {
          backup = gnetwork;
          gnetwork = &(gdata.networks[net]);
          gnetwork->serverconnectbackoff = 0;
          switchserver(num-1);
          gnetwork = backup;
        }
    }
  else
    {
      backup = gnetwork;
      gnetwork = &(gdata.networks[net]);
      gnetwork->serverconnectbackoff = 0;
      switchserver(-1);
      gnetwork = backup;
    }
}

static void u_servers(const userinput * const u)
{
  int i;
  server_t *ss;
  int net;
  
  updatecontext();
  
  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;
  
  u_respond(u,"Server List:");
  u_respond(u,"  Num  Server                         Port  Password");
  
  ss = irlist_get_head(&gdata.networks[net].servers);
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
            gdata.networks[net].curserver.hostname,
            gdata.networks[net].curserver.port,
            gdata.networks[net].curserveractualname ? gdata.networks[net].curserveractualname : "<unknown>");
  
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
  
  y = t_print_state(tr);
  
  u_respond(u,"User %s, Hostname %s, Status %s",
            tr->nick,tr->hostname,y);
  
  u_respond(u,"File: %s",getfilename(tr->xpack->file));
  
  u_respond(u, "Start %" LLPRINTFMT "dK, Current %" LLPRINTFMT "dK, End %" LLPRINTFMT "dK (%2.0f%% File, %2.0f%% Xfer)",
            (tr->startresume/1024),
            (tr->bytessent/1024),
            (tr->xpack->st_size/1024),
            ((float)tr->bytessent)*100.0/((float)tr->xpack->st_size),
            ((float)(tr->bytessent-tr->startresume))*100.0/((float)max2(1,(tr->xpack->st_size-tr->startresume))));
  
  tempstr2 = mycalloc(maxtextlengthshort);
  tempstr3 = mycalloc(maxtextlengthshort);
  
  snprintf(tempstr2, maxtextlengthshort, "%1.1fK/s", tr->xpack->minspeed);
  snprintf(tempstr3, maxtextlengthshort, "%1.1fK/s", tr->maxspeed);
  
  u_respond(u, "Min %s, Current %1.1fK/s, Max %s, In Transit %" LLPRINTFMT "dK",
            (tr->nomin || (tr->xpack->minspeed == 0.0)) ? "no" : tempstr2 ,
            tr->lastspeed,
            (tr->nomax || (tr->maxspeed == 0.0)) ? "no" : tempstr3 ,
            (tr->bytessent-tr->lastack/1024));
  
  mydelete(tempstr2);
  mydelete(tempstr3);
  
  left     = min2(359999,(tr->xpack->st_size-tr->bytessent)/((int)(max2(tr->lastspeed,0.001)*1024)));
  started  = min2(359999, gdata.curtime-tr->con.connecttime);
  lcontact = min2(359999, gdata.curtime-tr->con.lastcontact);
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
  
  u_respond(u, "Local: %s, Remote: %s",
            tr->con.localaddr, tr->con.remoteaddr);
  
  u_respond(u,"Sockets: Listen %i, Transfer %i, File %i",
            (tr->con.listensocket == FD_UNUSED) ? 0 : tr->con.listensocket,
            (tr->con.clientsocket == FD_UNUSED) ? 0 : tr->con.clientsocket,
            (tr->xpack->file_fd == FD_UNUSED) ? 0 : tr->xpack->file_fd);
  
#ifdef HAVE_MMAP
  if (tr->mmap_info)
    {
      u_respond(u,"MMAP: [%p] 0x%.8" LLPRINTFMT "X .. 0x%.8" LLPRINTFMT "X .. 0x%.8" LLPRINTFMT "X",
                tr->mmap_info->mmap_ptr,
                (ir_uint64)(tr->mmap_info->mmap_offset),
                (ir_uint64)(tr->bytessent),
                (ir_uint64)(tr->mmap_info->mmap_offset + tr->mmap_info->mmap_size - 1));
    }
#endif
  
}


void u_listdir(const userinput * const u, const char *dir)
{
  DIR *d;
  struct dirent *f;
  char *thefile, *tempstr;
  irlist_t dirlist = {0, 0, 0};
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
      u_respond(u, "Can't Access Directory: %s", strerror(errno));
      return;
    }
  
  while ((f = readdir(d)))
    {
      if (strcmp(f->d_name,".") && strcmp(f->d_name,".."))
        {
          irlist_add_string(&dirlist, f->d_name);
        }
    }
  
  closedir(d);
  
  irlist_sort2(&dirlist, irlist_sort_cmpfunc_string);
  
  if (!irlist_size(&dirlist))
    {
      u_respond(u,"Upload directory is empty");
    }
  else
    {
      u_respond(u, "Listing '%s':", dir);
      
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
  write_files();
}

static void u_rmul(const userinput * const u) {
   char *tempstr;
   
   updatecontext();
   
   if (!gdata.uploaddir) {
      u_respond(u, "No uploaddir defined.");
      return;
      }

   if (invalid_file(u, u->arg1) != 0)
      return;
   
   if (strstr(u->arg1, "/")) {
      u_respond(u,"Filename contains invalid characters");
      return;
      }
   
   tempstr = mystrjoin(gdata.uploaddir, u->arg1, '/');
   
   if (is_file_writeable(tempstr)) {
      if (save_unlink(tempstr) < 0)
         u_respond(u,"Unable to remove the file");
      else
         u_respond(u,"Deleted");
      }
   else
      u_respond(u,"That filename doesn't exist");
   
   mydelete(tempstr);
   
   }

static void u_crash(const userinput * const UNUSED(u)) {
   
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
  int net;
  
  updatecontext();

  net = get_network_msg(u, u->arg1);
  if (net < 0)
    return;

  u_respond(u,"Channel Members:");
  
  ch = irlist_get_head(&gdata.networks[net].channels);
  while(ch)
    {
      j = 0;
      member = irlist_get_head(&ch->members);
      while(member)
        {
          if (!(j%USERS_PER_CHAN_LINE))
            {
              snprintf(tempstr, maxtextlength, "%s: ", ch->name);
            }
          snprintf(tempstr + strlen(tempstr),
                   maxtextlength - strlen(tempstr),
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
      
      u_respond(u, "%s: %i %s", ch->name, j, j!=1 ? "users" : "user");
      
      ch = irlist_get_next(ch);
    }
  
  mydelete(tempstr);
  
}
/* End of File */
