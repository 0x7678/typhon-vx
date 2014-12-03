#!/usr/bin/python

import curses,time,re,os,sys,ConfigParser,dialog,subprocess,fcntl

stdscr = curses.initscr()
curses.noecho()
curses.cbreak()
stdscr.keypad(1)

width=80
height=24

mainpad = curses.newpad(height, width)
btspad = curses.newpad(1024, width)

config = ConfigParser.ConfigParser()
config.read(os.getenv('HOME') + '/.omgsm/config')
gsmpath = config.get('Main', 'GSMPATH')
gsmsession = config.get('Main', 'GSMSESSION')

menupos = 0

# type, filter, arfcn, cellid, mcnc, comment, signal, AD, AU, AT, TD, TU, TT, process
phones=[]

# arfcn, cellid, mcnc, comment, signal
bts=[]

def dbg(string):
	f=open("/tmp/dbg","a")
	f.write(string+"\n")
	f.close()

def phonelist():
	global phones
	f=open(os.getenv('HOME') + '/.omgsm/phones')
	for line in f:
		m=re.search('(.*)=(.*)=(.*)', line)
		if(m):
			phones.append(["S",m.group(3),0,0,0,"",0,0,0,0,0,0,0,None])
	f.close()

def phoneline(num):
	global mainpad
	global phones
	if phones[num][0] in ("M","CRASHED! "):
		mainpad.addstr(num,0," %i %s%s %i-%i-%i %i A:%iD,%iU,T=%is T:%iD,%iU,T=%is"%(num,phones[num][0],phones[num][1],phones[num][2],phones[num][3],phones[num][4],phones[num][6],phones[num][7],phones[num][8],phones[num][9],phones[num][10],phones[num][11],phones[num][12]))
	if phones[num][0] == "S":
		mainpad.addstr(num,0," %i %s%s A:%iD,%iU,T=%is T:%iD,%iU,T=%is"%(num,phones[num][0],phones[num][1],phones[num][7],phones[num][8],phones[num][9],phones[num][10],phones[num][11],phones[num][12]))
	if phones[num][0] == "B":
		mainpad.addstr(num,0," %i %s%s"%(num,phones[num][0],phones[num][1]))

def helptext():
	global phones
	mainpad.addstr(len(phones)+1,0,"phone:  Master, Slave, B0rky, Reset, clean All, clean Period")
	mainpad.addstr(len(phones)+2,0,"global: sCan, clean aLl, clean pEriod, reDraw, maNual, Automagic")

def maindraw():
	global phones
	global mainpad
	global height,width
	mainpad.erase()
	for i in range(0,len(phones)):
		phoneline(i)
	helptext()
	curses.noecho()
	curses.cbreak()
	stdscr.keypad(1)
	stdscr.refresh()
	mainpad.redrawwin()
	mainpad.refresh( 0,0, 0,0, height,width)

def btslist():
	global height,width,bts
	d = dialog.Dialog()
	d.setBackgroundTitle('Pick a BTS')
	f=open(gsmsession + '/scan.current')
	bts=[]
	mylist=[]
	for line in f:
		m=re.search('.*;(.*);(.*);(.*);(.*)', line)
		if(m):
			mylist.append([m.group(2),"%s-%s %s"%(m.group(1),m.group(3),m.group(4)),"helptext"])
			bts.append([m.group(2),m.group(3),m.group(1),"",m.group(4)])
	f.close()
	choice = d.menu("Pick a BTS", item_help=1, width=width-6, menu_height=height-10, choices=mylist)
	if choice[1]=="":
		return -1
	for sublist in bts:
		if sublist[0]==choice[1]:
			return sublist
	return -1 #wtf?

def btsline(num):
	global btspad,bts
	global height,width
	str1=" %s-%s-%s %s %s"%(bts[num][0],bts[num][1],bts[num][2],bts[num][4],bts[num][3])
	btspad.addstr(num*2,0,str1[:width])
	btspad.addstr(num*2+1,0,str1[width:2*width-1])

def arrpos(menuprev, menupos, lineheight, pad):
	global height,width
	lineheight
	pad.addch(menuprev*lineheight,0," ")
	pad.addch(menupos*lineheight,0,">")
	pad.refresh( 0,0, 0,0, height,width)

def master(menupos,picked = None):
	global phones
	if picked != None:
		phones[menupos][0] = "M"
		phones[menupos][2] = int(picked[0])
		phones[menupos][3] = int(picked[1])
		phones[menupos][4] = int(picked[2])
		phones[menupos][5] = picked[3]
		phones[menupos][6] = int(picked[4])
	resetT(phones[menupos])
	resetL(phones[menupos])
	mykill(phones[menupos][13])
	phones[menupos][13] = spawnmyprocess([gsmpath+"/mysrc/sniff", "--master", "-s", "/tmp/osmocom_l2_%i"%menupos, "--cellid", "%i"%phones[menupos][3], "--mcnc", "%i"%phones[menupos][4], "-a", "%i"%phones[menupos][2]])
	phoneline(menupos)
def slave(menupos):
	global phones
	phones[menupos][0] = "S"
	resetT(phones[menupos])
	resetL(phones[menupos])
	mykill(phones[menupos][13])
	phones[menupos][13] = spawnmyprocess([gsmpath+"/mysrc/sniff", "--slave", "-s", "/tmp/osmocom_l2_%i"%menupos])
	phoneline(menupos)

def borky(menupos):
	global phones
	phones[menupos][0] = "B"
	resetT(phones[menupos])
	resetL(phones[menupos])
	mykill(phones[menupos][13])
	phoneline(menupos)

def resetL(phone):
	phone[7] = phone[8] = phone[9] = 0

def resetT(phone):
	phone[10] = phone[11] = phone[12] = 0

def readmyline(num):
	global phones
	p=phones[num][13]
	f=open(gsmsession+"/phone_%i.log"%num,"a")
	while True:
		try: l=p.stdout.readline()
		except: break
		if l == "": break
		f.write(l)
		#return l
	while True:
		try: l=p.stderr.readline()
		except: break
		if l == "": break
		f.write(l)
		#return l
	f.close()
	if p.poll() != None:
		if phones[num][0] == "S":
			slave(num)
		else:
			master(num)
			phoneline(num)
			maindraw()
	return -1

def spawnmyprocess(sh):
	p = subprocess.Popen(sh, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)
	fd = p.stdout.fileno()
	fl = fcntl.fcntl(fd, fcntl.F_GETFL)
	fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
	fd = p.stderr.fileno()
	fl = fcntl.fcntl(fd, fcntl.F_GETFL)
	fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
	return p

def mykill(p):
	if not p:
		return
	try: p.kill()
	except: return
	while p.poll():
		time.sleep(0.1)

phonelist()
for i in range(0,len(phones)):
	slave(i)
maindraw()
while 1:
	curses.halfdelay(1)
	c = stdscr.getch()
	menuprev=menupos
	if c == curses.KEY_UP:
		if(menupos>0): menupos-=1
	elif c == curses.KEY_DOWN:
		if(menupos<len(phones)-1): menupos+=1
	elif c in(ord('m'),curses.KEY_ENTER,10):
		picked=btslist()
		master(menupos,picked)
		maindraw()
	elif c == ord('s'):
		slave(menupos)
		maindraw()
	elif c == ord('b'):
		borky(menupos)
		maindraw()
	arrpos(menuprev,menupos,1,mainpad)
	for i in range(0,len(phones)):
		readmyline(i)


curses.nocbreak(); stdscr.keypad(0); curses.echo()
curses.endwin()


