// Copied from https://git.musl-libc.org/cgit/musl/tree/src/prng?h=rs-1.0

#include <stdint.h>
#include <string.h>

static unsigned short __seed48[7] = { 0, 0, 0, 0xe66d, 0xdeec, 0x5, 0xb };

static uint64_t __rand48_step(unsigned short *xi, unsigned short *lc)
{
	uint64_t a, x;
	x = xi[0] | (xi[1]+0U)<<16 | (xi[2]+0ULL)<<32;
	a = lc[0] | (lc[1]+0U)<<16 | (lc[2]+0ULL)<<32;
	x = a*x + lc[3];
	xi[0] = x;
	xi[1] = x>>16;
	xi[2] = x>>32;
	return x & 0xffffffffffffull;
}

double erand48(unsigned short s[3])
{
	union {
		uint64_t u;
		double f;
	} x = { 0x3ff0000000000000ULL | __rand48_step(s, __seed48+3)<<4 };
	return x.f - 1.0;
}

double drand48(void)
{
	return erand48(__seed48);
}

long nrand48(unsigned short s[3])
{
	return __rand48_step(s, __seed48+3) >> 17;
}

long lrand48(void)
{
	return nrand48(__seed48);
}

long jrand48(unsigned short s[3])
{
	return (int32_t)(__rand48_step(s, __seed48+3) >> 16);
}

long mrand48(void)
{
	return jrand48(__seed48);
}

unsigned short *seed48(unsigned short s[3])
{
	static unsigned short p[3];
	memcpy(p, __seed48, sizeof p);
	memcpy(__seed48, s, sizeof p);
	return p;
}

void srand48(long seed)
{
	seed48((unsigned short [3]){ 0x330e, seed, seed>>16 });
}
