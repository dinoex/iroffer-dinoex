#!/bin/sh

#### WHAT THIS DOES ####
# 
# If you have a dynamic ip address and require setting 'usenatip'
# in the iroffer config file this script can auto-generate a 1 line
# config file containing your current ip address
# run iroffer with multiple config files (your normal config plus
# this auto-generated config)
# 
# If your ip address changes, the file will be updated by this script
# and iroffer rehashed to see the change
#
# Call this scrip when your IP changes or put in in your crontab.
# Rin "crontab -e" and place the following line in the editor:
# */5 * * * * /full/path/to/iroffer/dynip.sh

URL="http://iroffer.org/myip.cgi"
CONFIGFILE="dynip.config"
PIDFILE="mybot.pid"

set -e
set -u

curl --silent ${URL} \
 |sed -e 's=^=usenatip =' \
 > ${CONFIGFILE}.tmp

test -f ${CONFIGFILE} || touch ${CONFIGFILE}

if diff ${CONFIGFILE} ${CONFIGFILE}.tmp; then
  echo ip unchanged
  rm -f ${CONFIGFILE}.tmp
else
  echo ip changed
  mv -f ${CONFIGFILE}.tmp ${CONFIGFILE}
  if [ -f ${PIDFILE} ]; then
    echo rehashing iroffer
    kill -USR2 `cat ${PIDFILE}`
  else
    echo no iroffer running
  fi
fi

exit 0
