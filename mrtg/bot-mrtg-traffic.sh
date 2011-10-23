#!/bin/sh
log="${1-mybot.log}"
new=`grep -s Stat: "${log}" | tail -n 1`
new="${new#*Stat: }"
if test "${new}" = ""
then
	A="0"
	B="0"
else
	A="${new%% Down*}"
	A="${A##* }"
	A="${A%%K*}"
	B="${new%% Up,*}"
	B="${B##* }"
	B="${B%%/*}"
	B="${B%%K*}"
	if test "${A}" = ""
	then
		A="${new#*Bdw: }"
		A="${A#* }"
		A="${A%%K*}"
		B="0"
	fi
fi
echo "${A}"
echo "$B"
echo "$A"
echo "$B"
echo ""
echo "MIB"
# eof
