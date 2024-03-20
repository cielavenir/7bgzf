#!/bin/bash
if ! type icpx >/dev/null 2>/dev/null && [ -f /opt/intel/oneapi/setvars.sh ]; then
  . /opt/intel/oneapi/setvars.sh
fi
make clean
echo "Self (icx)"
#ZLIBNG_X86=1 ICX=1 ARCH="-march=core2 -mmmx -msse -msse2 -msse3 -pthread" LIBS="-pthread -ldl" SUFF=_icc make
ZLIBNG_X86=1 ICX=1 ARCH="-march=native -pthread" LIBS="-pthread -ldl" SUFF=_icx make
make clean

upx --best --lzma cielbox_icx
