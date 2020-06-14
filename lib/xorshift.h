#ifndef _XORSHIFT_H_
#define _XORSHIFT_H_

#ifdef __cplusplus
extern "C"{
#endif

unsigned int xor_rand();
void xor_srand(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif
