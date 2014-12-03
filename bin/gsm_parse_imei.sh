#!/bin/bash

# $1 - input file, if empty 

if [ ! -z "$1" ]; then
	options="-r $1";
#	sudo="";
else
	options="-i lo";
#	sudo="sudo";	#ble
fi

# -e gsm_a.cld_party_bcd_num 
# this seems to be addr of sms gateway... not interesting with downlink

#$sudo 
tshark -l $options -R "gsm_a.tmsi or gsm_a.imeisv" -T fields \
 -e gsmtap.uplink -e gsm_a.imeisv -e gsm_a.tmsi 2>/dev/null \
| while read -r i; do
	link=`echo "$i" | cut -c 1`;
	imeisv=`echo "$i" | cut -d '	' -f 2`;
	tmsi=`echo "$i" | cut -d '	' -f 3`;

#	echo "$i";

	if [ "$link" == 1 ]; then
		link='U';
	else
		link='D';
	fi

	if [ "$link" == 'U' ]; then
		stdbuf -oL printf "U IMEI: %17s\n" "$imeisv";
	else
		stdbuf -oL printf "D TMSI: %17s\n" "$tmsi";
	fi

	# XXX format string vulnerability? 
	# seems that bash printf is not vulnerable
	# if I put text right into first printf, I don't get utf diacritics
done
