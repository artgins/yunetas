# c_subscriptions test

Tests subscribe / unsubscribe / publish semantics of the GObj framework. Verifies that events reach every subscriber and that unsubscribes (explicit and automatic on `gobj_destroy`) clean up properly.

- `test1`, `test2`: subscribe, publish and unsubscribe on a timer, with and
  without a `__filter__`; a repeated subscription is overridden.
- `test3`: the HARD subscriptions (`__hard_subscription__` in `__config__`).
  A repeated hard subscription is one: the second `gobj_subscribe_event()`
  logs a warning and returns the one there, and so does a plain subscription
  that matches it; each event arrives once (up to 7.25.4 a second one was
  made with no log, and each event arrived twice).
  `gobj_unsubscribe_event()` leaves a hard subscription and logs a warning;
  `gobj_unsubscribe_list()` with `force` removes it. A plain subscription
  repeated is still overridden, and a hard one over a plain one replaces it.
  A repeated `__own_event__` subscription, and a repeated
  `__rename_event_name__` one with a `__global__`, are overridden too (one
  subscription, each event once), and `gobj_unsubscribe_event()` with the
  same kw removes them (up to 7.25.4 each repeat was a second subscription,
  and the withdrawal found nothing).
  A renamed subscription over a plain one, and two renames of one event,
  are two subscriptions (each event arrives once under each name), and each
  kw withdraws only its own; a renamed `__own_event__` subscription repeated
  is one (7.25.5 found the plain one, or the other rename, as a repeat of
  the renamed kw and replaced it).
  `gobj_unsubscribe_list()` removes the subscription it is given: with a
  plain and a filtered subscription coexisting, a stale plain one (already
  removed) removes nothing and is logged as a warning, and removing the live plain one
  leaves the filtered one in both lists (up to 7.25.20 the first entry
  whose fields matched was removed). A top-level `renamed_event` in the kw
  of `gobj_find_subscriptions()` filters nothing.
  In the other order a plain kw is a wildcard: a plain subscription over a
  renamed one replaces it, and a plain unsubscribe removes both. A
  subscription withdrawn by the `mt_subscription_deleted()` of an entry
  before it in the same unsubscribe is gone as asked, with no warning (it
  is not taken for a hard subscription kept). A subscription the
  publisher refuses (`mt_subscription_added()` answers -1) is not made and
  leaks nothing (up to 7.25.20 each refusal leaked it). A stale list that
  outlives its publisher (destroyed, its memory poisoned by
  `../emailsender/poison_alloc.c`) removes nothing and follows no pointer
  (up to 7.25.20 it read the freed publisher: a crash under the poison).

## Run

```bash
ctest -R '^c_subscriptions/' --output-on-failure --test-dir build
```
