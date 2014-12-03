#!/bin/sh

./compress.py

ld -s -r -o my_kernel_double.o -b binary my_kernel_double.Z
ld -s -r -o my_kernel_single.o -b binary my_kernel_single.Z

g++ -O2 -fPIC -o A5Il.so A5Il.cpp Advance.cpp A5IlPair.cpp A5JobQueue.cpp CalDevice.cpp CalModule.cpp kernelLib.cpp my_kernel_single.o my_kernel_double.o -D BUILDING_DLL -lpthread -shared -I/usr/local/atical/include -laticalrt -laticalcl -lz

g++ -g -fPIC -o a5il_test a5il_test.cpp A5IlStubs.cpp -ldl
