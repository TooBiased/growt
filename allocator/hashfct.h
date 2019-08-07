/*******************************************************************************
 * utils/hashfct.h
 *
 * Some hash functions -- the used hash function is chosen at compile time
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef HASHFCT_H
#define HASHFCT_H

#include <stdint.h>

#if (! (defined(CRC)     || \
        defined(MURMUR2) || \
        defined(MURMUR3) || \
        defined(XXHASH)) )
#define MURMUR2
#endif // NO HASH DEFINED

#ifdef CRC
#define HASHFCT crc_hasher
struct crc_hasher
{
    static const size_t significant_digits = 64;

    inline uint64_t operator()(const uint64_t& k) const {
        return uint64_t( __builtin_ia32_crc32di(1329235987123598723ull, k)
                      | (__builtin_ia32_crc32di(1383568923875084501ull, k) << 32));
    }
};
#endif //CRC


#ifdef MURMUR2
#define HASHFCT murmur2_hasher
struct murmur2_hasher
{
    static const size_t significant_digits = 64;

    inline uint64_t MurmurHash64A ( const void * key, int len, unsigned int seed ) const
    {
	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t * data = (const uint64_t *)key;
	const uint64_t * end = data + (len/8);

	while(data != end)
	{
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch(len & 7)
	{
	case 7: h ^= uint64_t(data2[6]) << 48;
	case 6: h ^= uint64_t(data2[5]) << 40;
	case 5: h ^= uint64_t(data2[4]) << 32;
	case 4: h ^= uint64_t(data2[3]) << 24;
	case 3: h ^= uint64_t(data2[2]) << 16;
	case 2: h ^= uint64_t(data2[1]) << 8;
	case 1: h ^= uint64_t(data2[0]);
	        h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
    }

    inline uint64_t operator()(const uint64_t k) const
    {
        auto local = k;
        return MurmurHash64A(&local, 8, 12039890u);
    }
};
#endif // MURMUR


#ifdef MURMUR3
// We include the cpp to avoid generating another compile unit
#include "MurmurHash3.cpp"
#define HASHFCT murmur3_hasher
struct murmur3_hasher
{
    static const size_t significant_digits = 64;

    inline uint64_t operator()(const uint64_t k) const
    {
        uint64_t local = k;
        uint64_t target[2];

        MurmurHash3_x64_128 (&local, 8, 12039890u, target);

        return target[0];
    }
};
#endif // MURMUR3



#ifdef XXHASH
#define XXH_PRIVATE_API
#include "xxhash.h"
#define HASHFCT xx_hasher
struct xx_hasher
{
    static const size_t significant_digits = 64;

    inline uint64_t operator()(const uint64_t k) const
    {
        auto local = k;
        return XXH64 (&local, 8, 1383568923875084501ull);
    }
};
#endif // XXHASH

#endif // HASHFCT_H
