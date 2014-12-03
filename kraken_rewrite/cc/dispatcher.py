import select
import socket, time, struct

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_socket.bind(('127.0.0.1', 6666))
server_socket.listen(50)

jobptr=0
to_dp=[0]      # cracking #0 0101010
to_delta=[0]   # 8+16320*8 structures
to_chainfin=[0]# dtto
b=["010001010110011110111000011110001100001111101100111100000110000001110000100111100010100001000001001000110001000101", 0]

status=0
delta_socket = None

read_list = [server_socket]

def crack_submit(s):
  global to_dp,jobptr,b
  data=s.recv(114)
  arr=[data, jobptr]
  to_dp.append(arr)
  b=arr
  s.send("Cracking #%i %s\n"%(jobptr,data))
  jobptr+=1
  print("Received yummy keystream %s"%data)

def send_burst(s):
  global to_dp,b,status
#  b=["010001010110011110111000011110001100001111101100111100000110000001110000100111100010100001000001001000110001000101", 0]
#  b=["010001010110011110111000011110001100001111101100111100000110000001110000100111100010100001000001001000110001000101", 0]
#  if len(to_dp):
#    b=to_dp[0]
#    to_dp.remove(b)
  if status == 1:
    s.send("%i %s\n"%(b[1],b[0]))

def read_dps(s):
  global to_delta
  #data=s.recv(8)
  #for i in range(0,40):
  #  data+=s.recv(408*8)
  sfile=s.makefile("rb")
  data=sfile.read(40*408*8+8)
  #data= bytearray(b"\0" * (8+40*408*8))
  #s.recv_into(data, 8+40*408*8)
  to_delta[0]=data
  print("received %i data"%len(data))
#  unstruk=struct.unpack("358Q357Q", to_delta[0])
#  for i in range(0,358+357):
#    print("%16x %i"%(unstruk[i],i)

def send_dps(s):
  global to_delta,delta_socket,status
  if not delta_socket:
    delta_socket=s
  if status == 2:
#    print("kop")
    b=to_delta[0]
#    unstruk=struct.unpack("16321Q", b)
#    for i in range(0,16320):
#      print("%16x"%unstruk[i])

    delta_socket.sendall(b)
#    to_delta.remove(b)

def read_sta(s):
  global to_chainfin
#  data= bytearray(b"\0" * (8+40*408*8))
#  s.recv_into(data, 8+40*408*8)
  sfile=s.makefile("rb")
  data=sfile.read(8+40*408*8)
#  for i in range(0,40):
#    data+=s.recv(408*8)
  print("dlen=%i"%len(data))
#  data=s.recv(8+40*408*8)
  to_chainfin[0]=data
#  unstruk=struct.unpack("16321Q", b)
#  for i in range(0,16320):
#    print("%16x"%unstruk[i])

#  b=data
#  unstruk=struct.unpack("358Q", b)
#  for i in range(0,358):
#    print("%16x"%unstruk[i])

def send_sta(s):
  global to_chainfin
  global to_dp,b,status
  if status==2:
#    b=["010001010110011110111000011110001100001111101100111100000110000001110000100111100010100001000001001000110001000101", 0]
#    unstruk=struct.unpack("1Q", to_chainfin[0])
#    print("")
    s.send("%i %s\n"%(b[1],b[0]))
    b=to_chainfin[0]
    print("blen=%i"%len(b))
    s.sendall(b)

def found(s,report_sock):
  data=s.recv(100)
  msg="Found "+data
  print(msg)
  if report_sock:
    report_sock.send(msg)

def took(s,report_sock):
  #data=s.recv()
  msg="crack #%i took 65535 msec\n"%(jobptr-1)
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
          status=1
        elif data == "getsmp":
          send_burst(s)
        elif data == "retdps":
          read_dps(s)
          status=2
          send_dps(s)
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
  print("status=%i"%status)

