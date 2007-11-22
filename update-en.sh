#!/bin/sh

copy_if_differ() {
	if diff "${1}" "${2}"
	then
		rm -f "${2}"
	else
		mv -f "${2}" "${1}"
	fi
}

LANG="C"
awk -f ./admin.awk src/iroffer_admin.c > help-admin-en.neu
copy_if_differ help-admin-en.txt help-admin-en.neu
lang="de"
if test ! -f "${lang}.sed"
then
	echo -n "parsing ... "
	grep -v "^#" "${lang}.txt" |
	while read nr text
	do
		if ! grep -q "^${nr} " en.txt
		then
			continue
		fi
		en=`grep "^${nr} " "en.txt"`
		en="${en#* }"
		text=`grep "^${nr} " "${lang}.txt"`
		text="${text#* }"
		if test "${en}" = "${text}"
		then
			continue
		fi
		echo "s°${en}°${text}°"
	done |
	sed -e 's|\\|\\\\|g' -e 's|\*|\\*|g' -e 's|\+|\\+|g' -e 's|\.|\\.|g' -e 's|\[|\\[|g' -e 's|\]|\\]|g' > "${lang}.sed"
	echo "done"
fi
sed -f "${lang}.sed" src/iroffer_admin.c |
awk -f ./admin.awk > "help-admin-${lang}.neu"
copy_if_differ "help-admin-${lang}.txt" "help-admin-${lang}.neu"
#
echo "New in en.txt:"
nr="0"
if test -f en.txt
then
	nr=`tail -1 en.txt | cut -d " " -f1`
fi
fgrep -h \" src/iroffer*.c src/dinoex*.c |
grep -v "^#include" |
sed -e 's|\\"|°|g' |
awk -F \[\"\] '
{
	for ( I = 2; I <= NF ; I ++ ) {
		gsub( "[\\\\]", "\\\\", $I )
		if (( $I != "" ))
			print "\"" $I "\""
		I ++
	}
}
' |
sed -e 's|°|\\\\"|g' |
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
sed -e 's|\\|\\\\|g' en.txt |
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
echo "Doppelt in en.txt:"
cut -d " " -f2- en.txt | sort >en.txt.1
cut -d " " -f2- en.txt | sort -u >en.txt.2
diff en.txt.1 en.txt.2
rm -f en.txt.1 en.txt.2
sort -n de.txt en.txt -u >de.txt.neu
if diff -q de.txt de.txt.neu
then
	rm -f de.txt.neu
	exit 0
fi
diff -u de.txt de.txt.neu
# eof
