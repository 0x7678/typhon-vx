#!/usr/bin/env python

from socket import *      #import the socket library
import select
import re
import sqlite3 as lite
import string
import sys
 
CLIADDR = ('',7777)
SRVADDR = ('localhost',6666)
counter = 1
con = None
cur = None

r=0

def process_cli(line):
	global cur
	global counter
	global r

	m=re.search("crack (.*)", line)
	if(m):
		print(r)
		r+=1
		burst=m.group(1)
		cur.execute("SELECT * FROM bursts WHERE burst='%s'"%burst)
		rows=cur.fetchall()
		buf=''
		cnt=0
		for row in rows:
			cnt=1
			if (row[1] != ''):
				buf = buf + "Found %s #99999%i  (table:cache)\n"%(row[1],counter)
		if (cnt):
			cli.send("Cracking #99999%i %s\n"%(counter,burst))
			cli.send(buf)
			cli.send("crack #99999%i took 0 msec\n"%counter)
			counter = counter + 1
		else:		
			srv.send(line + '\n')

def process_srv(line):
	global con
	global cur
	global counter

	m=re.search('Cracking #(.*) (.*)',line)
	if(m):
		cur.execute("INSERT INTO incrack VALUES('%s', %s)"%(m.group(2),m.group(1)))
		con.commit()
		cli.send(line + '\n')
	m=re.search('Found (.* @ .*) #(.*)  ',line)
	if(m):
		cur.execute("SELECT burst FROM incrack WHERE num=%s"%m.group(2))
		row=cur.fetchone()
		cur.execute("INSERT INTO bursts VALUES('%s', '%s')"%(row[0],m.group(1)))
		con.commit()
		cli.send(line + '\n')
	m=re.search('crack #(.*) took',line)
	if (m):
		cur.execute("SELECT burst FROM incrack WHERE num=%s"%m.group(1))
		row=cur.fetchone()
		cur.execute("INSERT INTO bursts VALUES('%s', '')"%row[0])
		con.commit()
		cli.send(line + '\n')


try:
#	con=lite.connect(gsmsession+'keys.db')
	con=lite.connect('keycache.db')
	cur = con.cursor()    
	cur.execute('SELECT count(1) from bursts')
	data = cur.fetchone()

except lite.Error, e:
	cur.execute("CREATE TABLE bursts(burst TEXT, found TEXT);")
	cur.execute("CREATE TABLE incrack(burst TEXT, num INT);")
	cur.execute("create index idx1 on bursts (burst);")
	con.commit()

cur.execute("PRAGMA cache_size = 1000000;")
#cur.execute("DELETE FROM incrack")
#cur.execute("delete from bursts where burst in (select burst from bursts b1 where found<>'' and (select count(*) from bursts b2 where b1.burst = b2.burst and b2.found='')=0);")
con.commit()

srv = socket(AF_INET,SOCK_STREAM)
srv.connect((SRVADDR))
print "CONNECTED"
 
serv = socket( AF_INET,SOCK_STREAM)    
serv.bind((CLIADDR))    #the double parens are to create a tuple with one element
serv.listen(5)    #5 is the maximum number of queued connections we'll allow
print "LISTENING"
 
cli,addr = serv.accept() #accept the connection
buf=['','']
#try:
if 1:
	while(1):
		inp,out,exc = select.select([cli, srv], [], [])
		for i in inp:
			if (i == srv):
				buf[1] = buf[1] + i.recv(4096)
				pos = buf[1].find('\n')
				while (pos >1):
					line=buf[1][0:pos]
#					print "srv " + line
					process_srv(string.strip(line))
					buf[1]=buf[1][pos+1:]
					pos = buf[1].find('\n')
			elif (i == cli):
				buf[0] = buf[0] + i.recv(4096)
				pos = buf[0].find('\n')
				while (pos >1):
					line=buf[0][0:pos]
#					print "cli " + line
					process_cli(string.strip(line))
					buf[0]=buf[0][pos+1:]
					pos = buf[0].find('\n')
			else:
				print "What?"
#except:
#	srv.close()
		
			
 
srv.close()
