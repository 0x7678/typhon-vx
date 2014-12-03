#/bin/sh

# motoload.sh
# moded by 0x7678
# Under GNU GPL 

# get config settings
CONFIG="$HOME/.omgsm/config";
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

if [ -z "$1" ]; then 
	echo "usage: $0 \"phone type\" [serial line] [l2_socket] [loader]";
	echo "suppoted phones:  C115/C117/C123/C121/C118/C139/C140/C155"
	echo "example: $0 C139 /dev/ttyUSB2 /tmp/testsocket /tmp/testloader"
	exit 0;
fi

if [ -z "$2" ]; then 
	stty=/dev/ttyUSB0; 
else 
	stty="$2";
fi

if [ -z "$3" ]; then 
	l2socket=""; 
else 
	l2socket=" -s $3";
fi

if [ -z "$4" ]; then 
	loader=""; 
else 
	loader=" -l $4";
fi

case "$1" in 
	C115|C117|C118|C119|C121|C123)
		# e88 
		# this is not ideal for C115 and C117,
		# but they seems to work..
		echo -n "Loading e88, press button on a phone...";
		"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/osmocon/osmocon $l2socket $loader -p "$stty" -m c123xor "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/compal_e88/layer1.compalram.bin;
		;;
	C139|C140)
		# e86
		echo -n "Loading e86, press button on a phone...";
		"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/osmocon/osmocon $l2socket $loader -p "$stty" -m c140xor -c "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/compal_e86/layer1.highram.bin  "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/compal_e86/chainload.compalram.bin;
		;;
	C155)	
		# e99
		echo -n "Loading e99, press button on a phone...";
		"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/osmocon/osmocon $l2socket $loader -p "$stty" -m c140xor -c "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/compal_e99/layer1.highram.bin  "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/compal_e99/chainload.compalram.bin;
		;;
	Openmoko)
		# gta0x
		echo -n "Loading gta0x...";
		killall -9 osmocon fsogsmd
		(echo 0 > /sys/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-gpio/reg-fixed-voltage.1/gta02-pm-gsm.0/power_on ; sleep 1; echo 1 > /sys/devices/platform/s3c2440-i2c/i2c-0/0-0073/pcf50633-gpio/reg-fixed-voltage.1/gta02-pm-gsm.0/power_on) &
		"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/osmocon/osmocon -i 13 -m romload $l2socket $loader -p "$stty" "$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/target/firmware/board/gta0x/layer1.highram.bin
		;;
	*)
		echo "Unknown phone $1."
		;;
esac
