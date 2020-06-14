#!/bin/sh
#make clean
echo "Self"
ZLIBNG_X86=1 ARCH="-march=native -mfpmath=sse -pthread" LIBS="-ldl -pthread" make
#make clean

#On Debian, use 3.08-2 or later.
#upx --best --lzma cielbox
# cielbox.exe
