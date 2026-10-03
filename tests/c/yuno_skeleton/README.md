# yuno_skeleton test

The templates of `yuno-skeleton` must build and run: no ctest compiled them,
and two fixes of theirs had no test. `test_yuno_skeletons.sh` instantiates
each template with `yuno-skeleton` (its answers on stdin, as a person types
them) and builds it against the SDK in `outputs/`:

- `yuno_standalone`, with a `gclass_child` and a `gclass_service` generated
  into its `src/` and added to its sources: built with no warning, and run
  three seconds -- its gclass's timer, a pure child, fires (`Timeout` on
  stdout). Run again with the `gclass_service` as its service (played: its
  timer is armed at the play) and the `gclass_child` as a second service
  (its timer is armed at the start): each one's timer reaches its
  `ac_timeout` (`Timeout`, `Timeout child`). Red with the timer of those two
  templates a plain child, as it was up to `1cab6b1cc`.
- `yuno_citizen`, built with no warning.

A build that fails or warns fails the test: red with `MSGSET_INTERNAL_ERROR`
in the standalone template, the name it had up to `3e140bb02`.

`KEEP_WORK=1` keeps the work directory (`/tmp/test_yuno_skeletons.*`).

## Run

```bash
ctest -R yuno_skeleton --output-on-failure --test-dir build
```
