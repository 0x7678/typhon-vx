#!/bin/bash

# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

lock="$GSMSESSION"/scan.lock

if [ -f "$lock" ]; then
  echo "Another scan running? ($lock)"
  exit 1
fi

touch "$lock"

# pick a slave
slave=`ps axu | grep -v "scan.lock" | grep "[s]niff --slave" | tr -s " " | cut -d " " -f 2 | head -n 1`

# get its socket
socket=`ps "$slave" | grep -oE "/tmp/osmocom_l2_.$"`

# kill it with fire
kill -TERM "$slave"
sleep 1
kill -9 "$slave"

# init a scan
"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/layer23/src/misc/cell_log -s "$socket" -l - 2>&1 | bash gsm_parse_cell_log.sh 10 | tee "$GSMSESSION"/scan.current

rm "$lock"

# return slave to pool
#"$GSMPATH"/mysrc/sniff --slave -s "$socket" &
