#!/usr/bin/python

import pyopencl as cl
import numpy as np
import tables

mf = cl.mem_flags

#ctx = cl.create_some_context()
ctx = cl.create_some_context()
queue = cl.CommandQueue(ctx)

#NDATA = 10

#HOST_TYPE = np.uint32
#GPU_TYPE = 'float'
#LOCAL_DIMS = (16, 16, 1)
#LOCAL_SIZE = np.prod(LOCAL_DIMS)
#SERIAL_COUNT = 4
#GRID_DIMS = (int(np.ceil(np.ceil(1.0*NDATA/LOCAL_SIZE)/SERIAL_COUNT)), 1, 1)
#CACHE_SIZE = np.prod(GRID_DIMS)

#get_global = lambda grid, local: (grid[0]*local[0], grid[1]*local[1], grid[2]*local[2])

#cipherstates, cs_len, max_rounds, ctrl
#cipherstates = np.array(0x4567b878c3ecf060, dtype=np.uint64)
a = np.array([
        0x4567b878c3ecf060,
        0x094437620afdad7a,
        0xf3a516b6918ceb40,
        0xb4c691af9d9c32c3,
        0x3c8e9da8a6c79129,
        0xa96f16ec94ef2d75,
        0x2d9c71d67ab45349,
        0x53c7ffe510ec4a4f,
        0x6a9f7fea9476dd7a,
        0x2,
        0x7,0x0,
        0x86f9858d4f428859,
        0x094437620afdad7a,
        0xf3a516b6918ceb40,
        0xb4c691af9d9c32c3,
        0x3c8e9da8a6c79129,
        0xa96f16ec94ef2d75,
        0x2d9c71d67ab45349,
        0x53c7ffe510ec4a4f,
        0x6a9f7fea9476dd7a,
        0x0,
        0x8,0xFB1528A1BE486E3B,
]
        , dtype=np.uint64)


# create the kernel input
#a = np.array(np.arange(10), dtype=np.uint64)


#s = np.uint32(a.shape)
#print(s)



 
# create context buffers for a and b arrays
# for a (input), we need to specify that this buffer should be populated from a
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

print("\n\nCracking the following data for a challenge:")
print(a)

# launch the kernel
event = prg.krak(queue, a.shape, None, a_dev, s)
event.wait()
 
# copy the output from the context to the Python process
cl.enqueue_copy(queue, a, a_dev)
 
# if everything went fine, b should contain squares of integers
print("CL returned:")
print(a)

if(a[0] == 0xdfd05a8b899b6000):
  print("[OK] Distinguished point search over colors 2-7 passed, result=0xdfd05a8b899b6000")
if(a[12] == 0x3e248e031efda051):
  print("[OK] Key in table in color 2 found, result=0x3e248e031efda051")

print(a[12])
print("Dame brmlab? Dame deku?")


#prg = cl.Program(ctx, SRC).build()
#krak = prg.krak

#event.wait()
# copy the output from the context to the Python process
#cl.enqueue_copy(queue, b, b_dev)

# if everything went fine, b should contain squares of integers
#print(b)


# kraken_vstupy:
# 1 : pointer na pole s 64 bit cislami (keystates)
# 2 : velkost pola (int)
# 3 : max pocet opakovani (ak netrafi distinguished point)
# 4 : crtl (nepouzivany)

# kraken vystupy:

# 1 : to co bolo povodne vstupne pole sa stane vystupnym.
# borky neriesi prechodove fcie.

# az tu davam argumenty
#krak.set_args(cipherstates, cs_len, max_rounds, ctrl)

# transfer argumentov z ramky na device (kartu)
#e  = [ cl.enqueue_copy(queue, cipherstates_buf, cipherstates), ]
#e += [ cl.enqueue_copy(queue, b_buf, b), ]

# 4 argument <- velkost vstupu (zavisi na def. fcie)
#krak.set_arg(3, np.uint32(1))
# vytvorenie kernelov, spusti samotny vypovet
#e  = [ cl.enqueue_nd_range_kernel(queue, product, get_global(GRID_DIMS, LOCAL_DIMS), LOCAL_DIMS, wait_for=e), ]

# cakanie na vysledok
#cl.enqueue_copy(queue, cipherstates, cipherstates_buf, wait_for=e)


#krak(queue, cipherstates.shape, None, cipherstates_buf, cs_len_buf, max_rounds_buf, ctrl_buf )
#c = numpy.empty_like(cipherstates)
#cl.enqueue_read_buffer(queue, dest_buf, c).wait()


#print c[:n.item()].suma()

#print "trololo"

# pocitam sumu vektoru v logaritmickom case (log n) krat volam openclko s parametrami

#while n > 1:
#    GRID_DIMS = (int(np.ceil(np.ceil(1.0*n.item()/LOCAL_SIZE)/SERIAL_COUNT)), 1, 1)
#    sum.set_arg(2, np.uint32(n.item()))
#    e = [ cl.enqueue_nd_range_kernel(queue, sum, get_global(GRID_DIMS, LOCAL_DIMS), LOCAL_DIMS), ]
#    cl.enqueue_copy(queue, n, n_buf, wait_for=e)
#    cl.enqueue_copy(queue, c, c_buf, wait_for=e)
#    print c[:n.item()].sum()

#cl.enqueue_copy(queue, d, d_buf, wait_for=e)

#print d.item()
#print np.dot(a,b)
#print (np.dot(a,b) - d.item()) / d.item()

