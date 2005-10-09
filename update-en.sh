#!/bin/sh
nr="0"
if test -f en.txt
then
	nr=`tail -1 en.txt | cut -d " " -f1`
fi
fgrep -h \" src/iroffer*.c |
grep -v "^#include" |
awk -F \" '
{
	for ( I = 2; I <= NF ; I += 2 ) {
		if ( $I == "" )
			continue
		print "\"" $I "\""
	}
#	print
#	print ""
}
' |
while read text
do
	if fgrep -q "${text}" en.txt
	then
		continue
	fi
	nr=$((${nr} + 1))
	echo "${nr} ${text}"
	echo "${nr} ${text}" >> en.txt
done

# eof
