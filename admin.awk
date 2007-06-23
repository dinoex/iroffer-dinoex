#!/usr/bin/awk -f
BEGIN {
	FS = "\""
	LAST = 0
}
END {
	print "** For additional help, see the documentation at http://iroffer.dinoex.net/"
}
/^{[0-9]/{
	LEVEL = substr( $0, 2, 1 )
	if ( LEVEL != LAST ) {
		LAST = LEVEL
		M = "<Unknown>"
		if ( LEVEL == "1" )
			M = "Info"
		if ( LEVEL == "2" )
			M = "Transfer"
		if ( LEVEL == "3" )
			M = "Pack"
		if ( LEVEL == "4" )
			M = "File"
		if ( LEVEL == "5" )
			M = "Misc"
		if ( LEVEL == "6" )
			M = "Bot"
		printf( "** -- %s Commands --\n", M )
	}
	if ( NF == 7 ) {
		printf( "**   %-20s : %s\n", $2 " " $4, $6 )
		next
	}
	if ( NF == 5 ) {
		printf( "**   %-20s : %s\n", $2, $4 )
		next
	}
	if ( NF == 13 ) {
		MORE = $6 "\"" $7 "\"" $8 "\"" $9 "\"" $10 "\"" $11 "\"" $12
		gsub( "\\\\", "", MORE )
		printf( "**   %-20s : %s\n", $2 " " $4, MORE )
		next
	}
	print
	print NF
}
# eof
