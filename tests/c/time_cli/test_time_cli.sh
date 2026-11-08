#!/bin/bash
#
#   time2date and time2range: what they print and their exit code, for good
#   input, a text that is no date, an unknown zone and a range that is no
#   integer. A refusal exits non-zero and prints no answer.
#
#   Usage: test_time_cli.sh <time2date> <time2range>
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
set -u

TIME2DATE=$1
TIME2RANGE=$2

export TZ=UTC
unset TZDIR

failures=0

#
#   Run one case: expected exit (0 or "refused"), a text the output must
#   hold, a text it must not hold, then the command
#
check() {
    local label=$1
    local expected_exit=$2
    local must_hold=$3
    local must_not_hold=$4
    shift 4
    local out
    out=$("$@" 2>&1)
    local rc=$?
    local ok=1
    if [ "$expected_exit" = "0" ]; then
        if [ $rc -ne 0 ]; then
            ok=0
        fi
    else
        if [ $rc -eq 0 ]; then
            ok=0
        fi
    fi
    if [ -n "$must_hold" ] && [[ "$out" != *"$must_hold"* ]]; then
        ok=0
    fi
    if [ -n "$must_not_hold" ] && [[ "$out" == *"$must_not_hold"* ]]; then
        ok=0
    fi
    if [ $ok -eq 1 ]; then
        echo "ok   $label (exit $rc)"
    else
        echo "FAIL $label (exit $rc, expected $expected_exit)"
        echo "$out" | sed 's/^/     | /'
        failures=$((failures + 1))
    fi
}

#
#   An error must be said once
#
check_said_once() {
    local label=$1
    local text=$2
    shift 2
    local count
    count=$("$@" 2>&1 | grep -c "$text")
    if [ "$count" -eq 1 ]; then
        echo "ok   $label: '$text' said once"
    else
        echo "FAIL $label: '$text' said $count times (expected 1)"
        failures=$((failures + 1))
    fi
}

#
#   time2date
#
check "time2date epoch seconds" 0 "UTC 2026-10-21T09:00:00+0000" "" \
    "$TIME2DATE" 1792573200
check "time2date a date" 0 "gmtime of 1790848800 " "" \
    "$TIME2DATE" 2026-10-01T10:00:00Z
check "time2date in a zone" 0 "Europe/Madrid 2026-10-21T11:00:00+0200" "" \
    "$TIME2DATE" -z Europe/Madrid 1792573200
check "time2date a text that is no date" refused "no date: '2026-10-01Tgarbage'" "gmtime of" \
    "$TIME2DATE" 2026-10-01Tgarbage
check "time2date a date with garbage after it" refused "" "gmtime of" \
    "$TIME2DATE" 2026-10-01T10:00:00Zgarbage
check "time2date an unknown zone" refused "Unknown time zone" "gmtime of" \
    "$TIME2DATE" -z Nowhere/Zone 1792573200
check_said_once "time2date an unknown zone" "Unknown time zone" \
    "$TIME2DATE" -z Nowhere/Zone 1792573200
check "time2date 0" 0 "gmtime of 0 " "" \
    "$TIME2DATE" 0
check "time2date a time before the epoch" 0 "UTC 1969-12-31T23:59:55+0000" "" \
    "$TIME2DATE" -- -5
check "time2date milliseconds" 0 "gmtime of 1792573200 " "" \
    "$TIME2DATE" -m 1792573200123
check "time2date milliseconds before the epoch" 0 "UTC 1969-12-31T23:59:55+0000" "" \
    "$TIME2DATE" -m -- -5000
for number in "abc" "007" "0x10" " 5" "+5" "5 " "1e3" "5.0" "-" ""; do
    check "time2date '$number'" refused "TIME_T refused, it is no integer: '$number'" "gmtime of" \
        "$TIME2DATE" -- "$number"
done
for number in "9223372036854775808" "-9223372036854775809" "18446744073709551615"; do
    check "time2date '$number'" refused "TIME_T refused, out of range: '$number'" "gmtime of" \
        "$TIME2DATE" -- "$number"
done

#
#   time2range
#
check "time2range days in a zone" 0 "Start: 2026-10-21T00:00:00+0200, End: 2026-10-22T00:00:00+0200" "" \
    "$TIME2RANGE" -t 1792573200 -p days -r 1 -z Europe/Madrid
check "time2range days back" 0 "Start: 2026-10-21T00:00:00+0000, End: 2026-10-20T00:00:00+0000" "" \
    "$TIME2RANGE" -t 1792573200 -p days -r -1
check "time2range -r 0" 0 "Start: 2026-10-21T00:00:00+0000, End: 2026-10-21T00:00:00+0000" "" \
    "$TIME2RANGE" -t 1792573200 -p days -r 0
check "time2range -r -0" 0 "Start: 2026-10-21T00:00:00+0000, End: 2026-10-21T00:00:00+0000" "" \
    "$TIME2RANGE" -t 1792573200 -p days -r -0
check "time2range the first day of the epoch, range 0" 0 "Start: 1970-01-01T00:00:00+0000, End: 1970-01-01T00:00:00+0000" "" \
    "$TIME2RANGE" -t 1 -p days -r 0
for range in " 5" "+5" "05" "5 " "5.0" "1e2" "-" "" "abc"; do
    check "time2range -r '$range'" refused "RANGE refused, it is no integer: '$range'" "Start:" \
        "$TIME2RANGE" -t 1792573200 -p days -r "$range"
done
for range in "2147483648" "-2147483649" "9223372036854775808"; do
    check "time2range -r '$range'" refused "RANGE refused, out of range: '$range'" "Start:" \
        "$TIME2RANGE" -t 1792573200 -p days -r "$range"
done
check "time2range an unknown zone" refused "Unknown time zone" "Start:" \
    "$TIME2RANGE" -t 1792573200 -p days -r 1 -z Nowhere/Zone
check_said_once "time2range an unknown zone" "Unknown time zone" \
    "$TIME2RANGE" -t 1792573200 -p days -r 1 -z Nowhere/Zone
check "time2range a time no calendar holds, range 0" refused "no calendar holds" "Start:" \
    "$TIME2RANGE" -t 67768036191676800 -p days -r 0
check "time2range an end no calendar holds" refused "No range" "Start:" \
    "$TIME2RANGE" -t 67768036191676799 -p days -r 1

echo
if [ $failures -eq 0 ]; then
    echo "test_time_cli: PASS"
    exit 0
fi
echo "test_time_cli: FAIL ($failures)"
exit 1
