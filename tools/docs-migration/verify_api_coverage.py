#!/usr/bin/env python3
"""
verify_api_coverage.py — Check that every public C function is documented
in the mystmd migration and vice versa.

Strategy:
  - Each C header is considered the source of truth for a set of PUBLIC
    function declarations.
  - Each header may be split across several mystmd landing pages (e.g.
    `helpers.h` feeds api/helpers/api_file_system.md,
    api/helpers/api_string_helper.md, api/helpers/api_json_helper.md, …).
  - For every header, we aggregate the anchors from all the landings
    that claim to cover it, then compare against the header's PUBLIC set.

  - A function exported by the header but not anchored in ANY landing
    that covers that header is flagged as MISSING.
  - A function anchored in some landing that does not correspond to any
    header exported name is flagged as EXTRA (stale/removed).

This avoids the false positives you get from naive per-landing diffs
when several landings share a single header.
"""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DOCS = REPO / "docs" / "doc.yuneta.io-mystmd"
KERNEL = REPO / "kernel" / "c"


# Each entry: header → list of landing pages that collectively should
# document every PUBLIC function in that header.
HEADER_TO_LANDINGS: dict[Path, list[str]] = {
    # ---- gobj-c: gobj.h is split by topic across the GObj API landings ----
    # NOTE: we exclude api_gobj_log.md and api_gobj_memory.md from the gobj.h
    # cover list — those landings document glogger.h and gbmem.h respectively,
    # not symbols from gobj.h. Including them here would produce spurious
    # EXTRA reports on gobj.h for legit glogger/gbmem anchors.
    KERNEL / "gobj-c/src/gobj.h": [
        "api/gobj/api_gobj_startup.md",
        "api/gobj/api_gobj_gclass.md",
        "api/gobj/api_gobj_creation.md",
        "api/gobj/api_gobj_attrs.md",
        "api/gobj/api_gobj_op.md",
        "api/gobj/api_gobj_events_state.md",
        "api/gobj/api_gobj_resource.md",
        "api/gobj/api_gobj_publish.md",
        "api/gobj/api_gobj_authz.md",
        "api/gobj/api_gobj_info.md",
        "api/gobj/api_gobj_stats.md",
        "api/gobj/api_gobj_node.md",
        "api/gobj/api_gobj_trace.md",
    ],
    KERNEL / "gobj-c/src/gbuffer.h":    ["api/gobj/api_gobj_gbuffer.md"],
    KERNEL / "gobj-c/src/dl_list.h":    ["api/gobj/api_gobj_dl.md"],
    KERNEL / "gobj-c/src/gbmem.h":      ["api/gobj/api_gobj_memory.md"],
    KERNEL / "gobj-c/src/glogger.h":    ["api/gobj/api_gobj_log.md"],

    # ---- gobj-c: helpers.h is split by topic across helper landings + the
    # daemon launcher landing (launch_daemon lives in helpers.h, not ydaemon.h).
    KERNEL / "gobj-c/src/helpers.h": [
        "api/helpers/api_file_system.md",
        "api/helpers/api_string_helper.md",
        "api/helpers/api_json_helper.md",
        "api/helpers/api_directory_walk.md",
        "api/helpers/api_time_date.md",
        "api/helpers/api_misc.md",
        "api/helpers/api_common_protocol.md",
        "api/helpers/api_url_parsing.md",
        "api/helpers/api_backtrace.md",
        "api/helpers/api_daemon_launcher.md",
    ],
    # kwid.h is only covered by api_kwid.md. The json_* helpers that also
    # appear in api_json_helper.md live in helpers.h, not kwid.h.
    KERNEL / "gobj-c/src/kwid.h":       ["api/kwid/api_kwid.md"],

    # ---- aux APIs ----
    KERNEL / "gobj-c/src/command_parser.h":  ["api/command_parser/api_command_parser.md"],
    KERNEL / "gobj-c/src/stats_parser.h":    ["api/stats_parser/api_stats_parser.md"],
    KERNEL / "gobj-c/src/log_udp_handler.h": ["api/log_udp_handler/api_log_udp_handler.md"],
    KERNEL / "gobj-c/src/rotatory.h":        ["api/rotatory/api_rotatory.md"],
    KERNEL / "gobj-c/src/testing.h":         ["api/testing/api_testing.md"],

    # ---- http parser ----
    # llhttp.h is the upstream third-party parser header used internally;
    # it is not part of the Yuneta public API surface, so we do not verify
    # it. The public wrapper lives in ghttp_parser.h.
    KERNEL / "root-linux/src/ghttp_parser.h": ["api/helpers/api_http_parser.md"],

    # ---- istream (root-linux) ----
    KERNEL / "root-linux/src/istream.h":     ["api/helpers/api_istream.md"],

    # ---- daemon launcher (root-linux/ydaemon.h) ----
    KERNEL / "root-linux/src/ydaemon.h":     ["api/helpers/api_daemon_launcher.md"],

    # ---- timeranger2 ----
    KERNEL / "timeranger2/src/timeranger2.h": ["api/timeranger2/api_timeranger2.md"],
    KERNEL / "timeranger2/src/fs_watcher.h":  ["api/timeranger2/api_fs_watcher.md"],
    KERNEL / "timeranger2/src/tr_msg.h":      ["api/timeranger2/api_tr_msg.md"],
    KERNEL / "timeranger2/src/tr_msg2db.h":   ["api/timeranger2/api_tr_msg2db.md"],
    KERNEL / "timeranger2/src/tr_queue.h":    ["api/timeranger2/api_tr_queue.md"],
    KERNEL / "timeranger2/src/tr_treedb.h":   ["api/timeranger2/api_treedb.md"],

    # ---- event loop & TLS ----
    KERNEL / "yev_loop/src/yev_loop.h":  ["api/yev_loop/api_yev_loop.md"],
    KERNEL / "ytls/src/ytls.h":          ["api/ytls/api_ytls.md"],
}


ANCHOR_RE = re.compile(r"^\(([a-zA-Z_][a-zA-Z0-9_]*)\)=\s*$", re.MULTILINE)

PUBLIC_RE = re.compile(
    r"""
    ^[ \t]*PUBLIC[ \t]+      # the PUBLIC macro
    (?:[\w*\s]+?)            # return type (non-greedy)
    \b([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*\(   # function name followed by '('
    """,
    re.VERBOSE | re.MULTILINE,
)


def parse_anchors(path: Path) -> set[str]:
    if not path.exists():
        return set()
    return set(ANCHOR_RE.findall(path.read_text(encoding="utf-8")))


def parse_public_fns(path: Path) -> set[str]:
    if not path.exists():
        return set()
    # Skip `PUBLIC` as used for variables (rare). We require a '(' after
    # the name on the same line, which PUBLIC_RE already enforces.
    return set(PUBLIC_RE.findall(path.read_text(encoding="utf-8")))


@dataclass
class HeaderReport:
    header: Path
    landings: list[str]
    exported: set[str] = field(default_factory=set)
    documented: set[str] = field(default_factory=set)
    # Which landing each documented name came from (first match wins)
    doc_origin: dict[str, str] = field(default_factory=dict)

    @property
    def missing(self) -> set[str]:
        return self.exported - self.documented

    @property
    def extra(self) -> set[str]:
        return self.documented - self.exported


def main() -> int:
    # ---- caches --------------------------------------------------------
    header_exports: dict[Path, set[str]] = {}
    landing_anchors: dict[str, set[str]] = {}
    for header, landings in HEADER_TO_LANDINGS.items():
        header_exports.setdefault(header, parse_public_fns(header))
        for rel in landings:
            landing_anchors.setdefault(rel, parse_anchors(DOCS / rel))

    # ---- reverse map: landing → set of headers that claim it ----------
    landing_headers: dict[str, set[Path]] = defaultdict(set)
    for header, landings in HEADER_TO_LANDINGS.items():
        for rel in landings:
            landing_headers[rel].add(header)

    reports: list[HeaderReport] = []

    for header, landings in HEADER_TO_LANDINGS.items():
        rep = HeaderReport(header=header, landings=landings)
        rep.exported = header_exports[header]
        for rel in landings:
            for name in landing_anchors[rel]:
                # Only count the anchor as "documented by this header"
                # when the header actually exports the symbol. Otherwise
                # the anchor belongs to a sibling header that shares the
                # same landing and will be matched by that header's own
                # report — we must not flag it as EXTRA here.
                if name in rep.exported:
                    rep.documented.add(name)
                    rep.doc_origin.setdefault(name, rel)
                else:
                    # Check whether any sibling header exports it. If
                    # yes, silently skip. If no, it's a genuine EXTRA
                    # (stale/removed symbol) and we attribute it to the
                    # first landing that documents it.
                    owned_by_sibling = any(
                        name in header_exports.get(h, set())
                        for h in landing_headers[rel] if h != header
                    )
                    if not owned_by_sibling:
                        rep.documented.add(name)  # keep in set so EXTRA logic flags it
                        rep.doc_origin.setdefault(name, rel)
        reports.append(rep)

    # ---- per-header report ----
    total_exp = total_doc = total_matched = 0
    problems = 0

    for rep in reports:
        matched = rep.exported & rep.documented
        total_exp += len(rep.exported)
        total_doc += len(rep.documented)
        total_matched += len(matched)

        has_problem = bool(rep.missing or rep.extra)
        if has_problem:
            problems += 1

        tag = "OK" if not has_problem else "!!"
        print(f"\n=== [{tag}] {rep.header.relative_to(REPO)} ===")
        print(f"  exported: {len(rep.exported):>4}   documented: {len(rep.documented):>4}   matched: {len(matched):>4}")
        print(f"  covering landings:")
        for rel in rep.landings:
            print(f"    - {rel}")
        if rep.missing:
            print(f"  MISSING  ({len(rep.missing)}): in header, not in any covering landing")
            for name in sorted(rep.missing):
                print(f"      - {name}")
        if rep.extra:
            print(f"  EXTRA    ({len(rep.extra)}): in docs, not declared in header")
            for name in sorted(rep.extra):
                origin = rep.doc_origin.get(name, "?")
                print(f"      - {name}   ({origin})")

    print("\n" + "=" * 70)
    print("GLOBAL SUMMARY")
    print("=" * 70)
    print(f"  Headers checked:    {len(reports)}")
    print(f"  Headers w/ issues:  {problems}")
    print(f"  Total exported:     {total_exp}")
    print(f"  Total documented:   {total_doc}")
    print(f"  Matched:            {total_matched}")
    print(f"  Missing (gaps):     {sum(len(r.missing) for r in reports)}")
    print(f"  Extra (stale):      {sum(len(r.extra) for r in reports)}")

    return 0 if problems == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
