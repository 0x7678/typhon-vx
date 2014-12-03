#!/usr/bin/python

import zlib

f = open("nsa.txt")
data = f.read()
f.close()
data2 = zlib.compress(data)
fo = open( "my_kernel_double.Z", "wb" )
fo.write(data2)
fo.close()

f = open("nsa-single.txt")
data = f.read()
f.close()
data2 = zlib.compress(data)
fo = open( "my_kernel_single.Z", "wb" )
fo.write(data2)
fo.close()
