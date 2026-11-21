# libjwt (vendored)

In-tree fork of [libjwt](https://github.com/benmcollins/libjwt)
adapted for Yuneta. To take fixes from upstream, follow the
[Re-vendor procedure](#re-vendor-procedure) below.

## What it does

The library creates and parses JSON Web Tokens (JWT), and verifies their
signature. The auth path uses it to validate the access tokens that an
upstream IdP (Keycloak / Auth0 / Azure AD) issues, on every incoming
WebSocket connection. See [`AUTH.md`](../../../yunos/c/yuno_agent/AUTH.md) §3.

Crypto backends:
- `src/openssl/`: OpenSSL implementations.
- `src/mbedtls/`: mbedTLS implementations.
- `src/gnutls/`: GnuTLS implementations (kept for portability).

The backend selection is the same `ytls` runtime choice that the rest of the
kernel uses (see
[`kernel/c/ytls/README.md`](../ytls/README.md)).

## Why vendored

Upstream libjwt worked well at version 1.16.0. The
2.x / 3.x branches introduced API changes that are incompatible with the
existing usage in Yuneta. Yuneta did not follow upstream and patch the
divergence. It forked libjwt into this directory and applied the necessary
changes in-tree. A new upstream feature can be ported commit by commit, when
it is worth the integration cost.

## Security review & upstream sync status

> Note on the version claim above: the in-tree source was fingerprinted
> against the upstream history. It derives from v3.2.1 + 2 commits, so it is
> on the 3.x branch, not 1.x. The "1.16.0" line is stale.

A security review of this vendored copy against upstream was done on
2026-06-05. It was done again against v3.4.0 on 2026-06-15, against v3.6.1
on 2026-06-28, against v3.6.3 on 2026-11-16, against v3.6.4 on
2026-11-17, and against v3.8.0 on 2026-11-21.

- Vendored base: upstream commit `375e539`
  (`v3.2.1`+2, 2025-06-04, "jwt: Use long long for json integers").
- Analyzed up to: upstream tag `v3.8.0` (2026-11-20).
  The prior passes stopped at `0081a8d` (v3.6.4, 2026-11-17), `a01e74b`
  (v3.6.3, 2026-11-16), `197ac58`
  (v3.6.1, 2026-06-28) and `6ea3b6b` (v3.4.0, 2026-06-15).
- Gap: v3.4.0 is a large feature release (full JWE, `crit` header, `jti`
  callbacks, PEM→JWK public API). Almost none of it touches the compiled subset
  of Yuneta: JWE, `jwks-curl.c`, and the `gnutls`/`json-c` backends are all
  excluded here. After filtering, only the #281–#285 hardening batch and two
  conformance features (`crit`, `jti`) land on files that we build. Of those,
  three items were backported (below, including the `crit` rejection), and the
  rest are N/A.

### Done (backported to this tree)

- `49c730a`: GHSA-q843-6q5f-w55g, algorithm-confusion JWT forgery
  (CRITICAL). An RSA/EC JWK was accepted as the HMAC verification key.
  Thus any token signed `HMAC("", header.payload)` verified against the
  public JWKS, and that is a forgery. It was reachable from the verify
  loop of `C_AUTHZ` (RS256 checker + an attacker HS256 token). All four
  layers were backported: `jwt_alg_required_kty()` (`jwt-private.h`), the
  `__setkey_check` kty check (`jwt-builder.c` + `jwt-checker.c`, because the
  vendored copy split the upstream `jwt-common.c` into both),
  `__verify_config_post` (`jwt-verify.c`), and the `__check_hmac` backstop
  (`jwt.c`).
- `cfd8902` (reachable subset): JWK octet/RSA-PSS NULL+bounds guards
  (`openssl/jwk-parse.c`), `strncpy` in `jwt_copy_error` (`jwt-private.h`),
  and a `volatile` constant-time compare (`jwt-memory.c`).
- `cfd8902` (low-severity remainder): key-material scrub before free
  (portable `jwt_scrub_and_free` in `jwt-private.h` for the HMAC key in
  `jwks.c`, and `OPENSSL_cleanse` for the PEM in `openssl/jwk-parse.c`),
  `secure_getenv` for crypto-provider selection (`jwt-crypto-ops.c`), the
  builder/checker OOM-path JSON leak (`jwt-builder.c`/`jwt-checker.c`), and the
  missing-alg-header error message (`jwt-verify.c`).
- Exact-alg pin (same-family downgrade), a local addition in
  `__verify_config_post` (`jwt-verify.c`). GHSA-q843 blocks *cross-family*
  confusion (HMAC vs RSA/EC) with the `kty` backstop. But that backstop works
  only at the level of the family: `jwt_alg_required_kty` maps every RS/PS alg
  to RSA. Also, on the common pinned path (`config->alg == config->key->alg`,
  both set), the alg-vs-alg chain never compares the token alg. Thus an RS512
  token verified against an RS256-pinned key still passed. The added check
  requires that `jwt->alg` is equal to the pinned alg (`config->alg` /
  `config->key->alg`). The check only adds a refusal: no legitimate token is
  newly rejected.
- Forgery regression test: [`tests/c/libjwt/test_jwt_alg_confusion.c`](../../../tests/c/libjwt/test_jwt_alg_confusion.c)
  keeps the GHSA-q843 fix closed (RSA/EC/OKP confusion + `alg:none` + positive
  controls). It is wired into ctest. It is a focused equivalent of the upstream
  `jwt_security.c`, not a verbatim port. It does not cover the same-family
  downgrade (the exact-alg pin above) yet. See the "Broaden the forgery test" TODO.

- `jwks_*` keyring NULL-safety (local backport, 2026-09-24). Upstream
  guards the keyring getters against a NULL set (`cfd8902` and later, shape as
  of `197ac58`). The vendored copy did not, and `jwks_item_get(NULL, 0)`
  dereferenced `jwk_set->head`. The guards were backported to `jwks.c`, in the
  upstream shape: `jwks_item_get` returns `NULL`, `jwks_error` and
  `jwks_error_any` return `1` (a NULL set is an error), `jwks_error_msg`
  returns `NULL`, and `jwks_error_clear` returns without doing anything.
  `jwks_free`, `jwks_item_free` and `jwks_item_free_all` already accepted NULL.
  This is not reachable from `C_AUTHZ`, because its keyring is always valid.

  Upstream does not guard the rest of the keyring API (its master still
  dereferences the set), but the first backport said that it did. Four more
  functions crashed on a NULL set (found 2026-09-25). They are guarded locally,
  in the same shape, marked `// ArtGins: NULL-safety, beyond upstream`.
  `jwks_item_count` returns `0`. `jwks_find_bykid` returns `NULL`, also for a
  NULL `kid` (before, it went to `strcmp`). `jwks_item_add` returns `1` for a
  NULL set or item (an ArtGins export, `static` upstream).
  `jwks_item_free_bad` returns `0`. Apply them again after an upstream sync.
  `test_null_safety` in `test_jwt_alg_confusion` covers them:

  ```c
  if(jwks_item_get(NULL, 0) == NULL && jwks_error(NULL) != 0) {
      /* a NULL keyring is refused, not dereferenced */
  }
  if(jwks_item_count(NULL) == 0 && jwks_find_bykid(NULL, "k") == NULL) {
      /* the local guards, beyond upstream */
  }
  jwks_free(NULL);    /* no-op */
  ```

- `cd9f46d`: a key in error is never used (GHSA-4jp4-6w6j-ff4q, v3.6.3,
  backported 2026-11-16). `__setkey_check` (`jwt-checker.c` +
  `jwt-builder.c`) refuses a key that `jwks_item_error()` flags, with *"Key is
  in error and cannot be used"*. A static `__check_key_error()` runs first
  in `__check_hmac()`/`__check_key_bits()` (`jwt.c`). Before this change, a key
  that loaded and was then flagged (an `"alg"` that is not a string) signed and
  verified tokens. A key that failed to load was accepted by setkey, and it
  failed every token later with a misleading *"Key too short"*.

  Two parts were not taken from upstream. The first is its
  `provider_data == NULL` test in `__check_key_error()`. The MbedTLS backend of
  this tree keeps the key in `item->pem` and never sets `provider_data`, so
  that test refuses every MbedTLS key. Here, a key with no key object has
  `bits == 0`, and the size checks refuse it. The second is the
  `key_required = 2` latch. This tree has no `key_required`. `C_AUTHZ` frees a
  checker whose setkey failed, and the controlcenter stops. The upstream NULL
  dereference (a key that MbedTLS failed to load, verified under OpenSSL)
  cannot happen here. One backend is active, and both of our backends set
  `bits` only after the key is good. `test_key_in_error` covers this change.
- `521af8f`: a token of `INT_MAX` bytes or more is refused (GHSA-75c8-gwgm-cvfm,
  v3.6.4, backported 2026-11-17). `jwt_checker_verify()` and the ArtGins
  `jwt_checker_verify2()` (`jwt-checker.c`) answer *"Token too large"* before
  `jwt_parse()`. The lengths of `jwt_parse()`, and those of
  `jwt_base64uri_decode()`, are `int`. Thus a token that long wraps them and
  sizes a buffer too small. This tree has the same code but not the
  reachability. The token reaches `C_AUTHZ` from a frame or an HTTP header. A
  frame cannot pass the maximum block of the yuno (1 GiB at most, 200 MB in the
  agent). A websocket frame bigger than that closes the connection (1009). The
  check stays as defense in depth. The upstream JWS JSON path (the
  out-of-bounds write of the advisory) and `jwt_checker_verify_detached()` do
  not exist here. No test covers the check: like upstream (`LCOV_EXCL`), the
  suite does not build a 2 GiB token.

- `c1ced0e`: an unsigned (`"alg":"none"`) token is refused (v3.7.0,
  backported 2026-11-21). `__verify_config_post` (`jwt-verify.c`) answers
  *"Unsigned tokens are not allowed"* before any other check. Before, a
  checker with no key accepted such a token. With a key, it was already
  refused, as *"Expected a signature, but JWT has none"*. `C_AUTHZ` always
  sets a key, so the change is defense in depth. Upstream lets an application
  opt in with `jwt_checker_setalgs()`. This tree has no `setalgs()`, so the
  refusal has no exception. The branch for *"JWT has signature block, but no
  alg set"* is gone: no `alg:none` token gets there now.
  `test_alg_none_keyless` covers it.
- `1760016`: a maximum token size, 1 MiB by default (v3.8.0, backported
  2026-11-21). `jwt_checker_verify()` and `jwt_checker_verify2()` answer
  *"Token too large"* before they parse a longer token. They read no more
  than the limit plus one byte (`jwt_token_too_large()` in `jwt-private.h`).
  Thus the empty-token test is now `*token == '\0'`, not `strlen()`.
  `jwt_checker_setmaxsize()` is exported with the API of upstream. The
  `INT_MAX` backstop of `521af8f` stays. This one is reachable: `C_AUTHZ`
  tries a token against the checker of each configured issuer, and a frame
  of the agent can be 200 MB. Thus a big token was decoded and parsed once
  for each issuer before the refusal. `test_max_size` covers it.
- `ccd7c6b`: an EC private JWK whose `d` is not the key of its `(x, y)`
  is refused when it loads (v3.7.0, backported 2026-11-21). OpenSSL only:
  `pctx_to_pem()` (`openssl/jwk-parse.c`) runs `EVP_PKEY_pairwise_check()`
  for a private EC key, and the key is in error with *"EC private key does
  not match its public key"*. Before, the key loaded and signed tokens that
  nothing verified. The controlcenter signs its assertions with an EC private
  JWK, so a key assembled wrongly now fails when the controlcenter loads it,
  not on each agent. The MbedTLS backend of this tree loads no EC JWK, so it
  has nothing to check. `test_ec_private_mismatch` covers it with another
  key's `d`, `d = 1`, `d = 0` and `d = n`.
- `338260c`: `jwt_copy_error` copies with `snprintf`, not `strncpy`
  (v3.7.0). The result is the same. gcc 15 under sanitizers warns about the
  `strncpy` form.

#### v3.7.0 / v3.8.0 re-review (2026-11-21): four backports

The v3.6.4→v3.8.0 gap (`0081a8d..v3.8.0`, 29 commits, 15 on the library)
carries no advisory. The four commits above are backported. The others need
a feature that this tree does not have:

- The JWS JSON and keyring paths: `14ad1de` (the alg pin of a callback that
  sets `JWT_ALG_NONE`), `15e5c97` (the reason of a failed signature),
  `1f79a4b` (Compact against a keyring) and `1606c5f` (the keyring scan).
  This tree verifies Compact tokens against one key.
- `89b1809` `jwt_checker_verified_jwt()`: `jwt_checker_verify2()` already
  returns the claims.
- `4203959`, the signer of `jwt_builder_setkey()`: our builder has no
  `jwt_builder_add_signature()`, and the controlcenter sets no builder
  callback.
- `dde9222` ECDH-ES: JWE is not compiled.
- `85d4afe`, `3993288` and `66edb24`: json-c only.
- `0d17c5d`, JSON `null` members: mostly json-c. On Jansson, this tree
  already puts a key with `"alg":null` in error, because it never took the
  3.6.3 change that read it as absent. A `"keys"` that is not an array still
  gives an empty set with no error. The set is read from the configuration, so
  the difference is only the message. No caller reads a scalar as
  `JWT_VALUE_JSON`.
- `tools/`, tests and docs: not compiled.

#### v3.4.0 re-review (2026-06-15)

- `18133e4` (L17): reject duplicate JSON members on the token parse
  (`jwt-verify.c`). RFC 8725 §2.4: the inbound header/payload is now parsed
  with `JSON_REJECT_DUPLICATES`. Without it, Jansson keeps the last
  occurrence. Thus a peer that selects a different occurrence of a duplicated
  claim/header cannot be made to disagree with us. It is reachable on every
  `jwt_parse`.
- `d180cc7`: strict base64url on decode (`jwt.c`, `jwt_base64uri_decode`).
  The decoder accepted the standard-base64 `+`/`/`, and it silently
  *truncated* on an embedded `=`. Both let a segment have another spelling
  (or be cut short) and still decode to the same or truncated bytes.
  Now the decoder rejects anything outside `[A-Za-z0-9_-]`. It is reachable on
  every token segment and on every decode of a JWK member. The change only
  adds refusals: the positive controls in `test_jwt_alg_confusion` still pass.
- `fe8840a`: `crit` (Critical) header rejection (RFC 7515 §4.1.11), checker
  side only (`jwt-verify.c` `jwt_check_crit` + both `jwt_checker_verify` /
  `jwt_checker_verify2` call sites). Before, the parser ignored `crit`.
  Per the specification, a recipient that understands none of the listed
  critical parameters MUST reject the JWS. This copy understands no extension
  headers and ports no `jwt_checker_understands()` (C_AUTHZ needs none). Thus
  it rejects any well-formed `crit`. The check runs right after the parse,
  before the key/signature work. It is not exploitable, because `crit` is in
  the signed header, so an attacker cannot inject it. Only a trusted IdP can
  emit one, and mainstream access tokens do not. But the check closes the
  conformance gap, and it matches the fail-closed posture. The builder side
  (`jwt_builder_setcrit`/`jwt_write_crit`) and the `understood`-list machinery
  were NOT ported, on purpose (C_AUTHZ does not sign). `test_crit_header`
  covers it: a well-formed and a malformed `crit` are rejected on both entry
  points. The no-`crit` positive control in `test_oct_key` still verifies.

#### v3.6.0 / v3.6.1 re-review (2026-06-28): nothing to backport

The v3.4.0→v3.6.1 gap (`6ea3b6b..197ac58`) is a feature release (v3.5.0 +
v3.6.0/v3.6.1), not a hardening pass on the verify path. Every commit was
mapped against the compiled subset (`CMakeLists.txt`) and against the only
reachable entry from `C_AUTHZ`: Compact verification (`jwt_parse` /
`jwt_checker_verify2`). No security fix lands on compiled, reachable code.
Nothing was backported.

- `eafe5e2` JWS JSON Serialization (multi-signature) + keyring verify, and its
  follow-ups `929bd68` (a `jwt_checker_require()` *policy bypass*) and `b3f1a70`
  (a per-verify header leak). All three live in `jwt_verify_json()`, a new
  serialization that this tree does not have (we only do Compact). The
  "bypass" is not reachable: the code path does not exist here.
- `2469253` + `eca511e`: RFC 7797 unencoded (`b64=false`) / detached
  payloads. A new feature. It is not reachable: the `crit` rejection, already
  backported (`fe8840a`), rejects any token that carries the `b64` critical
  header. Thus an unencoded-payload token never reaches claim handling.
- `f4a536f` (#320, route key ops to the parsing backend) + `b6d2c1c` (#327,
  free `provider_data` through the origin backend). A real multi-backend
  memory-safety fix upstream, but not reachable here. This tree selects a
  single crypto backend at startup (through `ytls`), and it never calls
  `jwt_set_crypto_ops`. It never changes the active backend between the parse
  of a JWK and a verify with it. Thus a key is always freed/used under the same
  ops that parsed it.
- `910e3e4`: RFC 8725 `typ` helper + `jwt_checker_setalgs()` algorithm
  allowlist. Opt-in checker hardening (active only when configured). It is
  redundant with the local *exact-alg pin* of this tree in
  `__verify_config_post`, which already rejects any alg that is not pinned,
  `alg:none` included. The upstream allowlist is the equivalent that the
  specification recommends. It was not ported, because it gives no behavioral
  gain.
- `6579215` application profiles (#317), `a1bc8d3` `jwks_generate()`, `f648626`
  RFC 7638/9278 thumbprints, `c222fb4` x5c/x5t, `9cd2114`/`6ef66d2` JWE
  PBES2/AES-GCM-KW, `a69d823` RFC 7800 `cnf`, ML-DSA additions. These are
  producer-side or opt-in feature primitives. Their changes to compiled files
  (for example the OpenSSL backend) only add code: new functions registered in
  `jwt_openssl_ops`, and no change to `openssl_verify_sha_pem`. `C_AUTHZ` only
  verifies, so it reaches none of them.
- `6f23796` / `22a089a` cached remote JWKS (including the `Cache-Control`
  `time_t` overflow), `590beb2` / `6903450` (`jwk-export.c`, tools), gnutls.
  They are in `jwks-curl.c` or in files that are not compiled here (JWKS is
  loaded from a configuration attr). N/A. Classify them again only if the curl
  backend is enabled one day.

#### v3.6.4 re-review (2026-11-17): one backport

The v3.6.3→v3.6.4 gap (`a01e74b..0081a8d`, 9 commits) carries one
advisory, GHSA-75c8-gwgm-cvfm. It is backported (`521af8f`, above). The rest:

- `f7f3e03`: the Jansson backend leaves the input excerpt (`" near
  '...'"`) out of its parse errors. This tree has no JSON backend layer:
  `jwks_process()` writes the text of Jansson as it is. N/A in practice. It
  parses only JWKs from the configuration or from an operator (public keys). A
  token is parsed with no error text (`json_loads(..., NULL)`), so no bytes of
  a peer reach a message.
- `1de5385`: the `jwt-verify` tool escapes C1 controls. `tools/` is not
  compiled.

#### v3.6.2 / v3.6.3 re-review (2026-11-16): one backport

The v3.6.1→v3.6.3 gap (`197ac58..a01e74b`) carries seven advisories. One
touches compiled code and is backported (`cd9f46d`, above). The others need a
feature that this tree does not have:

- GHSA-v7q9-pwfj-qmf5 (`0f0bbed`): NUL in an unencoded detached payload.
  No `b64=false` here, and every `crit` is rejected.
- GHSA-6gm4-gxx5-93hq (`9f54614`, `e308446`): a `"b64"` header that
  turned off the claim checks, and the `"b64"`/`"crit"` headers of the builder.
  No `b64`, no `jwt_builder_setcrit`.
- GHSA-mhc9-xvg5-qg29 (`4f09bc3`, `b3582f2`) + `f0115c4`: Compact verify
  against a keyring, and a verify callback that removes the key of the
  checker. No `jwt_checker_setkeyring` here, and `C_AUTHZ` sets no callback.
- GHSA-vmf4-mvgm-7vpv (`a4b6351`, `2143cf3`): a private OKP/AKP JWK whose
  `"d"` is not the key of its `"x"`, and a private key in the `"jwk"` header.
  Both are reached through embedded-JWK verification and RFC 7638
  thumbprints, which this tree does not have. Our OKP keys are from the
  configuration (trusted). The controlcenter signs with EC, which the advisory
  says is not affected.
- GHSA-4jp4-6w6j-ff4q, rest (`1329901`, `d927851`): the `"x5c"` leaf, and
  private members on export. No `x5c`, no `jwk-export.c`.
- GHSA-g7xw-4cjw-2247: the `jwt-verify` CLI tool. `tools/` is not
  compiled.
- The other commits are json-c parity (we build on Jansson), the JWS JSON
  path, the `jwt-verify`/`jwt-generate` tools, docs and tests. v3.6.2 also
  ships our exact-alg pin (#340, `c948840`).

### N/A in this tree (v3.4.0 batch, after reachability analysis)

- `5fada81`: mbedTLS RSA short-signature OOB heap read (the only CVE-class bug
  of the batch). *Not present here.* The vendored mbedTLS backend is a full
  v4.0/PSA rewrite. It verifies with the length-aware high-level
  `mbedtls_pk_verify_ext(…, sig, sig_len)`. The upstream bug is in the
  low-level `mbedtls_rsa_*_verify` path, which this copy never calls. It is
  immune by construction.
- `2ef9500` (L4) mbedTLS ECDSA exact-size: same reason (the PK API checks the
  length).
- `60902e1` jwks clone-OOM free: already correct here (`jwks.c` frees the
  owned `item`, not the borrowed `jwk`).
- `18133e4` L6 (aud-as-array) / L7 (jti-after-signature): `C_AUTHZ` enables
  only `JWT_CLAIM_ISS` (it validates `iss` in C) and registers no `jti` callback.
- `18133e4` L8/L9, all `jwe:` commits: JWE is not compiled.
- `927e8d4` length-math hardening: the `kid` `strcpy`→`memcpy` is cosmetic
  (the JWKS is from the configuration). The `INT_MAX` encode guards are
  producer-side, and `C_AUTHZ` only verifies.

### Can we link upstream directly instead of vendoring? No

The new public API (`jwks_create_fromkey`, PEM→JWK, JWE) does not close the
structural divergence. The divergence is the `gbmem` allocator wiring
(`jwt-memory.c`), the `#include <gobj.h>` + `gobj_trace_json` coupling
(`jwt-verify.c`), the mbedTLS v4.0/PSA backend rewrite, the
`jwt-common.c`→`jwt-builder.c`/`jwt-checker.c` split, and the ArtGins-only
exports. These exports are `jwt_checker_verify2`, which returns the payload as
`json_t *` and is the basis of `verify_token()`, and `jwk_process_one` /
`jwks_item_add` / `jwks_item_free2`. None of them is public upstream.
Re-vendor stays the model.

The `json_t *` boundary is the load-bearing one. Ben confirmed it
(2026-06-28). Upstream does not expose `json_t` through `jwt.h`, on purpose.
It compiles against Jansson or json-c. Also, `jwt_t` is volatile
verify/generate state, on purpose. That is a post-v3 security decision, so
that callers stop treating it as a durable, ambiguous "generated-or-verified"
object. His suggested path is a verify callback plus the `jwt_get_*()`
getters, to rebuild a `json_t` on the side of the caller. The getters can give
back the full claims JSON *string*. That does not fit Yuneta, where `json_t *`
is a *first-order* native currency. `verify_token()` must return the parsed
claims as a live `json_t *`, and not parse a string again. Thus our
`jwt_checker_verify2()`, which returns `json_t *` directly, is a permanent
divergence. It is not something to send upstream or to refactor away.
Likewise, his public incremental keyring API (`jwks_create()` +
`jwks_load*()`, with PEM/DER/BIN since v3.5.0) is a possible future
replacement for our `jwk_process_one`/`jwks_item_add`/`jwks_item_free2`
exports on a re-vendor. But it replaces only the keyring wiring: the
`json_t *` return contract stays. Bottom line: re-vendor, never link.

### Worth contributing upstream

- Exact-alg pin (same-family downgrade): MERGED upstream (2026-06-28) as
  [benmcollins/libjwt#340](https://github.com/benmcollins/libjwt/pull/340)
  (merge `94e3cdf`), issue
  [#339](https://github.com/benmcollins/libjwt/issues/339). Up to v3.6.1,
  upstream did not have it on either verify path. In `__verify_config_post`
  (Compact), the both-set pinned path only checked config↔key consistency. In
  `try_candidate` (JSON Serialization), the explicit-key and `kid`-matched
  branches gated only on the family-granular `kty`, never on the declared
  `alg` of the key. Thus an RS512 token still verified against an
  RS256-pinned key. The new v3.6.0 RFC 8725 `jwt_checker_setalgs()` allowlist
  does not close it, because it is opt-in and inactive on the default pinned
  path. Ben Collins asked for a PR. It was merged from our fork
  `artgins/libjwt` (branch `exact-alg-pin-verify`). It fixes both paths
  (`libjwt/jwt-verify.c`). It adds `tests/jwt_alg_downgrade.c` (Compact and
  Flattened-JSON, each proven to fail without its fix) and a no-`alg` RSA
  fixture. The full suite gives 40/40.

  Follow-up available (not yet sent): the merged Compact hunk keeps a
  redundant second `else if` branch. The `config->alg ==
  JWT_ALG_NONE` branch above already pins the key-only-pinned case exactly.
  Thus the second branch is dead code that nothing can reach, and it is the
  source of the 2 uncovered lines of codecov on the PR. If it collapses to the
  single `config->alg` check, the suite keeps 40/40 and the patch coverage goes
  back to 100% (`followup-collapse-dead-branch.diff` in the workspace). The
  vendored copy here carries the same redundant branch (`src/jwt-verify.c`),
  and it can take the same neutral cleanup.

  The patch, the issue text, and the PR description lived in the
  security-review workspace `/yuneta/development/projects/libjwt-review/`
  (machine-local, outside both git trees on purpose). That workspace was not
  on the machine of the 2026-11-16 review. Thus it is possible that the
  follow-up diff is lost. It is small enough to make again from the
  description above.
- mbedTLS v4.0/PSA backend: now moot. Per Ben, upstream moved to the
  MbedTLS PSA API in v3.5.0 (it works with 3.x and 4.1). Thus our PSA
  rewrite is no longer a differentiator. It is immune by construction to
  `5fada81` in the same way. A future re-vendor from ≥v3.5.0 inherits the
  upstream PSA backend.

### Still to do: see the repo [`TODO.md`](../../../TODO.md)

- Broaden the forgery test toward the rest of the upstream `jwt_security.c`
  (malformed JWKs, more `none`/`null` variants).
- N/A here: the JWKS-curl `Content-Length`/`atol` fixes. `jwks-curl.c` is
  disabled in CMakeLists, and the JWKS is loaded from a configuration attr,
  not from the curl path of libjwt. Classify them again if the curl backend is
  enabled one day.
- Periodic re-vendor from upstream, and not a longer backport list. The
  procedure is below.

### Re-vendor procedure

The upstream clone is machine-local (`/yuneta/development/projects/libjwt.original`
on the machine of the first reviews). Where it is missing, clone it. This tree
compiles the OpenSSL and MbedTLS backends on Jansson, not gnutls, json-c or
`jwks-curl.c`. To examine the drift again and pull fixes:

```bash
# 1. Refresh upstream and re-derive the gap from the last analyzed commit.
git clone https://github.com/benmcollins/libjwt.git libjwt.original  # if absent
cd libjwt.original && git pull
git log --oneline v3.8.0..HEAD         # last analyzed: v3.8.0
                                       # vendored base: 375e539 (v3.2.1+2)

# 2. For each new commit, classify: SEC / BUG / N-A (file not in our subset).
#    Map hunks carefully — the vendored copy SPLIT jwt-common.c into
#    jwt-builder.c + jwt-checker.c, so every jwt-common.c hunk lands in BOTH.

# 3. Backport SEC/reachable-BUG hunks by hand into kernel/c/libjwt/src/**,
#    matching local style (tabs; builder/checker use 4-space). Keep the
#    backend-agnostic headers free of an OpenSSL dependency.

# 4. Rebuild + run the regression test.
cd /yuneta/development/yunetas/kernel/c/libjwt/build && make install
cd /yuneta/development/yunetas/tests/c/libjwt/build && cmake .. && make \
    && ./test_jwt_alg_confusion

# 5. Update the recorded base commit + the lists above, and relink the JWT
#    consumers (auth_bff + the agent's OAuth2 path) per TODO.md.
```

When the backport list grows too large to port by hand, a full re-vendor is
the cleaner move. Put in the upstream OpenSSL+Jansson sources at a new tagged
release. Apply again the local split and the `// ArtGins` additions (for
example `jwt_checker_verify2`, `jwk_process_one`/`jwks_item_add` exports).
Then run the test again.

## Public API

The shipped headers expose:

```c
#include <jwt.h>
```

Key entry points (declared in `src/jwt.h`):

| Function                  | Purpose                                                   |
|---------------------------|-----------------------------------------------------------|
| `jwt_new` / `jwt_free`    | Construct / destroy a JWT object                          |
| `jwt_add_grant*`          | Add a claim                                               |
| `jwt_set_alg`             | Choose the signing algorithm (HS256, RS256, ES256, …)     |
| `jwt_encode`              | Sign and serialize to compact form                        |
| `jwt_decode` / `jwt_parse`| Parse + verify signature                                  |
| `jwks_*`                  | Load/iterate JWKS (`jwks.c`, `jwks-curl.c`)               |

The runtime entry that the `C_AUTHZ` of Yuneta calls is `jwt_parse()` in
`src/jwt-verify.c`.

## Build

It is built as part of the standard `yunetas build` flow. It produces
`outputs/lib/libjwt.a`, which is linked statically into the kernel.

## License

MPL-2.0 (Mozilla Public License 2.0), inherited from upstream libjwt.
Every file here carries the `SPDX-License-Identifier: MPL-2.0` header and the
upstream maClara, LLC copyright. The Yuneta-specific changes to these
files (the alg-confusion guards, `jwt_checker_verify2`, the mbedTLS v4.0/PSA
backend, the v3.4.0 backports, …) are also MPL-2.0 and source-available
in this public repo. See the source header of each file for attribution.

> Note: libjwt 1.x was LGPL-2.1. The project changed its license to MPL-2.0 on
> the 2.x/3.x branch that this copy derives from. Earlier revisions of this
> README said "LGPL-2.1+". That was stale. Also, because LGPL-2.1+ is copyleft
> on linking, it overstated the obligations. MPL-2.0 is *file-level* weak
> copyleft. It imposes no new license on the surrounding code and no
> LGPL-style relinking requirement for static builds. It requires only that
> these files stay MPL-2.0 and that their source stays available.

The rest of Yuneta is MIT. The top-level `LICENSE.txt` of the repo records the
MPL-2.0 carve-out for `kernel/c/libjwt/*`.
