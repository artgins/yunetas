# archive_shipping test

Tests the yunos `archive_sender` and `archive_keeper`, which ship the archive of
`tranger2_archive_key_files()` to a backup. The two gclasses are compiled into
one test yuno (`main_test1.c` + `c_test1.c`), with two stores. Each store is
archived into its own archive under `$HOME/tests_yuneta/archive_shipping/`.

- **native**: `archive_sender` ships to `archive_keeper` over
  `tcp://127.0.0.1:7755` (`C_PROT_TCP4H`, chunks of 100 bytes, a window of 2,
  so every file takes several frames):
  - two batches are in the archive before the sender plays: its scan finds
    them;
  - a third is archived while it runs: its ready mark tells it;
  - a fourth is a batch the keeper already has (copied back into the archive
    with a mark): it is shipped again, and the keeper answers `stored` at its
    `end`, the content being the same (`batches_already` 1). Its copy aside
    is not left in the backup.
- **rsync**: `archive_sender_rsync` ships to a local directory with
  `max_archive_bytes` of one batch, and two are waiting. The oldest is removed
  without being shipped (WARNING *"Batch removed WITHOUT being shipped"*), and
  the other arrives with its manifest.
- **poisoning**: a batch of the name of one the sender will ship, planted in
  the backup first with the same files and sizes and other bytes. The keeper
  does not answer `stored` (the sha256 of the `end` are not of the batch it
  has), and keeps the planted batch untouched. The sender keeps its batch (it
  failed, and is tried again later). With a check of the names and sizes only,
  this case fails: the sender removes its real batch.

At the end:

- every batch is whole in the backup, with a manifest that names its key and
  its first day, and the content of the day;
- no batch and no ready mark is left in the two archives;
- the dropped batch is not at the destination.

The driver (`C_TEST1`) watches the stats of the three services with a timer,
and every step must end within 10 seconds.

## Run

```bash
ctest -R '^archive_shipping/' --output-on-failure --test-dir build
```

The rsync part needs `/usr/bin/rsync` 3.2.3 or later (for `--mkpath`).
