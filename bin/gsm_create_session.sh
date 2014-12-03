#!/bin/bash
# moded by 0x7678
# Under GNU GPL 

# Creates sniffing session
# 
# Parameters:
#	$1 = session name to append


# get config settings 
CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;
GSMDEFSESSION=`cat "$CONFIG" | grep '^GSMDEFSESSION' | cut -d '=' -f 2`;
GSMMAXCELLS=`cat "$CONFIG" | grep '^GSMMAXCELLS' | cut -d '=' -f 2`;

# Create session first
GSMSESSION="$GSMDEFSESSION"/session-`date "+%F-%H-%M-%S"`;
if [ ! -z "$1" ]; then
	GSMSESSION="$GSMSESSION-$1/";
else
	GSMSESSION="$GSMSESSION/"
fi

if [ -d "$GSMSESSION" ]; then
	echo "Session dir already exists. Exiting...";
	exit 1;	
else 
	mkdir "$GSMSESSION";

	# nice race;)
	cat ~/.omgsm/config | grep -v '^GSMSESSION=' > /tmp/gsm_conf.$$;
	echo "GSMSESSION=$GSMSESSION" >> /tmp/gsm_conf.$$;
	mv /tmp/gsm_conf.$$ ~/.omgsm/config;

	mkdir -p "$GSMSESSION"/new "$GSMSESSION"/cracked "$GSMSESSION"/failed \
		 "$GSMSESSION"/strange "$GSMSESSION"/audio "$GSMSESSION"/plain

	# per captured burst information
	echo "CREATE TABLE IF NOT EXISTS keys(timestamp INT, file TEXT, tmsi text, key text, status INT, MCNC INT, cellid INT, rxlev INT);" | sqlite3 "$GSMSESSION"/keys.db
	# per BTS information
	echo "CREATE TABLE IF NOT EXISTS cells(mcnc INT, cellid INT, si5 TEXT, si5t TEXT, si6 TEXT, timestamp INT, arfcn INT); CREATE INDEX IF NOT EXISTS idx_id on cells(mcnc,cellid);" | sqlite3 "$GSMPATH"/stat/cellid.db
	# XXX cellid location

fi

echo "Session $GSMSESSION created."
echo "Continue with gsm_scan_bts";
