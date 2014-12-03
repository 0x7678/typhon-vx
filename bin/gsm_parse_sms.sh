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
tshark -l $options -R gsm_sms.sms_text -T fields \
 -e gsmtap.uplink -e gsm_sms.tp-oa -e gsm_sms.tp-da\
 -e gsm_sms.sms_text 2>/dev/null \
| while read -r i; do
	link=`echo "$i" | cut -c 1`;
	from=`echo "$i" | cut -d '	' -f 2`;

	# in downlink, this is addr of sms gateway
	to=`echo "$i" | cut -d '	' -f 3`; 
	text=`echo "$i" | cut -d '	' -f 4`;
	
	if [ "$link" == 1 ]; then
		link='U';
	else
		link='D';
	fi

	if [ "$from" == "" ]; then
		from='???';
	fi

	if [ "$to" == "" ]; then
		sms_gw='???';
	fi

	# from and to can be much longer, and does not have to be numbers ;(

	if [ "$link" == 'U' ]; then
		stdbuf -oL printf "%c  SMS: ??? -> %17s TXT: $text\n" "$link" "$to";
	else
		stdbuf -oL printf "%c  SMS: %17s -> ??? TXT: $text\n" "$link" "$from";
	fi
	# XXX format string vulnerability? 
	# seems that bash printf is not vulnerable
	# if I put text right into first printf, I don't get utf diacritics
done
