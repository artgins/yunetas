#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

void base64url_encode(const unsigned char *input, int length, char **output) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, bio);

    BIO_write(b64, input, length);
    (void)BIO_flush(b64);  // suppress unused-value warning

    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);

    *output = malloc(bptr->length + 1);
    memcpy(*output, bptr->data, bptr->length);
    (*output)[bptr->length] = '\0';

    for(int i = 0; (*output)[i]; i++) {
        if((*output)[i] == '+') (*output)[i] = '-';
        else if((*output)[i] == '/') (*output)[i] = '_';
    }

    int len = strlen(*output);
    while(len > 0 && (*output)[len - 1] == '=') {
        (*output)[--len] = '\0';
    }

    BIO_free_all(b64);
}

unsigned char *base64_decode(const char *b64input, int *out_len) {
    int len = strlen(b64input);
    char *tmp = malloc(len + 5);
    strcpy(tmp, b64input);

    for(int i = 0; i < len; i++) {
        if(tmp[i] == '-') tmp[i] = '+';
        else if(tmp[i] == '_') tmp[i] = '/';
    }

    int pad = (4 - (len % 4)) % 4;
    for(int i = 0; i < pad; i++) strcat(tmp, "=");

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new_mem_buf(tmp, -1);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    unsigned char *buffer = malloc(4096);
    *out_len = BIO_read(bio, buffer, 4096);

    BIO_free_all(bio);
    free(tmp);
    return buffer;
}

void sha256_kid(const char *n_b64url, const char *e_b64url, char **out_kid) {
    char json[2048];
    snprintf(json, sizeof(json),
        "{\"e\":\"%s\",\"kty\":\"RSA\",\"n\":\"%s\"}", e_b64url, n_b64url);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)json, strlen(json), hash);

    base64url_encode(hash, SHA256_DIGEST_LENGTH, out_kid);
}

void print_jwks_from_key(const char *b64pkey) {
    int der_len;
    unsigned char *der = base64_decode(b64pkey, &der_len);

    EVP_PKEY *pkey = d2i_PublicKey(EVP_PKEY_RSA, NULL, (const unsigned char **)&der, der_len);
    if(!pkey) {
        fprintf(stderr, "Invalid public key DER\n");
        exit(1);
    }

    BIGNUM *n = NULL, *e = NULL;
    EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &n);
    EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e);

    int nlen = BN_num_bytes(n);
    int elen = BN_num_bytes(e);
    unsigned char *nbuf = malloc(nlen);
    unsigned char *ebuf = malloc(elen);
    BN_bn2bin(n, nbuf);
    BN_bn2bin(e, ebuf);

    char *n_b64url = NULL, *e_b64url = NULL, *kid = NULL;
    base64url_encode(nbuf, nlen, &n_b64url);
    base64url_encode(ebuf, elen, &e_b64url);
    sha256_kid(n_b64url, e_b64url, &kid);

    printf("{\n");
    printf("  \"keys\": [\n");
    printf("    {\n");
    printf("      \"kty\": \"RSA\",\n");
    printf("      \"use\": \"sig\",\n");
    printf("      \"alg\": \"RS256\",\n");
    printf("      \"kid\": \"%s\",\n", kid);
    printf("      \"n\": \"%s\",\n", n_b64url);
    printf("      \"e\": \"%s\"\n", e_b64url);
    printf("    }\n");
    printf("  ]\n");
    printf("}\n");

    BN_free(n);
    BN_free(e);
    free(nbuf);
    free(ebuf);
    free(n_b64url);
    free(e_b64url);
    free(kid);
    EVP_PKEY_free(pkey);
    free(der);
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <base64_pkey>\n", argv[0]);
        return 1;
    }

    print_jwks_from_key(argv[1]);
    return 0;
}
