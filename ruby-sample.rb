#!/usr/local/bin/ruby -w

# iroffer_input returns an irc line
x = iroffer_input
wort = x.split( " ", 4 )
case wort[1]
when 'PRIVMSG', 'NOTICE'
  if /iroffer-dinoex/.match( wort[3] )
    nick = wort[0].delete( ':' ).sub( /!.*/, '' )
    msg = "Thanks for saying #{wort[2]}"
    # send message to nick
    r = iroffer_privmsg( nick, msg )
  end
end

# eof
