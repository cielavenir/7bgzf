#!/bin/sh

# Use below:
# android-ndk/build/tools/make-standalone-toolchain.sh --platform=android-21 --toolchain=aarch64-linux-android-4.9 --install-dir=android-arm64
# android-ndk/build/tools/make-standalone-toolchain.sh --platform=android-21 --toolchain=x86_64-4.9 --install-dir=android-x64

# I use NDK r11c, whose size is handy.
# Note: according to http://dsas.blog.klab.org/archives/52211448.html,
#       noPIE is supported until KitKat and PIE is supported from JellyBeans.
#       anyway we compile 64bit binary for PIE builds, which is supported from Lollipop.

make clean
echo "Android arm64"
### -march=armv7-a is required to generate Thumb-2 code.
### -mfloat-abi=softfp -mfpu=neon is required to enable NEON intrinsics.
### (keeping -D__ARM_NEON for noticing compilation error)
#ARCH="-mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon -pthread -fPIE -Dfseeko=fseek -Dftello=ftell -D__ARM_NEON"
ARCH="-pthread -fPIE -D__ARM_NEON -DHAVE_STPCPY -DDL_ANDROID" arch=aarch64 host_cpu=aarch64 PREFIX=aarch64-linux-android- LIBS="-pie -lm -ldl -pthread" SUFF=_android make
make clean
echo "Android x64"
ZLIBNG_X86=1 ARCH="-fPIE -pthread -DHAVE_STPCPY -DDL_ANDROID" arch= host_cpu=x86_64 PREFIX=x86_64-linux-android- LIBS="-pie -lm -ldl -pthread" SUFF=_android_x86 make
make clean

#upx-lzma --best --lzma cielbox_android

# [obsolete]
# 1. put droid-ndk-gcc to $PATH then link to droid-ndk-{g++,ld,strip}
# 2. put specs to /opt/android-ndk/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/lib/gcc/arm-linux-androideabi/4.4.3/

# for arm64, need `aarch64-none-eabi-gcc`. Homebrew can use gcc-aarch64-embedded cask. Debian can install gcc-aarch64-linux-gnu and symlink aarch64-linux-gnu-gcc to aarch64-none-eabi-gcc.