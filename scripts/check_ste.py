#!/usr/bin/env python3
"""
Flag the mechanically-detectable violations of Simplified Technical English.

The docs under `docs/doc.yuneta.io/` and `yunos/c/yuno_agent/*.md` are written
to the structural rules of ASD-STE100 (short sentences, one word one meaning,
simple tenses, active voice, condition before command). Most of those rules
need judgement. A useful subset does not: a contraction is a contraction, a
semicolon is a semicolon, `authorisation` is British.

This tool reports only that subset. It is a spell-checker, not a grader:
a clean run does NOT mean the text is good STE, and it can never mean
compliance. Read the prose.

Usage:

    scripts/check_ste.py docs/doc.yuneta.io/*.md
    scripts/check_ste.py --summary yunos/c/yuno_agent/*.md
    scripts/check_ste.py --rule semicolon FILE...     # one category
    scripts/check_ste.py --strict FILE...             # include judgement calls

Exit code 1 when anything is reported, so it works in a pre-commit hook.
`--strict` adds the categories that need a human decision (a possessive is
legal when correct; "just" is filler in "just run it" and temporal in "the
yuno you just built"). Those are OFF by default to keep a clean run meaningful.

What it deliberately does NOT read (the Untouchables — these are technical
names, and STE exempts them):

  - fenced code blocks (``` and ~~~) and indented code blocks
  - inline code spans, link targets, bare URLs
  - double-quoted strings, which are quoted text under rule 8.6 — this is
    what keeps the literal command description
    *"WARNING: Don't use in production!"* from being reported forever

Markdown table cells ARE read. They were skipped in an earlier version of
this script, which is how about 50 constructions survived a pass that
reported the files clean. Reference tables are exactly where an operator
looks something up.
"""

import argparse
import re
import sys

#
#   Rule catalog
#
#   (id, regex, description). The id is what --rule filters on. The
#   description names the ASD-STE100 rule where one applies; the numbering
#   comes from the simple-english skill's references/rules.md, never from
#   memory.
#
RULES = [
    (
        "contraction",
        r"\b\w+n['’]t\b|\b\w+['’](ll|re|ve|d|m)\b|\b(it|that|there|what|here|let|who|he|she)['’]s\b",
        "contraction (4.2) — keep complete grammar",
    ),
    (
        "perfect",
        r"\b(has|have|had) been\b",
        "perfect tense (3.4) — use the simple past",
    ),
    (
        "modal",
        r"\b(should|would|might|could|may)\b",
        "banned modal (3.2) — approved are can, will, must",
    ),
    (
        "semicolon",
        r";(\s|$)",
        "semicolon (8.1) — write two sentences",
    ),
    (
        "latin",
        r"\be\.g\.|\bi\.e\.|\betc\.",
        "latin abbreviation (GR-6) — for example / that is / name them",
    ),
    (
        "progressive",
        r"\b(is|are|was|were) being\b",
        "progressive passive (3.4) — active, simple tense",
    ),
    (
        "ing_clause",
        r",\s+(making|allowing|enabling|ensuring|meaning|leaving|causing|letting)\b",
        '"-ing" clause used as a verb (3.5) — start a new sentence',
    ),
    (
        "british",
        r"\b\w*(authoris|organis|recognis|initialis|serialis|normalis|standardis|minimis|maximis)\w*\b"
        r"|\banalys(e|ed|es|ing)\b"
        r"|\b(behaviour|colour|centralised|decentralised|licence|defence|cancelled|catalogue)\b",
        "British spelling (1.14) — use American English",
    ),
    (
        "phrasal",
        r"\b(stack up|fan out|fits in|set up|go down|falls? back|comes? back|comes? from"
        r"|ends? up|figures? out|carr(y|ies) out|turns? on|turns? off|shuts? down)\b",
        "phrasal verb (9.3) — a plain verb exists",
    ),
    (
        "slop",
        r"\b(leverage|utilize|utilise|in order to|prior to|ensure|functionality|plethora|myriad"
        r"|allows? you to|enables? you to|facilitate|under the hood|out of the box|streamline"
        r"|seamlessly|effortlessly|robust|blazingly)\b",
        "slop — say the fact instead",
    ),
    (
        "filler_hard",
        r"\b(simply|easily|actually|basically|obviously|of course|it is worth noting)\b",
        "filler — delete it",
    ),
    (
        "slash",
        r"\band/or\b",
        'and/or — pick one, or write "X, or Y, or both"',
    ),
]

#
#   Judgement calls (--strict only)
#
#   These need a human. A possessive is legal when it is correct (GR-8), and
#   "just" is filler in "just run it" but temporal in "the yuno you just
#   built". Reporting them by default would train the reader to ignore the
#   output.
#
SOFT_RULES = [
    (
        "perfect_maybe",
        r"\b(has|have) [a-z]+ed\b",
        "possible present perfect (3.4) — or an adjective, as in "
        '"configs have structured write commands"',
    ),
    (
        "possessive",
        r"\b\w+['’]s\b",
        "possessive (GR-8) — legal when correct, hard for non-native readers",
    ),
    (
        "filler_soft",
        r"\bjust\b",
        'filler or temporal — "just run it" is filler, "just built" is not',
    ),
]


#
#   Proper nouns that carry a construction the rules would otherwise flag.
#   A product name is a technical name (1.5), so it stays exact.
#
PROPER_NOUNS = [
    r"Let['’]s Encrypt",
]


#
#   Untouchable stripping
#
def strip_untouchables(line, in_quote):
    """Blank out everything STE exempts, keeping the line length stable.

    Returns (text, in_quote). `in_quote` carries an unterminated double quote
    into the next line: a quoted sentence often wraps, and the closing quote
    then sits on a later line. Without the carry-over, the first half of the
    quote gets scanned as prose.
    """
    def blank(match):
        return " " * len(match.group(0))

    for proper_noun in PROPER_NOUNS:
        line = re.sub(proper_noun, blank, line)

    line = re.sub(r"`[^`]*`", blank, line)          # inline code
    line = re.sub(r"\]\([^)]*\)", blank, line)      # link and image targets
    line = re.sub(r"<https?://[^>]*>", blank, line)  # autolinks
    line = re.sub(r"https?://\S+", blank, line)     # bare URLs

    #
    #   Quoted text is one word under rule 8.6, and it is Untouchable: it
    #   reproduces a log line, an error, or a command description.
    #
    out = []
    for char in line:
        if char == '"':
            in_quote = not in_quote
            out.append(" ")
        else:
            out.append(" " if in_quote else char)
    return "".join(out), in_quote


def scan(path, rules):
    """Yield (lineno, rule_id, description, match, raw_line)."""
    hits = []
    fence = None
    in_quote = False
    for lineno, raw in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        stripped = raw.strip()

        if fence:
            if stripped.startswith(fence):
                fence = None
            continue
        if stripped.startswith("```") or stripped.startswith("~~~"):
            fence = stripped[:3]
            continue
        if not stripped:
            in_quote = False                         # a quote never spans a paragraph
            continue
        if raw.startswith("    ") and not stripped.startswith(("-", "*", "|", ">", "1.")):
            continue                                 # indented code block

        text, in_quote = strip_untouchables(raw, in_quote)
        for rule_id, pattern, description in rules:
            for match in re.finditer(pattern, text, re.IGNORECASE):
                hits.append((lineno, rule_id, description, match.group(0).strip(), stripped))
    return hits


#
#   Entry point
#
def main():
    parser = argparse.ArgumentParser(
        description="Flag mechanical Simplified Technical English violations.")
    parser.add_argument("files", nargs="*")
    parser.add_argument("--summary", action="store_true",
                        help="one line per file, with a count per rule")
    parser.add_argument("--rule", metavar="ID",
                        help="report only this rule id")
    parser.add_argument("--strict", action="store_true",
                        help="add the judgement-call rules (possessive, filler_soft)")
    parser.add_argument("--list-rules", action="store_true",
                        help="print the rule ids and exit")
    args = parser.parse_args()

    rules = RULES + (SOFT_RULES if args.strict else [])
    if args.list_rules:
        for rule_id, _, description in rules:
            print(f"{rule_id:<14} {description}")
        return 0
    if args.rule:
        rules = [r for r in rules if r[0] == args.rule]
        if not rules:
            print(f"unknown rule: {args.rule} (see --list-rules)", file=sys.stderr)
            return 2
    if not args.files:
        parser.error("no files given")

    total = 0
    for path in args.files:
        try:
            hits = scan(path, rules)
        except OSError as error:
            print(f"{path}: {error}", file=sys.stderr)
            return 2
        total += len(hits)

        if args.summary:
            if hits:
                counts = {}
                for _, rule_id, _, _, _ in hits:
                    counts[rule_id] = counts.get(rule_id, 0) + 1
                detail = ", ".join(f"{k}={v}" for k, v in sorted(counts.items()))
                print(f"{path}: {len(hits)} ({detail})")
            else:
                print(f"{path}: clean")
            continue

        if hits:
            print(f"\n===== {path}: {len(hits)}")
            for lineno, rule_id, description, match, raw in hits:
                print(f"  {lineno:>5} | {rule_id:<12} | {match!r:<16} | {raw[:64]}")
                print(f"        | {description}")

    if total and not args.summary:
        print(f"\n{total} reported. A clean run is not compliance — read the prose.")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
