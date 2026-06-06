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
/* RSA-2048 PUBLIC key as a JWK, advertising alg RS256 (a normal server key). */
static const char *RSA_JWK =
    "{\"kty\":\"RSA\",\"kid\":\"test-rsa\",\"alg\":\"RS256\","
    "\"n\":\"rOZFafPW2chmRHt8Y_qj14jZy0lS-SCAe9jHkL1w7CITDNEGaFgbiPgFYadZ3uCv"
    "KxzYa6PYStbMVFoop7_P3e2jXI8Ss1bHjpPRwYPR9vVbThz558u2OhmXzXmk5Xm4TpJeJdR"
    "ummVPqXYQET5rtMs8KSvUNhPTh3s6wHVANLAtaH8-z_YvNjARPNr5AALcxLtYRJ8leUWbQm"
    "v3NjnKulCF98gqi9vqwhY-w-oD0PU0znQs3rtpNLRc8K0e9i67tav-3TXI5Y6jpLmsw2dPn"
    "Fd1vtBalIPqQvVsmu6T1HO1ObaoZ5sdbJBz9niM5TcnBWL6KTiiG0qs42KWKQ_55w\","
    "\"e\":\"AQAB\"}";

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
 *  Case 1+2+3: RSA key — forgery rejection (verify + setkey) and the
 *  RS256 positive control.
 ***************************************************************************/
static void test_rsa_key(void)
{
    printf("RSA key (GHSA-q843 forgery):\n");

    jwk_set_t *keyring = jwks_create(NULL);
    jwk_item_t *rsa = load_jwk(keyring, RSA_JWK);
    check(rsa != NULL, "RSA JWK parses into a jwk_item");
    if(rsa == NULL) {
        jwks_free(keyring);
        return;
    }

    /* (3) Positive control: RS256 + RSA must be a valid pairing, so a later
     *     NULL from verify can only be the HS256 rejection, not a bad setup. */
    jwt_checker_t *ok = jwt_checker_new();
    check(jwt_checker_setkey(ok, JWT_ALG_RS256, rsa) == 0,
        "setkey(RS256, RSA) accepted (legit config)");

    /* (1) The forgery: an HS256 token verified against the RSA-key checker
     *     must be rejected. NOTE the verify2 contract: it ALWAYS returns the
     *     parsed claims (incref'd) — success/failure is signalled only by
     *     jwt_checker_error(), exactly the flag c_authz keys "validated" off
     *     (c_authz.c: `if(!jwt_checker->error) validated = TRUE;`). Checking the
     *     return pointer alone would accept the forgery. */
    json_t *payload = jwt_checker_verify2(ok, FORGED_HS256);
    check(jwt_checker_error(ok) != 0,
        "verify(HS256 token, RSA-key checker) REJECTED (alg confusion blocked)");
    printf("        reason: %s\n", jwt_checker_error_msg(ok));
    json_decref(payload);
    jwt_checker_free(ok);

    /* (2) The same confusion blocked one layer earlier, at setkey time. */
    jwt_checker_t *bad = jwt_checker_new();
    check(jwt_checker_setkey(bad, JWT_ALG_HS256, rsa) != 0,
        "setkey(HS256, RSA) rejected (RSA key cannot be an HMAC key)");
    jwt_checker_free(bad);

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
 *  Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("test_jwt_alg_confusion: GHSA-q843-6q5f-w55g regression\n");

    test_rsa_key();
    test_oct_key();

    if(failed) {
        printf("RESULT: FAILED (%d)\n", failed);
        return 1;
    }
    printf("RESULT: OK\n");
    return 0;
}
