#!/bin/sh

# Use below:
# android-ndk/build/tools/make-standalone-toolchain.sh --platform=android-8 --toolchain=arm-linux-androideabi-4.9 --install-dir=android-arm
# android-ndk/build/tools/make-standalone-toolchain.sh --platform=android-9 --toolchain=x86-4.9 --install-dir=android-x86

# I personally use NDK r11c, whose size is handy.

make clean
echo "Android arm"
### -march=armv7-a is required to generate Thumb-2 code.
### -mfloat-abi=softfp -mfpu=neon is required to enable NEON intrinsics.
### (keeping -D__ARM_NEON for noticing compilation error)
ARCH="-mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon -pthread -Dfseeko=fseek -Dftello=ftell -D__ARM_NEON -DDL_ANDROID" arch=noarch host_cpu=base_aliases PREFIX=arm-linux-androideabi- LIBS="-lm -ldl -pthread" SUFF=_android_nopie make
make clean
echo "Android x86"
ZLIBNG_X86=1 ARCH="-pthread -Dfseeko=fseek -Dftello=ftell -DDL_ANDROID" arch=noarch host_cpu=base_aliases PREFIX=i686-linux-android- LIBS="-lm -ldl -pthread" SUFF=_android_x86_nopie make
make clean

#upx-lzma --best --lzma cielbox_android

# [obsolete]
# 1. put droid-ndk-gcc to $PATH then link to droid-ndk-{g++,ld,strip}
# 2. put specs to /opt/android-ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/lib/gcc/arm-linux-androideabi/4.4.3/
