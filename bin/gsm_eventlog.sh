#!/bin/bash

CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

trap bashtrap INT

bashtrap()
{
    echo "$0 exiting...";
    kill $smspid $callpid $mappid $imeipid
	# XXX solve sudo problem.
    exit;
}

stdbuf -oL "$GSMPATH"/bin/gsm_parse_sms.sh >> "$GSMSESSION"/event.log &
smspid=$!;
stdbuf -oL "$GSMPATH"/bin/gsm_parse_call.sh >> "$GSMSESSION"/event.log &
callpid=$!;
stdbuf -oL "$GSMPATH"/bin/gsm_parse_map.sh >> "$GSMSESSION"/event.log &
mappid=$!;
#stdbuf -oL "$GSMPATH"/bin/gsm_parse_imei.sh >> "$GSMSESSION"/event.log &
#imeipid=$!;

tail -F "$GSMSESSION"/event.log

bashtrap
