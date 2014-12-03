#!/bin/bash
# moded by 0x7678
# Under GNU GPL 

# "bts scan"
# 
# Scans for available bts, and pick few strongest 
# from each operator. 
# Parameters: XXX ignored now

# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;
GSMDEFSESSION=`cat "$CONFIG" | grep '^GSMDEFSESSION' | cut -d '=' -f 2`;
GSMMAXCELLS=`cat "$CONFIG" | grep '^GSMMAXCELLS' | cut -d '=' -f 2`;

if [ ! -d "$GSMSESSION" ]; then
	echo "Session dir does not exists. Exiting...";
	exit 1;	
fi

phones=`cat ~/.omgsm/phones | grep -v '^#' |grep '=C'| wc -l`;

channels=$((phones/1)); # XXX all - pool 

echo "Picking up $channels channels.";

# Scan for available BTS 

echo "Scanning for BTS...";

touch /tmp/bts_op_u."$$";
uniq_op=0;

# Main loop
"$GSMPATH"/osmocom-bb-sylvain-burst_ind/src/host/layer23/src/misc/cell_log -l "$GSMSESSION"/osmocom.log."$$" 2>&1 |\
 while read i; do 
	# parsing cell_log output...
	iscell=`echo "$i" | grep 'Cell:' | wc -l`;
	if [ "$iscell" -eq 1 ] ; then 
		operator=`echo "$i" | cut -d '(' -f 2 | cut -d ')' -f 1`; 
		arfcn=`echo "$i" | cut -d '=' -f 2 | cut -d ' ' -f 1`;
		mcnc=`echo "$i" | sed -re "s/^.* MCC=([0-9]{3}) MNC=([0-9]{2,3}).*$/\1\2/"`
		echo "Found \"$operator\" at channel $arfcn";
		echo "Found \"$operator\" at channel $arfcn" >> "$GSMSESSION"/bts.log;
		cells=$((cells+1));
		echo "$arfcn	$operator)" >> /tmp/bts_op_all_arfcn.$$;
		cat /tmp/bts_op_u.$$ | grep "$operator" > /dev/null || \
			(echo "$operator" >> /tmp/bts_op_u.$$; echo "$arfcn	/tmp/osmocom_l2_$uniq_op	$mcnc	($operator)" >> /tmp/bts_op_u_arfcn.$$)
		uniq_op=`wc -l /tmp/bts_op_u.$$| cut -d ' ' -f 1`;

		# if our slots are full by unique operators
		if [ "$uniq_op" -eq "$channels" ]; then
			echo "found $uniq_op operators:";
			cat /tmp/bts_op_u_arfcn."$$" > "$GSMSESSION"/arfcn;
			killall cell_log; # XXX
			exit;
		fi

		# We have some slots free, so fill them with other channels
		if [ "$cells" -gt "$GSMMAXCELLS" ]; then
#			channels=$((channels+1));
			echo "Max cells ($GSMMAXCELLS) found.";
			cat /tmp/bts_op_u_arfcn.$$ > "$GSMSESSION"/arfcn;
			cleft=$((channels-uniq_op));
			while read j; do
				found=0;
				j_arfcn=`echo "$j" | cut -d '	' -f 1`;
				j_op=`echo "$j" | cut -d '	' -f 2| cut -d ')' -f 1`;
				cat /tmp/bts_op_u_arfcn."$$" | grep "^$j_arfcn	" > /dev/null && found=1;
				if [ "$found" -eq 0 ]; then 
					echo "$j_arfcn	/tmp/osmocon_l2_$uniq_op	($j_op)" >> "$GSMSESSION"/arfcn;
					uniq_op=$((uniq_op+1));
				fi
				tmp=$((uniq_op-1));
				if [ "$uniq_op" -eq "$channels" ] || [ "$uniq_op" -gt "$channels" ]; then
					killall cell_log; # XXX
					exit;
				fi;
			done < /tmp/bts_op_all_arfcn."$$";
		fi
	
	fi;
done

# clean up
rm -f /tmp/bts_scan."$$" /tmp/bts_scan2."$$" /tmp/bts_operators."$$" \
	/tmp/bts_op_u."$$" /tmp/bts_op_all."$$" /tmp/bts_op_u_arfcn."$$" /tmp/bts_op_all_arfcn."$$";

echo "Scan done, file ${GSMSESSION}arfcn created";
echo "Edit this file to fit your requirements";
