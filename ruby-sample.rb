#!/usr/local/bin/ruby -w

# Write something in a logfile with date and time.
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

    # trigger on text somewhere in the message
    if /iroffer-dinoex/.match( message )
      msg = "Thanks for using iroffer."
      # send text to user
      privmsg( nick, msg )
      # log a warning in the iroffer logfile
      warning( nick + " uses iroffer in " + channel + " on " + network )
      # give voice to user in channel
      mode( channel, "+v " + nick )
    end

    # trigger on exact text
    if message == '!autoadd'
      # execute admin command
      command( "msg", irconfig( "owner_nick" ), "!autoadd was triggered" )
      command( "autoadd" )
    end

    # trigger on exact text
    if message == '!hop'
      # execute admin command
      command( "HOP", channel, network )
    end
  end

  # called on add / rename / remove
  def on_packlist
    command( "AMSG", "Packlist has been updated" )
  end

  # called on each pack added
  def on_added
    write_log( "Added pack nr", added_pack, "with file", added_file )

    group = info_pack(added_pack, "group" )
    desc = info_pack(added_pack, "desc" )
    bytes = info_pack(added_pack, "bytes" )
    megabytes = info_pack(added_pack, "size" ) # human readable
    crc = info_pack(added_pack, "crc32" )
    md5 = info_pack(added_pack, "md5sum" )
    xtime = info_pack(added_pack, "xtime" ) # added_time
    write_log( "group:",  group, "desc:", desc, "size:", bytes )

    # generate a trigger for each new pack.
    command( "CHTRIGGER", added_pack.to_s, "#{group}#{added_pack}" )

    # backup pack to a some other bots
    command( "BATCH", "XDCC|Archiv1", added_pack.to_s )
    command( "BATCH", "XDCC|Archiv2", added_pack.to_s )

    # custom announce
    text = "\"addded "
    text << "\00304" # color red
    text << desc
    text << "\003\017" # end color
    text << " - "
    text << megabytes.to_s
    if not group.nil?
      text << " in "
      text << group
    end
    text << " CRC "
    text << crc
    text << " - /MSG "
    text << mynick
    text << "XDCC SEND "
    text << added_pack.to_s
    text << "\""
    command( "AMSG", text )
  end

  # Admin Command: RUBY action nick msg
  def action( nick, msg )
    command( "msg", nick, "\001ACTION #{msg}\001" )
  end

  # Admin Command: RUBY rmdup [ test | remove ]
  def rmdup( remove = nil )
    seen = Hash.new
    pack = 1
    while true do
      file = info_pack(pack, "file" )
      if file.nil?
        break
      end
      file.sub!( /^.*\//, '' )
      if seen.has_key?( file )
        warning("duplicate file in pack #{pack} first pack #{seen[ file ]}: #{file}" )
        if  remove != "remove"
          pack += 1
          next
        end
        command( "remove", pack )
        next
      end
      seen[ file ] = pack
      pack += 1
    end
  end

end

# eof
