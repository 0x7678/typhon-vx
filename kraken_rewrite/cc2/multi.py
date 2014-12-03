#!/usr/bin/python

import pyopencl as cl
import numpy as np
import random,time,socket,re,os,sys,struct

import tables

mf = cl.mem_flags
ctx = cl.create_some_context()
queue = cl.CommandQueue(ctx)

# open connection to the Kraken
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#sock.settimeout(gsmkrakentimeout)
sock.connect(("localhost", 6666))

#XXX check return codes

#hack, not optimal (blocking & not timeouting)
sock.setblocking(0)
sfile=sock.makefile("rb")

bnum=0
bdata=""

#mytables=[100,108,116,124,132,140,148,156,164,172,180,188,196,204,212,220,230,238,250,260,268,276,292,324,332,340,348,356,364,372,380,388,396,404,412,420,428,436,492,500]
mytables=[380, 220, 100,108,116,124,132,140,148,156,164,172,180,188,196,204,212,230,238,250,260,268,276,292,324,332,340,348,356,364,372,388,396,404,412,420,428,436,492,500]

challenge=0
if (sys.argv[1] == "f"):
	challenge=1

def revbits(x):
	return int(bin(x)[2:].zfill(64)[::-1], 2)

def report(a):
	global sock
	global challenge
	r=[]
	#for i in range(0,len(a)):
		#print("%16x"%a[i])
	if challenge:
		for i in range(0,len(a),12):
			if a[i+9]&0x8000000000000000 !=0:
				key=a[i]
				pos=((int(a[i+10])>>4)&0xff)
				job=(int(a[i+10])>>12)
				print("found %x @ %i  #%i"%(key,pos,job))
				sock.sendall("Found ")
				sock.sendall("%x @ %i #%i    \n"%(key,pos,job))
		time.sleep(0.3)
		sock.sendall("took  \n")
	else:
		for i in range(0,12*len(mytables)*408,12):
			r.append(a[i])
		sock.send("retdps")
		print("rlen=%i"%len(r))
		struk=struct.pack("1Q16320Q", int(bnum), *r)
		print("slen=%i"%len(struk))
		si=sock.send(struk)
		print("sent %i"%si)
		#sock.sendall("\n")
	#sys.exit(0)

def krak(bnum,bdata,cdata):
	t=[]
	ctr=1
	for table in mytables:
		for pos in range(0,len(bdata)-63):
			for color in range(0,8):
				sample=bdata[pos:pos+64]
				if not cdata:
					t.append(int("0b%s"%sample, 2)) # pivot
					for i in range(0,8):
						t.append(tables.rft[table-100][i])
					t.append(color)
					t.append(0x7)
					t.append(0x0)
					#print("sample %s, color %x, table %i"%(sample,color,table))
				else: # hunting for a challenge
					if cdata[ctr] != 0: # block was found in table
						pivot = revbits(cdata[ctr])
						target = int("0b%s"%sample, 2)
						t.append(pivot)
						for i in range(0,8):
							t.append(tables.rft[table-100][i])
						t.append(0x0)
						scolor=color|pos<<4|int(bnum)<<12
						t.append(scolor)
						t.append(target)
						#if pos<5:
						#	print("pos %i, sample %s, color %x, s_color %x, table %i, pivot %x, target %x"%(pos,sample,color,scolor,table,pivot,target))
					ctr+=1

	a = np.array(t,dtype=np.uint64)
	a_dev = cl.Buffer(ctx, cl.mem_flags.READ_ONLY | cl.mem_flags.COPY_HOST_PTR, 
			    hostbuf=a) 

	s = np.uint32(a.shape)
	r = np.uint32(5000)
	z = np.uint32(0)

	# compile the kernel
	FILE_NAME="krak.cl"
	f=open(FILE_NAME,"r")
	SRC = ''.join(f.readlines())
	f.close()

	prg = cl.Program(ctx, SRC).build()

	x = time.time()

	# launch the kernel
	print("Launching kernel, size %i",a.shape)
	event = prg.krak(queue, a.shape, None, a_dev, s)
	event.wait()
	 
	# copy the output from the context to the Python process
	cl.enqueue_copy(queue, a, a_dev)

	print("lag=%f"%(time.time()-x))
	report(a)

while (1):
	if not challenge:
		sock.send("getsmp \n")
	else:
		sock.send("getsta \n")
	while True:
		try:
			line=sfile.readline().strip()
		except:
			line=""
			#break
		m=re.search('(.*) (.*)',line)
		time.sleep(1)
		#print(line)
		if(m):
			bnum=m.group(1)
			bdata=m.group(2)
			print("Cracking "+bdata)
			cdata=""
			if challenge:
				data = sfile.read(8+16320*8)
				print(len(data))
				cdata=struct.unpack("1Q16320Q",data)
				#for i in range(0,408,1):
					#print("cdata: %i %x"%(i,cdata[i]))
			krak(bnum,bdata,cdata)
		if line == "":
			break

