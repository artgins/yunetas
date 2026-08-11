---
title: 'gobj-ui: Time and periods'
description: >-
  The catalog of periods, the algebra of the buckets, the rolling
  windows and the conversions between epoch and milliseconds.
---

# Time and periods

A **period** is a bucket with an alignment: the hour, the day, the week, the
quarter. A **rolling window** is not a bucket: it ends at now and reaches back.
The two live next to each other, because a live log is read with a window and a
report is read with a bucket.

`C_YUI_PERIOD` draws these, and every function here works without it.

**Source code:** [`src/yui_time.js`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js)

---

## The catalog

(js_YUI_PERIODS)=
### [`YUI_PERIODS`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L61)

The periods that carry a name: `minute`, `5min`, `15min`, `hour`, `day`, `week`,
`fortnight`, `month`, `bimester`, `quarter`, `semester`, `year` and `decade`.

Each one is a pair of a unit and a count. The algebra knows nothing about the
identifiers, and it works with any pair. These are the ones with a name, and a
bucket with a name gives a better label than a range: `"Q3 2026"` reads better
than `"jul – sep 2026"`.

(js_YUI_PERIODS_DEFAULT)=
### [`YUI_PERIODS_DEFAULT`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L80)

The set that a navigator takes by default: `["hour", "day", "week", "month",
"year"]`. The rest of the catalog is one line of configuration away.

(js_YUI_ROLLING)=
### [`YUI_ROLLING`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L85)

The rolling windows: `1h`, `6h`, `24h`, `7d` and `30d`.

---

## The algebra

(js_period_spec)=
### [`period_spec(period)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L302)

Reads an identifier of the catalog, or a specification of its own, and gives
`{id, unit, count}`. Returns `null` for anything that is not a bucket, such as
the custom mode of a navigator.

(js_period_start)=
### [`period_start(period, anchor_ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L338)

Gives the start of the bucket that holds a time, as a local date.

The alignment is what makes a bucket a bucket. Each unit goes down to the
natural origin of the unit above it, so the edges are the edges that a human
holds already.

| Unit | It aligns to |
|---|---|
| minute | The hour. A bucket of 15 minutes starts at `:00`, `:15`, `:30` and `:45`. |
| hour | The local midnight. A bucket of 6 hours starts at 00, 06, 12 and 18. |
| day | 1970-01-01. A count of one gives the plain day. A bucket of 10 days has no origin in the calendar, so it aligns to the epoch. |
| week | The week of the epoch, and the week begins on Monday. |
| month | January. That is the reason why a count of 2, 3, 4, 6 or 12 gives the bimesters, the quarters and the semesters of the calendar. |
| year | The year 0. A decade begins in 2020, and not in 2021. |

(js_period_shift)=
### [`period_shift(period, anchor_ms, delta)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L385)

Gives the start of the bucket that is `delta` buckets away. A negative `delta`
goes back.

The arithmetic is of the calendar, and never of the milliseconds. A step of one
month carries the year by itself, and a step of one day across a change of
summer time lands on the midnight again.

(js_period_bounds)=
### [`period_bounds(period, anchor_ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L422)

Gives the bucket as `{from, to}` in milliseconds.

:::{important}
`to` is **inclusive**. It is the last millisecond of the bucket, and not the
first of the next one. The two ends of a match condition are inclusive, so an
exclusive end that goes to one of them loses the record that landed exactly on
the edge, and it loses it in silence.
:::

(js_period_bounds_epoch)=
### [`period_bounds_epoch(period, anchor_ms, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L438)

The same bucket, in the unit that the consumer speaks. A topic of timeranger
keeps its times in seconds, and its `system_flag` says when it keeps them in
milliseconds. This is the function that a builder of a query calls.

(js_rolling_bounds)=
### [`rolling_bounds(rolling, ms, now_ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L455)

Gives a rolling window as `{from, to}`.

The end `to` stays **open**, at `0`. An iterator with no upper end keeps taking
the records that arrive while the card is on the screen. An end that is pinned
to now freezes the window at the instant of the click.

(js_is_current_period)=
### [`is_current_period(period, anchor_ms, now_ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L469)

Tells if this is the last bucket, which is the one that holds now. It is what
makes the arrow of "next" grey, and what tells the navigator that it is at home.

(js_infer_period)=
### [`infer_period(from, to, candidates, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L497)

Finds the bucket that a pair of ends describes, and gives `{period, anchor}`.
Returns `null` when no candidate matches.

Only an exact match counts: both ends must land on the edges of the bucket, in
the unit that the consumer writes. `candidates` are tried in their order.

---

## Labels

(js_period_name)=
### [`period_name(period, t)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L528)

Gives the name of a granularity, which is what a segmented control shows. It
takes the identifier of the specification as the key of the translation, so an
application that declares `{id: "quarter"}` gets its own word as soon as it adds
the key.

(js_period_label)=
### [`period_label(period, anchor_ms, t, locale)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L554)

Gives the name of the bucket that a navigator sits on, which is the text between
the two arrows.

| Period | Example |
|---|---|
| day | `"Today"`, `"Yesterday"` or `"13 jul 2026"` |
| week | `"Week 27"`, with the year when it is not this one |
| month | `"July"`, with the year when it is not this one |
| quarter | `"Q3 2026"` |
| semester | `"H2 2026"` |
| year | `"2026"` |
| decade | `"2020 – 2029"` |
| any other | The edges of the bucket: `"1 jul – 31 aug 2026"` |

The ones with a name have a name because a range reads worse. Everything that an
application invents takes the range, which is always true.

(js_safe_locale)=
### [`safe_locale(locale)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L108)

Gives a locale that `Intl` accepts.

:::{warning}
Never give `navigator.language` to `Intl` without this function, and never build
a formatter at the top level of a module. Firefox can report the literal string
`"undefined"`, and every constructor of `Intl` throws on it. A module that built
its formatter at the time of the import took a whole bundle down that way.
:::

---

## Conversions

(js_epoch_to_ms)=
### [`epoch_to_ms(value, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L162)

Changes a time of the consumer into milliseconds. With `ms` set to `true` the
value is in milliseconds already.

(js_ms_to_epoch)=
### [`ms_to_epoch(value_ms, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L170)

Changes milliseconds into the unit of the consumer.

(js_epoch_to_local_input)=
### [`epoch_to_local_input(value, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L184)

Changes a time into the form that an input of date and time takes.

(js_local_input_to_epoch)=
### [`local_input_to_epoch(v, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L195)

Changes the value of an input of date and time into a time.

(js_fmt_epoch)=
### [`fmt_epoch(value, ms)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L213)

Writes a time for a human.

(js_iso_week)=
### [`iso_week(d)`](https://github.com/artgins/gobj-ui.js/blob/5.13.0/src/yui_time.js#L270)

Gives the number of the week of a date, in the form of ISO 8601, with Monday as
the first day.
