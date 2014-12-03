#!/bin/bash
# niekt0@hysteria.sk <- this is completely HIS work! HE should go to jail!
# moded by 0x7678
# Under GNU GPL 

# params: $0 socket

# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

scanres="$GSMSESSION"/scan.current
socket="$1"
operators="$GSMSESSION"/operators

while sleep 1; do
	sniffargs=""
	echo " + Picking up a BTS"
	#"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/layer23/src/misc/cell_log -s "/tmp/osmocom_l2_1" -l - 2>&1 | bash bts_scan_jenda.sh
	cat "$scanres" | \
	{ while read line; do
		mcnc=`echo "$line" | cut -d ";" -f 2`
		cid=`echo "$line" | cut -d ";" -f 4`
		arfcn=`echo "$line" | cut -d ";" -f 3`
		signal=`echo "$line" | cut -d ";" -f 5`
		if grep -q "$mcnc" "$operators"; then # we want to sniff this one
			if ! ps axu | grep -q "[a] $arfcn"; then # nobody is sniffing here yet
				sniffargs="--cellid $cid --mcnc $mcnc -a $arfcn"
				break
			fi
		fi
	done
	if [ -n "$sniffargs" ]; then
		echo " + Spawning sniff in master mode, $sniffargs"
		# start sniffing!
		xterm -T "sniff: $socket $sniffargs $signal" -e "cd $GSMSESSION/new/ && $GSMPATH/mysrc/sniff --master -s $socket $sniffargs"
	else
		echo "No acceptable BTS found"
	fi
	}
done
