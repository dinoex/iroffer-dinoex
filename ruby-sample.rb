#!/usr/local/bin/ruby -w

def write_log( msg )
  f = File.open( "ruby-dump.txt", "a+" )
  f.write Time.now.to_s
  f.write "\n"
  f.write msg
  f.write "\n"
  f.flush
  f.close
end

class IrofferEvent
  def on_server
    write_log( "SERVER on " + network + " " + inputline )
  end
  def on_notice
    write_log( "NOTICE from " + hostmask + " on " + network + " " + message )
  end
  def on_privmsg
    write_log( "PRIVMSG from " + hostmask + " on " + network + " " + message )
    if /iroffer-dinoex/.match( message )
      msg = "Thanks for using iroffer."
      privmsg( nick, msg )
      warning( nick + " uses iroffer in " + channel + " on " + network )
      mode( channel, "+v " + nick )
    end
    if /autoadd/.match( message )
      command( "autoadd" )
    end
  end
end

# eof
