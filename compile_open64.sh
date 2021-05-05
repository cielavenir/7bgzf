#!/bin/sh

: <<"#EOM"
export PATH=/opt/open64/bin:$PATH
sudo cp /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/crt1.o
sudo cp /usr/lib/x86_64-linux-gnu/crti.o /usr/lib/crti.o
sudo cp /usr/lib/x86_64-linux-gnu/crtn.o /usr/lib/crtn.o
#EOM

make clean
echo "Self (Open64)"
#ZLIBNG_X86=1
OPEN64=1 ARCH="-mmmx -msse -msse2 -msse3 -pthread -D__builtin_bswap32=byteswap32 -D__builtin_bswap64=byteswap64" LIBS="-pthread -ldl" SUFF=_open64 make
### now LIBS--nolibopen64rt is invalid.
make clean

upx --best --lzma cielbox_open64
