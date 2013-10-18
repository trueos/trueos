/*-
 * Copyright (c) 2005-2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2012 Gleb Kurtsou <gleb@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/malloc.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <opencrypto/cryptodev.h>

#include <fs/pefs/pefs_hmac.h>

#ifndef _KERNEL
#define	panic(...)	do {						\
	fprintf(stderr, __VA_ARGS__);					\
	abort();							\
} while (0)
#endif

typedef void pefs_hmac_hash_init_t(union pefs_hmac_hash_ctx *ctx);
typedef void pefs_hmac_hash_update_t(union pefs_hmac_hash_ctx *ctx,
    const uint8_t *, size_t);
typedef void pefs_hmac_hash_final_t(uint8_t *, union pefs_hmac_hash_ctx *ctx);

struct pefs_hmac_hash {
	u_int			block_len;
	u_int			digest_len;
	pefs_hmac_hash_init_t	*init;
	pefs_hmac_hash_update_t	*update;
	pefs_hmac_hash_final_t	*final;
};

static const struct pefs_hmac_hash pefs_hmac_hash_sha256 = {
	.block_len =	SHA256_BLOCK_LENGTH,
	.digest_len =	SHA256_DIGEST_LENGTH,
	.init =		(pefs_hmac_hash_init_t *)&SHA256_Init,
	.update =	(pefs_hmac_hash_update_t *)&SHA256_Update,
	.final =	(pefs_hmac_hash_final_t *)&SHA256_Final,
};

static const struct pefs_hmac_hash pefs_hmac_hash_sha384 = {
	.block_len =	SHA384_BLOCK_LENGTH,
	.digest_len =	SHA384_DIGEST_LENGTH,
	.init =		(pefs_hmac_hash_init_t *)&SHA384_Init,
	.update =	(pefs_hmac_hash_update_t *)&SHA384_Update,
	.final =	(pefs_hmac_hash_final_t *)&SHA384_Final,
};

static const struct pefs_hmac_hash pefs_hmac_hash_sha512 = {
	.block_len =	SHA512_BLOCK_LENGTH,
	.digest_len =	SHA512_DIGEST_LENGTH,
	.init =		(pefs_hmac_hash_init_t *)&SHA512_Init,
	.update =	(pefs_hmac_hash_update_t *)&SHA512_Update,
	.final =	(pefs_hmac_hash_final_t *)&SHA512_Final,
};

void
pefs_hmac_init(struct pefs_hmac_ctx *ctx, int algo, const uint8_t *hkey,
    size_t hkeylen)
{
	const struct pefs_hmac_hash *hash;
	u_int i;

	switch (algo) {
	case CRYPTO_SHA2_256_HMAC:
		hash = &pefs_hmac_hash_sha256;
		break;
	case CRYPTO_SHA2_384_HMAC:
		hash = &pefs_hmac_hash_sha384;
		break;
	case CRYPTO_SHA2_512_HMAC:
		hash = &pefs_hmac_hash_sha512;
		break;
	default:
		panic("HMAC: invalid alorithm: %d.", algo);
		return;
	}

	ctx->hash = hash;
	bzero(ctx->k_opad, hash->block_len);
	if (hkeylen == 0)
		; /* do nothing */
	else if (hkeylen <= hash->block_len)
		bcopy(hkey, ctx->k_opad, hkeylen);
	else {
		/*
		 * If key is longer than HMAC_BLOCK_LENGTH_MAX bytes
		 * reset it to key = HASH(key).
		 */
		hash->init(&ctx->hash_ctx);
		hash->update(&ctx->hash_ctx, hkey, hkeylen);
		hash->final(ctx->k_opad, &ctx->hash_ctx);
	}

	/* Perform inner SHA512. */
	hash->init(&ctx->hash_ctx);
	/* XOR key ipad value. */
	for (i = 0; i < hash->block_len; i++)
		ctx->k_opad[i] ^= 0x36;
	hash->update(&ctx->hash_ctx, ctx->k_opad, hash->block_len);
	/* XOR key opad value. */
	for (i = 0; i < hash->block_len; i++)
		ctx->k_opad[i] ^= 0x36 ^ 0x5c;
}

void
pefs_hmac_update(struct pefs_hmac_ctx *ctx, const uint8_t *data,
    size_t datasize)
{

	ctx->hash->update(&ctx->hash_ctx, data, datasize);
}

void
pefs_hmac_final(struct pefs_hmac_ctx *ctx, uint8_t *md, size_t mdsize)
{
	const struct pefs_hmac_hash *hash = ctx->hash;
	u_char digest[PEFS_HMAC_DIGEST_LENGTH_MAX];

	if (mdsize == 0 || mdsize > hash->digest_len) {
		panic("HMAC: invalid digest buffer size: %zu (digest length %u).",
		    mdsize, hash->digest_len);
		return;
	}

	hash->final(digest, &ctx->hash_ctx);
	/* Perform outer SHA512. */
	hash->init(&ctx->hash_ctx);
	hash->update(&ctx->hash_ctx, ctx->k_opad, hash->block_len);
	hash->update(&ctx->hash_ctx, digest, sizeof(digest));
	hash->final(digest, &ctx->hash_ctx);
	bzero(ctx, sizeof(*ctx));

	bcopy(digest, md, mdsize);
}

void
pefs_hmac(int algo, const uint8_t *hkey, size_t hkeysize, const uint8_t *data,
    size_t datasize, uint8_t *md, size_t mdsize)
{
	struct pefs_hmac_ctx ctx;

	pefs_hmac_init(&ctx, algo, hkey, hkeysize);
	pefs_hmac_update(&ctx, data, datasize);
	/* mdsize == 0 means "Give me the whole hash!" */
	if (mdsize == 0)
		mdsize = ctx.hash->digest_len;
	pefs_hmac_final(&ctx, md, mdsize);
}
