# fs_watcher

inotify-based filesystem watcher used by timeranger2 and by the `C_FS` gclass to react to on-disk changes.

Source code:

- [`fs_watcher.h`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.h)
- [`fs_watcher.c`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.c)

(fs_create_watcher_event)=
## [`fs_create_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.c#L127)

`fs_create_watcher_event()` initializes a new file system watcher event, monitoring the specified `path` for changes based on the given `fs_flag`. The event is associated with the provided `yev_loop` and invokes the specified `callback` when triggered.

```C
fs_event_t *fs_create_watcher_event(
    yev_loop_h     yev_loop,
    const char     *path,
    fs_flag_t      fs_flag,
    fs_callback_t  callback,
    hgobj          gobj,
    void           *user_data,
    void           *user_data2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the watcher event will be registered. |
| `path` | `const char *` | The directory or file path to monitor for changes. |
| `fs_flag` | `fs_flag_t` | Flags specifying monitoring options, such as recursive watching or file modification tracking. |
| `callback` | `fs_callback_t` | The function to be called when a file system event occurs. |
| `gobj` | `hgobj` | A generic object handle associated with the watcher event. |
| `user_data` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |

With `FS_FLAG_MOVED_AS_DELETED` a subdirectory renamed away (out of the watched directory, or to another name in it) is told as `FS_SUBDIR_DELETED_TYPE`, by its OLD name -- for a watch that is not recursive (in a recursive one the moved directory's own watch would keep its old path). The master of a timeranger2 topic watches `disks/` this way: a reader closes its feed by renaming `disks/<rt_id>/` away before removing it.

```C
fs_event_t *fs_event = fs_create_watcher_event(
    yev_loop, "/yuneta/store/db/topic/disks", FS_FLAG_MOVED_AS_DELETED,
    master_fs_callback, gobj, tranger, NULL
);
// mv disks/rtA disks/.closing.123-4567.1  ->  FS_SUBDIR_DELETED_TYPE, filename "rtA"
```

The descriptors of `FS_FLAG_DIR_FDS` live as long as their watch -- a
follower holds one per key directory of each feed -- so the watcher says
when the ones held by **every watcher of the process** reach half of the soft
open-files limit (*"Directories watched through descriptors: half of the
open-files limit"*, with `dir_fds` of the process, `dir_fds_of_this_watcher`
and `soft_limit`), and says it again only after they fell under the half. The
limit is the process's: up to 7.25.21 each watcher counted only its own, so
four followers of 400 directories each, under a limit of 1024, never said it. A directory whose descriptor cannot be opened (EMFILE) is
watched by its path; the first failure is an ERROR, the next ones are only
counted, and the count is said when a descriptor opens again (*"Directories
watched through their descriptor again"*, `watched_by_path`). A yuno raises its
own soft limit to its hard one at its start (C_YUNO `limit_open_files`, `0` by
default).

With `FS_FLAG_DIR_FDS` each SUBDIRECTORY watched is opened first and watched through that descriptor, so the watch and the descriptor are one inode: see [`fs_watcher_dir_fd()`](#fs_watcher_dir_fd). Every event carries `event_wd` (the watch of `directory`) and, for `FS_SUBDIR_CREATED_TYPE`, `subdir_wd` (the watch just set on the directory created, `-1` if it was gone).

**Returns**

Returns a pointer to a newly allocated [`fs_event_t`](#fs_event_t) structure representing the watcher event, or `NULL` on failure -- a path that is not a directory, no inotify instance, or a root that cannot be watched (`ENOSPC` at `fs.inotify.max_user_watches`, logged). Up to 7.25.20 a root that could not be watched gave a watcher all the same, running and watching nothing.

With `FS_FLAG_BATCH_END` the owner is also called with `FS_BATCH_END_TYPE` after each batch read from inotify (`offset` = where the batch ends), and after each slice of the pass that follows an overflow (`offset` = where the stream is), and every event carries `offset` and `offset_end`, its place in the watcher's stream: what the owner left for "when the stream is past here" can be done there. A timeranger2 follower defers the scan of a key directory that way. It notes the directory at its event, and asks where the queue ends once, at the end of the batch: [`fs_queued_events_end()`](#fs_queued_events_end) walks the whole inotify queue, and asked at each new directory it made a flood of 69632 new keys quadratic (18 s of a drain of 21). The order is the contract: first LOOK at each directory (who it is: the watch it was seen with, and its descriptor -- see [`FS_FLAG_DIR_FDS`](#fs_watcher_dir_fd); without one, inode and birth), THEN ask where the queue ends, and read a directory only if it is still the one looked at. A change made by another process after the look is either queued before the answer (and read before the directory is) or it changed the directory (and the read is skipped). With the question first and the look after, a directory removed and made again between the two is read as the new one, while the event of its removal is queued past the answer. The window is not small: the owner's own callbacks for the directories before it in the batch run there.

A file created in a directory that is noted or placed is left to the read of that directory, which takes all its files in order. Read at its own event, a second file of a new key came before the first.

```C
fs_event_t *fs = fs_create_watcher_event(
    yev_loop, path, FS_FLAG_RECURSIVE_PATHS|FS_FLAG_BATCH_END, my_fs_callback, gobj, NULL, NULL
);
...
case FS_SUBDIR_CREATED_TYPE:
case FS_RESCAN_DIR_TYPE:
    note_the_directory(my, fs_event);               // cheap: nothing asked here
    break;
case FS_FILE_CREATED_TYPE:
    if(directory_is_noted_or_placed(my, fs_event)) {
        break;                                      // its read takes this file, in order
    }
    read_the_file(my, fs_event);
    break;
case FS_BATCH_END_TYPE:
    read_the_placed_directories_due(my, fs_event->offset);  // each one if still the one looked at
    if(has_notes(my)) {
        look_at_the_noted_directories(my);          // FIRST: each one by its watch and descriptor
        uint64_t until = fs_queued_events_end(fs_event);    // THEN the question, once
        place_the_notes(my, until);                 // read each when the stream is past `until`
    }
    break;
```

**Notes**

The created watcher event must be started using [`fs_start_watcher_event()`](<#fs_start_watcher_event>) to begin monitoring. When no longer needed, it must be stopped using [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>), which will also free the associated resources.

---

(fs_start_watcher_event)=
## [`fs_start_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.c#L274)

`fs_start_watcher_event()` starts monitoring the specified file system event. This enables notifications for file and directory changes.

```C
int fs_start_watcher_event(
    fs_event_t *fs_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | Pointer to the file system event structure to be started. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Once started, the event will trigger the associated callback when file system changes occur. Use [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>) to stop monitoring and release resources.

---

(fs_stop_watcher_event)=
## [`fs_stop_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.c#L287)

`fs_stop_watcher_event()` stops the given file system watcher event and destroys the associated `fs_event_t` instance.

```C
int fs_stop_watcher_event(
    fs_event_t *fs_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | Pointer to the `fs_event_t` instance representing the file system watcher event to be stopped and destroyed. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Once [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>) is called, the `fs_event_t` instance is destroyed and must not be used again.

---

(fs_queued_events_end)=
## [`fs_queued_events_end()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/timeranger2/src/fs_watcher.c#L345)

`fs_queued_events_end()` says where the events the kernel holds for the
watcher BY NOW end, in the watcher's stream of events. Every event the
watcher hands over carries its own place in that stream, `fs_event->offset`
(the bytes read from inotify before it). An event handed over later with an
`offset` below the answer was already queued when the question was asked:
what it says may be what the owner has just read from the disk.

```C
uint64_t fs_queued_events_end(
    fs_event_t *fs_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | The watcher. |

**Returns**

The offset in the stream where the events queued by now end: the rest of the
batch being walked, if one is -- or else what a read the kernel completed and
the loop has not delivered took --, plus what the kernel still holds
(`FIONREAD`). `0` for a NULL watcher. If `FIONREAD` fails the error is logged
and the answer is past everything the kernel can hold for the fd
(`max_queued_events` events of the largest size, from
`/proc/sys/fs/inotify/max_queued_events`), plus a read whole outside a batch:
never short. Up to 7.25.21 only a read whole was added (inside a batch, at
first, nothing), and with more than a read queued the owner did too soon what
it left for after those events. If `FIONREAD` fails again when the watcher
comes to close that end, the watcher is gone (see below).

**Notes**

Exact, asked from the owner's own callback and of another watcher alike.
Between two batches the kernel completes the watcher's read at any return to
user space (an interrupt's too): its events are out of the kernel's queue and
not yet handed over. The completion is looked for in the loop's ring
([`yev_get_waiting_completion()`](#yev_get_waiting_completion)) before and
after asking the kernel; the same answer both times means nothing moved in
between (there is one read at a time, and once completed it waits for the
loop). Only when completions overflowed the ring, and whether one of this
read waits cannot be seen, is a read counted whole: the answer is then past
the end, never short.

An answer past the end is closed by the watcher itself. It notes it, and on
the next turn of the loop (and after each batch while it is open) it looks
again: once no completion of its read waits in the ring and the kernel holds
nothing, everything queued when it was asked has been handed over, so the
stream JUMPS to that answer and the owner is called with `FS_BATCH_END_TYPE`
at it (with `FS_FLAG_BATCH_END`). The offsets after it continue from there: an
offset is a place in the stream to compare, not a count of bytes read. Up to
7.25.21 nothing closed it, and an owner waiting for "the stream past here"
waited for about 8 KB of unrelated events -- on a quiet watcher, for ever.

```C
case FS_BATCH_END_TYPE:
    if(owner->scan_pending && fs_event->offset >= owner->scan_until) {
        scan_the_directory(owner);  // reached, even if no event came after it
    }
    break;
```

It costs what the queue holds: `FIONREAD` walks the whole inotify queue. Ask
it once per batch (`FS_BATCH_END_TYPE`), not once per event: asked at each of
69632 new directories it was 18 s of a drain of 21. A read takes up to 32
events of the longest name (up to 7.25.20 one: a backlog of 65536 events was
~8000 batches, and as many questions at their ends).

A timeranger2 follower uses it to tell apart what it already said from what
is new. At an overflow it reads `keys/` and tells the keys gone from there
deleted; the signal of such a delete can still be in the queue, behind the
overflow, and must not be told again:

```C
case FS_OVERFLOW_TYPE:
    list_what_is_on_disk_and_tell_it(owner);       // what the lost events said
    owner->told_until = fs_queued_events_end(fs_event);
    break;

case FS_SUBDIR_DELETED_TYPE:
    if(fs_event->offset < owner->told_until && already_told(owner, fs_event->filename)) {
        break;  // queued before the owner read the disk: said already
    }
    tell_deleted(owner, fs_event->filename);
    break;
```

---

(fs_watcher_dir_fd)=
## `fs_watcher_dir_fd()`

With `FS_FLAG_DIR_FDS`, the descriptor of the directory watched under `wd`
(an event's `event_wd` or `subdir_wd`). It is the very inode of that watch:
the directory is opened BEFORE it is watched, and watched through the
descriptor (`/proc/self/fd/N`), so a directory removed and made again under
the same path -- which ext4 gives the same inode number, and on 6.x kernels
the same birth time -- is never taken for it. `openat()` / `unlinkat()`
through it cannot reach another directory: in a directory gone they fail with
`ENOENT`.

```C
int fs_watcher_dir_fd(
    fs_event_t *fs_event,
    int wd
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | The watcher. |
| `wd` | `int` | A watch: `fs_event->event_wd`, `fs_event->subdir_wd`. |

**Returns**

The descriptor, owned by the watcher: do not close it, and take it again at
each event (it may be closed at the next one). `-1` and `errno`:

- `ENOENT`: the directory is gone, or `wd` is not watched;
- `ENOTSUP`: watched without a descriptor -- no `FS_FLAG_DIR_FDS`, the root
  (nothing above it would close it when it goes), or the descriptor could not
  be opened (logged). Use the path, as without the flag.

**Notes**

A descriptor open on a directory holds its inode: the kernel then holds back
its `IN_DELETE_SELF` and `IN_IGNORED` until the descriptor is closed. The
watcher closes it when the directory's parent reports it deleted
(`IN_DELETE|IN_ISDIR`) and it has no link left (`st_nlink` 0), and when a
new directory is watched at its path; then its events come, and the watch
goes as without the flag. The cost is one descriptor per subdirectory
watched (a timeranger2 follower: one per key directory of each feed), and an
`open()` per directory watched.

A timeranger2 follower takes the link of a new record through the
descriptor of the directory the event came from, so an event of a key
directory deleted and made again before it was read reaches nothing:

```C
case FS_FILE_CREATED_TYPE: {
    int dir_fd = fs_watcher_dir_fd(fs_event, fs_event->event_wd);
    if(dir_fd >= 0) {
        int fd = openat(dir_fd, fs_event->filename, O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(fd < 0 && errno == ENOENT) {
            break;      // consumed, or its directory is gone: not this one's
        }
        unlinkat(dir_fd, fs_event->filename, 0);
        read_the_life_of(fd);
        close(fd);
    } else if(errno == ENOTSUP) {
        read_by_path(fs_event->directory, fs_event->filename);
    }
    // ENOENT: the directory is gone, its delete comes
    break;
}
```

---

## Queue overflow (`IN_Q_OVERFLOW`)

Each watcher owns one inotify instance with a bounded kernel event queue
(`fs.inotify.max_queued_events`; the deb/rpm packagers set 65536). Under a
burst the kernel drops the events that do not fit and signals a single
`IN_Q_OVERFLOW`: from there the watcher cannot know every change it missed.

What the lost events said is still on the filesystem, so the watcher recovers
in place (since 7.25.9; in slices since 7.25.10):

1. a WARNING, *"inotify IN_Q_OVERFLOW: events lost, rescanning the watched
   tree"*, with the watched `path`;
2. the owner's callback is called ONCE with **`FS_OVERFLOW_TYPE`**
   (`directory` = the watched path): do there what is global and cheap. An
   owner may stop the watcher there (`fs_stop_watcher_event()`): then no
   pass is started, and the watcher goes when the batch ends (up to 7.25.20
   the pass was still set up -- its index built, its timer created and
   armed -- and thrown away);
3. a **pass** over the tree follows: every directory, the root included, is
   handed to the owner as **`FS_RESCAN_DIR_TYPE`** (`directory` = that
   directory) -- read it again, what it holds may never have been told. In a
   recursive watch a directory born while its `IN_CREATE` was dropped is
   watched before it is handed over, and so is one deleted and created again
   meanwhile: every directory of the pass is watched again
   (`inotify_add_watch()` on an inode already watched returns its wd), and a
   wd that differs from the one the table holds for that path replaces it.
   Up to 7.25.20 a path found in the table was taken as watched, and a
   directory reborn during an overflow (another inode, its `IN_IGNORED` lost)
   was never heard again; the table also let an `IN_IGNORED` pass without
   taking out its wd, which it does now. The ROOT is watched again too, in a
   watch that recurses and in one that does not: up to 7.25.20 the pass
   watched again the directories it met, never the root it started from, and
   a root deleted and created again during an overflow went deaf. At the end
   of the pass a directory of the table that the pass did not meet and that
   is no longer there is stopped too. The entry of a wd stopped goes with its
   `IN_IGNORED`; when that was dropped with the overflow, it goes once the
   stream is past where the `IN_IGNORED` would have come
   (`fs_queued_events_end()` at the stop): up to 7.25.21 it stayed for good.
   The watcher does not follow moves: a directory that is not there is gone
   for it;
4. the pass runs **a slice of 20 ms per loop turn**, and an INFO closes it:
   *"watched tree rescanned after lost inotify events"*, with `directories`,
   `ms`, and where that time went: `slices`, `ms_owner` (in the owner's
   callbacks), `ms_watcher` (the walk itself), `ms_loop` (the loop's own work
   between slices) and `max_loop_ms` (its longest turn). Another overflow during a pass schedules one more whole pass after it
   (starting again would starve the end of the tree under overflows that keep
   coming).

In 7.25.9 the owner rescanned the whole tree inside the one `FS_OVERFLOW_TYPE`
call. A timeranger2 follower of 50000 keys on the busy disk of a central took
56 s, then 243 s -- the yuno deaf to its agent, its commands and its timers all
that time. The slices keep it answering; the pass costs the same.

What the pass costs is mostly the owner's: the watcher's own part is a
`readdir()` per directory and a lookup in an index of the watched paths, built
once per pass. 7.25.10 rebuilt that index in every slice -- 50000 paths every
20 ms -- so its own cost grew with the tree (254 us per directory at 69632,
73 us since 7.25.11), and on the central a pass over 50501 directories took
7 minutes. `tests/c/timeranger2/test_fs_watcher_overflow` measures it (the
pass less the owner's time, per directory) and fails above 200 us.

A pass closes with its own account (since 7.25.12), so a long one says why:

```
"msg": "watched tree rescanned after lost inotify events",
"directories": 69633, "ms": 14374, "slices": 612,
"ms_owner": 11169, "ms_watcher": 887, "ms_loop": 2318, "max_loop_ms": 17
```

Here the owner took most of it (a test's owner that sleeps 100 us per
directory); a large `ms_loop` would say the slices were waiting for a busy
loop instead.

What it said on yunovatios' central, a timeranger2 follower of 50501 keys
while its master wrote a backlog of ~9 GB (2026-09-27): passes of 142-335 s,
**65-78 % in the owner**, 22-35 % in the loop, and **1.1-1.7 s in the
watcher** -- whatever the length of the pass. The owner's time is the records
it hands over: the same tree with nothing pending took 1 s, 0.35 s of it in
the owner. So a long pass is a follower working through a backlog at its own
pace (there, ~80 % of a core), not a slow walk; it ends about a minute after
the master stops writing, and the loop never waited more than 239 ms for a
slice.

Every owner handles both -- in its callback, before anything that reads the
type as bits:

```C
PRIVATE int my_fs_callback(fs_event_t *fs_event)
{
    switch(fs_event->fs_type) {
        case FS_OVERFLOW_TYPE:
            // events were lost: what is global and cheap (a pass follows)
            break;
        case FS_RESCAN_DIR_TYPE:
            // one directory of the tree: list it again
            rescan_my_dir(fs_event->gobj, (const char *)fs_event->directory);
            break;
        case FS_FILE_CREATED_TYPE:
            ...
    }
    return 0;
}
```

What the owners of the tree do:

- **timeranger2, a follower's rt_disk feed** (the heavy one: an event per new
  md2 of every key). At `FS_OVERFLOW_TYPE`: a deleted key leaves no trace in
  `disks/<rt_id>/` (its signal is a directory created and removed), so the
  follower's cache is compared with the topic's `keys/`, read once, and a key
  gone from there is heard as deleted (its `key_deleted` callback fires; INFO
  *"keys deleted while the inotify events were lost"*). The cache is shared by
  every feed of the topic and forgets a key with the first feed that hears its
  delete, so the first feed to hear one (the feed that owes it nothing and
  holds no doubt about it, below; the key's being in the cache does not say
  it, the key may never have been read by this follower) counts it as owed by every other watched feed (`deletes_unheard`, per feed), each
  paying when it hears it: the deletes a feed owes and that are gone from
  `keys/` are told at its overflow too. Up to 7.25.20 a feed that overflowed
  while another feed of its topic heard a delete never heard of it.

  Only the first feed to hear a delete FORGETS the key: out of the cache, its
  segments and the watermark of every feed. A feed that pays leaves them
  alone: the key may be back, loaded by a feed that heard its new record, and
  a new record seeds the watermark of every feed that wants the key. Up to
  7.25.20 every feed that heard a delete forgot the key: a slow one took the
  live key out of the cache and dropped its own fresh watermark (and, with
  the deletes owed counted, the delete was taken as new and owed by the
  feeds that had heard it).

  A feed is told the deletes of the keys it wants: its `key`, or the keys its
  `rkey` matches, or every key (up to 7.25.20 only `key` was looked at, and a
  feed opened with an `rkey` was told every key deleted).

  What an overflow told is not told again. The kernel queues its overflow at
  the end of a full queue, and as the watcher reads down to it room is made
  behind it: the master's signal of a delete can be queued there, and comes
  after the overflow that already told the key. The feed keeps the keys it
  told and where its stream ended once `keys/` was read
  ([`fs_queued_events_end()`](#fs_queued_events_end)); a delete of one of
  them queued before that is said already, and nothing is done (up to 7.25.20
  it was told twice). The set is let go at the first key-delete the feed
  hears from past that point, or at its next overflow. A delete the feed
  OWED and whose key is on disk again at the overflow (deleted and written
  again while the events were lost) is not told there; if its signal is
  queued behind the overflow, it is that delete, paid (told, nothing
  forgotten), not a new one. Up to the fix the debt went at the overflow and
  the signal was taken for a new delete: the live key out of the cache, and
  owed by the feed that had heard it.

  A feed opened while a delete was in flight may hear it or not: not if its
  directory was made after the master listed `disks/`, or if it was watched
  after the master signalled it; yes if the master signalled it after it was
  watched (the master signals the feeds one after another). When a feed
  opens it notes where the stream of every other feed ends (`watched_from`);
  a feed that hears a delete first, below that point of its own stream,
  cannot tell which case the other one is in. It does not make it owe the
  delete (it may never hear it, and the next delete of the key would pay
  it): it leaves it IN DOUBT (`deletes_in_doubt`), at the place where the
  other feed's stream ends then. If that feed hears the delete before that
  place, the delete is not new: paid, told, nothing forgotten and no debt
  made. Heard past that place, it is another delete, and the doubt goes.
  Up to the fix it took the delete for a new one and made the first feed owe
  it again: a debt never paid, and told again at that feed's next overflow
  (`[DEL DEL]`). This happens when a follower restarts: the master lists the
  old `disks/<rt_id>`, which the open removes and makes again. The doubt is
  only for a feed watched when the first feed hears the delete; one opened
  after that, and signalled after it was watched, still takes the delete
  for new -- it needs the master to stop between two signals while the
  follower reads, hears and opens.

  A debt holds where the
  stream of the debtor ended when it was made; when the debtor's stream, past
  that point, hands it a record of the key in its place in the stream (a
  link heard, at its own `IN_CREATE`, or a key directory read once the
  stream is past what could still remove it, below: the key lives), the
  debt is forgotten -- kept, the next delete of the key would pay it, and a
  feed that overflowed then would miss that one.

  A key directory is read when the stream is past every event queued when
  its `IN_CREATE` was read, not at that `IN_CREATE`. The master signals a
  delete to a feed without the key's directory by making it and removing
  it; if it writes the key again before the follower reads that signal,
  the directory is there again with the NEW key's links while the delete
  is still queued. Read at the signal's place, the new key's file was taken
  against the OLD key's cell of the cache: one new record was never
  handed, five came as rowids 4 and 5 (and the next append handed R1..R6),
  and the delete heard after took the live key out of the cache. Up to
  7.25.20 it was so, and so after an overflow: the pass read the key born
  again while its delete waited behind the overflow. Now the scan waits
  (`scans_pending`) until the stream is past `fs_queued_events_end()` --
  the master is one process and writes the key again only after it
  signalled this feed, so whatever can still remove the directory is queued
  by then; a delete of the key heard before drops it (the directory's own
  `IN_CREATE` reads it later); and it reads only the directory seen then:
  the one of the watch set at its `IN_CREATE`, through that watch's
  descriptor (`FS_FLAG_DIR_FDS`; inode and birth only without one). The feed is told `deleted`, then the new key's
  records from rowid 1. So for ANY key: a key the follower never saw may
  have been born, deleted and written again in the part of the stream not
  read yet ([R1 DEL] was handed, the key out of the cache, and the next
  append handed R1 R2), and a key may be deleted and written again more
  than once there ([DEL R1 DEL], [DEL R1 R2 R3 DEL]). Up to 7.25.20 a key
  not in the cache and not owed was read at once. The pass after an
  overflow defers its key directories the same way. The directories are
  noted at their event (`scans_new`) and placed at the end of the batch, or
  of the slice of the pass: each one is LOOKED AT (still there, through
  its descriptor) and
  then where the queued events end is asked, once for up to 256 of them;
  only with nothing queued after the batch is it read at once, and a read,
  at once or later, is done only if the directory is still the one looked
  at. Up to the fix the question came first and the look after: a key
  deleted and written again between them -- where the record callbacks of
  the keys before it in the batch run -- was read as the new directory
  while its delete was queued past the answer (`[R1 DEL]`, the key out of
  the cache). A link heard in a key directory that is noted or placed is
  left to the read of the directory, which takes every link of it in order
  (read at its own event, the second file of a new key came first: `[R1 R1]`
  and R2 lost). The watcher tells the feed
  where each batch and each slice ends (`FS_FLAG_BATCH_END`), where the
  scans come due when no other event follows. When the backlog is deeper
  than 256 KB the notes wait, unplaced, until the stream is past the end
  the backlog had then (an overflow in it drops them: the pass reads the
  directories), and the queue is not asked again meanwhile. The 69632-key
  flood of `test_rt_disk_overflow` drains in 3862 ms, against 3599 when a
  key not in the cache was read at once and with no look before the
  question (+7.3%, twenty alternated runs).

  When a delete is heard, the follower closes the descriptors it holds on
  the files of the key, as the master does in `tranger2_delete_key()`. Up to
  the fix it kept them (as in 7.25.20): a key it had read, deleted and
  written again in the same file -- one file a day is the usual mask -- was
  read through the descriptor of the old, unlinked file: its first record
  with the OLD content, the rest a short read (*"Cannot read record
  metadata, short read"*, CRITICAL) and lost, and the next append lost too.

  In a MASTER the watcher's echo of a delete forgets nothing:
  `tranger2_delete_key()` forgot the key when it deleted it, and the master
  may have written it again before its own rt_disk feed (a configuration of
  tests) hears the echo. Up to 7.25.20 the echo took the live key out of the
  master's cache. And since the master's cache cannot say later that a
  delete was lost, `tranger2_delete_key()` makes the debts itself (a feed
  hearing it makes none: a key in the cache then is the key written
  again): every
  feed of the master watched then owes it, and one that overflowed is told
  it (up to 7.25.20 it never heard it). The master is the only writer: it
  knows which feeds were watched when it signalled. Such a feed is handed
  no RECORD, by design: the master's cache counts each one at its append,
  so the link the feed hears is nothing new (a master feeds its lists from
  memory, with `tranger2_open_rt_mem()`); only the deletes reach it.

  At each `FS_RESCAN_DIR_TYPE`: the master hard-links each new md2 into
  `disks/<rt_id>/<key>/` and the follower consumes the link when it reads it,
  so a link still there IS a record not handed over yet, and the key directory
  is read (when the stream is past what could still remove it, above). Both
  are idempotent: nothing is handed over twice.
- **timeranger2, the master's watch of `disks/`**: at `FS_OVERFLOW_TYPE`, the
  feeds whose directory went are closed, and a feed is opened for every
  directory without one (a handful of directories: no pass needed).
- **`C_FS`** publishes `EV_FS_CHANGED` for the watched root, once.
- **`utils/c/fs_watcher`** prints *"Events LOST"* and each *"Rescan dir"*.

## A subdirectory that cannot be watched (`ENOSPC`)

A subdirectory met by a recursive watch whose watch cannot be made -- out of
watches (`fs.inotify.max_user_watches`, `ENOSPC`) or of kernel memory
(`ENOMEM`) -- is handed as created with `subdir_wd` -1, and nothing made in it
is heard. It is kept, and tried again at the end of each batch of the
watcher (64 per batch); once its watch is made it is handed AGAIN as
created, now with its `subdir_wd`, at the batch's end, so its owner reads
what was made in it meanwhile. One gone by then is forgotten (its parent
said it). Under `FS_FLAG_RECURSIVE_PATHS` what was made under it was not
heard either: its subdirectories not watched are queued behind it and tried
in the same way, parent first, so each one is handed as created after its
parent. Every try counts against the batch's 64, and an `ENOSPC`/`ENOMEM`
ends the batch's tries: a large subtree is watched over several batches, and
never blocks the loop. The first failure is an ERROR, *"Cannot watch a
directory, out of inotify watches or memory: tried again at each batch (and
the next ones that fail, counted)"*; at the end of the batch where none is
left, a warning, *"Directories watched again: every one that could not be is
watched now"*. Up to 7.25.22 a subdirectory made meanwhile was never
watched. Up to 7.25.21
it was an ERROR per directory, and the directory was never watched: a
timeranger2 follower took a key directory for gone, and lost every record
of the key.

```C
case FS_SUBDIR_CREATED_TYPE:
    if(fs_event->subdir_wd < 0) {
        break;  // gone, or not watched yet: if it lives, it comes again with its watch
    }
    read_what_is_in(fs_event->directory, fs_event->filename);
    break;
```

The retry needs a batch: a watcher whose only activity is inside the
directory it cannot watch hears nothing to retry on (nothing made there
reaches it), which the ERROR says. Raise `fs.inotify.max_user_watches`.

## When the watcher goes (`FS_WATCHER_GONE_TYPE`)

A watcher whose read FAILS, cannot be armed again, or is canceled by another
than its owner, is over: an ERROR, *"inotify read FAILED: the watcher is
gone"*, *"inotify read cannot be armed again: the watcher is gone"*, or
*"inotify read canceled, and not by its owner: the watcher is gone"*, with
`path` (and `errno` of a read), then the owner's callback is called once with
**`FS_WATCHER_GONE_TYPE`** (`directory` = the watched path), and the watcher
is destroyed when the call returns. Nothing else comes. So is a watcher that
cannot count the kernel's queue (`FIONREAD` failing) when it comes to close an
end said past the stream: *"The events queued by the kernel cannot be counted:
an end said past the stream cannot be closed, the watcher is gone"* -- up to
7.25.21 that end was left open, and on a quiet watcher never reached. The owner drops every
pointer it keeps to it -- and must not stop it: it is freed. An owner that
stopped the watcher itself (`fs_stop_watcher_event()`) is not told. Up to
7.25.20 the watcher went silently (the failure logged only under a trace), and
its owner kept a pointer to freed memory: a timeranger2 feed stopped it again
when closed.

No shutdown cancels a watcher behind its owner. `yev_loop_stop()` cancels
every operation of the loop, but a yuno calls it after its loop ended
(`yuno_shutdown()` only resets it), its services stopped and its gobjs
ended -- every watcher stopped by its owner, C_FS in `mt_stop`, the
trangers of the services in theirs -- and destroys the loop without running
it (`entry_point.c`). The tools either start their tranger without a loop
(no watcher), or shut it down before `yev_loop_stop()` (`tr2list` and
`treedb_list --follow`), or run no loop after it (`tr2migrate`). And a loop
run after `yev_loop_stop()` delivers nothing behind the stop's own
completion: it breaks there and leaves it at the head of the ring. So the
message means a cancel from outside fs_watcher -- an order broken -- and the
owner is still told (`tests/c/timeranger2/test_rt_disk_watcher_gone` makes
one on purpose, with `yev_stop_event()` on the watcher's read).

```C
case FS_WATCHER_GONE_TYPE:
    priv->fs_watcher = NULL;    // freed when this returns
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "the watch is gone",
        "path",         "%s", fs_event->path,
        NULL
    );
    break;
```

What the owners do: a timeranger2 rt_disk feed drops its watcher and its
accounts of deletes, and says it is deaf (*"rt_disk feed deaf: its watcher
is gone"*); every reader of the feed's watcher skips a feed without one. The
master's watch of `disks/` says new feeds are not heard any more. `C_FS`
says the path is not watched any more (its `size_dl_watch` reads 0).
`utils/c/fs_watcher` prints *"Watcher gone"*.

Up to 7.25.8 an overflow aborted the yuno, to be relaunched and reload clean.
Under a sustained burst the reload met the next overflow: in yunovatios' stress
test of its central, a `db_history_ce` following a `db_tracks_ce` at ~3000
records/s over ~60000 keys aborted five times in two hours, each relaunch
spending 2-3 minutes catching up before falling again.

A follower also hears its OWN consumption: each link it removes is an
`IN_DELETE` in a watched key directory, which it ignores. Half of what fills
its queue is that echo, which is why a single burst can overflow it twice.

`tests/c/timeranger2`:

- `test_rt_disk_overflow`: with the loop stopped, the master appends to
  `max_queued_events` + 4096 new keys, one `IN_CREATE` each, and deletes a key
  while the queue is full. Every record reaches the feed exactly once, the
  deleted key is heard once, and a key born during the overflow is watched
  afterwards. With the code that aborted, the test aborts. It needs about four
  open files per key and raises its soft limit to the hard one, as a yuno does;
  below that it is skipped. Before it, cheap cases that always run: two
  feeds of one topic, the whole-topic one overflowed (by directories created
  and removed in one of its key directories) while a key is deleted, and the
  one keyed on that key hearing it first -- both hear the delete once; the
  same with the signal of the delete queued BEHIND the overflow (the test
  reads some of the full queue itself, then deletes), with one feed and with
  two -- told once, and nobody left owing it; a feed opened while a delete
  was in flight -- it owes nothing, and when the key is born again, the
  other feed overflowed and the key deleted again, the overflowed feed is
  told the second delete; and an old delete heard by a slow feed after the
  key came back -- the live key stays in the cache, nobody owes anything,
  and the key's next record arrives.
- `test_fs_watcher_overflow`: the watcher alone, with an owner slow on purpose
  (100 us per directory) and a periodic timer probing the loop. `max_queued_events`
  + 4096 directories are created with the loop stopped: every one is told to the
  owner, the pass takes ~30 s and the loop is never deaf for more than 1 s (it is
  50 ms, the probe's period). A directory watched before the flood and deleted
  and created again in it is watched after the pass (a file created in it is
  heard), and so is the ROOT deleted and created again during an overflow,
  recursive or not. On a local disk the directories just created are
  in the kernel's cache and a pass in one piece takes milliseconds -- which is
  why only a slow owner shows what a busy disk does.
