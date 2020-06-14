#!/bin/sh
make clean
echo "so"
ZLIBNG_X86=1 ARCH="-march=native -mfpmath=sse -pthread -fPIC" LIBS="-ldl -pthread" LDF="-shared" TARGET=7bgzf.so make
make clean
