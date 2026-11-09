# authz_login test

Tests who `C_AUTHZ` (`kernel/c/root-linux/src/c_authz.c`) lets in at a
login, and what its refusals write.

A peer with no token and no password logs in by its address alone: from
`127.0.0.*` as the local user `yuneta`, with its real roles, or from an
address of the yuno's `allowed_ips`. A proxy on the same host (nginx in front
of a `C_IEVENT_SRV`) connects from `127.0.0.1`, so every client of the proxy
is such a peer. `trust_peer_ip` false turns that off.

One yuno holds a real `C_AUTHZ` (store under `/tmp/test_authz_login_store`,
or `/tmp/test_authz_login_no_peer_ip_store`) whose `jwks` has a key of kid `*` and, after it, one HS256 key
of the issuer `https://idp.test.example/realms/test`, with the roles `root`
(`service: "*"`) and `operator` (`service: checked`) and the users `yuneta`
(root) and alice (operator). The driver signs the tokens, and logs in with
`gobj_authenticate()` from a peer of its own, of the address the case says.
A session opened is closed with `EV_ON_CLOSE`.

`test_authz_login` (`trust_peer_ip` true, the default):

1. `yuneta`, no token, from `127.0.0.1`: in, a superuser;
2. `yuneta`, no token, from `10.1.2.3`: refused, *"Without JWT/passw only
   localhost is allowed"*, ONE warning;
3. `yuneta`, no token, from `10.1.2.4`, an address of `allowed_ips`: in.

`test_authz_login_no_peer_ip` (`trust_peer_ip` false): the same three are
refused, *"Without JWT/passw the peer address is not trusted"*, ONE warning
each.

In both:

4. alice, a good token, from `127.0.0.1`: in;
5. alice, a token signed by another key: refused, *"Authentication failed:
   Invalid token"*, ONE warning;
6. a good token with no `email`: refused, *"Username must be an email
   address"*, a warning and no error;
7. a good token with no `email_verified`: refused, *"Email not verified"*, a
   warning and no error;
8. alice, a good token that expired an hour ago, with a claim of its own:
   refused;
9. a token with no `iss`, signed by the key of the test issuer: refused
   (*"Invalid 'iss' claim"*), a warning and no error;
10. alice, a token expired AND not valid yet: refused, and its `status` names
    both claims, *"Failed claims: exp, nbf"*;
11. the key of kid `*`: refused at start up with ONE error, *"A jwks key with
    kid '*' is not allowed"*, the only error of the start up, and the key
    after it loads (case 4). Since 7.2.0 such a key validated no token, and
    nothing said so;
12. `add-jwk kid=*`: `-1`, its comment says why, nothing logged;
13. `remove-jwk kid=*`: `-1`, *"... it was never loaded, nothing to remove"*,
    nothing logged (up to 7.30.18: `0` "deleted", and three ERRORs, one with
    a stack);
14. the two keys of a rotation (two entries of the `kid`
    `https://idp.test.example/realms/rotate`): a token of the second key logs
    in; `remove-jwk` of the kid answers `0`, *"(2 keys)"*, its `data` the two
    records, nothing logged; then a token of either key is refused (up to
    7.30.18 only the first key went, and the second went on validating).

And no log line carries a token of the test, nor the claims of the expired
one, and no memory is left. Up to 7.30.14 there was no `trust_peer_ip`, the
warning of case 5 (and 8) carried the token whole, cases 6, 7 and 9 logged
an ERROR of the reader before their warning, libjwt traced the claims of
case 8, unverified, and the status of case 10 was *"What JWT_CLAIM_???"*.

`test_authz_store_full` and `test_authz_store_full_exit` (store
`/tmp/test_authz_store_full_store`, `..._exit_store`): a login whose history
the store cannot write, the disk full by a file size limit of 0
(`RLIMIT_FSIZE`, `SIGXFSZ` ignored).

- `test_authz_store_full`, `Authz.on_critical_error` `0` (the agent's):
  alice logs in; with the disk full she logs in again, the yuno goes on, ONE
  *"Login history cannot be saved"*; at once again, nothing logged (the
  record is not tried inside the wait of 1 s); the disk back, 1.2 s later,
  she logs in with no warning and ONE *"Login history saved again"*. Up to
  7.30.18 the store's `on_critical_error` was `LOG_OPT_EXIT_ZERO`, fixed: the
  second login exited the yuno (red: *"the yuno exited before the end of the
  test, at the login with the disk full"*).
- `test_authz_store_full_exit`, the default (`2`): the second login exits the
  yuno with code 0, its line says `"exiting": "0"` (an `atexit()` handler
  checks it was there).

```bash
ctest -R authz_login --output-on-failure --test-dir build
```
