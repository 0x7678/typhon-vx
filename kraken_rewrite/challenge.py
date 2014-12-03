#!/usr/bin/python

import pyopencl as cl
import numpy as np

mf = cl.mem_flags

ctx = cl.create_some_context()
queue = cl.CommandQueue(ctx)

# create the kernel input
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
        0x8292d076a9b92641,
        0x094437620afdad7a,
        0xf3a516b6918ceb40,
        0xb4c691af9d9c32c3,
        0x3c8e9da8a6c79129,
        0xa96f16ec94ef2d75,
        0x2d9c71d67ab45349,
        0x53c7ffe510ec4a4f,
        0x6a9f7fea9476dd7a,
        0x0,
        0x2,0x4567b878c3ecf060,
]
        , dtype=np.uint64)



# create context buffers for "a" array
# for a (input), we need to specify that this buffer should be populated from a
a_dev = cl.Buffer(ctx, cl.mem_flags.READ_ONLY | cl.mem_flags.COPY_HOST_PTR, 
                    hostbuf=a) 

# length of input array
s = np.uint32(a.shape)
# max rounds
r = np.uint32(5000)
# ctrl (not used)
z = np.uint32(0)

# read opencl file 
FILE_NAME="krak.cl"
f=open(FILE_NAME,"r")
SRC = ''.join(f.readlines())
f.close()

# compile the kernel
prg = cl.Program(ctx, SRC).build()

print("\n\nCracking the following data for a challenge:")
print(a)

# launch the kernel
event = prg.krak(queue, a.shape, None, a_dev, s)
event.wait()
 
# copy the output from the context to the Python process
cl.enqueue_copy(queue, a, a_dev)
 
print("CL returned:")
print(a)

if(a[0] == 0xdfd05a8b899b6000):
  print("[OK] Distinguished point search over colors 2-7 passed, result=0xdfd05a8b899b6000")
if(a[12] == 0x3e248e031efda051):
  print("[OK] Key in table in color 2 found, result=0x3e248e031efda051")

