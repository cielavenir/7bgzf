#!/bin/bash

# CC="arm-linux-androideabi-gcc -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon"

shopt -s nullglob
if [ -z "${CC}" ]; then
	CC="gcc -march=native"
fi
if [ ! -z "${ZLIBNG_X86}" ]; then
	ZLIBNG_X86="-DX86_CPUID -DX86_QUICK_STRATEGY lib/zlib-ng/arch/x86/*.c"
fi

for i in 7bgzf 7razf 7gzip 7png zlibrawstdio zlibrawstdio2
do
	# ZopfliCalculateEntropy uses log, which is implemented in libm.
	${CC} -O2 -std=gnu99 -DSTANDALONE -o ${i} applet/${i}.c applet/${i}_*.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/zlib-ng/*.c lib/zlib-ng/arch/arm/*.c ${ZLIBNG_X86} lib/memstream.c lib/zlibutil.c lib/zlibutil_zlibng.c lib/miniz.c lib/slz.c lib/libdeflate/deflate_compress.c lib/libdeflate/aligned_malloc.c lib/lzmasdk.c -lm -ldl
done

${CC} -O2 -std=gnu99 -shared -fPIC -o 7bgzf.so bgzf_compress.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/zlib-ng/*.c lib/zlib-ng/arch/arm/*.c ${ZLIBNG_X86} lib/memstream.c lib/zlibutil.c lib/zlibutil_zlibng.c lib/miniz.c lib/slz.c lib/libdeflate/deflate_compress.c lib/libdeflate/aligned_malloc.c lib/lzmasdk.c -lm -ldl
