 #include <stdio.h>
 
#if !CRYPTOPP
int cryptopp_deflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen,
	int level
){
	return -1;
}
int cryptopp_inflate(
	unsigned char *dest,
	size_t *destLen,
	const unsigned char *source,
	size_t sourceLen
){
	return -1;
}
#endif
