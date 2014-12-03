#!/bin/bash

CONFIG="$HOME/.omgsm/config";
GSMSESSION=`cat "$CONFIG" | grep '^GSMSESSION' | cut -d '=' -f 2`;
GSMPATH=`cat "$CONFIG" | grep '^GSMPATH' | cut -d '=' -f 2`;

DIR="$GSMSESSION/cracked"; #cracked

trap bashtrap INT

bashtrap()
{
    echo "$0 exiting...";
#    sudo kill $smspid $callpid $mappid
#	# XXX solve sudo problem.
#    exit;
}


# seems to order by time by default.
ls "$DIR" | while read i; do
	file=`basename "$i"`;
	key=`echo "select distinct(key) from keys where file like '$file';"| sqlite3 "$GSMSESSION"/keys.db`
	if [ ! -z "$key" ] && [ "$key" != '0000000000000000' ]; then
		echo "Parsing $file..."
		"$GSMPATH"/bin/gsm_convert -k "$key" -f "$DIR/$i" >/dev/null 2>&1;
	fi
done 

#bashtrap
