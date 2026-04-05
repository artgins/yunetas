# c_tcps2 test

Second-generation TLS tests for `C_TCP_S`. Uses the updated connection / state API (no legacy child-tree filter) and includes the large-payload test (`test4`) that validated the mbedTLS `bad_record_mac` fix.

## Run

```bash
ctest -R test_c_tcps2 --output-on-failure --test-dir build
```
