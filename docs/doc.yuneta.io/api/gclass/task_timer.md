# Task & Timer GClasses

Async task execution, timers, and counters.

**Source:** `kernel/c/root-linux/src/c_task.c`, `c_timer.c`, `c_timer0.c`,
`c_counter.c`

---

(gclass-c-task)=
## C_TASK

Task execution engine — orchestrates multi-step jobs with timeout and
result handling.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE`, `ST_WAIT_RESPONSE` |
| **Input events** | `EV_ON_MESSAGE`, `EV_TIMEOUT`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `jobs` | `json` | Job definitions (array of steps). |
| `input_data` | `json` | Input data for jobs. |
| `output_data` | `json` | Collected output data. |
| `gobj_jobs` | `pointer` | Gobj that executes jobs. |
| `gobj_results` | `pointer` | Gobj that receives results. |
| `timeout` | `integer` | Action timeout in seconds. |

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

Event counter — tracks incoming events until reaching a target count or
a timeout, then fires a final event.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE`, `ST_WAIT_COUNT` |
| **Input events** | `EV_ON_MESSAGE`, `EV_TIMEOUT` |
| **Output events** | custom (`final_event_name`) |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `max_count` | `integer` | Target event count. |
| `expiration_timeout` | `integer` | Timeout in seconds (fires even if count not reached). |
| `final_event_name` | `string` | Event name to fire on completion. |
| `input_schema` | `json` | Schema for counted events. |
