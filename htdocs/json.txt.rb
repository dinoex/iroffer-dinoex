# htdocs/json.txt.rb: export file info as list.

# Author::    Dirk Meyer
# Copyright:: Copyright (c) 2012 - 2019 Dirk Meyer
# License::   Distributes under the same terms as Ruby

require 'json'

bot = IrofferEvent.new

@info = {}
@info[ :bot ] = bot.mynick
@info[ :version ] = bot.irconfig( "version" )
@info[ :created ] = Time.now.to_s
@info[ :packs ] =  []

pack = 1
while true do
  bytes = bot.info_pack(pack, "bytes" )
  break if bytes.nil?

  desc = bot.info_pack(pack, "desc" )
  desc.force_encoding( 'utf-8' )
  size = bot.info_pack(pack, "size" )
  gets = bot.info_pack(pack, "gets" )
  @info[ :packs ] << [ Pack: pack, Gets: gets, Size: size, Desc: desc ]
  pack += 1
end

@info[ :count ] = pack
if pack > 500
  # compressed
  puts JSON.dump( @info )
else
  puts JSON.pretty_generate( @info )
end

# eof
