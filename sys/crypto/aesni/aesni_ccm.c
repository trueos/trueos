/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * Copyright (c) 2018 iXsystems, Inc
 * All rights reserved.
 *
 * This software was developed by John-Mark Gurney under
 * the sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$FreeBSD$
 *
 * This file implements AES-CCM+CBC-MAC, as described
 * at https://tools.ietf.org/html/rfc3610, using Intel's
 * AES-NI instructions.
 *
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <crypto/aesni/aesni.h>
#include <crypto/aesni/aesni_os.h>
#include <crypto/aesni/aesencdec.h>
#define AESNI_ENC(d, k, nr)	aesni_enc(nr-1, (const __m128i*)k, d)
#else
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <strings.h>
#include <err.h>
#endif

#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

typedef union {
	__m128i	block;
	uint8_t	bytes[sizeof(__m128i)];
} aes_block_t;

#ifndef _KERNEL
static void
panic(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrx(1, fmt, ap);
	va_end(ap);
}
#endif

#ifdef CRYPTO_DEBUG
static void
PrintBlock(const char *label, __m128i b)
{
	uint8_t *ptr = (uint8_t*)&b;
	printf("%s: ", label);
	for (size_t i = 0; i < sizeof(b); i++)
		printf("%02x ", ptr[i]);
	printf("\n");
}
#endif

#ifdef STANDALONE
static void PrintHex(const void *, size_t);
#endif

#ifndef _KERNEL
/*
 * Convenience wrapper to do AES encryption.
 */
static inline __m128i
aes_encrypt(__m128i data, const unsigned char *k, int nr)
{
	int i;
	__m128 retval = data;
	const __m128i *key = (const void*)k;
	retval = _mm_xor_si128(retval, key[0]);
	for (i = 1; i < nr; i++) {
		retval = _mm_aesenc_si128(retval, key[i]);
	}
	retval = _mm_aesenclast_si128(retval, key[nr]);
	return retval;
}
#endif

/*
 * Encrypt a single 128-bit block after
 * doing an xor.  This is also used to
 * decrypt (yay symmetric encryption).
 */
static inline __m128i
xor_and_encrypt(__m128i a, __m128i b, const unsigned char *k, int nr)
{
	__m128 retval = _mm_xor_si128(a, b);
#ifdef CRYPTO_DEBUG
	PrintBlock("\ta\t", a);
	PrintBlock("\tb\t", b);
	PrintBlock("\tresult\t", retval);
#endif
	retval = AESNI_ENC(retval, k, nr);
	return retval;
}

/*
 * put value at the end of block, starting at offset.
 * (This goes backwards, putting bytes in *until* it
 * reaches offset.)
 */
static void
append_int(size_t value, __m128i *block, size_t offset)
{
	int indx = sizeof(*block) - 1;
	uint8_t *bp = (uint8_t*)block;
	while (indx > (sizeof(*block) - offset)) {
		bp[indx] = value & 0xff;
		indx--;
		value >>= 8;
	}
}

/*
 * Start the CBC-MAC process.  This handles the auth data.
 */
static __m128i
cbc_mac_start(const unsigned char *auth_data, size_t auth_len,
	     const unsigned char *nonce, size_t nonce_len,
	     const unsigned char *key, int nr,
	     size_t data_len, size_t tag_len)
{
	aes_block_t  retval, temp_block;
	/* This defines where the message length goes */
	int L = sizeof(__m128i) - 1 - nonce_len;

	/*
	 * Set up B0 here.  This has the flags byte,
	 * followed by the nonce, followed by the
	 * length of the message.
	 */
	retval.block = _mm_setzero_si128();
	retval.bytes[0] = (auth_len ? 1 : 0) * 64 |
		(((tag_len - 2) / 2) * 8) |
		(L - 1);
	bcopy(nonce, &retval.bytes[1], nonce_len);
	append_int(data_len, &retval.block, L+1);
#ifdef CRYPTO_DEBUG
	PrintBlock("Plain B0", retval.block);
#endif
	retval.block = AESNI_ENC(retval.block, key, nr);

	if (auth_len) {
		/*
		 * We need to start by appending the length descriptor.
		 */
		uint32_t auth_amt;
		size_t copy_amt;
		const uint8_t *auth_ptr = auth_data;

		temp_block.block = _mm_setzero_si128();

		if (auth_len < ((1<<16) - (1<<8))) {
			uint16_t *ip = (uint16_t*)&temp_block;
			*ip = htobe16(auth_len);
			auth_amt = 2;
		} else {
			/*
			 * The current calling convention means that
			 * there can never be more than 4g of authentication
			 * data, so we don't handle the 0xffff case.
			 */
			uint32_t *ip = (uint32_t*)&temp_block.bytes[2];
			temp_block.bytes[0] = 0xff;
			temp_block.bytes[1] = 0xfe;
			*ip = htobe32(auth_len);
			auth_amt = 2 + sizeof(*ip);
		}
		/*
		 * Need to copy abytes into blocks.  The first block is
		 * already partially filled, by auth_amt, so we need
		 * to handle that.  The last block needs to be zero padded.
		 */
		copy_amt = MIN(auth_len - auth_amt, sizeof(temp_block) - auth_amt);
		bcopy(auth_ptr, &temp_block.bytes[auth_amt], copy_amt);
		auth_ptr += copy_amt;

		retval.block = xor_and_encrypt(retval.block, temp_block.block, key, nr);
		
		while (auth_ptr < auth_data + auth_len) {
			copy_amt = MIN((auth_data + auth_len) - auth_ptr, sizeof(temp_block));
			if (copy_amt < sizeof(retval))
				bzero(&temp_block, sizeof(temp_block));
			bcopy(auth_ptr, &temp_block, copy_amt);
			retval.block = xor_and_encrypt(retval.block, temp_block.block, key, nr);
			auth_ptr += copy_amt;
		}
	}
	return retval.block;
}

/*
 * Implement AES CCM+CBC-MAC encryption and authentication.
 *
 * A couple of notes:
 * The specification allows for a different number of tag lengths;
 * however, they're always truncated from 16 bytes, and the tag
 * length isn't passed in.  (This could be fixed by changing the
 * code in aesni.c:aesni_cipher_crypt().)
 * Similarly, although the nonce length is passed in, the
 * OpenCrypto API that calls us doesn't have a way to set the nonce
 * other than by having different crypto algorithm types.  As a result,
 * this is currently always called with nlen=12; this means that we
 * also have a maximum message length of 16MBytes.  And similarly,
 * since abyes is limited to a 32 bit value here, the AAD is
 * limited to 4gbytes or less.
 */
void
AES_CCM_encrypt(const unsigned char *in, unsigned char *out,
		const unsigned char *addt, const unsigned char *nonce,
		unsigned char *tag, uint32_t nbytes, uint32_t abytes, int nlen,
		const unsigned char *key, int nr)
{
	static const int tag_length = 16;	/* 128 bits */
	int L;
	int counter = 1;	/* S0 has 0, S1 has 1 */
	size_t copy_amt, total = 0;
	
	aes_block_t s0, last_block, current_block, s_x, temp_block;

	CRYPTDEB("%s(%p, %p, %p, %p, %p, %u, %u, %d, %p, %d)\n",
	       __func__, in, out, addt, nonce, tag, nbytes, abytes, nlen, key, nr);

	if (nbytes == 0)
		return;
	if (nlen < 0 || nlen > 15)
		panic("%s: bad nonce length %d", __FUNCTION__, nlen);

	/*
	 * We need to know how many bytes to use to describe
	 * the length of the data.  Normally, nlen should be
	 * 12, which leaves us 3 bytes to do that -- 16mbytes of
	 * data to encrypt.  But it can be longer or shorter;
	 * this impacts the length of the message.
	 */
	L = sizeof(__m128i) - 1 - nlen;

	/*
	 * Now, this shouldn't happen, but let's make sure that
	 * the data length isn't too big.
	 */
	if (nbytes > ((1 << (8 * L)) - 1))
		panic("%s: nbytes is %u, but length field is %d bytes",
		    __FUNCTION__, nbytes, L);
	/*
	 * Clear out the blocks
	 */
	explicit_bzero(&s0, sizeof(s0));
	explicit_bzero(&current_block, sizeof(current_block));

	last_block.block = cbc_mac_start(addt, abytes, nonce, nlen,
	    key, nr, nbytes, tag_length);

	/* s0 has flags, nonce, and then 0 */
	s0.bytes[0] = L-1;	/* but the flags byte only has L' */
	bcopy(nonce, &s0.bytes[1], nlen);
#ifdef CRYPTO_DEBUG
	PrintBlock("s0", s0.block);
#endif

	/*
	 * Now to cycle through the rest of the data.
	 */
	bcopy(&s0, &s_x, sizeof(s0));

	while (total < nbytes) {
		/*
		 * Copy the plain-text data into temp_block.
		 * This may need to be zero-padded.
		 */
		copy_amt = MIN(nbytes - total, sizeof(temp_block));
		bcopy(in+total, &temp_block, copy_amt);
		if (copy_amt < sizeof(temp_block)) {
			bzero(&temp_block.bytes[copy_amt],
			    sizeof(temp_block) - copy_amt);
		}
#ifdef CRYPTO_DEBUG
		PrintBlock("Plain text", temp_block.block);
#endif
		last_block.block = xor_and_encrypt(last_block.block,
		    temp_block.block, key, nr);
		/* Put the counter into the s_x block */
		append_int(counter++, &s_x.block, L+1);
		/* Encrypt that */
		__m128i X = AESNI_ENC(s_x.block, key, nr);
		/* XOR the plain-text with the encrypted counter block */
		temp_block.block = _mm_xor_si128(temp_block.block, X);
#ifdef CRYPTO_DEBUG
		PrintBlock("Encrypted block", temp_block.block);
#endif
		/* And copy it out */
		bcopy(&temp_block, out+total, copy_amt);
		total += copy_amt;
	}
	/*
	 * Allgedly done with it!  Except for the tag.
	 */
#ifdef CRYPTO_DEBUG
	PrintBlock("Final last block", last_block.block);
#endif
	s0.block = AESNI_ENC(s0.block, key, nr);
	temp_block.block = _mm_xor_si128(s0.block, last_block.block);
#ifdef CRYPTO_DEBUG
	printf("Tag length %d; ", tag_length);
	PrintBlock("Final tag", temp_block.block);
#endif
	bcopy(&temp_block, tag, tag_length);
	return;
}

/*
 * Implement AES CCM+CBC-MAC decryption and authentication.
 * Returns 0 on failure, 1 on success.
 *
 * The primary difference here is that each encrypted block
 * needs to be hashed&encrypted after it is decrypted (since
 * the CBC-MAC is based on the plain text).  This means that
 * we do the decryption twice -- first to verify the tag,
 * and second to decrypt and copy it out.
 *
 * To avoid annoying code copying, we implement the main
 * loop as a separate function.
 *
 * Call with out as NULL to not store the decrypted results;
 * call with hashp as NULL to not run the authentication.
 * Calling with neither as NULL does the decryption and
 * authentication as a single pass (which is not allowed
 * per the specification, really).
 *
 * If hashp is non-NULL, it points to the post-AAD computed
 * checksum.
 */
static void
decrypt_loop(const unsigned char *in, unsigned char *out, size_t nbytes,
    aes_block_t s0, size_t nonce_length, aes_block_t *hashp,
    const unsigned char *key, int nr)
{
	size_t total = 0;
	aes_block_t s_x = s0, hash_block;
	int counter = 1;
	const size_t L = sizeof(__m128i) - 1 - nonce_length;
	__m128i pad_block;

	/*
	 * The starting hash (post AAD, if any).
	 */
	if (hashp)
		hash_block = *hashp;
	
	while (total < nbytes) {
		aes_block_t temp_block;

		size_t copy_amt = MIN(nbytes - total, sizeof(temp_block));
		if (copy_amt < sizeof(temp_block)) {
			temp_block.block = _mm_setzero_si128();
		}
		bcopy(in+total, &temp_block, copy_amt);

		/*
		 * temp_block has the current block of input data,
		 * zero-padded if necessary.  This is used in computing
		 * both the decrypted data, and the authentication hash.
		 */
		append_int(counter++, &s_x.block, L+1);
		/*
		 * The hash is computed based on the decrypted data.
		 */
		pad_block = AESNI_ENC(s_x.block, key, nr);
		if (copy_amt < sizeof(temp_block)) {
			/*
			 * Need to pad out both blocks with 0.
			 */
			uint8_t *end_of_buffer = (uint8_t*)&pad_block;
			bzero(&temp_block.bytes[copy_amt],
			    sizeof(temp_block) - copy_amt);
			bzero(end_of_buffer + copy_amt,
			    sizeof(temp_block) - copy_amt);
		}
		temp_block.block = _mm_xor_si128(temp_block.block,
		    pad_block);

		if (out)
			bcopy(&temp_block, out+total, copy_amt);

		if (hashp)
			hash_block.block = xor_and_encrypt(hash_block.block,
			    temp_block.block, key, nr);
		total += copy_amt;
	}
	explicit_bzero(&pad_block, sizeof(pad_block));

	if (hashp)
		*hashp = hash_block;
	return;
}

/*
 * The exposed decryption routine.  This is practically a
 * copy of the encryption routine, except that the order
 * in which the hash is created is changed.
 * XXX combine the two functions at some point!
 */
int
AES_CCM_decrypt(const unsigned char *in, unsigned char *out,
		const unsigned char *addt, const unsigned char *nonce,
		const unsigned char *tag, uint32_t nbytes, uint32_t abytes, int nlen,
		const unsigned char *key, int nr)
{
	static const int tag_length = 16;	/* 128 bits */
	int L;
	aes_block_t s0, last_block, current_block, s_x, temp_block;
	
	CRYPTDEB("%s(%p, %p, %p, %p, %p, %u, %u, %d, %p, %d)\n",
	       __func__, in, out, addt, nonce, tag, nbytes, abytes, nlen, key, nr);

	if (nbytes == 0)
		return 1;	// No message means no decryption!
	if (nlen < 0 || nlen > 15)
		panic("%s: bad nonce length %d", __FUNCTION__, nlen);

	/*
	 * We need to know how many bytes to use to describe
	 * the length of the data.  Normally, nlen should be
	 * 12, which leaves us 3 bytes to do that -- 16mbytes of
	 * data to encrypt.  But it can be longer or shorter.
	 */
	L = sizeof(__m128i) - 1 - nlen;

	/*
	 * Now, this shouldn't happen, but let's make sure that
	 * the data length isn't too big.
	 */
	if (nbytes > ((1 << (8 * L)) - 1))
		panic("%s: nbytes is %u, but length field is %d bytes",
		      __FUNCTION__, nbytes, L);
	/*
	 * Clear out the blocks
	 */
	s0.block = _mm_setzero_si128();
	current_block = s0;

	last_block.block = cbc_mac_start(addt, abytes, nonce, nlen,
					 key, nr, nbytes, tag_length);
	/* s0 has flags, nonce, and then 0 */
	s0.bytes[0] = L-1;	/* but the flags byte only has L' */
	bcopy(nonce, &s0.bytes[1], nlen);
#ifdef CRYPTO_DEBUG
	PrintBlock("s0", s0.block);
#endif

	/*
	 * Now to cycle through the rest of the data.
	 */
	s_x = s0;

	decrypt_loop(in, NULL, nbytes, s0, nlen, &last_block, key, nr);

	/*
	 * Compare the tag.
	 */
	temp_block.block = _mm_xor_si128(AESNI_ENC(s0.block, key, nr),
	    last_block.block);
	if (bcmp(&temp_block, tag, tag_length) != 0) {
#ifdef CRYPTO_DEBUG
		PrintBlock("Computed tag", temp_block.block);
		PrintBlock("Input tag   ", *(const __m128i*)tag);
#endif
		return 0;
	}

	/*
	 * Push out the decryption results this time.
	 */
	decrypt_loop(in, out, nbytes, s0, nlen, NULL, key, nr);
	return 1;
}

#ifdef STANDALONE
/*
 * Used for testing
 */
/*
 * The hard-coded key expansion for an all-zeroes key.
 */
static uint8_t expanded_zero_key[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 
0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 
0x6f, 0x6c, 0x6c, 0xcf, 0x0d, 0x0f, 0x0f, 0xac, 0x6f, 0x6c, 0x6c, 0xcf, 0x0d, 0x0f, 0x0f, 0xac, 
0x7d, 0x8d, 0x8d, 0x6a, 0xd7, 0x76, 0x76, 0x91, 0x7d, 0x8d, 0x8d, 0x6a, 0xd7, 0x76, 0x76, 0x91, 
0x53, 0x54, 0xed, 0xc1, 0x5e, 0x5b, 0xe2, 0x6d, 0x31, 0x37, 0x8e, 0xa2, 0x3c, 0x38, 0x81, 0x0e, 
0x96, 0x8a, 0x81, 0xc1, 0x41, 0xfc, 0xf7, 0x50, 0x3c, 0x71, 0x7a, 0x3a, 0xeb, 0x07, 0x0c, 0xab, 
0x9e, 0xaa, 0x8f, 0x28, 0xc0, 0xf1, 0x6d, 0x45, 0xf1, 0xc6, 0xe3, 0xe7, 0xcd, 0xfe, 0x62, 0xe9, 
0x2b, 0x31, 0x2b, 0xdf, 0x6a, 0xcd, 0xdc, 0x8f, 0x56, 0xbc, 0xa6, 0xb5, 0xbd, 0xbb, 0xaa, 0x1e, 
0x64, 0x06, 0xfd, 0x52, 0xa4, 0xf7, 0x90, 0x17, 0x55, 0x31, 0x73, 0xf0, 0x98, 0xcf, 0x11, 0x19, 
0x6d, 0xbb, 0xa9, 0x0b, 0x07, 0x76, 0x75, 0x84, 0x51, 0xca, 0xd3, 0x31, 0xec, 0x71, 0x79, 0x2f, 
0xe7, 0xb0, 0xe8, 0x9c, 0x43, 0x47, 0x78, 0x8b, 0x16, 0x76, 0x0b, 0x7b, 0x8e, 0xb9, 0x1a, 0x62, 
0x74, 0xed, 0x0b, 0xa1, 0x73, 0x9b, 0x7e, 0x25, 0x22, 0x51, 0xad, 0x14, 0xce, 0x20, 0xd4, 0x3b, 
0x10, 0xf8, 0x0a, 0x17, 0x53, 0xbf, 0x72, 0x9c, 0x45, 0xc9, 0x79, 0xe7, 0xcb, 0x70, 0x63, 0x85, 
};

static uint8_t expanded_zero_key_2[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 0x62, 0x63, 0x63, 0x63, 
0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 0xaa, 0xfb, 0xfb, 0xfb, 
0x6f, 0x6c, 0x6c, 0xcf, 0x0d, 0x0f, 0x0f, 0xac, 0x6f, 0x6c, 0x6c, 0xcf, 0x0d, 0x0f, 0x0f, 0xac, 
0x7d, 0x8d, 0x8d, 0x6a, 0xd7, 0x76, 0x76, 0x91, 0x7d, 0x8d, 0x8d, 0x6a, 0xd7, 0x76, 0x76, 0x91, 
0x53, 0x54, 0xed, 0xc1, 0x5e, 0x5b, 0xe2, 0x6d, 0x31, 0x37, 0x8e, 0xa2, 0x3c, 0x38, 0x81, 0x0e, 
0x96, 0x8a, 0x81, 0xc1, 0x41, 0xfc, 0xf7, 0x50, 0x3c, 0x71, 0x7a, 0x3a, 0xeb, 0x07, 0x0c, 0xab, 
0x9e, 0xaa, 0x8f, 0x28, 0xc0, 0xf1, 0x6d, 0x45, 0xf1, 0xc6, 0xe3, 0xe7, 0xcd, 0xfe, 0x62, 0xe9, 
0x2b, 0x31, 0x2b, 0xdf, 0x6a, 0xcd, 0xdc, 0x8f, 0x56, 0xbc, 0xa6, 0xb5, 0xbd, 0xbb, 0xaa, 0x1e, 
0x64, 0x06, 0xfd, 0x52, 0xa4, 0xf7, 0x90, 0x17, 0x55, 0x31, 0x73, 0xf0, 0x98, 0xcf, 0x11, 0x19, 
0x6d, 0xbb, 0xa9, 0x0b, 0x07, 0x76, 0x75, 0x84, 0x51, 0xca, 0xd3, 0x31, 0xec, 0x71, 0x79, 0x2f, 
0xe7, 0xb0, 0xe8, 0x9c, 0x43, 0x47, 0x78, 0x8b, 0x16, 0x76, 0x0b, 0x7b, 0x8e, 0xb9, 0x1a, 0x62, 
0x74, 0xed, 0x0b, 0xa1, 0x73, 0x9b, 0x7e, 0x25, 0x22, 0x51, 0xad, 0x14, 0xce, 0x20, 0xd4, 0x3b, 
0x10, 0xf8, 0x0a, 0x17, 0x53, 0xbf, 0x72, 0x9c, 0x45, 0xc9, 0x79, 0xe7, 0xcb, 0x70, 0x63, 0x85, 
};
static void
PrintHex(const void *bytes, size_t len)
{
	const uint8_t *b = bytes;
	for (size_t x = 0; x < len; x++)
		printf("%02x ", b[x]);
	printf("\n");
	return;
}

int
main(int ac, char **av)
{
	uint8_t tag[16];
	uint8_t nonce[12] = { 0 };
	unsigned char aad[] = "How now brown cow";
//	unsigned char plain[] = "Four score and seven years ago, our forefathers brought Bill & Ted";
	unsigned char plain[4] = "abcd";
	unsigned char crypt[sizeof(plain)];
	unsigned char decrypted[sizeof(plain)];
	uint8_t key[256 / 8] = { 0 };
	int nrounds = 14;	// For a 256-bit key, use 14 rounds
	int rv;
	
	printf("Plaintext: "); PrintHex(plain, sizeof(plain));
	printf("aad size = %zx, nonce size = %zx, tag size = %zx\n", sizeof(aad), sizeof(nonce), sizeof(tag));
	AES_CCM_encrypt(plain, crypt, aad, nonce, tag,
			sizeof(plain), sizeof(aad), sizeof(nonce),
			(const unsigned char *)expanded_zero_key, nrounds);
	printf("Tag: "); PrintHex(tag, sizeof(tag));
	printf("Crypt: "); PrintHex(crypt, sizeof(crypt));
	
	rv = AES_CCM_decrypt(crypt, decrypted, aad, nonce, tag,
			sizeof(plain), sizeof(aad), sizeof(nonce),
			(const unsigned char *)expanded_zero_key_2, nrounds);
	printf("%s Decrypted: ", rv == 1 ? "Successfully" : "Unsuccessfully");
	PrintHex(decrypted, sizeof(decrypted));
		
	return 0;
}
#endif
