# c_ievent_cli_operator test

Tests how `C_IEVENT_CLI` (`kernel/c/root-linux/src/c_ievent_cli.c`) takes the
operator of a request of its server: `verify_operator`, `service_principal`,
and the local method `verify_jwt` of `C_AUTHZ`.

A controlcenter forwards an operator's command to an agent with the
operator's token in `__operator_jwt__`. With `verify_operator` the agent
verifies it and takes its email as `__username__`, instead of the
`__username__` the controlcenter says.

One yuno holds a real `C_IEVENT_CLI` (over `C_IOGATE` -> `C_CHANNEL` ->
`C_WEBSOCKET` -> `C_TCP`, `service_principal` *the-controlcenter*), a
`C_AUTHZ` whose `jwks` has one HS256 key of the issuer
`https://idp.test.example/realms/test` (store under
`/tmp/test_c_ievent_cli_operator`), and a raw websocket server on
`ws://127.0.0.1:7891` whose frames the test writes by hand. The client's
identity card must say `knows_operator_identity`. The server answers it with
the ACK and six commands `whoami` to the tester, each saying `__username__`
*liar@example.com*:

0. a good token of *alice@example.com*;
1. a token signed with another key;
2. an expired token;
3. a token whose email is not verified;
4. no token;
5. a token that is not a string.

- `test_c_ievent_cli_operator` (`verify_operator: true`): request 0 runs as
  *alice@example.com*, request 4 as *the-controlcenter*; 1, 2, 3 and 5 never
  reach the command, are answered with an error that names the operator
  (*"Token failed verification"*, *"Token has expired"*, *"Email not
  verified"*, *"not a string"*), and are ONE warning each, *"Request of the
  server refused: its operator is not verified"*.
- `test_c_ievent_cli_operator_unverified` (`OPERATOR_UNVERIFIED`, port 7892,
  `verify_operator: false`): the six run as *liar@example.com*.

In both, no command sees `__operator_jwt__`, and no log line carries a token
(`main.c` looks for each of them in every line). The sixth answer ends the
test.

A `C_AUTHZ` opens its treedb, and loads its `jwks`, only when its
`tranger_path` is a directory that exists: the test makes it. Without one, it
verifies no token ("No OAuth2 Issuer found"), which fails closed.

Up to 7.30.5 the token reached the command, the identity card did not say
`knows_operator_identity`, and every request ran as *liar@example.com*
(red: both builds fail).

## Run

```bash
ctest -R test_c_ievent_cli_operator --output-on-failure --test-dir build
```
