#!/bin/sh
ver=`grep ^VERSION= Configure  | cut -d '=' -f2`
#
all="LICENSE README-iroffer.txt THANKS header.html footer.html"
fen="README.modDinoex sample.config help-admin-en.txt"
fde="LIESMICH.modDinoex beispiel.config help-admin-de.txt"
#
set -e
name="iroffer-dinoex-${ver}"
./Configure -t -g -u
./Lang de
make
mv iroffer.exe iroffer-de.exe 
./Lang en
make clean
make
mkdir "${name}-win32"
mkdir "${name}-win32-de"
rsync -av htdocs "${name}-win32/"
rsync -av htdocs "${name}-win32-de/"
zip -l a.zip ${all} ${fen} ${fde}
unzip -o a.zip
rm -f a.zip
cp -p ${all} ${fen} iroffer.exe "${name}-win32/"
cp -p ${all} ${fde} iroffer-de.exe "${name}-win32-de/"
zip -r "${name}-win32.zip" "${name}-win32/"
zip -r "${name}-win32-de.zip" "${name}-win32-de/"
# eof
