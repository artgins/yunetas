# libjwt (JWT Authentication)

Yuneta-adapted fork of the **libjwt** C library for JSON Web Tokens,
JSON Web Keys (JWK), and JWK Sets (JWKS).

The upstream libjwt changed its API and removed functions required by
Yuneta, so this is a maintained fork tailored to Yuneta's needs.

**Source:** `kernel/c/libjwt/src/jwt.h`

## Core

(jwt_get_alg)=
### `jwt_get_alg()`

```C
jwt_alg_t jwt_get_alg(const jwt_t *jwt);
```

---

(jwt_alg_str)=
### `jwt_alg_str()`

```C
const char *jwt_alg_str(jwt_alg_t alg);
```

---

(jwt_str_alg)=
### `jwt_str_alg()`

```C
jwt_alg_t jwt_str_alg(const char *alg);
```

---

(jwt_set_alloc)=
### `jwt_set_alloc()`

```C
int jwt_set_alloc(jwt_malloc_t pmalloc, jwt_free_t pfree);
```

---

(jwt_get_alloc)=
### `jwt_get_alloc()`

```C
void jwt_get_alloc(jwt_malloc_t *pmalloc, jwt_free_t *pfree);
```

---

(jwt_get_crypto_ops)=
### `jwt_get_crypto_ops()`

```C
const char *jwt_get_crypto_ops(void);
```

---

(jwt_get_crypto_ops_t)=
### `jwt_get_crypto_ops_t()`

```C
jwt_crypto_provider_t jwt_get_crypto_ops_t(void);
```

---

(jwt_set_crypto_ops)=
### `jwt_set_crypto_ops()`

```C
int jwt_set_crypto_ops(const char *opname);
```

---

(jwt_set_crypto_ops_t)=
### `jwt_set_crypto_ops_t()`

```C
int jwt_set_crypto_ops_t(jwt_crypto_provider_t opname);
```

---

(jwt_crypto_ops_supports_jwk)=
### `jwt_crypto_ops_supports_jwk()`

```C
int jwt_crypto_ops_supports_jwk(void);
```

---

(jwt_init)=
### `jwt_init()`

```C
void jwt_init(void);
```

---

## Builder

(jwt_builder_new)=
### `jwt_builder_new()`

```C
jwt_builder_t *jwt_builder_new(void);
```

---

(jwt_builder_free)=
### `jwt_builder_free()`

```C
void jwt_builder_free(jwt_builder_t *builder);
```

---

(jwt_builder_error)=
### `jwt_builder_error()`

```C
int jwt_builder_error(const jwt_builder_t *builder);
```

---

(jwt_builder_error_msg)=
### `jwt_builder_error_msg()`

```C
const char *jwt_builder_error_msg(const jwt_builder_t *builder);
```

---

(jwt_builder_error_clear)=
### `jwt_builder_error_clear()`

```C
void jwt_builder_error_clear(jwt_builder_t *builder);
```

---

(jwt_builder_setkey)=
### `jwt_builder_setkey()`

```C
int jwt_builder_setkey(jwt_builder_t *builder, const jwt_alg_t alg, const jwk_item_t *key);
```

---

(jwt_builder_enable_iat)=
### `jwt_builder_enable_iat()`

```C
int jwt_builder_enable_iat(jwt_builder_t *builder, int enable);
```

---

(jwt_builder_setcb)=
### `jwt_builder_setcb()`

```C
int jwt_builder_setcb(jwt_builder_t *builder, jwt_callback_t cb, void *ctx);
```

---

(jwt_builder_getctx)=
### `jwt_builder_getctx()`

```C
void *jwt_builder_getctx(jwt_builder_t *builder);
```

---

(jwt_builder_generate)=
### `jwt_builder_generate()`

```C
char *jwt_builder_generate(jwt_builder_t *builder);
```

---

(jwt_builder_header_set)=
### `jwt_builder_header_set()`

```C
jwt_value_error_t jwt_builder_header_set(jwt_builder_t *builder, jwt_value_t *value);
```

---

(jwt_builder_header_get)=
### `jwt_builder_header_get()`

```C
jwt_value_error_t jwt_builder_header_get(jwt_builder_t *builder, jwt_value_t *value);
```

---

(jwt_builder_header_del)=
### `jwt_builder_header_del()`

```C
jwt_value_error_t jwt_builder_header_del(jwt_builder_t *builder, const char *header);
```

---

(jwt_builder_claim_set)=
### `jwt_builder_claim_set()`

```C
jwt_value_error_t jwt_builder_claim_set(jwt_builder_t *builder, jwt_value_t *value);
```

---

(jwt_builder_claim_get)=
### `jwt_builder_claim_get()`

```C
jwt_value_error_t jwt_builder_claim_get(jwt_builder_t *builder, jwt_value_t *value);
```

---

(jwt_builder_claim_del)=
### `jwt_builder_claim_del()`

```C
jwt_value_error_t jwt_builder_claim_del(jwt_builder_t *builder, const char *claim);
```

---

(jwt_builder_time_offset)=
### `jwt_builder_time_offset()`

```C
int jwt_builder_time_offset(jwt_builder_t *builder, jwt_claims_t claim, time_t secs);
```

---

## Checker

(jwt_checker_new)=
### `jwt_checker_new()`

```C
jwt_checker_t *jwt_checker_new(void);
```

---

(jwt_checker_free)=
### `jwt_checker_free()`

```C
void jwt_checker_free(jwt_checker_t *checker);
```

---

(jwt_checker_error)=
### `jwt_checker_error()`

```C
int jwt_checker_error(const jwt_checker_t *checker);
```

---

(jwt_checker_error_msg)=
### `jwt_checker_error_msg()`

```C
const char *jwt_checker_error_msg(const jwt_checker_t *checker);
```

---

(jwt_checker_error_clear)=
### `jwt_checker_error_clear()`

```C
void jwt_checker_error_clear(jwt_checker_t *checker);
```

---

(jwt_checker_setkey)=
### `jwt_checker_setkey()`

```C
int jwt_checker_setkey(jwt_checker_t *checker, const jwt_alg_t alg, const jwk_item_t *key);
```

---

(jwt_checker_setcb)=
### `jwt_checker_setcb()`

```C
int jwt_checker_setcb(jwt_checker_t *checker, jwt_callback_t cb, void *ctx);
```

---

(jwt_checker_getctx)=
### `jwt_checker_getctx()`

```C
void *jwt_checker_getctx(jwt_checker_t *checker);
```

---

(jwt_checker_verify)=
### `jwt_checker_verify()`

```C
int jwt_checker_verify(jwt_checker_t *checker, const char *token);
```

---

(jwt_checker_claim_get)=
### `jwt_checker_claim_get()`

```C
const char *jwt_checker_claim_get(jwt_checker_t *checker, jwt_claims_t type);
```

---

(jwt_checker_claim_set)=
### `jwt_checker_claim_set()`

```C
int jwt_checker_claim_set(jwt_checker_t *checker, jwt_claims_t type, const char *value);
```

---

(jwt_checker_claim_del)=
### `jwt_checker_claim_del()`

```C
int jwt_checker_claim_del(jwt_checker_t *checker, jwt_claims_t type);
```

---

(jwt_checker_time_leeway)=
### `jwt_checker_time_leeway()`

```C
int jwt_checker_time_leeway(jwt_checker_t *checker, jwt_claims_t claim, time_t secs);
```

---

## JWT Header & Claims (legacy)

(jwt_header_set)=
### `jwt_header_set()`

```C
jwt_value_error_t jwt_header_set(jwt_t *jwt, jwt_value_t *value);
```

---

(jwt_header_get)=
### `jwt_header_get()`

```C
jwt_value_error_t jwt_header_get(jwt_t *jwt, jwt_value_t *value);
```

---

(jwt_header_del)=
### `jwt_header_del()`

```C
jwt_value_error_t jwt_header_del(jwt_t *jwt, const char *header);
```

---

(jwt_claim_set)=
### `jwt_claim_set()`

```C
jwt_value_error_t jwt_claim_set(jwt_t *jwt, jwt_value_t *value);
```

---

(jwt_claim_get)=
### `jwt_claim_get()`

```C
jwt_value_error_t jwt_claim_get(jwt_t *jwt, jwt_value_t *value);
```

---

(jwt_claim_del)=
### `jwt_claim_del()`

```C
jwt_value_error_t jwt_claim_del(jwt_t *jwt, const char *claim);
```

---

## JWK / JWKS

(jwks_load)=
### `jwks_load()`

```C
jwk_set_t *jwks_load(jwk_set_t *jwk_set, const char *jwk_json_str);
```

---

(jwks_load_strn)=
### `jwks_load_strn()`

```C
jwk_set_t *jwks_load_strn(jwk_set_t *jwk_set, const char *jwk_json_str, const size_t len);
```

---

(jwks_load_fromfile)=
### `jwks_load_fromfile()`

```C
jwk_set_t *jwks_load_fromfile(jwk_set_t *jwk_set, const char *file_name);
```

---

(jwks_load_fromfp)=
### `jwks_load_fromfp()`

```C
jwk_set_t *jwks_load_fromfp(jwk_set_t *jwk_set, FILE *input);
```

---

(jwks_load_fromurl)=
### `jwks_load_fromurl()`

```C
jwk_set_t *jwks_load_fromurl(jwk_set_t *jwk_set, const char *url, int verify);
```

---

(jwks_create)=
### `jwks_create()`

```C
jwk_set_t *jwks_create(const char *jwk_json_str);
```

---

(jwks_create_strn)=
### `jwks_create_strn()`

```C
jwk_set_t *jwks_create_strn(const char *jwk_json_str, const size_t len);
```

---

(jwks_create_fromfile)=
### `jwks_create_fromfile()`

```C
jwk_set_t *jwks_create_fromfile(const char *file_name);
```

---

(jwks_create_fromfp)=
### `jwks_create_fromfp()`

```C
jwk_set_t *jwks_create_fromfp(FILE *input);
```

---

(jwks_create_fromurl)=
### `jwks_create_fromurl()`

```C
jwk_set_t *jwks_create_fromurl(const char *url, int verify);
```

---

(jwks_error)=
### `jwks_error()`

```C
int jwks_error(const jwk_set_t *jwk_set);
```

---

(jwks_error_any)=
### `jwks_error_any()`

```C
int jwks_error_any(const jwk_set_t *jwk_set);
```

---

(jwks_error_msg)=
### `jwks_error_msg()`

```C
const char *jwks_error_msg(const jwk_set_t *jwk_set);
```

---

(jwks_error_clear)=
### `jwks_error_clear()`

```C
void jwks_error_clear(jwk_set_t *jwk_set);
```

---

(jwks_free)=
### `jwks_free()`

```C
void jwks_free(jwk_set_t *jwk_set);
```

---

(jwks_item_get)=
### `jwks_item_get()`

```C
const jwk_item_t *jwks_item_get(const jwk_set_t *jwk_set, size_t index);
```

---

(jwks_find_bykid)=
### `jwks_find_bykid()`

```C
jwk_item_t *jwks_find_bykid(jwk_set_t *jwk_set, const char *kid);
```

---

(jwks_item_is_private)=
### `jwks_item_is_private()`

```C
int jwks_item_is_private(const jwk_item_t *item);
```

---

(jwks_item_error)=
### `jwks_item_error()`

```C
int jwks_item_error(const jwk_item_t *item);
```

---

(jwks_item_error_msg)=
### `jwks_item_error_msg()`

```C
const char *jwks_item_error_msg(const jwk_item_t *item);
```

---

(jwks_item_curve)=
### `jwks_item_curve()`

```C
const char *jwks_item_curve(const jwk_item_t *item);
```

---

(jwks_item_kid)=
### `jwks_item_kid()`

```C
const char *jwks_item_kid(const jwk_item_t *item);
```

---

(jwks_item_alg)=
### `jwks_item_alg()`

```C
jwt_alg_t jwks_item_alg(const jwk_item_t *item);
```

---

(jwks_item_kty)=
### `jwks_item_kty()`

```C
jwk_key_type_t jwks_item_kty(const jwk_item_t *item);
```

---

(jwks_item_use)=
### `jwks_item_use()`

```C
jwk_pub_key_use_t jwks_item_use(const jwk_item_t *item);
```

---

(jwks_item_key_ops)=
### `jwks_item_key_ops()`

```C
jwk_key_op_t jwks_item_key_ops(const jwk_item_t *item);
```

---

(jwks_item_pem)=
### `jwks_item_pem()`

```C
const char *jwks_item_pem(const jwk_item_t *item);
```

---

(jwks_item_key_oct)=
### `jwks_item_key_oct()`

```C
int jwks_item_key_oct(const jwk_item_t *item, const unsigned char **buf, size_t *len);
```

---

(jwks_item_key_bits)=
### `jwks_item_key_bits()`

```C
int jwks_item_key_bits(const jwk_item_t *item);
```

---

(jwks_item_free)=
### `jwks_item_free()`

```C
int jwks_item_free(jwk_set_t *jwk_set, size_t index);
```

---

(jwks_item_free_all)=
### `jwks_item_free_all()`

```C
int jwks_item_free_all(jwk_set_t *jwk_set);
```

---

(jwks_item_free_bad)=
### `jwks_item_free_bad()`

```C
int jwks_item_free_bad(jwk_set_t *jwk_set);
```

---

(jwks_item_count)=
### `jwks_item_count()`

```C
size_t jwks_item_count(const jwk_set_t *jwk_set);
```

---
