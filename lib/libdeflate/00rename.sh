#!/bin/sh
cd "$(dirname $(realpath $0))"
mv adler32.c libdeflate_adler32.c
mv crc32.c libdeflate_crc32.c
mv utils.c libdeflate_utils.c
echo '#include "arm/cpu_features.c"' > libdeflate_cpu_features_arm.c
echo '#include "x86/cpu_features.c"' > libdeflate_cpu_features_x86.c
