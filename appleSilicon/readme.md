## compilingForAppleSilicon

1. Copy arm64-apple-darwin-\* to PATH, editing xcode path
2. Launch `PREFIX=arm64-apple-darwin- ARCH="-pthread -flto -D__ARM_NEON" LIBS="-pthread" arch=aarch64 host_cpu=aarch64 SUFF=_osx_arm64 make`

After Xcode 12 gets official, `ARCH="-arch arm64 -pthread -flto -D__ARM_NEON" LIBS="-pthread" arch=aarch64 host_cpu=aarch64 SUFF=_osx_arm64 make` should be enough.

Now my modified isa-l support aarch64 assembly.

