## compilingForAppleSilicon

1. Copy arm64-apple-darwin-\* to PATH, editing xcode path
2. Launch `PREFIX=arm64-apple-darwin- ARCH="-pthread -flto -D__ARM_NEON" arch=noarch host_cpu=aarch64 LIBS="-pthread" SUFF=_osx_arm64 make`

After Xcode 12 gets official, `ARCH="-arch arm64 -pthread -flto -D__ARM_NEON" arch=noarch host_cpu=aarch64 LIBS="-pthread" SUFF=_osx_arm64 make` should be enough.

## caveats

Xcode as might not be compatible with isa-l aarch64 assembly code.

