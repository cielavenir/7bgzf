## 7bgzf
- a suite of DEFLATE (RFC 1951) algorithms
- For conference participants: please contact me **using the email on my github profile.**
- The one on conference abstract book got invalidated as they changed email system.
    - (This notice will be removed once published)

## Building Instruction
- Execute compile.sh.
    - 7bgzf is almost static; the dependency is only libc.
- p7zip and htslib could be installed by system administration command.
- 7bgzf.so can be injected into libhts.so using LD_PRELOAD environment variable.
    - However, since LD_PRELOAD utilizes procedure linkage table (PLT), samtools / bcftools need to link to libhts.so (not a).
    - BGZF_METHOD environment variable is used to set the compression method. For example, BGZF_METHOD=libdeflate12 means libdeflate level 12.
    - Example: `BGZF_METHOD=libdeflate12 LD_PRELOAD=./7bgzf.so samtools view ...`

- Detailed instruction for my ARM assessment should be available on the paper supplementary data.

## Short Article
- https://qiita.com/cielavenir/items/7583799d7d2d8d5cce37
- Note that you cannot site this article. Site this repository URL or upcoming paper (hopefully)

## Note
- (unlike internal version) deflation algorithms are kept untouched since 2019 Aug, to prepare the paper.
