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
utf8="cat"
sh ./Lang en
awk -f ./admin.awk src/iroffer_admin.c > help-admin-en.neu
copy_if_differ help-admin-en.txt help-admin-en.neu
for lang in de it fr
do
	if test ! "${lang}.sed" -nt "${lang}.txt"
	then
		echo -n "parsing ... "
		sed -e 's| |	|' en.txt > en.tmp
		sed -e 's| |	|' "${lang}.txt" > "${lang}.tmp"
		join -t '	' en.tmp "${lang}.tmp" |
		sed -e 's|^[0-9]*|s|' -e 's|" "|"`"|' -e 's|"$|"	|' |
		awk -F '	' '
{ 
        if ( $2 == $3 )
                next
        print
}
' |
		${utf8} |
		sed -e 's|\\|\\\\|g' -e 's|\*|\\*|g' -e 's|\+|\\+|g' -e 's|\.|\\.|g' -e 's|\[|\\[|g' -e 's|\]|\\]|g' > "${lang}.sed"
		rm -f en.tmp "${lang}.tmp"
	fi
	sed -f "${lang}.sed" src/iroffer_admin.c |
	awk -f ./admin.awk > "help-admin-${lang}.neu"
	copy_if_differ "help-admin-${lang}.txt" "help-admin-${lang}.neu"
done
#
echo "New in en.txt:"
nr="0"
if test -f en.txt
then
	nr=`tail -1 en.txt | cut -d " " -f1`
fi
fgrep -h \" src/iroffer*.c src/dinoex*.c |
grep -v "^#include" |
fgrep -v "NOTRANSLATE" |
sed -e 's|\\"|°|g' |
awk -F \[\"\] '
{
	for ( I = 2; I < NF ; I ++ ) {
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
	grep=`fgrep "${text}" src/*.c | fgrep -v NOTRANSLATE`
	if test "${grep}" != ""
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
else
	diff -u de.txt de.txt.neu
fi
cut -d " " -f1 en.txt >en.txt.1
cut -d " " -f1 de.txt >de.txt.1
cut -d " " -f1 it.txt >it.txt.1
cut -d " " -f1 fr.txt >fr.txt.1
echo "Obsolete in de.txt:"
diff de.txt.1 en.txt.1
echo "Obsolete in it.txt:"
diff it.txt.1 en.txt.1
echo "Obsolete in fr.txt:"
diff fr.txt.1 en.txt.1
rm -f de.txt.1 en.txt.1 it.txt.1 fr.txt.1
# eof
