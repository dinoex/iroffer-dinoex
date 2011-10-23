#!/bin/sh
if test $# != 2
then
	echo "Usage: ${0##*/} mybot.log mrtg.log" >&2
	exit 64
fi
grep "Stat: " "${1}" > /tmp/history-mrtg.1
cat "${2}" |
while read time rest
do
	key=`date -r "${time}" "+%Y-%m-%d-%H:%M:00:"`
	echo "** ${key} ${time} ${rest}"
done > /tmp/history-mrtg.2
sort /tmp/history-mrtg.1 /tmp/history-mrtg.2 > /tmp/history-mrtg.3
cat /tmp/history-mrtg.3 |
LANG="C"
export LANG
awk 'BEGIN {
	LAST = 0
}
/Stat:/ {
	LAST = 1
	TRAFFIC = $14
	sub( "K.*$", "", TRAFFIC )
	next
}
{
	if ( LAST == 0 )
		next
	if ( NF != 7 )
		next
	if ( ( $4 != 0 ) || ( $6 != 0 ) )
		next
	NEW = sprintf( "%s %d 0 %d 0", $3, TRAFFIC, TRAFFIC )
	OLD = substr( $0, 25 )
	if ( OLD == NEW )
		next
	print "s=^" OLD "=" NEW "="
}
' /tmp/history-mrtg.3 > /tmp/history-mrtg.sed
wc -l /tmp/history-mrtg.sed
if test -s /tmp/history-mrtg.sed
then
	sed -i .bak -f /tmp/history-mrtg.sed "${2}"
fi
exit 0
