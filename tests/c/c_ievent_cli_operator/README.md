# c_ievent_cli_operator test

Tests how `C_IEVENT_CLI` (`kernel/c/root-linux/src/c_ievent_cli.c`) takes the
operator of a request of its server: `verify_operator`, `service_principal`,
and the local method `verify_jwt` of `C_AUTHZ`.

A controlcenter forwards an operator's command to an agent with an operator
assertion in `__operator_assertion__`: a JWT it signs for that node (`aud` =
the node uuid), naming the operator (`sub`). With `verify_operator` the agent
verifies it (its `C_AUTHZ`'s `assertion_jwks`, local method `verify_jwt` with
an `audience`) and takes the `sub` as `__username__`, instead of the
`__username__` the controlcenter says.

One yuno holds a real `C_IEVENT_CLI` (over `C_IOGATE` -> `C_CHANNEL` ->
`C_WEBSOCKET` -> `C_TCP`, `service_principal` *the-controlcenter*), a
`C_AUTHZ` whose `jwks` has one HS256 login key of the issuer
`https://idp.test.example/realms/test` and whose `assertion_jwks` has one EC
P-256 key of `test-controlcenter` (`assertion_time_leeway` one hour; store
under `/tmp/test_c_ievent_cli_operator`), and a raw websocket server on
`ws://127.0.0.1:7891` whose frames the test writes by hand. The client's
identity card must say `knows_operator_assertion`. The server answers it with
the ACK and thirteen commands `whoami` to the tester, each saying
`__username__` *liar@example.com*:

0. a good assertion of *alice@example.com*;
1. an assertion signed with another key;
2. an assertion expired two hours ago (past the leeway);
3. an assertion for another node;
4. no assertion;
5. an assertion that is not a string;
6. a good assertion of *bob@example.com*, a user of the `C_AUTHZ` that is
   disabled (seeded in its `initial_load`);
7. a good LOGIN token of alice as the assertion;
8. an assertion that lives an hour (`max_assertion_lifetime` is 300 s);
9. an assertion with an empty `sub`;
10. a good login token of alice as `__operator_jwt__` (what 7.30.6 took), and
    no assertion;
11. an assertion of alice expired a minute ago (within the leeway);
12. an assertion of `yuneta`, a local user of the signer.

- `test_c_ievent_cli_operator` (`verify_operator: true`): 0 and 11 run as
  *alice@example.com*, 12 as *yuneta*, 4 and 10 as *the-controlcenter*; the
  rest never reach the command, are answered with an error that names the
  operator, and are ONE warning each, *"Request of the server refused: its
  operator is not verified"*.
- `test_c_ievent_cli_operator_unverified` (`OPERATOR_UNVERIFIED`, port 7892,
  `verify_operator: false`): the thirteen run as *liar@example.com*.

In both, no command sees `__operator_assertion__` or `__operator_jwt__`, and no
log line carries a token (`main.c` looks for each of them in every line). The
thirteenth answer ends the test.

A `C_AUTHZ` opens its treedb, and loads its keys, only when its
`tranger_path` is a directory that exists: the test makes it. Without one, it
verifies no token ("No OAuth2 Issuer found"), which fails closed.

The test keys are made for this test and good for nothing else.

## Run

```bash
ctest -R test_c_ievent_cli_operator --output-on-failure --test-dir build
```
