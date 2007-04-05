#!/bin/sh
awk -f ./admin.awk src/iroffer_admin.c > admin.neu
#wdiff admin.txt admin.neu
if diff admin.txt admin.neu
then
	rm -f admin.neu
else
	mv -f admin.neu admin.txt
fi
echo "New in en.txt:"
nr="0"
if test -f en.txt
then
	nr=`tail -1 en.txt | cut -d " " -f1`
fi
fgrep -h \" src/iroffer*.c src/dinoex*.c |
grep -v "^#include" |
awk -F \[\"\] '
{
	MORE = 0
	for ( I = 2; I <= NF ; I ++ ) {
		gsub( "[\\\\]", "\\\\", $I )
		LEN = length( $I ) - 1
		if ( substr( $I, LEN, 2 ) == "\\\\" ) {
			printf( "\"%s\\", $I )
			MORE = 1
		} else {
			if ( MORE == 0 ) {
				if ( $I != "" )
					print "\"" $I "\""
				I ++
			} else {
				MORE = 0
				if ( $I != "" ) {
					print "\"" $I "\""
				} else {
					print $I "\""
				}
				I ++
			}
		}
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
#
# 
echo "Obsolete in en.txt:"
sed -e 's|\\|\\\\|g' -e "s| \"| '\"|" -e "s|\"$|\"'|" en.txt |
while read nr text
do
	text="${text#'}"
	text="${text%'}"
	if fgrep -q "${text}" src/*.c
	then
		continue
	fi
	echo "${nr} ${text}"
done
# eof
