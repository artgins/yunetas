#!/bin/bash
#
#   Test of packages/templates/yuneta-label-print: the bytes it sends to a
#   Brother QL, compared with the bytes built here by hand from Brother's
#   Raster Command Reference, and its answers to the statuses of a printer
#   (fake ones, given in YUNETA_LABEL_PRINT_FAKE_STATUS). No printer is
#   needed. Runs as a normal user.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
TOOL="${HERE}/../templates/yuneta-label-print"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT
export PYTHONDONTWRITEBYTECODE=1
unset YUNETA_LABEL_PRINT_FAKE_STATUS
export YUNETA_LABEL_PRINT_SYSFS="${WORK}/sysfs"
mkdir -p "$YUNETA_LABEL_PRINT_SYSFS"

ERR="${WORK}/err.txt"
OUT="${WORK}/out.txt"
GOT="${WORK}/got.bin"
FAILED=0

#
#   Helpers
#
ok() {
    echo "ok   - $1"
}

fail() {
    echo "FAIL - $1"
    FAILED=1
}

hex() {
    od -An -v -tx1 "$1" | tr -d ' \n'
}

# run <expected exit> <what> <args...>: stdout in OUT, stderr in ERR
run() {
    local expected="$1" what="$2" rc
    shift 2
    rm -f "$GOT"
    "$TOOL" "$@" > "$OUT" 2> "$ERR"
    rc=$?
    if [ "$rc" = "$expected" ]; then
        ok "$what: exit $rc"
    else
        fail "$what: exit $rc, expected $expected ($(head -c 300 "$ERR"))"
    fi
}

check_bytes() {
    local what="$1" expected="$2" got
    got="$(hex "$GOT" 2>/dev/null)"
    if [ -n "$expected" ] && [ "$got" = "$expected" ]; then
        ok "$what"
    else
        fail "$what: the bytes differ"
        echo "       got:      ${got:0:240}..."
        echo "       expected: ${expected:0:240}..."
    fi
}

check_err() {
    local what="$1" pattern="$2"
    if grep -q -- "$pattern" "$ERR"; then
        ok "$what"
    else
        fail "$what: stderr has no '$pattern' ($(head -c 300 "$ERR"))"
    fi
}

check_out() {
    local what="$1" pattern="$2"
    if grep -q -- "$pattern" "$OUT"; then
        ok "$what"
    else
        fail "$what: stdout has no '$pattern' ($(head -c 300 "$OUT"))"
    fi
}

# repeat <n> <hex byte>
repeat() {
    local i out=""
    for ((i = 0; i < $1; i++)); do
        out+="$2"
    done
    printf '%s' "$out"
}

#
#   PNGs, written by a small encoder here (python3, as the tool): an image
#   is a list of rows of 0 (white) and 1 (black) dots, and every PNG of one
#   image must give the same bytes to the printer
#
make_png() {
    python3 - "$@" <<'PYEOF'
import struct, sys, zlib
path, width, pattern, color, filt = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
rows = []
for spec in pattern.split(","):
    row = [0] * width
    if spec == "black":
        row = [1] * width
    elif spec == "first":
        row[0] = 1
    elif spec == "last":
        row[width - 1] = 1
    rows.append(row)
channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
def pixel(dot, x):
    if color == 0:
        return [0 if dot else 255]
    if color == 2:
        return [0, 0, 0] if dot else [255, 255, 255]
    if color == 3:
        return [1 if dot else 0]
    if color == 4:
        return [0, 255] if dot else [0, 0]          # white = transparent
    return [0, 0, 0, 255] if dot else [0, 0, 0, 0]  # white = transparent
raw = b""
prev = bytes(width * channels)
for row in rows:
    line = bytes(v for x, d in enumerate(row) for v in pixel(d, x))
    bpp = channels
    out = bytearray(len(line))
    for i in range(len(line)):
        a = line[i - bpp] if i >= bpp else 0
        b = prev[i]
        c = prev[i - bpp] if i >= bpp else 0
        if filt == 0:
            pred = 0
        elif filt == 1:
            pred = a
        elif filt == 2:
            pred = b
        elif filt == 3:
            pred = (a + b) >> 1
        else:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
        out[i] = (line[i] - pred) & 0xFF
    raw += bytes([filt]) + bytes(out)
    prev = line
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, len(rows), 8, color, 0, 0, 0))
if color == 3:
    png += chunk(b"PLTE", bytes([255, 255, 255, 0, 0, 0]))
png += chunk(b"IDAT", zlib.compress(raw))
png += chunk(b"IEND", b"")
open(path, "wb").write(png)
PYEOF
}

# A status of 32 bytes (offsets of the reference): 0-3 80 20 42 34, 4 the
# model, 5-7 30 00 00, 8-9 the errors, 10 the width, 11 the media type,
# 12-16 00, 17 the length, 18 the status type, 19-21 the phase, 22 the
# notification, 23-31 00.
# status <model> <err1> <err2> <width> <type> <length> <status type> <notification>
status() {
    printf '80204234%s300000%s%s%s%s0000000000%s%s000000%s' "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8"
    repeat 9 00
}

#
#   What the reference says the printer must receive, for the image
#   "white, black, first dot, last dot" 696 dots wide on continuous 62 mm:
#
#     - 200 x 00, then ESC @ (1b 40)
#     - ESC i z: valid flags ce, type 0a, width 3e (62), length 00,
#       4 lines (04 00 00 00), starting page 00, 00
#     - ESC i M 40, ESC i A 01, ESC i K 08, ESC i d 23 00 (35 dots)
#     - per line 67 00 5a and 90 bytes. The 696 dots are pins 12..707 of
#       the 720, and the line is mirrored: the sent line starts at the
#       right side of the label. So the FIRST dot of the image is sent as
#       pin 707: byte 88, MSB-first bit 3 = 10. The LAST dot is sent as
#       pin 12: byte 1, bit 4 = 08. A black line is byte 1 = 0f (pins
#       12-15), bytes 2..87 = ff, byte 88 = f0 (pins 704-707).
#     - 1a
#
PREAMBLE="$(repeat 200 00)1b40"
JOB_HEAD_4="1b697ace0a3e0004000000""0000""1b694d40""1b694101""1b694b08""1b69642300"
LINE_WHITE="67005a$(repeat 90 00)"
LINE_BLACK="67005a000f$(repeat 86 ff)f000"
LINE_FIRST="67005a$(repeat 88 00)1000"
LINE_LAST="67005a0008$(repeat 88 00)"
LINES_4="${LINE_WHITE}${LINE_BLACK}${LINE_FIRST}${LINE_LAST}"
EXPECTED_4="${PREAMBLE}${JOB_HEAD_4}${LINES_4}1a"

#
#   1. The bytes of a 696-dot image, from every PNG color type and filter
#
for color in 0 2 3 4 6; do
    for filt in 0 1 2 3 4; do
        make_png "${WORK}/img.png" 696 white,black,first,last "$color" "$filt"
        run 0 "color type $color, filter $filt" --printer "file:${GOT}" "${WORK}/img.png"
        check_bytes "color type $color, filter $filt: the bytes of the reference" "$EXPECTED_4"
    done
done

#
#   2. A narrower image is centered: 8 black dots on pins 356..363, the
#      same pins mirrored, so bytes 44 (bits 4-7 = 0f) and 45 (f0)
#
make_png "${WORK}/narrow.png" 8 black 0 0
run 0 "8 dots wide" --printer "file:${GOT}" "${WORK}/narrow.png"
check_bytes "8 dots wide: centered" \
    "${PREAMBLE}1b697ace0a3e0001000000""0000""1b694d40""1b694101""1b694b08""1b69642300""67005a$(repeat 44 00)0ff0$(repeat 44 00)1a"

#
#   3. Die-cut 62x29: no feed, 271 lines at most
#
make_png "${WORK}/271.png" 696 "$(repeat 270 white,)white" 0 0
run 0 "die-cut 62x29, 271 lines" --printer "file:${GOT}" --media die-cut-62x29 "${WORK}/271.png"
check_bytes "die-cut 62x29: type 0b, length 1d, 271 lines, no feed" \
    "${PREAMBLE}1b697ace0b3e1d0f010000""0000""1b694d40""1b694101""1b694b08""1b69640000$(repeat 271 "$LINE_WHITE")1a"
make_png "${WORK}/272.png" 696 "$(repeat 271 white,)white" 0 0
run 1 "die-cut 62x29, 272 lines" --printer "file:${GOT}" --media die-cut-62x29 "${WORK}/272.png"
check_err "die-cut 62x29, 272 lines: refused" "die-cut label 271"

#
#   4. Images it refuses
#
make_png "${WORK}/wide.png" 697 white 0 0
run 1 "697 dots wide" --printer "file:${GOT}" "${WORK}/wide.png"
check_err "697 dots wide: refused" "prints 696"
printf 'not a png' > "${WORK}/bad.png"
run 1 "not a PNG" --printer "file:${GOT}" "${WORK}/bad.png"
check_err "not a PNG: refused" "is not a PNG"
python3 - "${WORK}/16.png" <<'PYEOF'
import struct, sys, zlib
def chunk(k, d):
    return struct.pack(">I", len(d)) + k + d + struct.pack(">I", zlib.crc32(k + d))
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 16, 0, 0, 0, 0))
    + chunk(b"IDAT", zlib.compress(b"\x00\xff\xff")) + chunk(b"IEND", b""))
PYEOF
run 1 "a 16-bit PNG" --printer "file:${GOT}" "${WORK}/16.png"
check_err "a 16-bit PNG: refused" "only 8-bit PNGs"

#
#   5. With a printer: the status first, then the job, then "completed"
#
make_png "${WORK}/img.png" 696 white,black,first,last 0 0
JOB_4="${JOB_HEAD_4}${LINES_4}1a"
QL700_62="$(status 35 00 00 3e 0a 00 00 00)"
COMPLETED="$(status 35 00 00 3e 0a 00 01 00)"
PHASE="$(status 35 00 00 3e 0a 00 06 00)"

export YUNETA_LABEL_PRINT_FAKE_STATUS="${QL700_62},${PHASE},${COMPLETED}"
run 0 "QL-700, continuous 62 mm" --printer "file:${GOT}" "${WORK}/img.png"
check_bytes "QL-700: status request, then the job" "${PREAMBLE}1b6953${JOB_4}"

export YUNETA_LABEL_PRINT_FAKE_STATUS="${QL700_62}"
run 1 "no 'printing completed'" --printer "file:${GOT}" "${WORK}/img.png"
check_err "no 'printing completed': said" "no 'printing completed'"

export YUNETA_LABEL_PRINT_FAKE_STATUS="${QL700_62},$(status 35 00 10 3e 0a 00 02 00)"
run 1 "cover opened while printing" --printer "file:${GOT}" "${WORK}/img.png"
check_err "cover opened while printing: said" "cover opened while printing"

export YUNETA_LABEL_PRINT_FAKE_STATUS="$(status 35 01 00 00 00 00 00 00)"
run 1 "no media" --printer "file:${GOT}" "${WORK}/img.png"
check_err "no media: refused" "no media"
check_bytes "no media: no job sent" "${PREAMBLE}1b6953"

export YUNETA_LABEL_PRINT_FAKE_STATUS="$(status 35 00 00 1d 0a 00 00 00)"
run 1 "29 mm tape" --printer "file:${GOT}" "${WORK}/img.png"
check_err "29 mm tape: refused" "continuous 29 mm: load a 62 mm roll"

export YUNETA_LABEL_PRINT_FAKE_STATUS="$(status 50 00 00 3e 0a 00 00 00)"
run 1 "a QL-1050 (162 bytes a line)" --printer "file:${GOT}" "${WORK}/img.png"
check_err "a QL-1050: refused" "model code 0x50"

export YUNETA_LABEL_PRINT_FAKE_STATUS="$(status 35 00 00 3e 0b 1d 00 00),${COMPLETED}"
run 0 "die-cut 62x29 in the printer" --printer "file:${GOT}" "${WORK}/img.png"
check_bytes "die-cut 62x29 in the printer: its job" \
    "${PREAMBLE}1b6953""1b697ace0b3e1d04000000""0000""1b694d40""1b694101""1b694b08""1b69640000${LINES_4}1a"

export YUNETA_LABEL_PRINT_FAKE_STATUS="0102"
run 1 "a reply that is not a status" --printer "file:${GOT}" "${WORK}/img.png"
check_err "a reply that is not a status: refused" "sent 2 bytes of a status"

export YUNETA_LABEL_PRINT_FAKE_STATUS="${QL700_62}"
run 0 "--status" --printer "file:${GOT}" --status
check_out "--status: the model" "model:  QL-700"
check_out "--status: the media" "media:  continuous 62 mm"
check_out "--status: the errors" "errors: none"
unset YUNETA_LABEL_PRINT_FAKE_STATUS

#
#   6. auto: the printer on the USB bus (a fake sysfs)
#
usb_dev() {
    local name="$1" product="$2" busnum="$3" devnum="$4" d
    d="${YUNETA_LABEL_PRINT_SYSFS}/${name}"
    mkdir -p "$d" "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_81" "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_02"
    echo 04f9 > "$d/idVendor"
    echo "$product" > "$d/idProduct"
    echo "$busnum" > "$d/busnum"
    echo "$devnum" > "$d/devnum"
    echo Bulk > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_81/type"
    echo in > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_81/direction"
    echo 81 > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_81/bEndpointAddress"
    echo Bulk > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_02/type"
    echo out > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_02/direction"
    echo 02 > "${YUNETA_LABEL_PRINT_SYSFS}/${name}:1.0/ep_02/bEndpointAddress"
}

run 1 "auto, no printer" "${WORK}/img.png"
check_err "auto, no printer: said" "no Brother printer on the USB bus"

usb_dev 1-1 2049 1 4
run 1 "auto, a QL-700 in Editor Lite mode" "${WORK}/img.png"
check_err "auto, Editor Lite: what to do" "hold its Editor Lite"

usb_dev 1-2 2042 1 7
if [ -e /dev/bus/usb/001/007 ] && [ -w /dev/bus/usb/001/007 ]; then
    echo "skip - auto, a QL-700: /dev/bus/usb/001/007 is writable here"
else
    run 1 "auto, a QL-700 (usbfs)" "${WORK}/img.png"
    check_err "auto, a QL-700: its usbfs device" "/dev/bus/usb/001/007"
fi

usb_dev 2-1 2042 2 3
run 1 "auto, two printers" "${WORK}/img.png"
check_err "auto, two printers: refused" "2 Brother printers"
rm -rf "${YUNETA_LABEL_PRINT_SYSFS:?}"/*

#
#   7. Usage
#
run 2 "no arguments"
run 2 "an image and --status" --status "${WORK}/img.png"
run 2 "an unknown media" --media die-cut-29x90 "${WORK}/img.png"
run 1 "an unknown printer" --printer lpt1 "${WORK}/img.png"
check_err "an unknown printer: said" "unknown printer"

echo
if [ "$FAILED" = "0" ]; then
    echo "test-yuneta-label-print: all passed"
    exit 0
fi
echo "test-yuneta-label-print: FAILED"
exit 1
