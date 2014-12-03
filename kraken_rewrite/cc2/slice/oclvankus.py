#!/usr/bin/python

import pyopencl as cl
import numpy as np
import random,time,socket,re,os,sys,struct

import tables

mf = cl.mem_flags
#ctx = cl.create_some_context()

platform = cl.get_platforms()
my_gpu_devices = platform[0].get_devices(device_type=cl.device_type.GPU)
ctx = cl.Context(devices=my_gpu_devices)
queue = cl.CommandQueue(ctx)

#mytables=[100,108,116,124,132,140,148,156,164,172,180,188,196,204,212,220,230,238,250,260,268,276,292,324,332,340,348,356,364,372,380,388,396,404,412,420,428,436,492,500]
mytables=[380, 220, 100,108,116,124,132,140,148,156,164,172,180,188,196,204,212,230,238,250,260,268,276,292,324,332,340,348,356,364,372,388,396,404,412,420,428,436,492,500]

clblobsize=150000

clmeta=[]
clq=dict()
cldb=np.zeros(clblobsize+16321,dtype=np.uint32)

def revbits(x):
	return int(bin(x)[2:].zfill(64)[::-1], 2)

def report(a):
	global challenge
	global clq,cldb
	r=[]
	for i in range(0,len(a),12):
		bpos=((int(a[i+10])>>4)&0x3fff)
		job=((int(a[i+10])>>18)&0xffffffff)
		#print("job %i, bpos %i"%(job,bpos))
		if a[i+9]&np.uint64(0x4000000000000000) !=0: # found
			key=a[i]
			pos=(bpos%40)/8
			print("found %x @ %i  #%i"%(key,pos,job))
		#print("Updating %i by %x -> %x"%(bpos,clq[job][bpos][0],a[i]))
		clq[job][bpos][0]=a[i] #cpos
		clq[job][bpos][1]=a[i+9] #ccolor

def took(bnum):
	print("crack #%i took"%bnum)

def fin(bnum,burst):
	r=[]
	r.append(bnum)
	for frag in burst:
		r.append(frag[0])
	struk=struct.pack("%iQ"%len(r), *r)
	f=open("cl-endpoints-%09i.bin"%bnum,"wb")
	f.write(struk)
	f.close()

def addq(bnum,bdata,cdata):
	global clq,mytables
	clq[bnum]=[]
	ctr=0
	inctr=0
	for table in mytables:
		for pos in range(0,len(bdata)-63):
			for color in range(0,8):
				sample=bdata[pos:pos+64]
				if not cdata:
					scmagic=(ctr&0x3fff)<<4|(bnum&0xffffffff)<<18
					pivot=int("0b%s"%sample, 2)
					ccolor=color
					scolor=0x7|scmagic
					challenge=0x0
					#print("sample %s, color %x, table %i"%(sample,color,table))
				else: # hunting for a challenge
					pivot=0
					if cdata[ctr] != 0: # block was found in table
						scmagic=(inctr&0x3fff)<<4|(bnum&0xffffffff)<<18
						pivot = revbits(cdata[ctr])
						challenge = int("0b%s"%sample, 2)
						ccolor=0x0
						scolor=color|scmagic
						inctr+=1
				ctr+=1
				if pivot:
					clq[bnum].append([pivot,ccolor,scolor,challenge,0,table])
# scolor:
# jobnum(32b) bpos(14b) scolor(4b)

def krak():
	global clq,cldb,clblobsize
	t=[]
	globctr=0

	# rft entries
	for table in mytables:
		for j in range(0,8):
			t.append(

	for i in clq.keys():
		alldone=1
		ctr=0
		calburst=0
		for frag in clq[i]:
			if frag[3] != 0:
				calburst=1
			if frag[1]&0x8000000000000000 == 0: # not solved
				alldone=0
				pivot=frag[0]
				t.append(pivot)
				#print("adding fragment %X"%pivot)
				for j in range(0,8):
					t.append(tables.rft[frag[5]-100][j])
				t.append(frag[1])
				t.append(frag[2])
				t.append(frag[3])
				frag[4]+=1
				#print("sample %s, color %x, table %i"%(sample,color,table))
				ctr+=1
			if frag[4]>=10: # FIXME detect cycle in color
				print("Dropping fragment %x in table %x color %x"%(frag[0],frag[5],frag[1]))
			globctr+=1
		if alldone:
			print("alldone")
			if calburst:
				took(i)
			else:
				fin(i,clq[i])
			print("del %i len %i"%(i,len(clq)))
			del clq[i]
		if globctr>clblobsize:
			break
	if len(t) == 0:
		time.sleep(1)
		return
	
	print("Compressed blob from %i to %i"%(globctr,len(t)/12))

	a = np.array(t,dtype=np.uint64)
	a_dev = cl.Buffer(ctx, cl.mem_flags.READ_ONLY | cl.mem_flags.COPY_HOST_PTR, hostbuf=a) 

	s = np.uint32(a.shape)
	r = np.uint32(5000)
	z = np.uint32(0)

	# compile the kernel
	FILE_NAME="krak.c"
	f=open(FILE_NAME,"r")
	SRC = ''.join(f.readlines())
	f.close()

	prg = cl.Program(ctx, SRC).build()

	x = time.time()

	# launch the kernel
	print("Launching kernel, size %i"%a.shape)
	event = prg.krak(queue, a.shape, None, a_dev, s)
	event.wait()
	 
	# copy the output from the context to the Python process
	cl.enqueue_copy(queue, a, a_dev)

	print("lag=%f"%(time.time()-x))
	report(a)

def mergenew():
	for item in os.listdir("."):
		m=re.search('cl-burst-(.*).bin', item)
		if(m):
			nums=m.group(1)
			f=open(item,"r")
			bdata = f.read()
			f.close()
			bnum = int(nums)
			cdata=[]
			os.system("mv %s cl/"%(item))
			addq(bnum,bdata,cdata)
		m=re.search('cl-startpoints-(.*).bin', item)
		if(m):
			nums=m.group(1)
			f=open(item,"r")
			cdata = f.read()
			f.close()
			f=open("cl/cl-burst-%s.bin"%nums,"r")
			bdata = f.read()
			f.close()
			bnum = int(nums)
			cdata=struct.unpack("1Q16320Q",cdata)
			os.system("mv %s cl/"%(item))
			addq(bnum,bdata,cdata)
			#print("add %s"%item)

while (1):
	mergenew()
	krak()

