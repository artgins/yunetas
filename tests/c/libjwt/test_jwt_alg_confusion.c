/***********************************************************************
 *          TEST_JWT_ALG_CONFUSION.C
 *
 *  Regression test for GHSA-q843-6q5f-w55g — the algorithm-confusion JWT
 *  forgery (an RSA/EC public-key JWK accepted to verify an HS* token, which
 *  the buggy HMAC path then validates against the public-key bytes -> auth
 *  bypass). The fix binds the JWA algorithm to the JWK's actual key type at
 *  both setkey time (jwt_checker_setkey -> __setkey_check) and verify time
 *  (jwt_checker_verify2 -> __verify_config_post / __check_hmac).
 *
 *  Mirrors the live reachable path in c_authz.c: jwks_create -> jwk_process_one
 *  -> jwks_item_add -> jwt_checker_setkey -> jwt_checker_verify2.
 *
 *  Beyond the forgery proper, it sweeps malformed JWKs, malformed/none/null
 *  tokens, and jwt_checker_* NULL-safety (ported from upstream
 *  tests/jwt_security.c) to confirm the parser degrades gracefully instead of
 *  crashing or silently accepting. See README.md.
 *
 *  Standalone (no gobj framework): libjwt + jansson both default to libc
 *  malloc when no allocator is installed, so there is no allocator-timing
 *  dependency here.
 *
 *          Copyright (c) 2026 ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include <jwt.h>

/***************************************************************************
 *  Fixtures (deterministically generated; see README.md)
 ***************************************************************************/
/* RSA-2048 PUBLIC modulus/exponent, shared by every RSA fixture below so the
 * malformed variants differ only in the field under test. */
#define RSA_N \
    "rOZFafPW2chmRHt8Y_qj14jZy0lS-SCAe9jHkL1w7CITDNEGaFgbiPgFYadZ3uCv" \
    "KxzYa6PYStbMVFoop7_P3e2jXI8Ss1bHjpPRwYPR9vVbThz558u2OhmXzXmk5Xm4TpJeJdR" \
    "ummVPqXYQET5rtMs8KSvUNhPTh3s6wHVANLAtaH8-z_YvNjARPNr5AALcxLtYRJ8leUWbQm" \
    "v3NjnKulCF98gqi9vqwhY-w-oD0PU0znQs3rtpNLRc8K0e9i67tav-3TXI5Y6jpLmsw2dPn" \
    "Fd1vtBalIPqQvVsmu6T1HO1ObaoZ5sdbJBz9niM5TcnBWL6KTiiG0qs42KWKQ_55w"

/* RSA-2048 PUBLIC key as a JWK, advertising alg RS256 (a normal server key). */
static const char *RSA_JWK =
    "{\"kty\":\"RSA\",\"kid\":\"test-rsa\",\"alg\":\"RS256\","
    "\"n\":\"" RSA_N "\",\"e\":\"AQAB\"}";

/* Forged token: header alg=HS256, HMAC signature over header.payload. An
 * attacker crafts this using the RSA public key as the HMAC secret; it must be
 * REJECTED before any signature check by the kty<->alg guard. */
static const char *FORGED_HS256 =
    "eyJhbGciOiAiSFMyNTYiLCAidHlwIjogIkpXVCJ9."
    "eyJzdWIiOiAiYXR0YWNrZXIiLCAiYWRtaW4iOiB0cnVlfQ."
    "-NwGCILt6w4sYl-H8UOUJpeEu4rnjCas3yicgNHZ5U0";

/* A genuine octet (HMAC) key + a token correctly signed with it: the positive
 * control proving the guard does not break legitimate HS256 verification. */
static const char *OCT_JWK =
    "{\"kty\":\"oct\",\"kid\":\"test-oct\",\"alg\":\"HS256\","
    "\"k\":\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8\"}";
static const char *LEGIT_HS256 =
    "eyJhbGciOiAiSFMyNTYiLCAidHlwIjogIkpXVCJ9."
    "eyJzdWIiOiAibGVnaXQtdXNlciJ9."
    "OCrX7FnQNBRKxGw-s8WYrBGtqjSXKHi6l5ESlRbXTtQ";

/* EC P-256 and Ed25519 (OKP) PUBLIC keys — the asymmetric-confusion siblings
 * of the RSA case: presenting the same HS256 token against either must be
 * rejected by the kty<->alg guard. */
static const char *EC_JWK =
    "{\"kty\":\"EC\",\"kid\":\"test-ec\",\"crv\":\"P-256\",\"alg\":\"ES256\","
    "\"x\":\"hdDVnG87UkyLwriVoB2sHO7MUAlgS3QeQMXwVjMoEn8\","
    "\"y\":\"zOnl6oZn0r0X0Tmfpu2GKuqmKGRukajReTxwX7zPJ9c\"}";
static const char *OKP_JWK =
    "{\"kty\":\"OKP\",\"kid\":\"test-okp\",\"crv\":\"Ed25519\",\"alg\":\"EdDSA\","
    "\"x\":\"5RJJy6yoVz8qPUotL-SwaP95OwC3jHLivx00c7yyY_M\"}";

/* alg:none token (no signature). The classic alg-stripping attack: a checker
 * bound to a real key/alg must reject it. */
static const char *NONE_TOKEN =
    "eyJhbGciOiAibm9uZSIsICJ0eXAiOiAiSldUIn0."
    "eyJzdWIiOiAiYXR0YWNrZXIiLCAiYWRtaW4iOiB0cnVlfQ.";

/***************************************************************************
 *  Tiny assert harness
 ***************************************************************************/
static int failed = 0;

static void check(int cond, const char *msg)
{
    if(cond) {
        printf("  ok   - %s\n", msg);
    } else {
        printf("  FAIL - %s\n", msg);
        failed++;
    }
}

/***************************************************************************
 *  Load a JWK json string into a fresh item added to the keyring.
 *  jwk_process_one json_deep_copy's its argument, so we still own (and free)
 *  the parsed json — exactly as c_authz does.
 ***************************************************************************/
static jwk_item_t *load_jwk(jwk_set_t *keyring, const char *jwk_json)
{
    json_error_t jerr;
    json_t *jn = json_loads(jwk_json, 0, &jerr);
    if(jn == NULL) {
        printf("  FAIL - json_loads JWK: %s\n", jerr.text);
        failed++;
        return NULL;
    }
    jwk_item_t *item = jwk_process_one(keyring, jn);
    json_decref(jn);
    if(item != NULL) {
        jwks_item_add(keyring, item);
    }
    return item;
}

/***************************************************************************
 *  Asymmetric-key forgery (RSA / EC / OKP): an HS256 token must never verify
 *  against an asymmetric key, and that key must not be settable as an HMAC
 *  key. Parameterized over the three asymmetric key types.
 *
 *  NOTE the verify2 contract: it ALWAYS returns the parsed claims (incref'd) —
 *  success/failure is signalled only by jwt_checker_error(), exactly the flag
 *  c_authz keys "validated" off (c_authz.c: `if(!jwt_checker->error) validated
 *  = TRUE;`). Checking the return pointer alone would accept the forgery.
 ***************************************************************************/
static void test_asym_forgery(const char *label, const char *jwk_json,
    jwt_alg_t native_alg, const char *native_alg_name)
{
    printf("%s key (GHSA-q843 forgery):\n", label);

    jwk_set_t *keyring = jwks_create(NULL);
    jwk_item_t *key = load_jwk(keyring, jwk_json);
    check(key != NULL, "JWK parses into a jwk_item");
    if(key == NULL) {
        jwks_free(keyring);
        return;
    }

    /* Positive control: the key's native alg is a valid pairing, so a later
     * rejection can only be the HS256 confusion, not a broken setup. */
    jwt_checker_t *ok = jwt_checker_new();
    check(jwt_checker_setkey(ok, native_alg, key) == 0,
        "setkey(native alg, key) accepted (legit config)");
    printf("        native alg: %s\n", native_alg_name);

    /* The forgery: an HS256 token verified against the asymmetric-key checker
     * must be rejected. */
    json_t *payload = jwt_checker_verify2(ok, FORGED_HS256);
    check(jwt_checker_error(ok) != 0,
        "verify(HS256 token, asym-key checker) REJECTED (alg confusion blocked)");
    printf("        reason: %s\n", jwt_checker_error_msg(ok));
    json_decref(payload);
    jwt_checker_free(ok);

    /* The same confusion blocked one layer earlier, at setkey time. */
    jwt_checker_t *bad = jwt_checker_new();
    check(jwt_checker_setkey(bad, JWT_ALG_HS256, key) != 0,
        "setkey(HS256, asym key) rejected (cannot be an HMAC key)");
    jwt_checker_free(bad);

    jwks_free(keyring);
}

/***************************************************************************
 *  alg:none stripping — a checker bound to a real key/alg must reject a token
 *  that declares alg=none and carries no signature.
 ***************************************************************************/
static void test_alg_none(void)
{
    printf("alg:none (stripping attack):\n");

    jwk_set_t *keyring = jwks_create(NULL);
    jwk_item_t *rsa = load_jwk(keyring, RSA_JWK);
    if(rsa == NULL) {
        check(0, "RSA JWK parses for alg:none test");
        jwks_free(keyring);
        return;
    }

    jwt_checker_t *checker = jwt_checker_new();
    jwt_checker_setkey(checker, JWT_ALG_RS256, rsa);

    json_t *payload = jwt_checker_verify2(checker, NONE_TOKEN);
    check(jwt_checker_error(checker) != 0,
        "verify(alg:none token, real-key checker) REJECTED");
    printf("        reason: %s\n", jwt_checker_error_msg(checker));
    json_decref(payload);
    jwt_checker_free(checker);

    jwks_free(keyring);
}

/***************************************************************************
 *  Case 4: octet key — legitimate HS256 verification still works.
 ***************************************************************************/
static void test_oct_key(void)
{
    printf("OCT key (positive control):\n");

    jwk_set_t *keyring = jwks_create(NULL);
    jwk_item_t *oct = load_jwk(keyring, OCT_JWK);
    check(oct != NULL, "OCT JWK parses into a jwk_item");
    if(oct == NULL) {
        jwks_free(keyring);
        return;
    }

    jwt_checker_t *checker = jwt_checker_new();
    check(jwt_checker_setkey(checker, JWT_ALG_HS256, oct) == 0,
        "setkey(HS256, OCT) accepted");

    json_t *payload = jwt_checker_verify2(checker, LEGIT_HS256);
    check(jwt_checker_error(checker) == 0,
        "verify(legit HS256 token, OCT key) ACCEPTED (guard not over-rejecting)");
    if(payload != NULL) {
        const char *sub = json_string_value(json_object_get(payload, "sub"));
        check(sub != NULL && strcmp(sub, "legit-user") == 0,
            "verified payload carries the expected sub claim");
    }
    json_decref(payload);
    jwt_checker_free(checker);

    jwks_free(keyring);
}

/***************************************************************************
 *  Malformed-JWK robustness (ported from upstream tests/jwt_security.c).
 *  A malformed JWK must be handled gracefully — either rejected at JSON parse
 *  or surfaced as jwks_item_error() != 0 — never a crash or a silently-usable
 *  key. WANT_REJECT asserts rejection; WANT_ACCEPT asserts a clean parse (the
 *  non-string-"alg" cases exercise the alg_str NULL-deref hardening: the alg
 *  hint is ignored, the key still parses).
 ***************************************************************************/
/* WANT_REJECT: must surface an error. WANT_ACCEPT: must parse clean.
 * WANT_NOCRASH: either outcome is fine — the assertion is only that the
 * malformed input was handled without a crash (e.g. the non-string-"alg"
 * NULL-deref hardening; the vendored copy rejects, upstream tolerates — both
 * safe, so don't pin the verdict). */
enum { WANT_ACCEPT = 0, WANT_REJECT = 1, WANT_NOCRASH = 2 };

static void check_jwk(const char *jwk_json, int want, const char *label)
{
    jwk_set_t *keyring = jwks_create(NULL);
    json_error_t jerr;
    json_t *jn = json_loads(jwk_json, 0, &jerr);
    if(jn == NULL) {
        /* Not even valid JSON -> a parse-level rejection is correct. */
        check(want != WANT_ACCEPT, label);
        jwks_free(keyring);
        return;
    }
    jwk_item_t *item = jwk_process_one(keyring, jn);
    json_decref(jn);
    if(item == NULL) {
        check(want != WANT_ACCEPT, label);
        jwks_free(keyring);
        return;
    }
    jwks_item_add(keyring, item);
    int err = jwks_item_error(item);
    if(want == WANT_REJECT) {
        check(err != 0, label);
    } else if(want == WANT_ACCEPT) {
        check(err == 0, label);
    } else { /* WANT_NOCRASH: reaching here is the pass */
        check(1, label);
    }
    if(err != 0 && want != WANT_ACCEPT) {
        printf("        reason: %s\n", jwks_item_error_msg(item));
    }
    jwks_free(keyring);
}

static void test_malformed_jwks(void)
{
    printf("Malformed JWKs (graceful handling):\n");

    /* "alg" of a non-string type: tolerated (hint ignored), key still parses —
     * the alg_str NULL-deref hardening. */
    check_jwk("{\"kty\":\"RSA\",\"n\":\"" RSA_N "\",\"e\":\"AQAB\",\"alg\":256}",
        WANT_NOCRASH, "RSA with alg as integer handled (no NULL-deref)");
    check_jwk("{\"kty\":\"RSA\",\"n\":\"" RSA_N "\",\"e\":\"AQAB\",\"alg\":null}",
        WANT_NOCRASH, "RSA with alg as null handled (no NULL-deref)");
    check_jwk("{\"kty\":\"RSA\",\"n\":\"" RSA_N "\",\"e\":\"AQAB\",\"alg\":true}",
        WANT_NOCRASH, "RSA with alg as boolean handled (no NULL-deref)");
    check_jwk("{\"kty\":\"RSA\",\"n\":\"" RSA_N "\",\"e\":\"AQAB\",\"alg\":\"PS256\"}",
        WANT_ACCEPT, "RSA with alg PS256 parses (positive control)");

    /* Missing / wrong-typed kty. */
    check_jwk("{\"kty\":123}", WANT_REJECT, "kty as integer rejected");
    check_jwk("{}", WANT_REJECT, "empty object rejected (no kty)");
    check_jwk("{\"kty\":\"BOGUS\",\"n\":\"" RSA_N "\",\"e\":\"AQAB\"}",
        WANT_REJECT, "unknown kty rejected");

    /* Missing required key components. */
    check_jwk("{\"kty\":\"RSA\",\"e\":\"AQAB\",\"alg\":\"RS256\"}",
        WANT_REJECT, "RSA missing n rejected");
    check_jwk("{\"kty\":\"RSA\",\"n\":\"" RSA_N "\",\"alg\":\"RS256\"}",
        WANT_REJECT, "RSA missing e rejected");
    check_jwk("{\"kty\":\"EC\",\"crv\":\"P-256\","
        "\"y\":\"zOnl6oZn0r0X0Tmfpu2GKuqmKGRukajReTxwX7zPJ9c\",\"alg\":\"ES256\"}",
        WANT_REJECT, "EC missing x rejected");
    check_jwk("{\"kty\":\"EC\","
        "\"x\":\"hdDVnG87UkyLwriVoB2sHO7MUAlgS3QeQMXwVjMoEn8\","
        "\"y\":\"zOnl6oZn0r0X0Tmfpu2GKuqmKGRukajReTxwX7zPJ9c\",\"alg\":\"ES256\"}",
        WANT_REJECT, "EC missing crv rejected");
    check_jwk("{\"kty\":\"OKP\",\"crv\":\"Ed25519\",\"alg\":\"EdDSA\"}",
        WANT_REJECT, "OKP/EdDSA missing x rejected");

    /* Wrong value types / structural abuse. */
    check_jwk("{\"kty\":\"RSA\",\"n\":12345,\"e\":[1,2,3],\"alg\":\"RS256\"}",
        WANT_REJECT, "RSA with non-string n/e rejected");
    check_jwk("{\"kty\":\"RSA\",\"n\":{\"a\":{\"b\":{\"c\":\"deep\"}}},\"e\":\"AQAB\"}",
        WANT_REJECT, "RSA with deeply-nested n rejected");
    check_jwk("{\"kty\":\"oct\",\"k\":\"!!!not-valid-base64!!!\",\"alg\":\"HS256\"}",
        WANT_REJECT, "oct with invalid base64 k rejected");

    /* Not valid JSON at all. */
    check_jwk("", WANT_REJECT, "empty string rejected at parse");
}

/***************************************************************************
 *  Malformed-token robustness (ported from upstream tests/jwt_security.c).
 *  Against a checker bound to a real key, every structurally broken or
 *  alg-stripped token must be rejected (jwt_checker_verify() != 0). NULL/empty
 *  inputs must be handled without crashing.
 ***************************************************************************/
static void expect_token_rejected(jwt_checker_t *checker, const char *token,
    const char *label)
{
    jwt_checker_error_clear(checker);
    check(jwt_checker_verify(checker, token) != 0, label);
}

static void test_malformed_tokens(void)
{
    printf("Malformed tokens (rejected by a real-key checker):\n");

    jwk_set_t *keyring = jwks_create(NULL);
    jwk_item_t *rsa = load_jwk(keyring, RSA_JWK);
    if(rsa == NULL) {
        check(0, "RSA JWK parses for malformed-token test");
        jwks_free(keyring);
        return;
    }
    jwt_checker_t *checker = jwt_checker_new();
    jwt_checker_setkey(checker, JWT_ALG_RS256, rsa);

    expect_token_rejected(checker, NULL,         "NULL token rejected (no crash)");
    expect_token_rejected(checker, "",           "empty token rejected");
    expect_token_rejected(checker, "nodotshere", "token with no dots rejected");
    expect_token_rejected(checker, "one.dot",    "token with one dot rejected");
    expect_token_rejected(checker, "a.b.c.d.e.f","token with many dots rejected");
    expect_token_rejected(checker, ".eyJ0ZXN0IjoiMSJ9.",
        "empty header rejected");
    expect_token_rejected(checker, "bm90IGpzb24.eyJ0ZXN0IjoiMSJ9.",
        "header valid base64 but not JSON rejected");
    expect_token_rejected(checker, "eyJ0eXAiOiJKV1QifQ.eyJ0ZXN0IjoiMSJ9.",
        "header missing alg rejected");
    expect_token_rejected(checker, "eyJhbGciOiJCT0dVUyJ9.eyJ0ZXN0IjoiMSJ9.",
        "header invalid alg (BOGUS) rejected");
    expect_token_rejected(checker, "eyJhbGciOjI1Nn0.eyJ0ZXN0IjoiMSJ9.",
        "header alg as integer rejected");
    expect_token_rejected(checker, "eyJhbGciOiJub25lIn0.!!!invalid!!!.",
        "alg:none with invalid-base64 payload rejected");

    jwt_checker_free(checker);
    jwks_free(keyring);
}

/***************************************************************************
 *  NULL-safety of the jwt_checker_* entry points (ported from upstream
 *  tests/jwt_security.c). These are the verify-path API c_authz drives; they
 *  must not crash and must report failure on a NULL checker.
 *
 *  Scope note: upstream also hardens the jwks_* keyring API against NULL
 *  (jwks_item_get(NULL)/jwks_free(NULL)); the vendored v3.2.1+2 copy has NOT
 *  backported those guards (jwks_item_get derefs jwk_set->head — jwks.c:201),
 *  so they are deliberately NOT asserted here. Not reachable from c_authz
 *  (the keyring is always valid there); tracked as a drift item, not tested.
 ***************************************************************************/
static void test_null_safety(void)
{
    printf("NULL safety (jwt_checker_* entry points):\n");

    check(jwt_checker_verify(NULL, "x") != 0, "verify(NULL checker) fails");
    check(jwt_checker_error(NULL) != 0,       "error(NULL checker) != 0");
    check(jwt_checker_error_msg(NULL) == NULL,"error_msg(NULL checker) == NULL");
    check(jwt_checker_setkey(NULL, JWT_ALG_NONE, NULL) != 0,
        "setkey(NULL checker) fails");
}

/***************************************************************************
 *  Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("test_jwt_alg_confusion: GHSA-q843-6q5f-w55g regression\n");

    test_asym_forgery("RSA", RSA_JWK, JWT_ALG_RS256, "RS256");
    test_asym_forgery("EC", EC_JWK, JWT_ALG_ES256, "ES256");
    test_asym_forgery("OKP/EdDSA", OKP_JWK, JWT_ALG_EDDSA, "EdDSA");
    test_alg_none();
    test_oct_key();
    test_malformed_jwks();
    test_malformed_tokens();
    test_null_safety();

    if(failed) {
        printf("RESULT: FAILED (%d)\n", failed);
        return 1;
    }
    printf("RESULT: OK\n");
    return 0;
}
