#!/usr/bin/env python3
"""
check_store_schemas.py -- what the treedb and msg2db schemas persisted in a
store will make the SDK say when a yuno opens them.

Since 7.27.0 a topic already on disk that breaks a rule of a NEW topic is
opened as it was, and the treedb says so with an ERROR at EVERY open; a few
faults leave the topic out, and one class refuses it (a CRITICAL, which exits
a yuno whose on_critical_error asks for it). Run this on every store of a node
BEFORE its SDK is upgraded, and fix what it prints: on a clean store it prints
nothing but the count of what it read.

    python3 check_store_schemas.py                    # every store under /yuneta/store
    python3 check_store_schemas.py /yuneta/store/x    # one store, or several dirs
    python3 check_store_schemas.py --list-ids-gobj tranger_x <dir>   # see LIST below
    python3 check_store_schemas.py --list-ids-yuno db_history_ce^1620 <dir>

It needs nothing but python3, so it runs on a node with no SDK sources:
installed with the packages under /yuneta/development/yunetas/tools/treedb/,
and, on a node still on an older package, streamed over ssh:

    ssh <node> python3 - /yuneta/store < tools/treedb/check_store_schemas.py

What it reads: every `<treedb>.treedb_schema.json` (the schema treedb_open_db()
persists, its `topics` a list or a dict), every `<treedb>-<topic>.treedb_schema.json`
(the schema of one topic, written by treedb_create_topic() with create_schema),
and every `<msg2db>.msg2db_schema.json`. One line per fault:

    <file>: <LEVEL> <topic>[.<column>]: <the fault> -- "<what the SDK logs>"

LEVEL, by what the SDK does at the open:
    NO-OPEN     the treedb is not opened at all (an ERROR), when this file is
                the schema it opens with (the persisted one wins unless the
                code's has a higher schema_version): the line says the
                schema_version a master's schema from C must differ from to
                replace it
    LEFT-OUT    the topic is not opened (an ERROR): its records stay on disk,
                unreachable until the schema is fixed
    CRITICAL    "Cannot create topic of the schema": the treedb is not the one
                the schema describes, and a yuno whose on_critical_error exits
                exits
    ERROR       the topic opens as it was, and the ERROR is logged at every open
    PROJECTION  the projection of the schema into __system__ (C_TREEDB, on the
                master) refuses the node, an ERROR at every projection
    CUT         the __assets__ hook of a 'file' column is cut to NAME_MAX at
                the open, with no log (and refused for a new topic)
    LIST        the id of a list of the topic's indexes is cut to NAME_MAX: it
                holds the yuno that opens the store (`<role>^<name>`, after
                7.27.10) and the gclass and the name of the gobj that owns the
                tranger, which no file says; it is ESTIMATED with a yuno of 32
                bytes (longer than the yunos of our nodes: `db_history_ce^1620`
                is 18), or the one given with --list-ids-yuno (an empty one for
                an SDK up to 7.27.10), and with C_TRANGER and `tranger_<treedb>`
                (the name C_TREEDB gives it), or the name given with
                --list-ids-gobj

Not checked: the fields of a column against the column meta-schema
(parse_schema_cols(): its types and flags), which the SDK says as "a column is
not valid (see the log)"; it is derived from the __system__ schema at run time,
and a copy here would drift from it.

Exit status: 0 nothing found, 1 a fault found, 2 a file could not be read.
"""
import argparse
import json
import os
import sys

NAME_MAX = 255
RECORD_KEY_VALUE_MAX = NAME_MAX + 1
TREEDB_SUFFIX = ".treedb_schema.json"
MSG2DB_SUFFIX = ".msg2db_schema.json"

#
#   The rules, as the SDK writes them (timeranger2.c, tr_treedb.c, kwid.c)
#


def topic_name_ok(name):
    """tranger2_name_is_identifier(): a directory and a key"""
    return isinstance(name, str) and name not in ("", ".", "..") and "/" not in name and "`" not in name


def col_name_ok(name):
    """treedb_col_name_is_identifier(): a key only"""
    return isinstance(name, str) and name != "" and "`" not in name


def has_word(flag, word):
    """kw_has_word(): a dict asks `word: true`, a list or a string a SUBSTRING"""
    if isinstance(flag, dict):
        return flag.get(word) is True
    if isinstance(flag, list):
        return any(isinstance(f, str) and word in f for f in flag)
    if isinstance(flag, str):
        return word in flag
    return False


def ids_of(value):
    """kwid_get_ids(): a name, a list of names (or of dicts with `id`), a dict of them"""
    if value is None:
        return []
    if isinstance(value, str):
        return [value]
    if isinstance(value, bool):
        return []
    if isinstance(value, int):
        return [str(value)]
    if isinstance(value, dict):
        return list(value.keys())
    if isinstance(value, list):
        ids = []
        for v in value:
            if isinstance(v, str) and v:
                ids.append(v)
            elif isinstance(v, bool):
                continue
            elif isinstance(v, int):
                ids.append(str(v))
            elif isinstance(v, dict) and isinstance(v.get("id"), str) and v.get("id"):
                ids.append(v["id"])
        return ids
    return []


def cols_as_pairs(cols):
    """[(column id, column)] of a dict of columns or a list of them"""
    if isinstance(cols, dict):
        return list(cols.items())
    if isinstance(cols, list):
        return [(c.get("id") if isinstance(c, dict) else None, c) for c in cols]
    return []


def nbytes(s):
    return len(s.encode("utf-8"))


#
#   One topic of a treedb schema
#


class Report:
    def __init__(self):
        self.faults = 0
        self.unreadable = 0
        self.files = 0

    def say(self, path, level, where, fault, logged):
        self.faults += 1
        print(f'{path}: {level} {where}: {fault} -- "{logged}"')


def topic_names(t, key):
    """The name treedb_open_db() gives a schema topic: `topic_name`, else `id`
    (in a dict of topics, the key is written into `id`)"""
    if key is not None:
        t = dict(t, id=key)
    tn = t.get("topic_name")
    if isinstance(tn, str) and tn:
        return tn
    tid = t.get("id")
    return tid if isinstance(tid, str) else ""


def check_treedb_topic(rep, path, treedb, t, key, tranger_dir, list_gobj, list_yuno):
    tn = topic_names(t, key)
    if not tn:
        rep.say(path, "LEFT-OUT", "?", "a topic with no id nor topic_name",
                "Schema topic without id/topic_name")
        return
    if t.get("pkey") != "id":
        rep.say(path, "LEFT-OUT", tn, f"pkey is {t.get('pkey')!r}, not 'id'",
                "Schema topic without pkey=id")
        return
    if t.get("system_flag") != "sf_string_key":
        rep.say(path, "LEFT-OUT", tn, f"system_flag is {t.get('system_flag')!r}",
                "Schema topic without system_flag=sf_string_key")
        return

    cols = t.get("cols")
    pairs = cols_as_pairs(cols)

    #   The name rule: left out, not fatal (treedb_open_db())
    bad = None
    if not topic_name_ok(tn):
        bad = tn
    else:
        for cid, _c in pairs:
            if isinstance(cid, str) and not col_name_ok(cid):
                bad = f"{tn}.{cid}"
                break
    if bad is not None:
        rep.say(path, "LEFT-OUT", json.dumps(bad), "a name breaks the name rule",
                "Topic of the schema left out: a name breaks the name rule")
        return

    #   What treedb_create_topic() refuses of any topic (check_new_topic()):
    #   a CRITICAL at the open
    def critical(where, fault, logged):
        rep.say(path, "CRITICAL", where, fault, logged)

    if nbytes(tn) > NAME_MAX:
        critical(tn, "the topic name is longer than NAME_MAX", "Topic refused: bad name")
        return
    if not isinstance(cols, (dict, list)) or len(cols) == 0:
        critical(tn, "no columns", "Topic refused: bad columns")
        return
    for cid, c in pairs:
        if not isinstance(c, dict):
            critical(f"{tn}.{cid}", "a column is not a json object", "Topic refused: bad columns")
            return
        if isinstance(cols, list) and not isinstance(cid, str):
            critical(tn, "a column without an id, or an id that is not a string",
                     "Topic refused: bad columns")
            return
    faulty = False
    for cid, c in pairs:
        flag = c.get("flag", [])
        if has_word(flag, "file"):
            if not has_word(flag, "fkey"):
                critical(f"{tn}.{cid}", "a 'file' column without 'fkey'",
                         "a 'file' column must be flagged 'fkey' too")
                faulty = True
            elif c.get("type") != "string":
                critical(f"{tn}.{cid}", f"a 'file' column of type {c.get('type')!r}",
                         "a 'file' column must be of type 'string': one file per column")
                faulty = True
        if has_word(flag, "hook") and has_word(flag, "fkey"):
            critical(f"{tn}.{cid}", "a column both 'hook' and 'fkey'",
                     "A column cannot be both 'hook' and 'fkey'")
            faulty = True
    if faulty:
        return
    if not any(cid == "id" for cid, _c in pairs):
        critical(tn, "no 'id' column (the pkey is 'id')", "Topic refused: bad columns")
        return

    tkey = t.get("tkey") or ""
    if not isinstance(tkey, str):
        tkey = ""
    pkey2s = t.get("pkey2s")
    colids = {cid for cid, _c in pairs if isinstance(cid, str)}

    #   A topic ON DISK keeps its pkey2s and list ids, each fault said at
    #   every open (say_faults_of_topic_on_disk()); a topic not on disk yet
    #   is refused for them (check_new_topic(), the CRITICAL)
    on_disk = os.path.isfile(os.path.join(tranger_dir, tn, "topic_desc.json"))
    level = "ERROR" if on_disk else "CRITICAL"
    logged_refs = ("Topic on disk opened as it was, with a name of its references or of its keys at fault"
                   if on_disk else "Topic refused: a name of its references or of its keys")
    logged_pkey2 = ("Topic on disk opened as it was, with bad pkey2s"
                    if on_disk else "Topic refused: bad pkey2s")

    #   refs_and_keys_fault()
    if "^" in tn:
        rep.say(path, level, tn, "the topic name holds a '^', the separator of a reference", logged_refs)
    for cid, c in pairs:
        flag = c.get("flag", [])
        if has_word(flag, "hook") and ("^" in cid or nbytes(cid) >= NAME_MAX):
            rep.say(path, level, f"{tn}.{cid}",
                    "a hook id with a '^', or too long to be a part of a reference", logged_refs)
        elif has_word(flag, "file") and "^" in cid:
            rep.say(path, level, f"{tn}.{cid}", "a 'file' column id with a '^'", logged_refs)
    if isinstance(pkey2s, (str, list, dict)):
        for p2 in ids_of(pkey2s):
            if p2 and p2 not in colids:
                rep.say(path, level, f"{tn}.{p2}",
                        "a pkey2 names no column of the topic: its index would hold nothing", logged_refs)

    #   pkey2s_fault()
    if pkey2s is not None and not isinstance(pkey2s, (str, list, dict)):
        rep.say(path, level, tn, "pkey2s must be a name, a list of names or a dict of them", logged_pkey2)
    else:
        seen = set()
        for p2 in ids_of(pkey2s):
            if not p2:
                continue
            why = None
            if not col_name_ok(p2):
                why = "a pkey2 name breaks the column name rule"
            elif p2 == "id":
                why = "a pkey2 cannot be the pkey 'id'"
            elif tkey and p2 == tkey:
                why = "a pkey2 cannot be the tkey: the lists of their indexes would share one id"
            elif p2 in seen:
                why = "a pkey2 named twice"
            if why:
                rep.say(path, level, f"{tn}.{p2}", why, logged_pkey2)
                break
            seen.add(p2)

    #   list_id_too_long(): "treedb-<yuno>-<gclass>-<gobj>-<treedb>-<topic>-<key>"
    gobj_name = list_gobj or f"tranger_{treedb}"
    yuno = f"{list_yuno}-" if list_yuno else ""
    keys = [tkey] + [k for k in ids_of(pkey2s) if k] if isinstance(pkey2s, (str, list, dict)) else [tkey]
    for k in keys:
        list_id = f"treedb-{yuno}C_TRANGER-{gobj_name}-{treedb}-{tn}-{k}"
        if nbytes(list_id) >= NAME_MAX:
            rep.say(path, "LIST", f"{tn}.{k or '(tkey)'}",
                    f"the id of a list of its indexes is {nbytes(list_id)} bytes (estimated)",
                    "Topic on disk opened as it was, the id of a list of its indexes cut to NAME_MAX"
                    if on_disk else "Topic refused: the id of a list of its indexes is longer than NAME_MAX")
            break

    #   The projection into __system__: "<treedb>.<topic>.<col>" is a record
    #   key (build_schema_node_id()), and a link column of a wrong type is
    #   refused there (link_col_type_is_valid())
    if nbytes(f"{treedb}.{tn}") >= RECORD_KEY_VALUE_MAX:
        rep.say(path, "PROJECTION", tn, "'<treedb>.<topic>' is longer than a record key",
                "Qualified 'id' does not fit in a record key")
    for cid, c in pairs:
        if nbytes(f"{treedb}.{tn}.{cid}") >= RECORD_KEY_VALUE_MAX:
            rep.say(path, "PROJECTION", f"{tn}.{cid}", "'<treedb>.<topic>.<column>' is longer than a record key",
                    "Qualified 'id' does not fit in a record key")
        flag = c.get("flag", [])
        ctype = c.get("type") if isinstance(c.get("type"), str) else ""
        if has_word(flag, "hook") and ctype not in ("dict", "object", "list", "array"):
            rep.say(path, "PROJECTION", f"{tn}.{cid}", f"a hook column of type {ctype!r}",
                    "A hook column must be of type dict or list")
        elif has_word(flag, "fkey") and not has_word(flag, "hook") and \
                ctype not in ("dict", "object", "list", "array", "string"):
            rep.say(path, "PROJECTION", f"{tn}.{cid}", f"an fkey column of type {ctype!r}",
                    "An fkey column must be of type dict, list or string")


def file_hook_of(tn, cid):
    """derive_file_hooks(): 'as_<topic>_<col>' in a char[NAME_MAX], cut with no log"""
    return f"as_{tn}_{cid}".encode("utf-8")[:NAME_MAX - 1].decode("utf-8", "ignore")


def check_file_hooks(rep, path, topics):
    """The __assets__ hook each 'file' column derives: one per column"""
    derived = {}
    for key, t in topics:
        if not isinstance(t, dict):
            continue
        tn = topic_names(t, key)
        for cid, c in cols_as_pairs(t.get("cols")):
            if not (isinstance(cid, str) and isinstance(c, dict) and has_word(c.get("flag", []), "file")):
                continue
            hook = file_hook_of(tn, cid)
            if nbytes(f"as_{tn}_{cid}") >= NAME_MAX:
                rep.say(path, "CUT", f"{tn}.{cid}",
                        "its __assets__ hook 'as_<topic>_<column>' is longer than NAME_MAX: cut at the open, "
                        "with no log, and a new topic with it is refused",
                        "the names of the topic and of its 'file' column are too long for the name of their "
                        "__assets__ hook")
            other = derived.get(hook)
            if other and other != f"{tn}.{cid}":
                rep.say(path, "ERROR", f"{tn}.{cid}", f"derives the same __assets__ hook {hook!r} as {other}",
                        "two 'file' columns derive the same __assets__ hook")
            derived.setdefault(hook, f"{tn}.{cid}")


def check_main_topics(rep, path, topics):
    """main_topic: one per treedb, on a topic hooked to itself (treedb_open_db())"""
    marked = None
    for key, t in topics:
        if not isinstance(t, dict) or t.get("main_topic") is not True:
            continue
        tn = topic_names(t, key)
        if marked:
            rep.say(path, "ERROR", tn, f"a second main_topic ({marked!r} is the first)",
                    "Schema marks more than one main_topic, the first one is kept")
            continue
        hooked = False
        for _cid, c in cols_as_pairs(t.get("cols")):
            if isinstance(c, dict) and has_word(c.get("flag", []), "hook") and \
                    isinstance(c.get("hook"), dict) and tn in c["hook"]:
                hooked = True
                break
        if not hooked:
            rep.say(path, "ERROR", tn, "main_topic on a topic not hooked to itself",
                    "main_topic marks a topic that is not hooked to itself, ignored")
            continue
        marked = tn


INT64_MIN = -(1 << 63)
INT64_MAX = (1 << 63) - 1


def strtoll_base0(text):
    """strtoll(text, 0, 0): leading blanks, a sign, then 0x/0X hexadecimal, a
    leading 0 octal, else decimal; the longest prefix that reads, 0 if none,
    clamped to the range of a json_int_t"""
    i = 0
    n = len(text)
    while i < n and text[i] in " \t\n\v\f\r":
        i += 1
    sign = 1
    if i < n and text[i] in "+-":
        sign = -1 if text[i] == "-" else 1
        i += 1
    base = 10
    digits = "0123456789"
    if text[i:i + 2] in ("0x", "0X") and i + 2 < n and text[i + 2].lower() in "0123456789abcdef":
        base = 16
        digits = "0123456789abcdef"
        i += 2
    elif text[i:i + 1] == "0":
        base = 8
        digits = "01234567"
    value = 0
    while i < n and text[i].lower() in digits:
        value = value * base + digits.index(text[i].lower())
        i += 1
    value *= sign
    return max(INT64_MIN, min(INT64_MAX, value))


def wild_int(value):
    """kw_get_int(..., KW_WILD_NUMBER), the way treedb_open_db() reads the
    schema_version of the file: an integer as it is, a real truncated, a
    boolean 1 or 0, a string by strtoll() with base 0 ("3" is 3, "0x10" 16,
    "010" 8, "3abc" 3), a null 0; a list or a dict is an error, 0"""
    if isinstance(value, bool):
        return 1 if value else 0
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if value != value or value >= 2.0 ** 63 or value < -2.0 ** 63:
            return INT64_MIN    # what the cast of x86-64 gives out of range
        return int(value)
    if isinstance(value, str):
        return strtoll_base0(value)
    return 0


def unless_replaced(data):
    """What lifts a NO-OPEN: treedb_open_db() opens with the persisted schema
    unless the yuno is the MASTER and its schema from C has another
    schema_version than the file's (a higher one, or any other when imposed)
    -- then the code's schema replaces the file at that open"""
    v = wild_int(data.get("schema_version", 0)) if isinstance(data, dict) else 0
    return (f" (unless a master opens it with a schema from C whose schema_version is not {v},"
            f" a higher one when not imposed: that one replaces this file)")


def check_treedb_file(rep, path, data, list_gobj, list_yuno):
    fn = os.path.basename(path)
    tranger_dir = os.path.dirname(path)
    if isinstance(data, dict) and "topics" not in data and "topic_name" not in data:
        rep.say(path, "NO-OPEN", fn[:-len(TREEDB_SUFFIX)], "a treedb schema with no 'topics'" + unless_replaced(data), "No topics found")
    elif isinstance(data, dict) and "topics" in data:
        treedb = fn[:-len(TREEDB_SUFFIX)]
        topics = data.get("topics")
        if isinstance(topics, dict):
            items = list(topics.items())
        elif isinstance(topics, list):
            items = [(None, t) for t in topics]
        else:
            rep.say(path, "NO-OPEN", treedb, "'topics' is neither a list nor a dict" + unless_replaced(data), "No topics found")
            return
        for key, t in items:
            if not isinstance(t, dict):
                rep.say(path, "LEFT-OUT", "?", "a topic that is not a json object", "Schema topic without id/topic_name")
                continue
            check_treedb_topic(rep, path, treedb, t, key, tranger_dir, list_gobj, list_yuno)
        check_main_topics(rep, path, items)
        check_file_hooks(rep, path, items)
    elif isinstance(data, dict):
        #   "<treedb>-<topic>.treedb_schema.json": one topic, not read at an
        #   open (it names the topic as the treedb's own): judged as a topic
        tn = data.get("topic_name") if isinstance(data.get("topic_name"), str) else ""
        stem = fn[:-len(TREEDB_SUFFIX)]
        treedb = stem[:-(len(tn) + 1)] if tn and stem.endswith("-" + tn) else stem
        check_treedb_topic(rep, path, treedb, data, None, tranger_dir, list_gobj, list_yuno)
    else:
        rep.say(path, "NO-OPEN", "?", "the schema is not a json object" + unless_replaced(data), "No topics found")


def check_msg2db_file(rep, path, data):
    """msg2db_open_db(): a topic is named by `id` first, then `topic_name`"""
    topics = data.get("topics") if isinstance(data, dict) else None
    if isinstance(topics, dict):
        items = [dict(t, id=k) if isinstance(t, dict) else t for k, t in topics.items()]
    elif isinstance(topics, list):
        items = topics
    else:
        return
    for t in items:
        if not isinstance(t, dict):
            continue
        tid = t.get("id") if isinstance(t.get("id"), str) and t.get("id") else t.get("topic_name")
        tn = tid if isinstance(tid, str) else ""
        if not tn:
            rep.say(path, "LEFT-OUT", "?", "a topic with no id nor topic_name", "Schema topic without topic_name")
        elif not topic_name_ok(tn):
            rep.say(path, "LEFT-OUT", json.dumps(tn), "the topic name breaks the name rule",
                    "Topic of the msg2db left out: its name breaks the name rule")
        elif t.get("pkey") != "id":
            rep.say(path, "LEFT-OUT", tn, f"pkey is {t.get('pkey')!r}, not 'id'", "Schema topic without pkey=id")
        elif not (isinstance(t.get("pkey2"), str) and t.get("pkey2")):
            rep.say(path, "LEFT-OUT", tn, "no pkey2", "Schema topic without pkey2")


#
#   Main
#


def main():
    ap = argparse.ArgumentParser(
        description="Say what the schemas persisted in a store make the SDK log when they are opened."
    )
    ap.add_argument("dirs", nargs="*", default=["/yuneta/store"], help="store directories (default /yuneta/store)")
    ap.add_argument("--list-ids-gobj", default=None,
                    help="the name of the gobj that owns the tranger, for the LIST estimate (default tranger_<treedb>)")
    ap.add_argument("--list-ids-yuno", default="y" * 32,
                    help="the <role>^<name> of the yuno that opens the store, for the LIST estimate "
                         "(default 32 bytes; empty for an SDK up to 7.27.10)")
    args = ap.parse_args()

    rep = Report()
    for root in args.dirs:
        if not os.path.isdir(root):
            print(f"{root}: not a directory", file=sys.stderr)
            rep.unreadable += 1
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames.sort()
            for fn in sorted(filenames):
                is_treedb = fn.endswith(TREEDB_SUFFIX)
                is_msg2db = fn.endswith(MSG2DB_SUFFIX)
                if not (is_treedb or is_msg2db):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    with open(path) as f:
                        data = json.load(f)
                except (OSError, ValueError) as e:
                    print(f"{path}: UNREADABLE {e}", file=sys.stderr)
                    rep.unreadable += 1
                    continue
                rep.files += 1
                if is_treedb:
                    check_treedb_file(rep, path, data, args.list_ids_gobj, args.list_ids_yuno)
                else:
                    check_msg2db_file(rep, path, data)

    print(f"checked {rep.files} schema files: {rep.faults} faults, {rep.unreadable} unreadable", file=sys.stderr)
    if rep.unreadable:
        return 2
    return 1 if rep.faults else 0


if __name__ == "__main__":
    sys.exit(main())
