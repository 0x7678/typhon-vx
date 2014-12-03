#!/bin/bash
# moded by 0x7678
# Under GNU GPL 

# "start sniffing"
# 

# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

id=0;
channels=0;
while read i; do
	arfcn=`echo "$i" | cut -f 1 -d '	'`;
	l2socket=`echo "$i" | cut -f 2 -d '	'|sed 's/osmocon/osmocom/'`; 
	mcnc=`echo "$i" | cut -f 3 -d '	'`;
	cellid=`echo "$i" | cut -f 4 -d '	'`;

	# if arfcn is empty, phone should be a slave
	#DBG echo "$arfcn"
	if [ "$arfcn" == "s" ]; then
		mode="--slave";
	else
#		mode="-a $arfcn"
		mode="-a $arfcn --master --mcnc $mcnc --cellid $cellid"
		channels=$((channels+1));
	fi

	# one xterm for each sniff
	# in while cycle, because sniff may crash sometimes.
	echo "DBG: xterm -e \"cd $GSMSESSION/new/ && $GSMPATH/mysrc/sniff $mode  -s $l2socket;\"";
	xterm -T "sniff $mode -s $l2socket" -e "cd $GSMSESSION/new/ && while true; do $GSMPATH/mysrc/sniff $mode -s $l2socket; echo \"sniff $mode -s $l2socket\" >> $GSMSESSION/crashlog; sleep 10; done" &
	id=$((id+1));
done < "$GSMSESSION"/arfcn;

echo "Sniffing started on $channels channels.";
