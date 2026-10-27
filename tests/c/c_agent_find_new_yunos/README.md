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

## Run

```bash
ctest -R test_c_agent_find_new_yunos --output-on-failure --test-dir build
```
