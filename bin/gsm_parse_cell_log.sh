# usage: /home/jenda/gsm/osmocom-bb-sylvain-burst_ind/src/host/layer23/src/misc/cell_log -s /tmp/osmocom_l2_3 -l - 2>&1 | bash bts_scan_jenda.sh

maxcells=255

if [ $# -eq 1 ]; then
	maxcells="$1"
fi

curcells=0

while read line; do
	if echo $line|grep -q Cell; then
		op=`echo "$line" | cut -d '(' -f 2 | cut -d ')' -f 1`
		arfcn=`echo "$line" | cut -d '=' -f 2 | cut -d ' ' -f 1`
		mcnc=`echo "$line" | sed -re "s/^.* MCC=([0-9]{3}) MNC=([0-9]{2,3}).*$/\1\2/"`
	fi
	if echo $line|grep -q "^rxlev"; then
		rxlev=`echo "$line" | cut -d " " -f 2`
	fi
	if echo $line|grep -q "^si3"; then
		cid=$(printf "%d\n" 0x`echo "$line" | cut -c 14,15,17,18`)
	fi
	if echo $line|grep -q si4; then
		echo "$op;$mcnc;$arfcn;$cid;$rxlev"
		let curcells++
		if [ $curcells -ge $maxcells ]; then
			killall -TERM cell_log # cell_log does not respond to sigpipe
			exit 0
		fi
	fi
done

