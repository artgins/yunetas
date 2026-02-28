/* Copyright (C) 2015-2025 maClara, LLC <info@maclara-llc.com>
   This file is part of the JWT C Library

   SPDX-License-Identifier:  MPL-2.0
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Updated for Mbed TLS v4.0: low-level rsa/ecdsa/entropy/ctr_drbg APIs are
 * removed; use mbedtls_pk_sign_ext / mbedtls_pk_verify_ext (public pk.h) and
 * psa_crypto_init() instead.  HMAC now uses PSA mac API (md HMAC helpers are
 * private in v4.0). */

#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>
#include <psa/crypto.h>
#include <string.h>

#include "../jwt.h"

#include "../jwt-private.h"

#include "jwt-mbedtls.h"

static int mbedtls_sign_sha_hmac(jwt_t *jwt, char **out, unsigned int *len,
				 const char *str, unsigned int str_len)
{
	void *key;
	size_t key_len;
	psa_algorithm_t psa_hash;
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
	uint8_t mac[PSA_MAC_MAX_SIZE];
	size_t mac_len = 0;
	psa_status_t status;

	key = jwt->key->oct.key;
	key_len = jwt->key->oct.len;

	*out = NULL;

	/* Determine the PSA hash algorithm based on jwt->alg */
	switch (jwt->alg) {
	case JWT_ALG_HS256:
		psa_hash = PSA_ALG_SHA_256;
		break;
	case JWT_ALG_HS384:
		psa_hash = PSA_ALG_SHA_384;
		break;
	case JWT_ALG_HS512:
		psa_hash = PSA_ALG_SHA_512;
		break;
	default:
		return 1;
	}

	/* PSA must be initialised before any crypto in v4.0 */
	psa_crypto_init();

	/* Import the HMAC key into the PSA key store */
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attr, PSA_ALG_HMAC(psa_hash));
	psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
	status = psa_import_key(&attr, (const uint8_t *)key, key_len, &key_id);
	if (status != PSA_SUCCESS)
		return 1;

	/* Compute HMAC in one shot */
	status = psa_mac_compute(key_id, PSA_ALG_HMAC(psa_hash),
				 (const uint8_t *)str, str_len,
				 mac, sizeof(mac), &mac_len);
	psa_destroy_key(key_id);

	if (status != PSA_SUCCESS)
		return 1;

	*out = jwt_malloc(mac_len);
	if (*out == NULL)
		return 1;
	memcpy(*out, mac, mac_len);
	*len = (unsigned int)mac_len;

	return 0;
}

#define SIGN_ERROR(_msg) { jwt_write_error(jwt, "JWT[MbedTLS]: " _msg); goto sign_clean_key; }

static int mbedtls_sign_sha_pem(jwt_t *jwt, char **out, unsigned int *len,
				const char *str, unsigned int str_len)
{
	mbedtls_pk_context pk;
	const mbedtls_md_info_t *md_info;
	unsigned char hash[MBEDTLS_MD_MAX_SIZE];
	unsigned char sig[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
	size_t sig_len = 0;

	if (jwt->key->pem == NULL)
		SIGN_ERROR("Key is not compatible"); // LCOV_EXCL_LINE

	/* PSA must be initialised before any crypto in v4.0 */
	psa_crypto_init();

	mbedtls_pk_init(&pk);

	/* Load the private key (f_rng/p_rng removed in v4.0) */
	if (mbedtls_pk_parse_key(&pk, (unsigned char *)jwt->key->pem,
				 strlen(jwt->key->pem) + 1,
				 NULL, 0))
		SIGN_ERROR("Error parsing private key"); // LCOV_EXCL_LINE

	/* Determine the hash algorithm */
	switch (jwt->alg) {
	case JWT_ALG_RS256:
	case JWT_ALG_PS256:
	case JWT_ALG_ES256:
	case JWT_ALG_ES256K:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
		break;
	case JWT_ALG_RS384:
	case JWT_ALG_PS384:
	case JWT_ALG_ES384:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
		break;
	case JWT_ALG_RS512:
	case JWT_ALG_PS512:
	case JWT_ALG_ES512:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
		break;
	default:
		SIGN_ERROR("Unsupported algorithm"); // LCOV_EXCL_LINE
	}

	/* Compute the hash of the input string */
	if (mbedtls_md(md_info, (unsigned char *)str, str_len, hash))
		SIGN_ERROR("Error initializing md context"); // LCOV_EXCL_LINE

	/* Sign: ECDSA produces IEEE P1363 (raw R||S), RSA produces raw sig */
	if (mbedtls_pk_can_do_psa(&pk, PSA_ALG_ECDSA(PSA_ALG_ANY_HASH),
				  PSA_KEY_USAGE_SIGN_HASH)) {
		if (mbedtls_pk_sign_ext(MBEDTLS_PK_SIGALG_ECDSA, &pk,
					mbedtls_md_get_type(md_info),
					hash, mbedtls_md_get_size(md_info),
					sig, sizeof(sig), &sig_len))
			SIGN_ERROR("Error signing token"); // LCOV_EXCL_LINE
	} else if (mbedtls_pk_can_do_psa(&pk, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH),
					 PSA_KEY_USAGE_SIGN_HASH)) {
		switch (jwt->alg) {
		case JWT_ALG_PS256:
		case JWT_ALG_PS384:
		case JWT_ALG_PS512:
			if (mbedtls_pk_sign_ext(MBEDTLS_PK_SIGALG_RSA_PSS, &pk,
						mbedtls_md_get_type(md_info),
						hash, mbedtls_md_get_size(md_info),
						sig, sizeof(sig), &sig_len))
				SIGN_ERROR("Failed signing RSASSA-PSS"); // LCOV_EXCL_LINE
			break;

		case JWT_ALG_RS256:
		case JWT_ALG_RS384:
		case JWT_ALG_RS512:
			if (mbedtls_pk_sign_ext(MBEDTLS_PK_SIGALG_RSA_PKCS1V15, &pk,
						mbedtls_md_get_type(md_info),
						hash, mbedtls_md_get_size(md_info),
						sig, sizeof(sig), &sig_len))
				SIGN_ERROR("Error signing token"); // LCOV_EXCL_LINE
			break;

		default:
			SIGN_ERROR("Unexpected algorithm"); // LCOV_EXCL_LINE
		}
	} else {
		SIGN_ERROR("Unsupported key type"); // LCOV_EXCL_LINE
	}

	*out = jwt_malloc(sig_len);
	if (*out == NULL)
		SIGN_ERROR("Out of memory"); // LCOV_EXCL_LINE
	memcpy(*out, sig, sig_len);
	*len = sig_len;

sign_clean_key:
	mbedtls_pk_free(&pk);

	return jwt->error;
}

#define VERIFY_ERROR(_msg) { jwt_write_error(jwt, "JWT[MbedTLS]: " _msg); goto verify_clean_key; }

static int mbedtls_verify_sha_pem(jwt_t *jwt, const char *head,
				  unsigned int head_len,
				  unsigned char *sig, int sig_len)
{
	mbedtls_pk_context pk;
	unsigned char hash[MBEDTLS_MD_MAX_SIZE];
	const mbedtls_md_info_t *md_info = NULL;
	int ret = 1;

	mbedtls_pk_init(&pk);

	if (jwt->key->pem == NULL)
		return 1;

	/* PSA must be initialised before any crypto in v4.0 */
	psa_crypto_init();

	/* Attempt to parse the key as a public key */
	ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)
					  jwt->key->pem,
					  strlen(jwt->key->pem) + 1);
	if (ret) {
		/* Try loading as private key (f_rng/p_rng removed in v4.0) */
		if (mbedtls_pk_parse_key(&pk, (const unsigned char *)
					   jwt->key->pem,
					   strlen(jwt->key->pem) + 1,
					   NULL, 0))
			VERIFY_ERROR("Failed to parse key"); // LCOV_EXCL_LINE
	}

	/* Determine the hash algorithm */
	switch (jwt->alg) {
	case JWT_ALG_RS256:
	case JWT_ALG_PS256:
	case JWT_ALG_ES256:
	case JWT_ALG_ES256K:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
		break;
	case JWT_ALG_RS384:
	case JWT_ALG_PS384:
	case JWT_ALG_ES384:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
		break;
	case JWT_ALG_RS512:
	case JWT_ALG_PS512:
	case JWT_ALG_ES512:
		md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
		break;
	default:
		VERIFY_ERROR("Unsupported algorithm"); // LCOV_EXCL_LINE
	}

	if (md_info == NULL)
		VERIFY_ERROR("Failed to get hash alg info"); // LCOV_EXCL_LINE

	/* Compute the hash of the input string */
	if ((ret = mbedtls_md(md_info, (const unsigned char *)head, head_len,
			      hash)))
		VERIFY_ERROR("Failed to computer hash"); // LCOV_EXCL_LINE

	/* Verify: ECDSA expects IEEE P1363 (raw R||S), RSA expects raw sig */
	if (mbedtls_pk_can_do_psa(&pk, PSA_ALG_ECDSA(PSA_ALG_ANY_HASH),
				  PSA_KEY_USAGE_VERIFY_HASH)) {
		if (mbedtls_pk_verify_ext(MBEDTLS_PK_SIGALG_ECDSA, &pk,
					  mbedtls_md_get_type(md_info),
					  hash, mbedtls_md_get_size(md_info),
					  sig, sig_len))
			VERIFY_ERROR("Failed to verify signature"); // LCOV_EXCL_LINE
	} else if (mbedtls_pk_can_do_psa(&pk, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH),
					 PSA_KEY_USAGE_VERIFY_HASH)) {
		if (jwt->alg == JWT_ALG_PS256 || jwt->alg == JWT_ALG_PS384 ||
		    jwt->alg == JWT_ALG_PS512) {
			if (mbedtls_pk_verify_ext(MBEDTLS_PK_SIGALG_RSA_PSS, &pk,
						  mbedtls_md_get_type(md_info),
						  hash, mbedtls_md_get_size(md_info),
						  sig, sig_len))
				VERIFY_ERROR("Failed to verify signature"); // LCOV_EXCL_LINE
		} else {
			if (mbedtls_pk_verify_ext(MBEDTLS_PK_SIGALG_RSA_PKCS1V15, &pk,
						  mbedtls_md_get_type(md_info),
						  hash, mbedtls_md_get_size(md_info),
						  sig, sig_len))
				VERIFY_ERROR("Failed to verify signature"); // LCOV_EXCL_LINE
		}
	} else {
		VERIFY_ERROR("Unexpected key typ"); // LCOV_EXCL_LINE
	}

verify_clean_key:
	mbedtls_pk_free(&pk);

	return jwt->error;
}

/* Export our ops */
struct jwt_crypto_ops jwt_mbedtls_ops = {
	.name			= "mbedtls",
	.provider		= JWT_CRYPTO_OPS_MBEDTLS,

	.sign_sha_hmac		= mbedtls_sign_sha_hmac,
	.sign_sha_pem		= mbedtls_sign_sha_pem,
	.verify_sha_pem		= mbedtls_verify_sha_pem,

	.jwk_implemented	= 1,
	.process_eddsa		= mbedtls_process_eddsa,
	.process_rsa		= mbedtls_process_rsa,
	.process_ec		= mbedtls_process_ec,
	.process_item_free	= mbedtls_process_item_free,
};
