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
   The same as `EV_ON_MESSAGE` with a `__temp__` that is a string or an
   integer: each replaced, one ERROR each (a fix that replaced only a list
   dies here). One that is a dict without the channel gets the channel
   written into it, its other keys kept, and no log.
2. A drop of a channel the gate does not have (*"channel NOT FOUND"*, said),
   then 2500 sends (`EV_SEND_MESSAGE`) with no channel to go to: *"No channel
   FOUND to send"* is said for the 1st, the 1001st and the 2001st, with
   `repeated` the ones not said since the last one (`0`, `999`, `999`). The
   drop counts in a run of its own: with one count for both, the first send
   of the outage was not said, and its end counted 2501 sends. Up to 7.30.26
   the count of its brake never moved: 2500 ERRORs, each with `repeated` `0`.
3. A send that finds a channel (the test itself, named in the `__temp__` of
   the send) ends the run: ONE INFO, *"Sends with no channel ended"*, with
   `without_channel` 2500 and `not_said` 499. The count starts again: the
   next send with no channel is said at once (`repeated` 0), and a send with
   a channel ends that run too (1, 0).
4. The same brake for 1500 sends to all channels (`__send_type__` 1) with
   none open (said at the 1st and the 1001st); then a send to all that finds
   an open channel (a `C_CHANNEL` of the gate whose bottom is the test)
   ends the run: 1500, 499. Then 1200 drops of a channel the gate does not
   have, in the run of the first drop (*"channel NOT FOUND"* said at its
   1001st, `repeated` 999); that run is ended by the gate's stop, an INFO of
   its own, *"Drops of a channel not found ended"*: 1201, 200. Up to 7.30.26
   both said every one, the send to all with a stack, and nothing said the
   end of a run.
5. `stats` of the gate: `txMsgsNoChannel` 4001 (2500 + 1 + 1500; a drop is
   not a message), `txMsgs` 3; after `stats __reset__`, `txMsgsNoChannel` 0.

The log is checked in order: those fourteen ERRORs and nothing else; and the
four INFOs of the ends of the runs, in order, with their figures.

Mutants killed: a drop counted in the run of the sends; `txMsgsNoChannel`
not reset by `__reset__`; a send to all that finds a channel and does not
end the run.

```
ERROR: {"msg": "__temp__ of a message is not a dict, replaced", "event": "EV_ON_MESSAGE", ...}
...
ERROR: {"msg": "No channel FOUND to send", "event": "EV_SEND_MESSAGE", "repeated": "999", ...}
```

## Run

```bash
ctest -R test_c_iogate_channel --output-on-failure --test-dir build
```
