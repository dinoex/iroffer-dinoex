Welcome to iroffer 1.4.b03, by PMG (released December 12th 2005)


 --- Info ---

iroffer by David Johnson (PMG)
Copyright (C) 1998-2005 David Johnson

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
or view it online at http://www.gnu.org/copyleft/gpl.html

All files except src/iroffer_md5.[ch] are covered by the GPL.
src/iroffer_md5.[ch] (written by Colin Plumb in 1993 and modified
by Ian Jackson in 1995) are in the public domain.

 --- Website and Mailing List ---

Visit the iroffer web site http://iroffer.org/

Have you joined the iroffer mailing list? see the web site

 --- How to Contact the Author ---

To obtain an up-to-date email address please goto http://iroffer.org/contact.html

 --- What is iroffer? ---

iroffer is a software program that acts as a fileserver for IRC.  It
is similar to a FTP server or WEB server, but users can download
files using the DCC protocol of IRC instead of a web browser.

Unlike similar programs, iroffer is not a script, it is a standalone
executable written entirely in c from scratch with high transfer
speed and effeciency in mind.  iroffer has been found to transfer
over 50MByte/sec over a gigabit ethernet connection.

 --- Supported Platforms ---

Linux
FreeBSD, OpenBSD, BSD/OS, NetBSD
Solaris/SunOS
UP-UX
IRIX
Digital UNIX
MacOS X Server
MacOS X
AIX
Win95/98/NT/2000

All other platforms have not been tested, but will probabily work


 --- Documentation ---

Documentation about iroffer is included on the website, http://iroffer.org/


 --- To Compile ---

Run the "Configure" script by typing "./Configure"
then type "make".
no errors or warnings should appear when compiling

A sample config file is provided.


 --- Why use iroffer to offer? ---

 - extremely fast dcc transfers
 - extremely low cpu usage, and reasonable ram usage
 - its a program not a script that is slowed by a bulky irc program
 - only completed transfers are counted
 - supports dcc resume
 - set max amount of transfers per hostname
 - user friendly error messages for users (no "clamp timeout")
 - allows a pack to be designated as a "high demand" pack which can
     have special limitations and its own separate queue
 - supports virtual hosts
 - auto-send feature, send a pack to someone when they say something
 - auto-saves xdcc information
 - remote administration via /msg or DCC CHAT
 - bandwidth monitoring, shows last 2 minutes bandwidth average
 - Allow sending of queued packs when using low amounts of bandwidth,
     comes in handy when all slots are filled with people transferring
     1k/sec, will keep sending out queued items while bandwidth usage
     is under a specified amount
 - Background or Foreground mode. background mode does not require
     screen and is cronable
 - Chroot support (run iroffer from inside a chroot'ed environment)
 - overall and pack minspeed
 - maximum bandwidth limiting, when set, iroffer will not use more than
     the set amount of bandwidth (keeps your sysadmin happy)
 - can set different maximum bandwidth limits depending on time of day
     and day of week (keeps your sysadmin very happy) 
 - logging
 - auto-ignores flooders
 - support for direct, bnc, wingate, and custom proxy irc server connections
 - ignore list


 --- What files can I offer? ---

 - Your channel's rules or FAQ
 - Pictures, Music, Programs, Shareware/Freeware, Programs you have
     written, etc... 
 - Help elevate overloaded ftp and http servers by mirroring content
     for your channel's members
 - Note: Be sure to consult and follow the appropriate copyright
   statement, distibution policy, and/or license agreement before
   offering any content you didn't create yourself


 --- How to let colors work while using screen ---

create a file in your home directory named ".screenrc", and put the
following lines in it:

termcap  vt100 'AF=\E[3%dm:AB=\E[4%dm'
terminfo vt100 'AF=\E[3%p1%dm:AB=\E[4%p1%dm'


 --- How To Use Cron ---

Edit the iroffer.cron file's iroffer_dir, iroffer_exec, and
iroffer_pid variables

then crontab -e and place the following line in the editor

*/5 * * * * /full/path/to/iroffer/iroffer.cron


 --- Signal Handling ---

iroffer will handle the following signal:

SIGUSR1 (kill -USR1 xxxx)  jumps to another server (same as admin command "jump")
SIGUSR2 (kill -USR2 xxxx)  re-reads config file    (same as admin command "rehash")
SIGTERM (kill xxxx)        shuts down iroffer      (same as admin command "shutdown")

 --- End of README ---

@(#) README 1.93@(#)
pmg@wellington.i202.centerclick.org|README|20051213024432|46824
