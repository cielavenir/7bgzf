## 7bgzf
- a suite of DEFLATE (RFC 1951) algorithms
- contact: please use my github profile or the email on the paper (now published!)

## Building Instruction
- Execute compile.sh.
    - 7bgzf is almost static; the dependency is only libc.
- p7zip and htslib could be installed by system administration command.
- 7bgzf.so can be injected into libhts.so using LD_PRELOAD environment variable.
    - However, since LD_PRELOAD utilizes procedure linkage table (PLT), samtools / bcftools need to link to libhts.so (not a).
        - The simplest way is to change config.mk.in `@Hsource@HTSLIB = $(HTSDIR)/libhts.a` to libhts.so.
        - Note: `configure --enable-plugins` does not work as symbols in exe are prioritized than LD_PRELOAD.
    - BGZF_METHOD environment variable is used to set the compression method. For example, BGZF_METHOD=libdeflate12 means libdeflate level 12.
    - Example: `BGZF_METHOD=libdeflate12 LD_PRELOAD=./7bgzf.so samtools view ...`

- Detailed instruction for my ARM assessment should be available on the paper supplementary data.

## Side notes (oh I can write this as the paper is published)

- on-the-fly conversion of compressed files

I have not mentioned explicitly in the paper, but that conversion means [PetaGene](https://www.petagene.com/). They say they provide virtual filesystem via LD_PRELOAD. But I was not able to get evaluation copy - they say no personal evaluation.
