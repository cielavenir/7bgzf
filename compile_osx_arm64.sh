#!/bin/sh
make clean
echo "arm64"
PREFIX=arm64-apple-darwin- ARCH="-pthread -flto -D__ARM_NEON -fno-stack-check" arch=aarch64 host_cpu=aarch64 SUFF=_osx_arm64 make
make clean

if ! type ldid >/dev/null; then
  ldid -S cielbox_osx_arm64
fi
