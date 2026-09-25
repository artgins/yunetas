# c_ievent_srv_peer_subs test

Tests what a remote peer may put in a subscription, and what it may hold,
through `C_IEVENT_SRV` (security review 19, SDK 7.25.5).

One yuno holds both sides: a `C_IEVENT_SRV` gate on `ws://127.0.0.1:7795`
whose channels allow `max_subscriptions` 4 and `max_subscription_size` 512,
a `publisher` service, two local subscribers (`strip` and `local`, of the
gclass `C_TEST_PEER_SINK`) and two `C_IEVENT_CLI`: `alice`, whose frames the
test writes by hand, as a hostile peer would, and `bob`, a plain client.

1. The subscribers of `EV_TEST_FEED`, in this order: `strip` (local, with a
   `__local__` that removes `secret` and a `__global__` that adds `sink`),
   `alice` (a `__global__` with `topic_name: users`, a forged `node`,
   `__md_iev__` and `__service__`, a `__local__` with `secret`, and a stray
   key), `bob` (a `__filter__` on `topic_name: devices` and a `__global__`
   with `tag`) and `local` (plain). `alice` also subscribes `EV_TEST_OPEN`
   with a `__global__` holding `gbuffer: 1`. The publisher publishes
   `{topic_name: devices, node: dev1, secret}`:
   - `bob` and `local` get the event whole, `bob` with his `tag`, `local`
     with no `__md_iev__`;
   - `strip` gets it without `secret`;
   - `alice` gets her own `topic_name`, and the `secret` (a peer may not
     remove keys);
   - the publisher's kw comes back as it went;
   - the keys `alice` may not set are logged ONCE.

   Up to 7.25.4 the publish shared one kw: `bob` got nothing (alice's
   `topic_name` failed his filter), `local` got the forgery without the
   secret and alice's `__md_iev__`, and the publisher's kw was changed.
2. `alice` holds 3 subscriptions (the third, `EV_TEST_BIN`, below) and asks
   4 more: 1 is accepted, 3 refused (`max_subscriptions`, logged once); 2 with a `__filter__` over 512 bytes
   are refused (logged once); 5 withdrawals of nothing, each with a 2 KB
   key, are ONE warning, capped (`main.c` counts the lines and measures
   them); 3 subscriptions of `EV_TEST_SECRET`, which only `bob` may read,
   are ONE error.
3. `bob` withdraws his subscription with its `__global__`, as
   `C_IEVENT_CLI` does it: it goes. Up to 7.25.4 the server compared it with
   the `__global__` it had stored, which carries its own back-metadata, and
   the subscription stayed until the channel closed.
3c. `EV_TEST_BIN` carries a gbuffer to two remote subscribers (`alice`,
   `bob`: each subscription gets a `kw_twin()` of the kw, sharing the
   gbuffer, which the gate serializes and releases) and one local (`local`,
   the kw itself). Each gets the bytes, and when the publish returns the
   gbuffer holds only the publisher's own reference: no twin releases it
   twice, none keeps it.
4. `alice` sends a command, a stats request and an event (and one more
   subscription), each with a `__username__` of her own (`admin`, `bob`) and,
   in the routing stack, a forged `__username__`, `input_channel` and
   `input_service`. The service gets the gate's values, never hers. Up to
   7.25.4 `kw_set_dict_value()` kept a key that was already there, so the
   peer's `__username__` reached the command, the stats and the event -- and
   the authz of `command_parser` reads that key (fixed in `kwid.c` by
   fdf138144; this test holds it).
5. The publisher publishes `EV_TEST_OPEN`: `alice`'s subscription without a
   filter gets it, without `gbuffer`. Up to 7.25.4 the gate took her integer
   for a gbuffer when it serialized the event back to her, and the yuno
   crashed.

## Run

```bash
ctest -R test_c_ievent_srv_peer_subs --output-on-failure --test-dir build
```
