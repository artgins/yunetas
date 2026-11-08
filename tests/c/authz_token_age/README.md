# authz_token_age test

Tests the age of a login token in `C_AUTHZ`
(`kernel/c/root-linux/src/c_authz.c`): `max_token_age` and
`token_age_exempt_users`.

`max_token_age` is Yuneta's own limit, whatever the identity provider lets a
token live: a login token opens a session only if it was issued (its `iat`)
no more than `max_token_age` seconds ago. `0` is no limit.

One yuno holds a real `C_AUTHZ` (store under `/tmp/test_authz_token_age_store`)
whose `jwks` has one HS256 key of the issuer
`https://idp.test.example/realms/test`, with the role `operator` of the
service `checked` and the users alice, bob and old-agent (`@example.com`).
The driver signs the tokens, and logs in with `gobj_authenticate()` from a
peer of its own (it holds what `C_AUTHZ` writes in the src of a session). A
session opened is closed with `EV_ON_CLOSE`.

`test_authz_token_age` (`max_token_age` 3600, old-agent exempt):

1. alice, issued a minute ago: a session;
2. alice, issued two hours ago: refused, *"The token is too old"*, ONE
   warning *"Authentication failed: token too old"*;
3. alice, no `iat`: refused, *"The token has no iat"*, ONE warning;
4. old-agent, issued ten years ago: a session (exempt);
5. bob, issued 59 minutes ago: a session;
6. the local method `verify_jwt` with no audience, alice issued two hours
   ago: refused, no log;
7. `verify_jwt`, alice issued a minute ago: good.

`test_authz_token_age_unlimited` (`max_token_age` 0, the default): the same
seven, all good, no warning.

In both, no log line carries a token of the test, and no memory is left.

```bash
ctest -R authz_token_age --output-on-failure --test-dir build
```
