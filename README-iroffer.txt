 --- What is iroffer? ---

iroffer is a software program that acts as a fileserver for IRC.
It is similar to a FTP server or WEB server, but users can download
files using the DCC protocol of IRC instead of a web browser.

iroffer is a standalone executable written entirely in C.
Designed for speed and efficency, iroffer can transfer over 95MByte/sec
over a gigabit ethernet connection.

See the file "README.modDinoex" for changelog and features.

 --- Website ---

Visit the iroffer web site http://iroffer.dinoex.net/

This is an improved version of iroffer
 the unmodifed version can be found at http://iroffer.org/

 --- How to Contact the Author ---

Suggestions and feeback welcome, visit me on:
 #dinoex on irc.euirc.net
 #dinoex on irc.otakubox.at
 #dinoex on irc.rizon.net

 --- Supported Platforms ---

Linux
FreeBSD, OpenBSD, BSD/OS, NetBSD
Solaris/SunOS
MacOS X Server
MacOS X
Win95/98/NT/2000/2003/2008/7

All other platforms have not been tested, but will probabily work

 --- License ---

Please read the file "LICENSE"

 --- Documentation ---

Please read the file "doc/iroffer.1.txt" and other Documentation.

A sample config file with name "sample.conifg" is provided.

More Documentation is online at http://iroffer.dinoex.de/wiki/iroffer

Please view the file "doc/INSTALL-linux-en.html"
or browse it online at http://iroffer.dinoex.net/INSTALL-linux-en.html

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

 --- How To Use Cron ---

Edit the iroffer.cron file's iroffer_dir, iroffer_exec, and
iroffer_pid variables

then crontab -e and place the following line in the editor

*/5 * * * * /full/path/to/iroffer/iroffer.cron

 --- End of README ---
