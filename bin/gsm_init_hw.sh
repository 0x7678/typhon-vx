#!/bin/bash
# moded by 0x7678
# Under GNU GPL 

# "initialize hardware"
# 
# spawn xterm with osmocon for each phone defined in config file
#
#	$1 - how many phones to use. XXX ignored, use ~/.omgsm/phones instead

# get config settings
CONFIG="$HOME/.omgsm/config";
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH=' | cut -d '=' -f 2`;
GSMBRMBORACTL=`cat ~/.omgsm/config | grep '^GSMBRMBORACTL='| cut -d = -f 2`;

id=0;
cat ~/.omgsm/phones | grep -v '^#' |grep -v '^\[' |  while read i; do
	serial=`echo "$i"| cut -d = -f 1`;
	phone=`echo "$i"| cut -d = -f 2`;
	
	if [ ! -z "$phone" ]; then
		echo "DBG: $GSMPATH/bin/motoload.sh $phone $serial /tmp/osmocom_l2_$id /tmp/osmocom_loader_$id";
		#xterm -e "$GSMPATH/bin/motoload.sh $phone $serial /tmp/osmocom_l2_$id /tmp/osmocom_loader_$id; cat" &
		$GSMPATH/bin/motoload.sh $phone $serial /tmp/osmocom_l2_$id /tmp/osmocom_loader_$id > /tmp/log_l1_$$_$id 2>&1 &

		if [ "$id" -eq 0 ]; then # backwards compatibility
			rm -f /tmp/osmocom_l2;
			ln -s /tmp/osmocom_l2_0 /tmp/osmocom_l2;
			rm -f /tmp/osmocom_loader;
			ln -s /tmp/osmocom_loader_0 /tmp/osmocom_loader;

		fi
		id=$((id+1));
	fi
done

# switch on the phones and start loading firmware, if brmbora is available
sleep 1;

if [ ! -z "$GSMBRMBORACTL" ]; then
	$GSMPATH/bin/brmbora_ctl.sh
fi
