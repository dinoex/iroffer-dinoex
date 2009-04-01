#!/bin/sh
#
all="LICENSE README-iroffer.txt THANKS header.html footer.html"
fen="README.modDinoex sample.config help-admin-en.txt"
fde="LIESMICH.modDinoex beispiel.config help-admin-de.txt"
fit="README.modDinoex sample.config help-admin-it.txt"
#
set -e
ver=`grep ^VERSION= Configure  | cut -d '=' -f2`
name="iroffer-dinoex-${ver}"
#
# Convert into DOS files
zip -l a.zip ${all} ${fen} ${fde} ${fit}
unzip -o a.zip
rm -f a.zip
#
./Configure -t -g -u
# Build translatetd versions
for lang in de it en
do
	./Lang "${lang}"
	make clean
	make
	case "${lang}" in
	en)
		dir="${name}-win32"
		bin="iroffer.exe"
		;;
	*)
		dir="${name}-win32-${lang}"
		bin="iroffer-${lang}.exe"
		;;
	esac
	mkdir "${dir}"
	mv iroffer.exe "${dir}/${bin}"
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
	esac
done
# Build enlglish version
zip -r "${name}-win32.zip" "${name}-win32/"
zip -r "${name}-win32-de.zip" "${name}-win32-de/"
zip -r "${name}-win32-it.zip" "${name}-win32-it/"
# eof
