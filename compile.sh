#!/bin/bash

# CC=arm-linux-androideabi-gcc CFLAGS="-mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon" arch=noarch host_cpu=base_aliases
# CC=i686-w64-mingw32-gcc LIBS="-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic -lole32 -loleaut32" CFLAGS="-mmmx -msse -msse2" arch=noarch host_cpu=base_aliases ZLIBNG_X86=1
# CC=x86_64-w64-mingw32-gcc LIBS="-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic -lole32 -loleaut32" CFLAGS="-march=core2 -mfpmath=sse -mmmx -msse -msse2 -msse3 -mno-ssse3" arch=mingw host_cpu=x86_64 ZLIBNG_X86=1

shopt -s nullglob
if [ -z "${CC}" ]; then
	CC="gcc"
	if [ -z "${CFLAGS}" ]; then
		CFLAGS="-march=native"
	fi
fi
if [ -z "${LIBS}" ]; then
	LIBS="-lm -ldl -pthread"
fi
if [ ! -z "${ZLIBNG_X86}" ]; then
	ZLIBNG_X86="-DX86_CPUID -DX86_QUICK_STRATEGY lib/zlib-ng/arch/x86/*.c"
fi
SOURCES="lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/zlib-ng/*.c lib/zlib-ng/arch/arm/*.c ${ZLIBNG_X86} lib/memstream.c lib/zlibutil.c lib/zlibutil_zlibng.c lib/zlibutil_igzip.c lib/miniz.c lib/slz.c lib/libdeflate/deflate_compress.c lib/libdeflate/deflate_decompress.c lib/libdeflate/aligned_malloc.c lib/libdeflate/*/cpu_features.c lib/lzmasdk.c"

mkdir -p bin

make -C lib/isa-l -f Makefile.unx lib
for i in 7bgzf 7razf 7gzip 7png 7migz 7ciso 7daxcr zlibrawstdio zlibrawstdio2
do
	# ZopfliCalculateEntropy uses log, which is implemented in libm.
	${CC} -O2 -std=gnu99 ${CFLAGS} -DSTANDALONE -o bin/${i} applet/${i}.c applet/${i}_*.c ${SOURCES} lib/isa-l/bin/isa-l.a ${LIBS}
done
make -C lib/isa-l -f Makefile.unx clean

export CFLAGS="${CFLAGS} -fPIC"
make -C lib/isa-l -f Makefile.unx lib
${CC} -O2 -std=gnu99 ${CFLAGS} -shared -o bin/7bgzf.so bgzf_compress.c ${SOURCES} lib/isa-l/bin/isa-l.a ${LIBS}
make -C lib/isa-l -f Makefile.unx clean
