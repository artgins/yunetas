# Task & Timer GClasses

Async task execution, timers, and counters.

**Source:** `kernel/c/root-linux/src/c_task.c`, `c_timer.c`, `c_timer0.c`,
`c_counter.c`

---

(gclass-c-task)=
## C_TASK

Task execution engine — runs a list of jobs, one after the other, each one
an action and the result it waits for, with a timeout per job.

| Property | Value |
|----------|-------|
| **States** | `ST_IDLE` (working), `ST_WAIT_STOPPED` (its end waits for the next turn of the loop) |
| **Input events** | from `gobj_results`: `EV_ON_OPEN`, `EV_ON_MESSAGE`, `EV_ON_CLOSE`, `EV_STOPPED`; `EV_TIMEOUT` (its own `C_TIMER`, or sent from outside); `EV_FINISH_TASK` (private, posted to itself; in `ST_IDLE` only the one of a run before a restart, dropped) |
| **Output events** | `EV_END_TASK` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `jobs` | `json` | The jobs: a list of `{"exec_action", "exec_result", "exec_timeout"}`. The first two are names of local methods of `gobj_jobs`; `exec_timeout` is in milliseconds. |
| `input_data` | `json` | Data for the jobs, as they want. |
| `output_data` | `json` | Data the jobs leave, as they want. A timeout adds `"comment": "Task failed by timeout"`. |
| `gobj_jobs` | `json` | The gobj that defines the jobs: a json integer with its `hgobj`, or the name of a service. |
| `gobj_results` | `json` | The gobj that runs them and answers (an http client, a database client): an `hgobj` or a service name. It is the task's bottom gobj, and the task subscribes to everything it publishes. |
| `timeout` | `integer` | Default `exec_timeout`, in milliseconds (`10000`). |

### How it runs

At its start the task runs the first action if `gobj_results` says it is
`connected`, or waits for its `EV_ON_OPEN`. An action returns `0` to wait for
the result (`CONTINUE_TASK()`; the job's timeout is armed) or `-1` to stop
(`STOP_TASK()`). The result arrives as `EV_ON_MESSAGE` of `gobj_results`,
and goes to the job's result method: `-1` stops, anything else goes on to the
next job. The end is `EV_END_TASK`:

| `result` | When |
|---|---|
| `0` | The last job answered. |
| `-1` | An action or a result returned `-1`. |
| `-2` | A job timed out (`output_data` says *"Task failed by timeout"*). |

The kw is `{result, last_job, input_data, output_data}`. It is published on
the turn of the loop after the end is decided (see below), and a volatile task
destroys itself after it.

```C
json_t *kw_task = json_pack("{s:I, s:I, s:[{s:s, s:s, s:i}]}",
    "gobj_jobs",    (json_int_t)(uintptr_t)gobj,            // its local methods run the jobs
    "gobj_results", (json_int_t)(uintptr_t)priv->gobj_http, // the client that answers
    "jobs",
        "exec_action",  "action_get_token",
        "exec_result",  "result_get_token",
        "exec_timeout", 5000
);
hgobj task = gobj_create("auth", C_TASK, kw_task, gobj);
gobj_subscribe_event(task, EV_END_TASK, 0, gobj);
gobj_set_volatil(task, TRUE);   // destroys itself after EV_END_TASK
gobj_start(task);
```

### The end runs in the next turn

The end of a task is never run where it is decided. A timeout is decided
inside the publication of the task's own `C_TIMER`, and a result can arrive
inside the job's action (a local service that answers in the call), with that
action still running on the task further up the stack: ending a volatile task
there frees the timer, or the task, under code that goes on with it. So every
end posts `EV_FINISH_TASK` to the task
([`gobj_post_event()`](../gobj/events_state.md#gobj_post_event)), and
`EV_END_TASK` is published from that action, on the next turn of the loop. A
timeout sent from outside (`C_AUTH_BFF`'s watchdog sends `EV_TIMEOUT` to a task
stuck connecting) takes the same path.

In between, the task is in `ST_WAIT_STOPPED`, and what `gobj_results` says
belongs to a job that is over: a late answer is not processed, and the end is
published once. A job's action whose result came inside it leaves the task as
that result left it (it arms no timeout, and does not end the task again).

The owner may act in that turn too, and it wins (7.27.0):

- a task its owner STOPS publishes no `EV_END_TASK` (the owner left it), and
  a volatile one is destroyed all the same. A stopped task is destroyed by
  nobody else: an owner that stops a task it no longer wants destroys it
  (`C_AUTH_BFF` does when the browser goes away);
- a task its owner starts AGAIN drops the end of the run before (each end
  names its run, and `mt_start` begins a new one), so `EV_FINISH_TASK` is
  declared in `ST_IDLE` too, where only such an end can arrive.

An owner that keeps the task in a pointer compares `src` of `EV_END_TASK` with
it, and ignores (and logs) the end of a task it no longer has in flight:

```C
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!src || src != priv->gobj_task) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_END_TASK of a task not in flight, ignored",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    priv->gobj_task = NULL;     // the volatile task destroys itself at its end
    ...
}
```

Up to 7.26.9 the task ended at once: the timer, or the task, was freed under
code still running on it, and `C_TIMER`, `execute_action()` and the kernel
read it afterwards (the kernel now says it with an ERROR, see
[`gobj_destroy()`](../gobj/creation.md#gobj_destroy)). `tests/c/c_task`.

---

(gclass-c-timer)=
## C_TIMER

High-level timer — recommended for general use. Uses periodic yuno
events with millisecond configuration. See the
[Timer API](../runtime/timer.md) for helper functions.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Output events** | `EV_TIMEOUT` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `periodic` | `bool` | `TRUE` for recurring, `FALSE` for one-shot. |
| `msec` | `integer` | Timeout / interval in milliseconds. |
| `subscriber` | `pointer` | Gobj receiving `EV_TIMEOUT`. |

### Notes

- Do not call `gobj_start()` / `gobj_stop()` directly — use
  [`set_timeout()`](../runtime/timer.md#set_timeout),
  [`set_timeout_periodic()`](../runtime/timer.md#set_timeout_periodic),
  and [`clear_timeout()`](../runtime/timer.md#clear_timeout).
- **It counts on the yuno's tick.** At its first start a `C_TIMER` subscribes
  to the yuno's `EV_TIMEOUT_PERIODIC`, and the subscription stays until the
  timer is destroyed: a cleared timer still gets the tick and ignores it.
  Up to 7.27.7 every start subscribed it and every clear unsubscribed it, a
  subscription made, and searched for among all the yuno's, for every
  timeout armed and cleared. So the yuno's subscriptions list every
  `C_TIMER` that ever ran, and a timer keeps the place it took at its first
  start among the subscribers of the tick.

  ```C
  set_timeout(priv->timer, 5*1000);   // first start: subscribed to the yuno
  clear_timeout(priv->timer);         // stopped: still subscribed, the tick is ignored
  set_timeout(priv->timer, 5*1000);   // runs again, no new subscription
  ```

---

(gclass-c-timer0)=
## C_TIMER0

Low-level timer — uses io_uring directly. Each instance opens a file
descriptor. **Prefer C_TIMER** unless you need lower-level control.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Output events** | `EV_TIMEOUT` |

### Key attributes

Same as [C_TIMER](#gclass-c-timer).

### Notes

- Use [`set_timeout0()`](../runtime/timer.md#set_timeout0),
  [`set_timeout_periodic0()`](../runtime/timer.md#set_timeout_periodic0),
  and [`clear_timeout0()`](../runtime/timer.md#clear_timeout0).
- **Not for a deferral.** A timer of one millisecond, armed to continue on the
  next cycle of the loop, is not a time: it costs an io_uring timeout for
  something with nothing to wait for, and every continuation then arrives as
  `EV_TIMEOUT`, so the `machine` trace says "timeout" instead of what happened.
  Use [`gobj_post_event()`](../gobj/events_state.md#gobj_post_event) for
  that, and a timer when there is a real time to measure.
- **Cleared and armed again in one turn of the loop** (a pause and a play):
  the cancel of the first arm is in flight, and the io_uring timer cannot
  start until it ends. The new arm is kept and done when the cancel comes
  back, and that cancel is not published as `EV_STOPPED`: the timer runs.
  Up to 7.25.21 the arm failed (*"cannot start timer: is CANCELING"*) and the
  timer stayed off.

  ```C
  PRIVATE int mt_pause(hgobj gobj)
  {
      clear_timeout0(priv->timer);    // the cancel goes out
      return 0;
  }
  PRIVATE int mt_play(hgobj gobj)
  {
      set_timeout_periodic0(priv->timer, priv->timeout);  // kept, done when the cancel ends
      return 0;
  }
  ```

  If the gobj is stopped in that same turn (`gobj_stop()` and an arm), the
  cancel is a stop after all: `EV_STOPPED` is said and the arm dropped. Up to
  7.25.22 nothing was said.
- **A stopped timer never ends the loop.** Its callback answers 0 whatever
  the state of its gobj; the yuno's loop ends with `set_yuno_must_die()`.
  Up to 7.25.22 it answered -1 when its gobj was not running, and `yev_loop`
  ends the loop on a -1: a child timer stopped while the yuno ran -- a
  service stopped alone, clearing and stopping its own timer in `mt_stop`
  -- stopped the whole yuno.

  ```C
  PRIVATE int mt_stop(hgobj gobj)
  {
      clear_timeout0(priv->timer);
      gobj_stop(priv->timer);         // its cancel comes back as EV_STOPPED; the yuno runs on
      return 0;
  }
  ```

---

(gclass-c-counter)=
## C_COUNTER

Event counter — counts incoming events until it reaches a target count or
it expires, then publishes a final event. The agent counts with it the
yunos that answer a `run-yuno`, `kill-yuno`, `play-yuno` or `pause-yuno`.

| Property | Value |
|----------|-------|
| **States** | `ST_IDLE` (counting), `ST_WAIT_STOPPED` (expired, its final count waits for the next turn of the loop) |
| **Input events** | `EV_COUNT` (an event to count), `EV_TIMEOUT` (its own `C_TIMER`, or sent from outside), `EV_COUNT_EXPIRED` (private, posted to itself; in `ST_IDLE` only the one of a run before a restart, dropped) |
| **Output events** | `final_event_name` (default `EV_FINAL_COUNT`) |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `max_count` | `integer` | Target event count. |
| `expiration_timeout` | `integer` | Milliseconds until it expires, count reached or not. |
| `final_event_name` | `string` | Event published at the end (`EV_FINAL_COUNT`), with `{info, max_count, cur_count}`. |
| `input_schema` | `json` | What counts: a list of `{"event", "filters"}`. `event` is compared with the `__original_event_name__` of the kw, `filters` (optional) with the kw. |
| `info` | `string` | Put in the final event. |

The events reach it through a subscription renamed to `EV_COUNT`, which adds
`__original_event_name__` to each kw:

```C
json_t *kw_counter = json_pack("{s:s, s:i, s:i, s:[{s:s}]}",
    "info", "yunos to run",
    "max_count", 3,
    "expiration_timeout", 10000,
    "input_schema",
        "event", "EV_ON_OPEN"
);
hgobj counter = gobj_create_volatil("run", C_COUNTER, kw_counter, gobj);
gobj_subscribe_event(gobj_input_side, EV_ON_OPEN,
    json_pack("{s:{s:s}}", "__config__", "__rename_event_name__", EV_COUNT),
    counter
);
gobj_subscribe_event(counter, EV_FINAL_COUNT, 0, gobj);
gobj_start(counter);
```

A volatile counter destroys itself after its final count.

### The expiration ends the counter in the next turn

`EV_TIMEOUT` comes inside the publication of the counter's own `C_TIMER`, and
a volatile counter destroys itself, and that timer with it, after its final
count. So the expiration posts `EV_COUNT_EXPIRED` to the counter, and the
final count is published from that action, on the next turn of the loop. A
count that arrives in between does not count (`ST_WAIT_STOPPED`). The final
count of a count reached is published where it happens, inside the last
`EV_COUNT`. Up to 7.26.9 the expiration published and destroyed at once, and
`C_TIMER` and the kernel read the freed timer afterwards. `tests/c/c_counter`.

As with `C_TASK`, the owner wins the turn in between: a counter it STOPS
publishes no final count (a volatile one is destroyed all the same), and one
it starts AGAIN drops the expiration of the run before (`EV_COUNT_EXPIRED` is
declared in `ST_IDLE` for it).
