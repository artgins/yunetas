# c_iogate_channel test

Tests what `C_IOGATE` (`kernel/c/root-linux/src/c_iogate.c`) writes in a
message it publishes -- the channel it came in by, in `__temp__` -- and what
it says when it has no channel to send a message to.

One yuno holds `C_TEST_IOGATE`, with a `C_IOGATE` pure child (it publishes to
the test) that has no channel below it. The test is the channel itself: it
sends the gate its messages as a channel would.

1. A message whose `__temp__` is not a dict (`[1094795585]`), as
   `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` and `EV_ON_IEV_MESSAGE` (with
   `__iev_event__` `EV_ON_MESSAGE`): each is published with a `__temp__` that
   names the channel it came in by (`channel` its name, `channel_gobj` its
   pointer), and the gate says it once, an ERROR *"__temp__ of a message is
   not a dict, replaced"* naming the event. `__temp__` is local: a peer's is
   dropped when its frame is decoded (`iev_create_from_gbuffer()`), so only
   code of this yuno can put one there. Up to 7.30.26 each was two ERRORs
   (*"path holds a value that is not a json dict, cannot create"*) and the
   list reached the subscriber, which took `1094795585` for the channel.
2. 2500 sends (`EV_SEND_MESSAGE`) with no channel to go to: *"No channel
   FOUND to send"* is said for the 1st, the 1001st and the 2001st, with
   `repeated` the ones not said since the last one (`0`, `999`, `999`). A send
   that finds a channel starts the count again. Up to 7.30.26 the count of its
   brake never moved: 2500 ERRORs, each with `repeated` `0`.

The log is checked in order: those seven ERRORs and nothing else.

```
ERROR: {"msg": "__temp__ of a message is not a dict, replaced", "event": "EV_ON_MESSAGE", ...}
...
ERROR: {"msg": "No channel FOUND to send", "event": "EV_SEND_MESSAGE", "repeated": "999", ...}
```

## Run

```bash
ctest -R test_c_iogate_channel --output-on-failure --test-dir build
```
