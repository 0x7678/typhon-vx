#/bin/bash

# brmbora control interface
# moded by 0x7678

# $1 - number of phone to reset  
# otherewise all phones are reset

# XXX assumes, than phones are in ~/.omgsm/phones stored sorted

port=`cat ~/.omgsm/config | grep '^GSMBRMBORACTL='| cut -d = -f 2`;
timeout=`cat ~/.omgsm/config | grep '^GSMBRMBORAFWTIMEOUT='| cut -d = -f 2`;

# XXX hack for controll interface, find out something nicer
stty 9600 -F "$port" -echo;
cat "$port" > /dev/null &

# This is brmbora "standard", lol
# phone		power		switch		physical cable (starting from red)
# 0		d		c		
# 1		e		f
# 2		h		g
# 3		i		j
# 4		l		k
# 5		m		u
# 6		s		t
# 7		q		r

power[0]="i"; switch[0]="j";
power[1]="h"; switch[1]="g";
power[2]="d"; switch[2]="c";
power[3]="e"; switch[3]="f";
power[4]="q"; switch[4]="r";
power[5]="s"; switch[5]="t";
power[6]="l"; switch[6]="k";
power[7]="m"; switch[7]="u";

# $1 - phone to turn off
turn_off() {
	if [ ! -z "$1" ]; then
		a=${power[$1]};
		echo "s${a}0" > "$port";
		echo -n "$a";
	else
		for a in ${power[@]} ${switch[@]}; do 
			echo "s${a}0" > "$port"; 
			echo -n "$a";
			sleep 0.1; 
		done
	fi
}

# $1 - phone to turn on (boot firmware)
turn_on() {

	#echo "DBG: $1 s${power[$1]}1 s${switch[$1]}1 s${switch[$1]}0 $timeout";

	if [ ! -z "$1" ]; then
		# turn on power
		echo s${power[$1]}1 > "$port"
		sleep 0.1	

		# press button
		echo s${switch[$1]}1 > "$port"
		sleep 0.5

		#release button
		echo s${switch[$1]}0 > "$port"
		sleep "$timeout";
	else
		exit 23; # no parameter given
	fi
}



echo -n "Turning off...";

turn_off "$1";

sleep 1
echo " OK";


#parsing .omgsm/phones
id=0;
cat ~/.omgsm/phones | grep -v '^#' |grep -v '^\[' |  while read i; do
	serial=`echo "$i"| cut -d = -f 1`;
	phone=`echo "$i"| cut -d = -f 2`;

#	get_id=`echo "$i"| sed 's/^.*USB//;s/=.*$//'`;
#	echo "lala $i $get_id"
	# reset only one phone
	if [ ! -z "$1" ]; then
		if [ "$1" -eq "$id" ]; then
#			echo "DBG: $serial, $phone, id: $id";
			echo -n "Init $serial  ($phone, id $id)...";
			turn_on "$id";
			echo "OK";
		fi
	else	
	# reset all phones
		if [ ! -z "$serial" ]; then
#			echo "DBG: $serial, $phone, id: $id";
			echo -n "Init $serial  ($phone, id $id)...";
			turn_on "$id";
			echo "OK";
		fi
	fi
	id=$((id+1));
done
