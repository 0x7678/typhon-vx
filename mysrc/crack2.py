#!/usr/bin/env python


import socket, string, re, os, sys, ConfigParser, time

HOST = '127.0.0.1'
PORT = 6666

config = ConfigParser.ConfigParser()
config.read(os.getenv('HOME') + '/.omgsm/config')

gsmpath = config.get('Main', 'GSMPATH')
gsmkrakenhost = config.get('Main', 'GSMKRAKENHOST')
gsmkrakenport = config.getint('Main', 'GSMKRAKENPORT')

# foundkey = A5/1 secret state; bitpos = #iteration with this state
# framecount = # of burst on which we cracked the key
# frame2 = other burst we use to verify that key is correct
# bursts_pos = position of possible cracked burst in "bursts" array

def verify(foundkey, bitpos, framecount, bursts_pos):
	key=''
	# verify only previous or next burst (each frame have 4 bursts => %4)
	if(bursts_pos%4 == 3):
		frame2=bursts[bursts_pos-1];
	else:
		frame2=bursts[bursts_pos+1];

#	print     ("%s %s %s %s %s %s %s"%(gsmpath + '/kraken/Utilities/find_kc', foundkey, bitpos, framecount, frame2[0], frame2[1],fflags))
	f=os.popen("%s %s %s %s %s %s %s"%(gsmpath + '/kraken/Utilities/find_kc', foundkey, bitpos, framecount, frame2[0], frame2[1],fflags))
	res=f.read()
	m=re.search('.*: (..) (..) (..) (..) (..) (..) (..) (..)  ... MATCHED', res)
	f.close()
	if (m):
		key=(m.group(1)+m.group(2)+m.group(3)+m.group(4)+m.group(5)+m.group(6)+m.group(7)+m.group(8)).upper()

	return key

def crackall(bursts):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	sock.connect((gsmkrakenhost, gsmkrakenport))
	print "connect"
	file = sock.makefile("rb")
	jobnums={}
	pos=0

	for i in range(parallel):
		sock.send("crack %s\n"%bursts[pos][1])
		pos=pos+1
		if (pos == len(bursts)):
			break
#		time.sleep(4)
	
	while (1):
		s=file.readline().strip()
		#print s
		if(len(s)==0):
			print 'Unexpected connection close'
			sock.close()
			sys.exit(1)

		m=re.search('crack #(.*) took',s)
		if(m):
			del jobnums[m.group(1)]
			if (pos != len(bursts)):
				sock.send("crack %s\n"%bursts[pos][1])
				pos=pos+1
			
		m=re.search('Found (.*) @ (.*) #(.*)  ',s)
		if(m):
			key = verify(m.group(1),m.group(2), jobnums[m.group(3)][0], jobnums[m.group(3)][1])
			if (key != ''):
				sock.close()
				print sys.argv[1]+"\t"+key
				sys.exit(0);

		m=re.search('Cracking #(.*) (.*)',s)
		if(m):
			for i in range(len(bursts)):
				if (m.group(2) == bursts[i][1]): # frame number
					jobnums[m.group(1)]=[]
					jobnums[m.group(1)].append(bursts[i][0])
					jobnums[m.group(1)].append(i) # pos in bursts (for veriify)
			#print "Matched "+s
		if (len(jobnums) == 0):
			sock.close()
			return ''

cflags="" # convert flags
fflags="" # find_kc flags
if len(sys.argv) < 2:
	print("Usage: "+sys.argv[0]+" bursts_file")
	sys.exit(1)

if len(sys.argv) >= 3:
	cflags=" -p "+sys.argv[2] # known plaintext
if len(sys.argv) >= 4:
	fflags="u" # uplink
	cflags=cflags+" -u"

#print(gsmpath+"/bin/gsm_convert -f %s %s 2>/dev/null |grep ^S"%(sys.argv[1],cflags))
f=os.popen(gsmpath+"/bin/gsm_convert -f %s %s 2>/dev/null |grep ^S"%(sys.argv[1],cflags))
lines=[]
for line in f.readlines():
	m=re.search('^S.* .* (.*): (.*)', line)
	lines.append([m.group(1), m.group(2)])

f.close()
# file does not exits, or something is wrong
if not lines:
	print "Cannot_read_data\t0000000000000000"
	sys.exit(42)

parallel=16
bursts=[]
for line in lines:
	bursts=bursts+[line]
crackall(bursts)
print sys.argv[1]+"\t0000000000000000"
