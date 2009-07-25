#!/usr/local/bin/ruby -w

# Write somthing in a logfile with date and time.
def write_log( *args )
  f = File.open( "ruby-dump.txt", "a+" )
  f.write Time.now.to_s
  f.write " "
  f.write args.join(' ')
  f.write "\n"
  f.flush
  f.close
end

# Do not change the name of this class.
class IrofferEvent

  # for esay use of commands
  def command2( *args )
    command( args.join(' ') )
  end

  # callend on each server line
  def on_server
    write_log( "SERVER on", network, inputline )
  end

  # called on each notice
  def on_notice
    write_log( "NOTICE from", hostmask, "in", channel, "on", network, message )
  end

  # called on each privms, including channel posts
  def on_privmsg
    write_log( "PRIVMSG from", hostmask, "in", channel, "on", network, message )

    # trigger on text
    if /iroffer-dinoex/.match( message )
      msg = "Thanks for using iroffer."
      # send text to user
      privmsg( nick, msg )
      # log a warning in the iroffer logfile
      warning( nick + " uses iroffer in " + channel + " on " + network )
      # give voice to user in channel
      mode( channel, "+v " + nick )
    end

    # trigger on text
    if /!autoadd/.match( message )
      # execute admin command
      command( "autoadd" )
    end

    # trigger on text
    if /!hop/.match( message )
      # execute admin command
      command2( "HOP", channel, network )
    end
  end
end

# eof
