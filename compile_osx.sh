#!/bin/sh
#compile using clang++. You need OSX Lion.
#Actually compiling cielbox using gcc isn't recommended on Lion or later. 

make clean
echo "i686"

: <<'#EOM'
if [ ! -d /opt/MacOSX10.13.sdk ]; then
  curl -L https://github.com/phracker/MacOSX-SDKs/releases/download/10.13/MacOSX10.13.sdk.tar.xz | sudo tar -C /opt -xJ
fi
if [ ! -d /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk ]; then
  sudo ln -s /opt/MacOSX10.13.sdk /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
fi
#EOM

#LIBS="-Wl,-macosx_version_min,10.6 -lcrt1.10.5.o"
#ZLIBNG_X86=1 CLANG=1 ARCH="-arch i686 -march=core2 -mmmx -msse -msse2 -msse3 -mssse3 -flto -pthread" LIBS="-pthread" arch=noarch host_cpu=base_aliases SUFF=_osx_i686 make
CRYPTOPP= ZLIBNG_X86=1 CLANG=1 ARCH="-arch i686 -march=core2 -mmmx -msse -msse2 -msse3 -mssse3 -flto -pthread -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk" LIBS="-pthread -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk" arch=32 host_cpu=base_aliases SUFF=_osx_i686 make
make clean

#removed. anyway cielbox isn't intended to big endian machines.
#echo "ppc"
#ARCH="-arch ppc -pthread" LIBS="-pthread" arch=noarch host_cpu=base_aliases SUFF=_osx_ppc make
#make clean

echo "x86_64"
#LIBS="-Wl,-macosx_version_min,10.6 -lcrt1.10.5.o"
ZLIBNG_X86=1 CLANG=1 ARCH="-arch x86_64 -march=core2 -mmmx -msse -msse2 -msse3 -mssse3 -flto -pthread" LIBS="-pthread" arch=Darwin host_cpu=x86_64 SUFF=_osx_x86_64 make
make clean

./cielbox_osx_i686
./cielbox_osx_x86_64

#upx --best --lzma cielbox_osx_i686 cielbox_osx_x86_64
lipo cielbox_osx_i686 cielbox_osx_x86_64 -create -output cielbox_osx
#rm cielbox_osx_i686 cielbox_osx_x86_64
