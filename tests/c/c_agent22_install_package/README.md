# c_agent22_install_package test

Runs the real agent22 (`yunos/c/yuno_agent22/build/yuneta_agent22`) in the
sandbox of [`c_agent22_write_tty`](../c_agent22_write_tty/README.md), with
its control center (`control_center.py`). The root program of
`install-package` is the real one, `packages/templates/install-package`. It
runs as the root of the namespace.

The sandbox adds these items to the one of `c_agent22_write_tty`:

- A tmpfs on `/var/lib`.
- A directory of the test on `/usr/libexec/yuneta`.
- Stubs on `/usr/bin/sudo` (it runs its command), `systemd-run` (it writes
  its arguments in `/yuneta/systemd-run.log`), `systemctl` and `journalctl`.

The control center runs a line `SH <command>` with `sh`, in the sandbox,
between two commands. The test uses such lines to put the program in place
and to read the files.

## What it pins

1. The package has no `/usr/libexec/yuneta/install-package` (7.31.29 or
   before). `install-package` is refused with an answer that says so, and
   `agent22` writes nothing.
2. The program is there. `install-package` answers
   `installing <file> in the unit yuneta-install-package-<time>`. The unit
   runs `/usr/libexec/yuneta/install-package run`, and the URL is not on its
   command line. The URL and the sha256 are in
   `/var/lib/yuneta/install-package`, mode `0755`, owned by root. `agent22`
   writes only `url` and `sha256` in `/yuneta/agent/install-package`.
3. `agent22` refuses an `http://` URL, and a URL with a `curl` pattern
   (`https://example.com/[1-1000000000]/<file>`).
4. `install-package-status` reads `result` from
   `/var/lib/yuneta/install-package`. When that file is not there, it reads
   the `result` that an older `agent22` left in
   `/yuneta/agent/install-package`.

Up to 7.31.29, `agent22` wrote a script into `/yuneta/agent/install-package`,
a directory of `yuneta`, and root ran it. Root also downloaded the package and
wrote its result there.

Run it by hand:

```bash
bash tests/c/c_agent22_install_package/run_test.sh yunos/c/yuno_agent22/build/yuneta_agent22
```

It exits 77 (skipped) when the machine cannot run it, and says why.
