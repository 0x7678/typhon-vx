#!/bin/bash

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

#print both hex dump and text. Text is decoded only in text field (?)
#$sudo
tshark -l $options -R gsm_map.ss.ussd_String -T fields -e gsmtap.uplink -e text 2>/dev/null\
|  while read -r i; do
	link=`echo "$i" | cut -c 1`;
	text=`echo "$i" | sed 's/^.*USSD String: //;s/,SS Version Indicator,.*$//'`;
	# XXX this is hack, if text contains ",SS Version...", this substitution will fail,
	# problem is that wireshark return all this fields as text together.

	if [ "$link" == 1 ]; then
		link='U';
	else
		link='D';
	fi

	# this useless formating is to keep same alignment as parse_sms 
	printf "%c  MAP:                          TXT: $text\n" "$link";
	# XXX format string vulnerability? 
	# seems that bash printf is not vulnerable
	# if I put text right into first printf, I don't get utf diacritics

done
