#!/usr/local/bin/ruby -w

def write_log( *args )
  f = File.open( "ruby-dump.txt", "a+" )
  f.write Time.now.to_s
  f.write " "
  f.write args.join(' ')
  f.write "\n"
  f.flush
  f.close
end

class IrofferEvent
  def command2( *args )
    command( args.join(' ') )
  end
  def on_server
    write_log( "SERVER on", network, inputline )
  end
  def on_notice
    write_log( "NOTICE from", hostmask, "in", channel, "on", network, message )
  end
  def on_privmsg
    write_log( "PRIVMSG from", hostmask, "in", channel, "on", network, message )
    if /iroffer-dinoex/.match( message )
      msg = "Thanks for using iroffer."
      privmsg( nick, msg )
      warning( nick + " uses iroffer in " + channel + " on " + network )
      mode( channel, "+v " + nick )
    end
    if /!autoadd/.match( message )
      command( "autoadd" )
    end
    if /!hop/.match( message )
      command2( "HOP", channel, network )
    end
  end
end

# eof
