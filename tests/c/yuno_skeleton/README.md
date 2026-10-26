# yuno_skeleton test

The templates of `yuno-skeleton` must build and run: no ctest compiled them,
and two fixes of theirs had no test. `test_yuno_skeletons.sh` instantiates
each template with `yuno-skeleton` (its answers on stdin, as a person types
them) and builds it against the SDK in `outputs/`:

- `yuno_standalone`, with the SERVICE subscription block and the
  `subscriber` attr in its root gclass (it had neither up to 7.28.6), and a
  `gclass_child` and a `gclass_service` generated into its `src/` and added
  to its sources: built with no warning, and run
  three seconds -- its gclass's timer, a pure child, fires (`Timeout` on
  stdout). Run again with the `gclass_service` as its service (played: its
  timer is armed at the play) and the `gclass_child` as a second service
  (its timer is armed at the start): each one's timer reaches its
  `ac_timeout` (`Timeout`, `Timeout child`). Red with the timer of those two
  templates a plain child, as it was up to `1cab6b1cc`.
- `yuno_citizen`, with the SERVICE subscription block in its root gclass
  and the license in its README (no empty `Name:`), built with no warning.
  Red with the template of 7.28.6, whose blocks were both commented out.
- `c_project`, with the license in its README, built with no warning.
- `js_gclass` and `js_gclass_service`, loaded by node against this tree's
  gobj-js (its `src/`, so no npm build is needed): the child is created under
  the yuno and the service with `gobj_create_service()`, each one's timer
  fires (`Timeout`, `Timeout service`), and nothing is logged as an error.
  Red with the `js_gclass` of 7.28.6, which imported from `"yunetas"`.
  Without `node` in `PATH` the run is skipped and says `SKIP`.
- `project` and `project_commercial`: each gets a `yuno_citizen` made in its
  `yunos/` and added to `yunos/CMakeLists.txt`, and is built from the
  project root with no warning; the `LICENSE` is the one of its kind (MIT,
  proprietary) and the README says the license asked for.

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

No template holds a `{{marker}}` that nothing fills: `yuno-skeleton` says
each one on stderr, and the test fails on any (red with the README templates
of 7.28.6 and their `{{yunoname}}`).

`KEEP_WORK=1` keeps the work directories (`/tmp/test_yuno_skeletons.*`,
`/tmp/test_cloners.*`).

## Run

```bash
ctest -R yuno_skeleton --output-on-failure --test-dir build
```
