#!/bin/bash

shopt -s nullglob
if [ -z "${CC}" ]; then
	CC="gcc"
fi

for i in 7bgzf 7razf 7gzip 7png zlibrawstdio zlibrawstdio2
do
	# ZopfliCalculateEntropy uses log, which is implemented in libm.
	${CC} -O2 -std=gnu99 -DSTANDALONE -o ${i} applet/${i}.c applet/${i}_*.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/deflate_compress.c lib/libdeflate/aligned_malloc.c lib/lzmasdk.c -lm -ldl
done

${CC} -O2 -std=gnu99 -shared -fPIC -o 7bgzf.so bgzf_compress.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/deflate_compress.c lib/libdeflate/aligned_malloc.c lib/lzmasdk.c -lm -ldl
