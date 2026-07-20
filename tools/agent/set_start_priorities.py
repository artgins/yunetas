#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DEPRECATED shim. The real set_start_priorities.py now ships inside the yunetas CLI, as
``yunetas.agent_tools.set_start_priorities``.

It used to live here and travel in the .deb/.rpm while the CLI that drives it
shipped on PyPI: one tool, two release channels, two cadences. A CLI upgrade
could hand a new flag to a script from an older SDK and die with
"unrecognized arguments", and fixing a script meant cutting a whole SDK
release to deliver it.

This file stays for a release or two because operator runbooks reference the
path. It forwards to the installed CLI. Prefer the CLI directly:

    yunetas set-start-priorities [args]
"""
import os
import subprocess
import sys

MODULE = "yunetas.agent_tools.set_start_priorities"


def main():
    sys.stderr.write(
        "[!] %s is deprecated: it now ships in the yunetas CLI.\n"
        "    Use 'yunetas set-start-priorities'. Forwarding to %s ...\n"
        % (os.path.basename(__file__), MODULE)
    )
    try:
        return subprocess.run([sys.executable, "-m", MODULE] + sys.argv[1:]).returncode
    except (OSError, subprocess.SubprocessError):
        sys.stderr.write(
            "[-] Cannot reach %s. Install the CLI:  pipx install yunetas\n" % MODULE
        )
        return 2


if __name__ == "__main__":
    sys.exit(main())
