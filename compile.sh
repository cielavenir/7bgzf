g++ -O2 -fno-exceptions -fno-rtti -fPIC -c -o lzmasdk.o lib/lzmasdk.cpp

gcc -O2 -std=c99 -DSTANDALONE -o 7razf applet/7razf.c applet/7razf_testdecode.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl
gcc -O2 -std=c99 -DSTANDALONE -o 7bgzf applet/7bgzf.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl
gcc -O2 -std=c99 -DSTANDALONE -o 7gzip applet/7gzip.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl
gcc -O2 -std=c99 -DSTANDALONE -o 7png applet/7png.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl

gcc -O2 -std=c99 -DSTANDALONE -o zlibrawstdio applet/zlibrawstdio.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl
gcc -O2 -std=c99 -DSTANDALONE -o zlibrawstdio2 applet/zlibrawstdio2.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl

gcc -O2 -std=c99 -shared -fPIC -o 7bgzf.so bgzf_compress.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/zlibutil.c lib/miniz.c lib/slz.c lib/libdeflate/libdeflate.c lzmasdk.o -lsupc++ -lm -ldl
