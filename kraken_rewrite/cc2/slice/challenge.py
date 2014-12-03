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

# key, rft
a = np.array([0,0,0,0,0,1,1,0,0,0,0,0,1,1,1,1,0,0,1,1,0,1,1,1,1,1,0,0,0,0,1,1,0,0,0,1,1,1,1,0,0,0,0,1,1,1,0,1,1,1,1,0,0,1,1,0,1,0,1,0,0,0,1,0,
              1,1,1,1,0,1,0,1,1,0,0,0,1,0,0,1,0,1,1,0,0,0,1,1,0,0,1,0,1,1,0,1,1,1,0,0,0,0,1,1,0,1,0,0,1,1,0,0,0,0,1,1,1,0,0,1,1,0,1,1,1,0,0,1],
      dtype=np.uint32)

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
FILE_NAME="slice.c"
f=open(FILE_NAME,"r")
SRC = ''.join(f.readlines())
f.close()

prg = cl.Program(ctx, SRC).build()

#print("\n\nCracking the following data for a challenge:")
#print(a)

# launch the kernel
event = prg.krak(queue, a.shape, None, a_dev, s)
event.wait()
 
# copy the output from the context to the Python process
cl.enqueue_copy(queue, a, a_dev)
 
#print("CL returned:")
#print(a)

