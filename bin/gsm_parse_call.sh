#!/bin/bash

# Extract information about GSM calls from packet capture

# Under GNU GPL
# moded by 0x7678


# $1 - input file, if empty 

if [ ! -z "$1" ]; then
	options="-r $1";
#	sudo="";
else
	options="-i lo";
#	sudo="sudo";	#ble
fi

#$sudo 
tshark -l $options -R gsm_a.clg_party_bcd_num -T fields -e gsmtap.uplink -e gsm_a.clg_party_bcd_num 2>/dev/null\
| while read -r i; do
	link=`echo "$i" | cut -c 1`;
	from=`echo "$i" | cut -d '	' -f 2`;
	
	if [ "$link" == 1 ]; then
		link='U';
	else
		link='D';
	fi

	# from and to can be much longer, and does not have to be numbers ;(
	stdbuf -oL printf "%c CALL: %17s -> ???\n" "$link" "$from";
done

# sed -u 's/^1	/U CAL: /;s/^0	/D CAL: /;s/$/ -> ???/';
