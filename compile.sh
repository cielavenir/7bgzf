gcc -O2 -DSTANDALONE -o 7razf applet/7razf.c applet/7razf_testdecode.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
gcc -O2 -DSTANDALONE -o 7bgzf applet/7bgzf.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
gcc -O2 -DSTANDALONE -o 7gzip applet/7gzip.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
gcc -O2 -DSTANDALONE -o 7png applet/7png.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
#gcc -O2 -DSTANDALONE -o 7ciso applet/7ciso.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
#gcc -O2 -DSTANDALONE -o 7daxcr applet/7daxcr.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++

gcc -O2 -DSTANDALONE -o 7png applet/zlibrawstdio.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
gcc -O2 -DSTANDALONE -o 7png applet/zlibrawstdio2.c lib/zopfli/*.c lib/popt/*.c lib/zlib/*.c lib/memstream.c lib/lzmasdk.cpp -lstdc++
