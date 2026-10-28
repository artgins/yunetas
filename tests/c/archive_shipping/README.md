# archive_shipping tests

Tests the yunos `archive_sender` and `archive_keeper`, which ship the archive of
`tranger2_archive_key_files()` to a backup.

## test1: the two yunos, end to end

The two gclasses are compiled into one test yuno (`main_test1.c` + `c_test1.c`),
with two stores. Each store is archived into its own archive under
`$HOME/tests_yuneta/archive_shipping/`.

- **native**: `archive_sender` ships to `archive_keeper` over
  `tcps://127.0.0.1:7755` (`C_PROT_TCP4H`, chunks of 100 bytes, a window of 2,
  so every file takes several frames), with mutual TLS: the test makes a CA, the
  keeper's certificate and two client certificates of the CA (`test_node`,
  `other_node`) with the openssl command, in `/tmp/test_archive_shipping_pki`.
  The keeper asks a certificate in mode `"optional"`, so that a sender without
  one reaches the keeper's own check:
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
- **the node bound to the certificate**: a sender with the certificate of
  `other_node`, then one without a certificate, both naming `test_node`. The
  keeper refuses the batch of each (WARNING *"A sender names a node that is not
  the name of its client certificate"*, then *"no verified client
  certificate"*), and each keeps it; nothing of it is in the backup. Without
  the check, this case fails.

At the end:

- every batch is whole in the backup, with a manifest that names its key and
  its first day, and the content of the day;
- no batch and no ready mark is left in the two archives;
- the dropped batch is not at the destination.

The driver (`C_TEST1`) watches the stats of the three services with a timer,
and every step must end within 10 seconds.

## test2: the keeper against a fake sender

`main_test2.c` + `c_test2.c`: the driver (`C_TEST2`) IS the sender. It writes
the frames of the native transport by hand to an `archive_keeper` over
`tcp://127.0.0.1:7758` (`require_client_certificate` FALSE: plain tcp; test1
runs the mutual TLS), and checks every answer of the keeper, in order. The
cases are the frames a real sender never writes and a peer can:

1. an `end` whose sha256 is not of the bytes that arrived -- refused, *"a file
   is not the content that its sha256 says"*;
2. an `end` with fewer sha256 than files -- *"the end does not carry a sha256
   for each file"*;
3. a sha256 that is not 64 lowercase hex characters -- refused as 1;
4. data of a file before the files before it are whole -- *"data of a file
   before the files before it are whole"*;
5. a good batch -- `stored`, whole in the backup (the frames of the driver are
   right);
6. the same batch again -- `stored`, `batches_already` 1, its copy aside
   removed;
7. a batch of that name with the same files and sizes and other bytes, with
   THEIR right sha256 -- refused, *"a different batch of this name is here"*,
   the batch kept untouched.

A refused batch leaves nothing in the backup, neither the batch nor its copy
aside. At the end the keeper counts 5 refused, 1 stored, 1 already here.
Without the check of the sha256 in the `end`, this test fails.

## Run

```bash
ctest -R '^archive_shipping/' --output-on-failure --test-dir build   # test1 and test2
```

The rsync part needs `/usr/bin/rsync` 3.2.3 or later (for `--mkpath`), and the
certificates the `openssl` command.
