#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>
#include <openssl/bn.h>
#include <openssl/err.h>

/* Base64url encode: no padding, replace '+' → '-', '/' → '_' */
static char *base64url_encode(const unsigned char *data, size_t len)
{
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, data, len);
    (void)BIO_flush(b64);  // fix warning: cast return to void

    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);

    char *tmp = malloc(bptr->length + 1);
    memcpy(tmp, bptr->data, bptr->length);
    tmp[bptr->length] = 0;
    BIO_free_all(b64);

    // base64url: + → -, / → _, remove =
    for(char *p = tmp; *p; ++p) {
        if(*p == '+') *p = '-';
        else if(*p == '/') *p = '_';
    }
    while(tmp[strlen(tmp) - 1] == '=') tmp[strlen(tmp) - 1] = 0;

    return tmp;
}

int main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <base64_der_rsa_key>\n", argv[0]);
        return 1;
    }

    const char *base64 = argv[1];

    // Decode base64
    BIO *bio = BIO_new_mem_buf(base64, -1);
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    unsigned char der[4096];
    int der_len = BIO_read(bio, der, sizeof(der));
    BIO_free_all(bio);

    if(der_len <= 0) {
        fprintf(stderr, "Error decoding base64 input\n");
        return 1;
    }

    const unsigned char *p = der;
    EVP_PKEY *pkey = d2i_PUBKEY(NULL, &p, der_len);
    if(!pkey) {
        fprintf(stderr, "Error parsing DER public key: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        return 1;
    }

    // Extract n and e via OpenSSL 3.0 API
    BIGNUM *n = NULL;
    BIGNUM *e = NULL;
    if(!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &n) ||
       !EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e)) {
        fprintf(stderr, "Error extracting RSA parameters\n");
        EVP_PKEY_free(pkey);
        return 1;
    }

    int n_len = BN_num_bytes(n);
    int e_len = BN_num_bytes(e);
    unsigned char *n_bin = malloc(n_len);
    unsigned char *e_bin = malloc(e_len);
    BN_bn2bin(n, n_bin);
    BN_bn2bin(e, e_bin);

    char *n_b64url = base64url_encode(n_bin, n_len);
    char *e_b64url = base64url_encode(e_bin, e_len);

    printf("{\n");
    printf("  \"kty\": \"RSA\",\n");
    printf("  \"alg\": \"RS256\",\n");
    printf("  \"use\": \"sig\",\n");
    printf("  \"n\": \"%s\",\n", n_b64url);
    printf("  \"e\": \"%s\"\n", e_b64url);
    printf("}\n");

    free(n_bin);
    free(e_bin);
    free(n_b64url);
    free(e_b64url);
    BN_free(n);
    BN_free(e);
    EVP_PKEY_free(pkey);
    return 0;
}
