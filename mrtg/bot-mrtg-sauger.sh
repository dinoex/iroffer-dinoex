#!/bin/sh
log="${1-mybot.log}"
new=`grep -s Stat: "${log}" | tail -n 1`
new="${new#*Stat: }"
if test "${new}" = ""
then
	A="0"
	B="0"
else
	A="${new%% Sls,*}"
	A="${A##* }"
	A="${A%%/*}"
	B="${new%% Q,*}"
	B="${B##* }"
	B="${B%%/*}"
fi
echo "$A"
echo "$B"
echo "$A"
echo "$B"
echo ""
echo "MIB"
# ** 2006-02-22-13:39:32: Stat: 1/20 Sls, 2/20 Q, 2464.8K/s Rcd, 0 SrQ (Bdw: 10776K, 89.8K/s, 2473.3K/s Rcd)
