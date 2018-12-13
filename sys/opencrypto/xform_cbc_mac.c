#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <opencrypto/ccm-cbc.h>
#include <opencrypto/xform_auth.h>

/* Authentication instances */
struct auth_hash auth_hash_ccm_cbc_mac_128 = {
	CRYPTO_AES_128_CCM_CBC_MAC, "CBC-CCM-AES-128",
	AES_128_CBC_MAC_KEY_LEN, AES_CBC_MAC_HASH_LEN, sizeof(struct aes_cbc_mac_ctx),
	CCM_CBC_BLOCK_LEN,
	(void (*)(void *)) AES_CBC_MAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Update,
	(void (*)(u_int8_t *, void *)) AES_CBC_MAC_Final
};
struct auth_hash auth_hash_ccm_cbc_mac_192 = {
	CRYPTO_AES_192_CCM_CBC_MAC, "CBC-CCM-AES-192",
	AES_192_CBC_MAC_KEY_LEN, AES_CBC_MAC_HASH_LEN, sizeof(struct aes_cbc_mac_ctx),
	CCM_CBC_BLOCK_LEN,
	(void (*)(void *)) AES_CBC_MAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Update,
	(void (*)(u_int8_t *, void *)) AES_CBC_MAC_Final
};
struct auth_hash auth_hash_ccm_cbc_mac_256 = {
	CRYPTO_AES_256_CCM_CBC_MAC, "CBC-CCM-AES-256",
	AES_256_CBC_MAC_KEY_LEN, AES_CBC_MAC_HASH_LEN, sizeof(struct aes_cbc_mac_ctx),
	CCM_CBC_BLOCK_LEN,
	(void (*)(void *)) AES_CBC_MAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_CBC_MAC_Update,
	(void (*)(u_int8_t *, void *)) AES_CBC_MAC_Final
};
