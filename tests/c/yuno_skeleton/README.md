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
- `js_gclass`, loaded by node against this tree's gobj-js (its `src/`, so no
  npm build is needed): it registers, its timer fires (`Timeout`), and
  nothing is logged as an error. Red with the template of 7.28.6, which
  imported from `"yunetas"`. Without `node` in `PATH` the run is skipped and
  says `SKIP`.

A build that fails or warns fails the test: red with `MSGSET_INTERNAL_ERROR`
in the standalone template, the name it had up to `3e140bb02`.

`test_cloners.sh` runs the three cloners -- `yclone-gclass`, `yclone-project`
and `ymake-skeleton`, one engine (`utils/c/yclone-project/clone_tree_dir.c`)
-- on a standalone yuno made by `yuno-skeleton`. Each check is a bug they had
up to 7.28.6 (21 checks fail against those binaries):

- `yclone-gclass time clock` leaves `C_TIMER`, `set_timeout` and `EV_TIMEOUT`
  as they were (a key is replaced as a whole word), reports `C_TIMER` as left
  untouched, and the clone builds.
- `yclone-project` to a name that holds the old one (`sktcln` ->
  `sktcln_two`) replaces it once; a binary is copied byte for byte, a
  script keeps its mode, a symlink is kept, `.git` and `build` are not
  cloned; the clone builds and says its new role; a destination inside the
  source is refused.
- `ymake-skeleton` makes a template (`+rootname+` names, `{{yunorole}}`
  markers, `_tmpl` on text files only) that `yuno-skeleton` instantiates and
  that builds: the round trip.

`KEEP_WORK=1` keeps the work directories (`/tmp/test_yuno_skeletons.*`,
`/tmp/test_cloners.*`).

## Run

```bash
ctest -R yuno_skeleton --output-on-failure --test-dir build
```
