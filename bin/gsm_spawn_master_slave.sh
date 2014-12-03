#!/bin/bash
# moded by 0x7678
# Under GNU GPL 

# "start sniffing"
# 

# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

masters="/tmp/osmocom_l2_0" # /tmp/osmocom_l2_3"
slaves="/tmp/osmocom_l2_1 /tmp/osmocom_l2_2"

lock="$GSMSESSION"/scan.lock

for master in $masters; do
	xterm -T "master_paplon $master" -e "cd $GSMSESSION/new/ && gsm_guard_master.sh $master" &
done

for slave in $slaves; do
	xterm -T "slave $slave" -e "cd $GSMSESSION/new/ && while sleep 2; do if [ -f "$lock" ]; then echo Scan in progress; else $GSMPATH/mysrc/sniff --slave -s $slave; fi; done" &
done 


