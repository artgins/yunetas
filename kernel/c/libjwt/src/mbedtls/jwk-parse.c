/* Copyright (C) 2015-2025 maClara, LLC <info@maclara-llc.com>
   This file is part of the JWT C Library

   SPDX-License-Identifier:  MPL-2.0
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* mbedTLS v4.0 JWK parsing:
 * - RSA public keys: decode n/e from JWK, build SubjectPublicKeyInfo DER
 *   using mbedtls_asn1_write_* helpers, parse with mbedtls_pk_parse_public_key,
 *   then export to PEM for storage in item->pem.
 * - EC and EdDSA remain unimplemented stubs. */

#include <string.h>

#include <mbedtls/pk.h>
#include <mbedtls/asn1write.h>
#include <psa/crypto.h>

#include "../jwt.h"
#include "../jwt-private.h"
#include "jwt-mbedtls.h"

JWT_NO_EXPORT
int mbedtls_process_eddsa(json_t *jwk, jwk_item_t *item)
{
	jwt_write_error(item, "MBedTLS does not yet implement EdDSA JWK");
	return -1;
}

JWT_NO_EXPORT
int mbedtls_process_rsa(json_t *jwk, jwk_item_t *item)
{
	/* OID for rsaEncryption: 1.2.840.113549.1.1.1 */
	static const char oid_rsa[] = "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01";

	json_t *jn, *je;
	unsigned char *bin_n = NULL, *bin_e = NULL;
	int len_n = 0, len_e = 0;
	int ret = -1;

	/* Require n (modulus) and e (exponent) */
	jn = json_object_get(jwk, "n");
	je = json_object_get(jwk, "e");

	if (jn == NULL || je == NULL ||
	    !json_is_string(jn) || !json_is_string(je)) {
		jwt_write_error(item, "Missing or invalid RSA components n or e");
		goto cleanup;
	}

	bin_n = jwt_base64uri_decode(json_string_value(jn), &len_n);
	bin_e = jwt_base64uri_decode(json_string_value(je), &len_e);

	if (bin_n == NULL || len_n <= 0 || bin_e == NULL || len_e <= 0) {
		jwt_write_error(item, "Failed to decode RSA n or e from base64url");
		goto cleanup;
	}

	{
		/*
		 * Step 1: Build RSAPublicKey DER (PKCS#1):
		 *   SEQUENCE { INTEGER n, INTEGER e }
		 *
		 * mbedtls_asn1_write_* works backwards from the end of the buffer.
		 * Each call moves the pointer left and writes before prior content.
		 */
		unsigned char rsa_buf[4096];
		unsigned char *rp = rsa_buf + sizeof(rsa_buf);
		size_t rsa_seq_len;

		unsigned char *rsa_der;
		size_t rsa_der_len;

		/* Write e (written first — goes at higher address, i.e. end) */
		ret = mbedtls_asn1_write_integer(&rp, rsa_buf,
						 (const unsigned char *)bin_e,
						 (size_t)len_e);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write e failed");
			goto cleanup;
		}

		/* Write n */
		ret = mbedtls_asn1_write_integer(&rp, rsa_buf,
						 (const unsigned char *)bin_n,
						 (size_t)len_n);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write n failed");
			goto cleanup;
		}

		/* Wrap both INTEGERs in a SEQUENCE */
		rsa_seq_len = (size_t)(rsa_buf + sizeof(rsa_buf) - rp);
		ret = mbedtls_asn1_write_len(&rp, rsa_buf, rsa_seq_len);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write SEQUENCE len failed");
			goto cleanup;
		}
		ret = mbedtls_asn1_write_tag(&rp, rsa_buf,
					     MBEDTLS_ASN1_CONSTRUCTED |
					     MBEDTLS_ASN1_SEQUENCE);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write SEQUENCE tag failed");
			goto cleanup;
		}

		rsa_der = rp;
		rsa_der_len = (size_t)(rsa_buf + sizeof(rsa_buf) - rp);

		/*
		 * Step 2: Build SubjectPublicKeyInfo DER (RFC 5480):
		 *   SEQUENCE {
		 *     SEQUENCE { OID(rsaEncryption), NULL },
		 *     BIT STRING { RSAPublicKey }
		 *   }
		 *
		 * spki_buf is separate from rsa_buf so that
		 * mbedtls_asn1_write_bitstring can safely copy from rsa_der.
		 */
		unsigned char spki_buf[4096 + 64];
		unsigned char *sp = spki_buf + sizeof(spki_buf);
		size_t spki_outer_len;

		unsigned char *spki_der;
		size_t spki_der_len;

		/* BIT STRING wrapping RSAPublicKey (0 unused bits) */
		ret = mbedtls_asn1_write_bitstring(&sp, spki_buf,
						   rsa_der,
						   rsa_der_len * 8);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write BIT STRING failed");
			goto cleanup;
		}

		/* AlgorithmIdentifier: SEQUENCE { OID(rsaEncryption), NULL } */
		ret = mbedtls_asn1_write_algorithm_identifier(&sp, spki_buf,
							      oid_rsa,
							      sizeof(oid_rsa) - 1,
							      0);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write AlgorithmIdentifier failed");
			goto cleanup;
		}

		/* Outer SEQUENCE */
		spki_outer_len = (size_t)(spki_buf + sizeof(spki_buf) - sp);
		ret = mbedtls_asn1_write_len(&sp, spki_buf, spki_outer_len);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write outer SEQUENCE len failed");
			goto cleanup;
		}
		ret = mbedtls_asn1_write_tag(&sp, spki_buf,
					     MBEDTLS_ASN1_CONSTRUCTED |
					     MBEDTLS_ASN1_SEQUENCE);
		if (ret < 0) {
			jwt_write_error(item, "ASN.1 write outer SEQUENCE tag failed");
			goto cleanup;
		}

		spki_der = sp;
		spki_der_len = (size_t)(spki_buf + sizeof(spki_buf) - sp);

		/*
		 * Step 3: Parse the SubjectPublicKeyInfo DER into a pk context,
		 * obtain the bit length, and export to PEM for storage.
		 */
		mbedtls_pk_context pk;
		mbedtls_pk_init(&pk);

		psa_crypto_init();

		ret = mbedtls_pk_parse_public_key(&pk, spki_der, spki_der_len);
		if (ret != 0) {
			jwt_write_error(item, "Failed to parse RSA public key DER");
			mbedtls_pk_free(&pk);
			goto cleanup;
		}

		item->bits    = mbedtls_pk_get_bitlen(&pk);
		item->provider = JWT_CRYPTO_OPS_MBEDTLS;

		unsigned char pem_buf[4096];
		ret = mbedtls_pk_write_pubkey_pem(&pk, pem_buf, sizeof(pem_buf));
		mbedtls_pk_free(&pk);
		if (ret != 0) {
			jwt_write_error(item, "Failed to export RSA public key to PEM");
			goto cleanup;
		}

		size_t pem_len = strlen((char *)pem_buf);
		item->pem = jwt_malloc(pem_len + 1);
		if (item->pem == NULL) {
			jwt_write_error(item, "Out of memory for RSA PEM");
			goto cleanup;
		}
		memcpy(item->pem, pem_buf, pem_len + 1);
		ret = 0;
	}

cleanup:
	jwt_freemem(bin_n);
	jwt_freemem(bin_e);
	return ret;
}

JWT_NO_EXPORT
int mbedtls_process_ec(json_t *jwk, jwk_item_t *item)
{
	jwt_write_error(item, "MBedTLS does not yet implement EC JWK");
	return -1;
}

JWT_NO_EXPORT
void mbedtls_process_item_free(jwk_item_t *item)
{
	if (item == NULL || item->provider != JWT_CRYPTO_OPS_MBEDTLS)
		return;

	jwt_freemem(item->pem);
	item->pem = NULL;
	item->provider = JWT_CRYPTO_OPS_NONE;
}
