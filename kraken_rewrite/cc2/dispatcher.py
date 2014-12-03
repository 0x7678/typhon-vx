import select
import socket, time, struct

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_socket.bind(('127.0.0.1', 6666))
server_socket.listen(50)

jobptr=0
lookup=[]
to_dp=[]       # (data,jobptr)
to_delta=[]    # 8+16320*8 structures
to_chainfin=[] # dtto
# test burst
b=["010001010110011110111000011110001100001111101100111100000110000001110000100111100010100001000001001000110001000101", 0]

finptr=0

# token - burst (key-value)
token_bursts=[]

status=0
#delta_socket = None

read_list = [server_socket]

# extract job number from a packed blob
def extract_jobnum(blob):
  u=struct.unpack("1Q16320Q",blob)
  return u[0]

def crack_submit(s):
  global to_dp,jobptr,b,lookup
  data=s.recv(114)
  arr=[data, jobptr]
  to_dp.append(arr)
  lookup.append(arr)
  b=arr
  s.send("Cracking #%i %s\n"%(jobptr,data))
  print("Received yummy keystream %i/%s"%(jobptr,data))
  jobptr+=1

def send_burst(s):
  global to_dp,b,status
  if len(to_dp):
    s.send("%i %s\n"%(to_dp[0][1],to_dp[0][0]))
    print("Submitted to DPS: %i"%(to_dp[0][1]))
    del to_dp[0]

def read_dps(s):
  global to_delta
  sfile=s.makefile("rb")
  data=sfile.read(40*408*8+8)
  to_delta.append(data)
  print("received %i blob from DP solver"%len(data))

def send_dps(s):
  global to_delta,status
  print("[+] send_dps called")
  if len(to_delta):
    s.sendall(to_delta[0])
    del to_delta[0]

def read_sta(s):
  global to_chainfin
  sfile=s.makefile("rb")
  data=sfile.read(8+40*408*8)
  print("read %i blob from delta lookup"%len(data))
  to_chainfin.append(data)

def send_sta(s):
  global to_chainfin
  global to_dp,b,status
  if len(to_chainfin):
    print("<- sendsta req")
    num=extract_jobnum(to_chainfin[0])
    print("-> replying with challenge %s"%lookup[num][1])
    s.send("%i %s\n"%(lookup[num][1],lookup[num][0]))
    b=to_chainfin[0]
    print("-> and sending blob of %i"%len(b))
    s.sendall(b)
    del to_chainfin[0]

def found(s,report_sock):
  data=s.recv(100)
  msg="Found "+data
  print(msg)
  if report_sock:
    report_sock.send(msg)

def took(s,report_sock):
  global finptr
  #data=s.recv()
  msg="crack #%i took 65535 msec\n"%(finptr)
  finptr+=1
  print(msg)
  if report_sock:
    report_sock.send(msg)

report_sock=None

while True:
  readable, writable, errored = select.select(read_list, [], [], 1)
  for s in readable:
    if s is server_socket:
      client_socket, address = server_socket.accept()
      read_list.append(client_socket)
      print "Connection from", address
    else:
      try:
        data = s.recv(6)
      except:
        print("FFFUUUUU!!")
      if data:
        if data == "crack ":
          report_sock = s
          crack_submit(s)
        elif data == "getsmp":
          send_burst(s)
        elif data == "retdps":
          read_dps(s)
#          send_dps(s)
        elif data == "getdps":
          send_dps(s)
        elif data == "retsta":
          read_sta(s)
        elif data == "getsta":
          send_sta(s)
        elif data == "Found ":
          found(s,report_sock)
        elif data == "took  ":
          took(s,report_sock)
          status=0
        else:
          print(data)
      else:
        s.close()
        read_list.remove(s)
  print("ptr %i lookup %i to_dp %i to_delta %i to_chainfin %i"%(jobptr,len(lookup),len(to_dp),len(to_delta),len(to_chainfin)))

