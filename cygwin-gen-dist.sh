#!/bin/sh
#
all="LICENSE README THANKS header.html footer.html doc/iroffer.1.txt doc/xdcc.7.txt"
fen="README.modDinoex sample.config help-admin-en.txt"
fde="LIESMICH.modDinoex beispiel.config help-admin-de.txt"
fit="README.modDinoex sample.config help-admin-it.txt"
ffr="README.modDinoex exemple.config help-admin-fr.txt"
#
set -e
ver=`grep ^VERSION= Configure  | cut -d '=' -f2 | tr -d '"'`
name="iroffer-dinoex-${ver}"
cygwin="cygwin-"`uname -r`
cygwin="${cygwin%(*}"
#
# Convert into DOS files
zip -l a.zip ${all} ${fen} ${fde} ${fit} ${ffr}
unzip -o a.zip
rm -f a.zip
#
# Enable options
# Activer les options
./Configure -tls -geoip -upnp -ruby -debug
make clean
if test ! -f doc/iroffer.1.txt
then
	# generate ASCII files
	make doc
	./update-en.sh
fi
#
# Build translated versions
# Construire la version traduite
for lang in de it fr en
do
	make ${lang}
	case "${lang}" in
	en)
		dir="${name}-win32-${cygwin}"
		bin="iroffer.exe"
		src="iroffer.exe"
		;;
	*)
		dir="${name}-win32-${cygwin}-${lang}"
		bin="iroffer-${lang}.exe"
		src="iroffer-${lang}.exe"
		;;
	esac
	rm -rf "${dir}"
	mkdir "${dir}"
	mv "${src}" "${dir}/${bin}"
	rsync -av htdocs ${all} "${dir}/"
	case "${lang}" in
	en)
		cp -p ${fen} "${dir}/"
		;;
	de)
		cp -p ${fde} "${dir}/"
		;;
	it)
		cp -p ${fit} "${dir}/"
		;;
	fr)
		cp -p ${ffr} "${dir}/"
		;;
	esac
	if test -e /bin/7za
	then
		rm -f "${dir}.7z"
		7za a -mx=9 -r "${dir}.7z" "${dir}/"
	else
		rm -f "${dir}.zip"
		zip -r "${dir}.zip" "${dir}/"
	fi
done
#
# Build DLL pack
dlldir="iroffer-dinoex-win32-${cygwin}-dll"
rm -rf "${dlldir}"
mkdir "${dlldir}"
cygcheck "${name}-win32-${cygwin}/iroffer.exe" |
tr '\\' '/' |
while read line
do
	dll="${line## }"
	case "${dll}" in
	*.exe|*/system32/*|*/syswow64/*|*/SysWOW64/*)
		continue
		;;
	esac
	echo "${dll}"
	cp -p "${dll}" "${dlldir}/"
done
if test -e /bin/7za
then
	rm -f "${dlldir}.7z"
	7za a -mx=9 -r "${dlldir}.7z" "${dlldir}"
else
	rm -f "${dlldir}.zip"
	zip -r "${dlldir}.zip" "${dlldir}"
fi
# eof
