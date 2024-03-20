#!/bin/sh

# LIBRARY_PATH=/usr/lib/x86_64-linux-gnu will add crt1/i/n.o path
# PATH=/opt/open64/bin:$PATH will add open64 from /opt/open64

make clean
echo "Self (Open64)"
#ZLIBNG_X86=1
PATH=/opt/open64/bin:$PATH LIBRARY_PATH=/usr/lib/x86_64-linux-gnu OPEN64=1 ARCH="-I/usr/include/x86_64-linux-gnu -DOPEN64=1 -mmmx -msse -msse2 -msse3 -pthread -D__builtin_bswap32=byteswap32 -D__builtin_bswap64=byteswap64" LIBS="-pthread -ldl" SUFF=_open64 make
### now LIBS--nolibopen64rt is invalid.
make clean

upx --best --lzma cielbox_open64
