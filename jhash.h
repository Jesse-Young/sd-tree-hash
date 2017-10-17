#ifndef _LINUX_JHASH_H
#define _LINUX_JHASH_H

/* Best hash sizes are of power of two */
#define jhash_size(n)   ((unsigned int)1<<(n))
/* Mask the hash value, i.e (value & jhash_mask(n)) instead of (value % n) */
#define jhash_mask(n)   (jhash_size(n)-1)

struct __una_u16 { unsigned short x; } __attribute__((packed));
struct __una_u32 { unsigned int x; } __attribute__((packed));
struct __una_u64 { unsigned long long x; } __attribute__((packed));


/**
 * rol32 - rotate a 32-bit value left
 * @word: value to rotate
 * @shift: bits to roll
 */
static inline unsigned int rol32(unsigned int word, unsigned int shift)
{
	return (word << shift) | (word >> ((-shift) & 31));
}

static inline unsigned int __get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}

/* __jhash_mix -- mix 3 32-bit values reversibly. */
#define __jhash_mix(a, b, c)			\
{						\
	a -= c;  a ^= rol32(c, 4);  c += b;	\
	b -= a;  b ^= rol32(a, 6);  a += c;	\
	c -= b;  c ^= rol32(b, 8);  b += a;	\
	a -= c;  a ^= rol32(c, 16); c += b;	\
	b -= a;  b ^= rol32(a, 19); a += c;	\
	c -= b;  c ^= rol32(b, 4);  b += a;	\
}

/* __jhash_final - final mixing of 3 32-bit values (a,b,c) into c */
#define __jhash_final(a, b, c)			\
{						\
	c ^= b; c -= rol32(b, 14);		\
	a ^= c; a -= rol32(c, 11);		\
	b ^= a; b -= rol32(a, 25);		\
	c ^= b; c -= rol32(b, 16);		\
	a ^= c; a -= rol32(c, 4);		\
	b ^= a; b -= rol32(a, 14);		\
	c ^= b; c -= rol32(b, 24);		\
}

/* An arbitrary initial parameter */
#define JHASH_INITVAL		0xdeadbeef

/* jhash - hash an arbitrary key
 * @k: sequence of bytes as key
 * @length: the length of the key
 * @initval: the previous hash, or an arbitray value
 *
 * The generic version, hashes an arbitrary sequence of bytes.
 * No alignment or length assumptions are made about the input key.
 *
 * Returns the hash value of the key. The result depends on endianness.
 */
static inline unsigned int jhash(const void *key, unsigned int length, unsigned int initval)
{
	unsigned int a, b, c;
	const u8 *k = key;

	/* Set up the internal state */
	a = b = c = JHASH_INITVAL + length + initval;

	/* All but the last block: affect some 32 bits of (a,b,c) */
	while (length > 12) {
		a += __get_unaligned_cpu32(k);
		b += __get_unaligned_cpu32(k + 4);
		c += __get_unaligned_cpu32(k + 8);
		__jhash_mix(a, b, c);
		length -= 12;
		k += 12;
	}
	/* Last block: affect all 32 bits of (c) */
	/* All the case statements fall through */
	switch (length) {
	case 12: c += (unsigned int)k[11]<<24;
	case 11: c += (unsigned int)k[10]<<16;
	case 10: c += (unsigned int)k[9]<<8;
	case 9:  c += k[8];
	case 8:  b += (unsigned int)k[7]<<24;
	case 7:  b += (unsigned int)k[6]<<16;
	case 6:  b += (unsigned int)k[5]<<8;
	case 5:  b += k[4];
	case 4:  a += (unsigned int)k[3]<<24;
	case 3:  a += (unsigned int)k[2]<<16;
	case 2:  a += (unsigned int)k[1]<<8;
	case 1:  a += k[0];
		 __jhash_final(a, b, c);
	case 0: /* Nothing left to add */
		break;
	}

	return c;
}

/* jhash2 - hash an array of unsigned int's
 * @k: the key which must be an array of unsigned int's
 * @length: the number of unsigned int's in the key
 * @initval: the previous hash, or an arbitray value
 *
 * Returns the hash value of the key.
 */
static inline unsigned int jhash2(const unsigned int *k, unsigned int length, unsigned int initval)
{
	unsigned int a, b, c;

	/* Set up the internal state */
	a = b = c = JHASH_INITVAL + (length<<2) + initval;

	/* Handle most of the key */
	while (length > 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		__jhash_mix(a, b, c);
		length -= 3;
		k += 3;
	}

	/* Handle the last 3 unsigned int's: all the case statements fall through */
	switch (length) {
	case 3: c += k[2];
	case 2: b += k[1];
	case 1: a += k[0];
		__jhash_final(a, b, c);
	case 0:	/* Nothing left to add */
		break;
	}

	return c;
}


/* __jhash_nwords - hash exactly 3, 2 or 1 word(s) */
static inline unsigned int __jhash_nwords(unsigned int a, unsigned int b, unsigned int c, unsigned int initval)
{
	a += initval;
	b += initval;
	c += initval;

	__jhash_final(a, b, c);

	return c;
}

static inline unsigned int jhash_3words(unsigned int a, unsigned int b, unsigned int c, unsigned int initval)
{
	return __jhash_nwords(a, b, c, initval + JHASH_INITVAL + (3 << 2));
}

static inline unsigned int jhash_2words(unsigned int a, unsigned int b, unsigned int initval)
{
	return __jhash_nwords(a, b, 0, initval + JHASH_INITVAL + (2 << 2));
}

static inline unsigned int jhash_1word(unsigned int a, unsigned int initval)
{
	return __jhash_nwords(a, 0, 0, initval + JHASH_INITVAL + (1 << 2));
}

#endif
