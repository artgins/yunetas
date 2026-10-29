# c_agent_find_new_yunos test

Tests the rows of the agent's **`find-new-yunos`** command, on the agent's own
treedb schema and the agent's own source
(`yunos/c/yuno_agent/src/find_new_yunos.c`, compiled into the test).

`find-new-yunos` lists one `create-yuno` command for each yuno whose binary or
configuration has a newer version than the release it runs. The old release
stays the **primary** row until `deactivate-snap` promotes the new one. So
after a `find-new-yunos create=1` that was not promoted (a resumed upgrade),
the old row still matches. Up to 7.25.4 the preview listed that instance as
"would be created", and `create=1` failed on it with *"Yuno already exists"*.
Each row now says whether its instance at the new release exists.

## What it pins

1. a yuno whose new release is already registered comes back with
   `registered` true;
2. a yuno with a newer binary and nothing registered comes back with
   `registered` false, with the `create-yuno` command to run;
3. a yuno with nothing newer does not come back;
4. a `yuno_multiple` row is judged by its own id: another instance of the same
   role and name registered at the new release does not answer for it;
5. the fixture links each yuno to its realm, binary and configuration as
   `create-yuno` does, so a new release links a child whose fkey already
   names the yuno's id (the configuration it keeps, a binary installed later
   that inherited the ref): that logs nothing. Up to 7.25.4 it warned
   *"Parent ref already in child fkey, skipping duplicate"* on every
   `create-yuno` of a new release. A real duplicate (yuno_c linked twice to
   its binary) still warns, once: the strict FIFO of expected logs pins both.

6. what `create=1` runs (`find_new_yunos_preview()`, which
   `cmd_find_new_yunos` calls): only the rows not registered, yuno_b and
   yuno_m2; the preview keeps all four, the two registered marked *"already
   registered, pending promotion (deactivate-snap): ..."*;
7. the release name in `create-yuno`'s `NAME_MAX` buffer
   (`build_yuno_release_name()`): one that fits exactly, and one a byte
   longer, which is refused (logged) instead of cut. A cut name was stored
   as the pkey2 up to 7.25.4, and find-new-yunos offered that yuno again at
   every call.

8. the binary and the configuration at a version
   (`find_binary_version()`, `find_configuration_version()`, which
   `create-yuno` and `update-binary` look up): the instance of THAT version
   (role_b 2.0.0, not its primary 2.1.0), and none for a version not
   installed. Up to 7.28.9 a version that matched nothing gave the last
   instance listed.

9. what a restart of the node does with a running yuno (`restart_plan.c`):
   its launch signature holds the saved columns of its row, binary,
   configuration, realm and public services -- no runtime, no hook -- so
   running does not change it; and the decision keeps an unchanged yuno,
   restarts a changed one, and stops one that is gone or disabled (a
   disabled yuno does not run).

10. the restart compares against the signature noted at the LAUNCH
    (`note_yuno_launch()`, which `run_yuno()` calls: it writes
    `bin/<role>^<name>.launch.json`, read back by
    `yuno_launched_signature()`; none before a launch), not the treedb of when
    the restart begins: after an `update-config` of the version in use the
    treedb agrees with itself (the 7.28.11-7.29.1 comparison, "keep") while
    the noted launch does not ("restart"); a yuno with no launch noted is
    restarted. `must_play`, `traced`, `start_priority` and the `yunos` fkeys
    of the binary and configuration are left out of the signature: changing
    them keeps the yuno.

11. the shapes of a binary version `install-binary` warns of: the old
    `x.y.z.N` passes only with N `0` (`1.9.1.8` compares newer than
    `1.9.1-7.27.11`; up to 7.29.1 it passed).

## Run

```bash
ctest -R test_c_agent_find_new_yunos --output-on-failure --test-dir build
```
