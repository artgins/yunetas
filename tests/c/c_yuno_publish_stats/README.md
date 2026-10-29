# c_yuno_publish_stats test

Tests the `publish-stats` command of `C_YUNO`: a yuno that sends the stats of
one of its services to its requester every period, from its own periodic
timer, as the answers of a stats request (`EV_MT_STATS_ANSWER`, by
`EV_SEND_IEV` to the requester) with the routing of the request reversed.

The yuno runs at `timeout_periodic` 100 ms and `publish_stats_ttl` 300 ms
(`main.c`); the test gclass is the requester (a service), and asks
`publish-stats stats_service=__yuno__ period=100` as a peer does, with an
ievent stack and a mark of its own in `__md_iev__`.

## What it pins

1. **A**, `ttl=450`: a few publications, then it ends (*"publish-stats
   ended"*, expired). Each one is an `EV_MT_STATS_ANSWER` with the stats in
   `data`, the mark of the request, and the route reversed (its
   `dst_service` is the requester);
2. **C**, no `ttl`: the attribute `publish_stats_ttl` ends it;
3. **B**, `ttl=-1`: it does not end on its own; `stop=1` ends it, and
   nothing comes after;
4. **D**: a service that does not exist is refused;
5. **E**: a requester that is not a service is refused;
6. **F**: a `stats_service` longer than `NAME_MAX` (no service is named so)
   is refused, `stop=1` too: it was cut to fit the key of the publication,
   in silence.

The counts allow for a slow machine (A: 3 to 6 in 450 ms at 100 ms).

## Run

```bash
ctest -R test_c_yuno_publish_stats --output-on-failure --test-dir build
```
