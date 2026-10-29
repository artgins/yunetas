/****************************************************************************
 *          TEST_MBUS_TELEGRAM.C
 *
 *          The M-Bus telegram decoder of modules/c/mbus, without a yuno:
 *          frames in, frames or json out.
 *
 *          Two kinds of case:
 *          - SYNTHETIC: a record built by hand for one rule of EN 13757-3
 *            (a coding, a table, a VIFE), with the answer worked out from
 *            the standard, not from the decoder.
 *          - REAL: telegrams of real meters. Their answers were checked
 *            record by record against libmbus's decoding of the same
 *            telegrams (893 of 909 records equal over its 78 telegrams; the
 *            rest are differences on purpose: a BCD in error is null here,
 *            a "date of" VIFE is a date here, and the fixed data structure
 *            is not decoded).
 *
 *          The real telegrams come from test/test-frames of libmbus
 *          (github.com/rscada/libmbus), Copyright (c) 2010-2012, Raditex
 *          Control AB, under the BSD 3-Clause License, which is in
 *          LICENSE-libmbus-frames.txt next to this file.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>
#include <mbus_telegram.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_DATA_FOR_TEST   253     // one byte more than a frame carries

/***************************************************************************
 *              Helpers
 ***************************************************************************/
PRIVATE int failed = 0;

PRIVATE void fail(const char *title, const char *detail)
{
    printf("FAIL %-56s %s\n", title, detail?detail:"");
    failed++;
}

PRIVATE void pass(const char *title)
{
    printf("ok   %s\n", title);
}

/*
 *  "68 03 03 68 ..." to bytes
 */
PRIVATE size_t hex_to_bytes(const char *hex, uint8_t *bf, size_t size)
{
    size_t n = 0;
    const char *p = hex;
    while(*p && n < size) {
        while(*p == ' ') {
            p++;
        }
        if(!*p) {
            break;
        }
        unsigned v;
        if(sscanf(p, "%2x", &v) != 1) {
            break;
        }
        bf[n++] = (uint8_t)v;
        p += 2;
    }
    return n;
}

/*
 *  json_equal(), but a real equals a real within 1e-9 of it: a value
 *  converted from F to C, or from J to Wh, is not exact in binary.
 */
PRIVATE BOOL json_close(json_t *a, json_t *b)
{
    if(json_is_number(a) && json_is_number(b) && (json_is_real(a) || json_is_real(b))) {
        double x = json_number_value(a);
        double y = json_number_value(b);
        double d = x > y? x - y : y - x;
        double m = (x < 0? -x : x) > 1? (x < 0? -x : x) : 1;
        return json_typeof(a) == json_typeof(b) && d <= 1e-9 * m;
    }
    if(json_is_object(a) && json_is_object(b)) {
        if(json_object_size(a) != json_object_size(b)) {
            return FALSE;
        }
        const char *k; json_t *v;
        json_object_foreach(a, k, v) {
            json_t *w = json_object_get(b, k);
            if(!w || !json_close(v, w)) {
                return FALSE;
            }
        }
        return TRUE;
    }
    if(json_is_array(a) && json_is_array(b)) {
        if(json_array_size(a) != json_array_size(b)) {
            return FALSE;
        }
        for(size_t i=0; i<json_array_size(a); i++) {
            if(!json_close(json_array_get(a, i), json_array_get(b, i))) {
                return FALSE;
            }
        }
        return TRUE;
    }
    return json_equal(a, b);
}

/*
 *  Decode a long frame and compare with the expected json (NULL: the
 *  decoder must refuse it, and say why).
 */
PRIVATE void check_decode(
    const char *title,
    const uint8_t *bf,
    size_t len,
    BOOL expected_more,
    const char *expected
) {
    mbus_frame_t frame;
    char why[256] = "";
    int r = mbus_parse_frame(bf, len, &frame, why, sizeof(why));
    if(r != (int)len) {
        char detail[300];
        snprintf(detail, sizeof(detail), "parse answered %d for %d bytes: %s", r, (int)len, why);
        fail(title, detail);
        return;
    }

    BOOL more = !expected_more;
    json_t *jn = mbus_decode_user_data(&frame, &more, why, sizeof(why));

    if(!expected) {
        if(jn) {
            char *got = json_dumps(jn, JSON_COMPACT|JSON_PRESERVE_ORDER);
            fail(title, got);
            gbmem_free(got);
            JSON_DECREF(jn)
            return;
        }
        if(empty_string(why)) {
            fail(title, "refused with no reason");
            return;
        }
        printf("ok   %-56s refused: %s\n", title, why);
        return;
    }

    if(!jn) {
        fail(title, why);
        return;
    }
    json_t *want = json_loads(expected, 0, 0);
    if(!want) {
        fail(title, "the expected json does not parse");
        JSON_DECREF(jn)
        return;
    }
    if(!json_close(jn, want) || more != expected_more) {
        char *got = json_dumps(jn, JSON_COMPACT|JSON_PRESERVE_ORDER);
        char detail[8192];
        snprintf(detail, sizeof(detail), "\n     got  %s (more %d)\n     want %s (more %d)",
            got, more, expected, expected_more
        );
        fail(title, detail);
        gbmem_free(got);
    } else {
        pass(title);
    }
    JSON_DECREF(want)
    JSON_DECREF(jn)
}

/*
 *  The records of a variable data answer with no header (CI 78), in a
 *  long frame
 */
PRIVATE void check_records(const char *title, const char *records_hex, BOOL expected_more, const char *expected)
{
    uint8_t data[MBUS_FRAME_MAX_LENGTH];
    size_t n = hex_to_bytes(records_hex, data, sizeof(data));
    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    size_t len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_RESP_NO_HEADER, data, n);
    if(!len) {
        fail(title, "mbus_build_long_frame refused the records");
        return;
    }
    check_decode(title, bf, len, expected_more, expected);
}

PRIVATE void check_parse(const char *title, const char *hex, int expected_ret, mbus_frame_type_t expected_type)
{
    uint8_t bf[MBUS_FRAME_MAX_LENGTH + 16];
    size_t n = hex_to_bytes(hex, bf, sizeof(bf));
    mbus_frame_t frame;
    char why[256] = "";
    int r = mbus_parse_frame(bf, n, &frame, why, sizeof(why));
    if(r != expected_ret || (r > 0 && frame.type != expected_type)) {
        char detail[300];
        snprintf(detail, sizeof(detail), "answered %d type %d (want %d type %d) %s",
            r, (int)frame.type, expected_ret, (int)expected_type, why
        );
        fail(title, detail);
        return;
    }
    if(r < 0 && empty_string(why)) {
        fail(title, "refused with no reason");
        return;
    }
    if(r < 0) {
        printf("ok   %-56s refused: %s\n", title, why);
    } else {
        pass(title);
    }
}

PRIVATE void check_bytes(const char *title, const uint8_t *bf, size_t len, const char *expected_hex)
{
    uint8_t want[MBUS_FRAME_MAX_LENGTH];
    size_t n = hex_to_bytes(expected_hex, want, sizeof(want));
    if(n != len || memcmp(bf, want, n) != 0) {
        char got[3*MBUS_FRAME_MAX_LENGTH + 1] = "";
        for(size_t i=0; i<len && 3*i + 3 < sizeof(got); i++) {
            snprintf(got + 3*i, sizeof(got) - 3*i, "%02X ", bf[i]);
        }
        fail(title, got);
        return;
    }
    pass(title);
}

/***************************************************************************
 *  Link layer: finding the frame
 ***************************************************************************/
PRIVATE void test_frames(void)
{
    check_parse("ACK", "E5", 1, MBUS_FRAME_ACK);
    check_parse("short frame, REQ_UD2 to 1", "10 5B 01 5C 16", 5, MBUS_FRAME_SHORT);
    check_parse("short frame, bad checksum", "10 5B 01 5D 16", -1, 0);
    check_parse("short frame, bad stop byte", "10 5B 01 5C 17", -1, 0);
    check_parse("short frame, incomplete", "10 5B", 0, 0);
    check_parse("control frame, SND_UD broadcast", "68 03 03 68 53 FE 51 A2 16", 9, MBUS_FRAME_CONTROL);
    check_parse("long frame, the two lengths differ", "68 04 05", -1, 0);
    check_parse("long frame, length under 3", "68 02 02 68 08 01 16", -1, 0);
    check_parse("long frame, fourth byte not 68", "68 03 03 67 53 FE 51 A2 16", -1, 0);
    check_parse("long frame, incomplete", "68 05 05 68 08 01", 0, 0);
    check_parse("long frame, bad checksum", "68 03 03 68 53 FE 51 A3 16", -1, 0);
    check_parse("a frame, then more bytes: only the frame", "E5 10 5B", 1, MBUS_FRAME_ACK);
    check_parse("no frame starts with AA", "AA", -1, 0);
    check_parse("nothing yet", "", 0, 0);
}

/***************************************************************************
 *  Link layer: building the requests
 ***************************************************************************/
PRIVATE void test_builders(void)
{
    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    char why[256];

    size_t n = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_REQ_UD2, 1);
    check_bytes("REQ_UD2 to 1", bf, n, "10 5B 01 5C 16");

    n = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_REQ_UD2|MBUS_C_FCB, 250);
    check_bytes("REQ_UD2 with FCB to 250", bf, n, "10 7B FA 75 16");

    n = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_SND_NKE, MBUS_ADDRESS_SELECTED);
    check_bytes("SND_NKE to the selected slave", bf, n, "10 40 FD 3D 16");

    if(mbus_build_short_frame(bf, 4, MBUS_C_SND_NKE, 1) != 0) {
        fail("short frame into 4 bytes", "built");
    } else {
        pass("short frame into 4 bytes: refused");
    }

    uint8_t sec[MBUS_SECONDARY_ADDRESS_SIZE];
    if(mbus_build_secondary_address(sec, "12345678", "KAM", -1, -1, why, sizeof(why)) < 0) {
        fail("secondary address 12345678 KAM", why);
    } else {
        check_bytes("secondary address 12345678 KAM", sec, sizeof(sec), "78 56 34 12 2D 2C FF FF");
        n = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED,
            MBUS_CI_SELECT_SECONDARY, sec, sizeof(sec)
        );
        check_bytes("select 12345678 KAM", bf, n,
            "68 0B 0B 68 53 FD 52 78 56 34 12 2D 2C FF FF 0D 16"
        );
    }

    if(mbus_build_secondary_address(sec, "1234ffff", NULL, 1, 7, why, sizeof(why)) < 0) {
        fail("secondary address 1234FFFF, version 1, water", why);
    } else {
        check_bytes("secondary address 1234FFFF, version 1, water", sec, sizeof(sec),
            "FF FF 34 12 FF FF 01 07"
        );
    }

    struct {
        const char *title;
        const char *id;
        const char *man;
        int version;
        int medium;
    } bad[] = {
        {"secondary id of 7 digits", "1234567", "", -1, -1},
        {"secondary id with a letter", "1234567A", "", -1, -1},
        {"manufacturer with a digit", "12345678", "K1M", -1, -1},
        {"manufacturer of 2 letters", "12345678", "KA", -1, -1},
        {"version 256", "12345678", "", 256, -1},
        {"medium -2", "12345678", "", -1, -2},
    };
    for(size_t i=0; i<ARRAY_SIZE(bad); i++) {
        why[0] = 0;
        if(mbus_build_secondary_address(sec, bad[i].id, bad[i].man, bad[i].version, bad[i].medium,
                why, sizeof(why)) == 0) {
            fail(bad[i].title, "accepted");
        } else {
            printf("ok   %-56s refused: %s\n", bad[i].title, why);
        }
    }

    uint8_t big[MAX_DATA_FOR_TEST];
    memset(big, 0, sizeof(big));
    if(mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, 0x78, big, 253) != 0) {
        fail("long frame with 253 bytes of data", "built");
    } else {
        pass("long frame with 253 bytes of data: refused");
    }
    n = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, 0x78, big, 252);
    if(n != MBUS_FRAME_MAX_LENGTH) {
        fail("long frame with 252 bytes of data", "not 261 bytes");
    } else {
        pass("long frame with 252 bytes of data: 261 bytes");
    }

    if(strcmp(mbus_medium_name(7), "water") != 0 || mbus_medium_code("gas") != 3 ||
            mbus_medium_name(0x7E) != NULL || mbus_medium_code("lava") != -1) {
        fail("medium names", "");
    } else {
        pass("medium names");
    }
}

/***************************************************************************
 *  Records, one rule each. The bytes after CI 78; the answer from the
 *  standard.
 ***************************************************************************/
PRIVATE void test_records(void)
{
    check_records("int32, volume 10^-3 m3",
        "04 13 39 30 00 00", FALSE,
        "{\"records\":{\"volume\":{\"value\":12.345,\"unit\":\"m3\"}}}"
    );
    check_records("int8 negative, external temperature 10^-1 C",
        "01 66 F6", FALSE,
        "{\"records\":{\"external_temperature\":{\"value\":-1.0,\"unit\":\"C\"}}}"
    );
    check_records("int64, energy Wh",
        "07 03 00 00 00 00 01 00 00 00", FALSE,
        "{\"records\":{\"energy\":{\"value\":4294967296,\"unit\":\"Wh\"}}}"
    );
    check_records("BCD 4 digits, F in the MSD is the minus sign",
        "0A 5A 23 F1", FALSE,
        "{\"records\":{\"flow_temperature\":{\"value\":-12.3,\"unit\":\"C\"}}}"
    );
    check_records("BCD 8 digits, energy kWh",
        "0C 06 78 56 34 12", FALSE,
        "{\"records\":{\"energy\":{\"value\":12345678000,\"unit\":\"Wh\"}}}"
    );
    check_records("BCD with a hex digit: no value",
        "0A 13 2B 01", FALSE,
        "{\"records\":{\"volume\":{\"value\":null,\"unit\":\"m3\"}}}"
    );
    check_records("real 32, power W",
        "05 2B 00 00 48 42", FALSE,
        "{\"records\":{\"power\":{\"value\":50.0,\"unit\":\"W\"}}}"
    );
    check_records("real 32 +Inf: no value, the rest of the telegram decoded",
        "05 13 00 00 80 7F 04 06 01 00 00 00", FALSE,
        "{\"records\":{\"volume\":{\"value\":null,\"unit\":\"m3\"},"
        "\"energy\":{\"value\":1000,\"unit\":\"Wh\"}}}"
    );
    check_records("real 32 -Inf: no value",
        "05 2B 00 00 80 FF", FALSE,
        "{\"records\":{\"power\":{\"value\":null,\"unit\":\"W\"}}}"
    );
    check_records("energy in MJ comes out in Wh",
        "04 0E 10 0E 00 00", FALSE,
        "{\"records\":{\"energy\":{\"value\":1000000.0,\"unit\":\"Wh\"}}}"
    );
    check_records("volume flow 10^-1 m3/min comes out in m3/h",
        "02 46 0A 00", FALSE,
        "{\"records\":{\"volume_flow\":{\"value\":60.0,\"unit\":\"m3/h\"}}}"
    );
    check_records("on time in hours comes out in s",
        "02 22 18 00", FALSE,
        "{\"records\":{\"on_time\":{\"value\":86400,\"unit\":\"s\"}}}"
    );
    check_records("FB: energy 10^-1 MWh comes out in Wh",
        "04 FB 00 05 00 00 00", FALSE,
        "{\"records\":{\"energy\":{\"value\":500000,\"unit\":\"Wh\"}}}"
    );
    check_records("FB: flow temperature 10^-2 F comes out in C",
        "02 FB 59 D0 52", FALSE,
        "{\"records\":{\"flow_temperature\":{\"value\":100.0,\"unit\":\"C\"}}}"
    );
    check_records("FD: voltage 10^-1 V",
        "02 FD 48 FC 08", FALSE,
        "{\"records\":{\"voltage\":{\"value\":230.0,\"unit\":\"V\"}}}"
    );
    check_records("FD: error flags",
        "02 FD 17 04 00", FALSE,
        "{\"records\":{\"error_flags\":{\"value\":4}}}"
    );
    check_records("plain text VIF: the unit is the text, reversed",
        "01 7C 03 72 61 62 2A", FALSE,
        "{\"records\":{\"custom\":{\"value\":42,\"unit\":\"bar\"}}}"
    );
    check_records("LVAR text: firmware version",
        "0D FD 0E 03 33 2E 31", FALSE,
        "{\"records\":{\"firmware_version\":{\"value\":\"1.3\"}}}"
    );
    check_records("LVAR binary of 10 bytes: hex, most significant first",
        "0D 13 EA 01 02 03 04 05 06 07 08 09 0A", FALSE,
        "{\"records\":{\"volume\":{\"value\":\"0A090807060504030201\",\"unit\":\"m3\"}}}"
    );
    check_records("fabrication number, int32 or BCD, is text",
        "04 78 2E 25 4C 00 0C 78 17 58 85 06", FALSE,
        "{\"records\":{\"fabrication_number\":{\"value\":\"4990254\"},"
        "\"fabrication_number#2\":{\"value\":\"06855817\"}}}"
    );
    check_records("DIFE: storage 5, tariff 1, subunit 1",
        "C4 52 13 01 00 00 00", FALSE,
        "{\"records\":{\"volume@s5@t1@u1\":{\"value\":0.001,\"unit\":\"m3\"}}}"
    );
    check_records("function: max, min, value during error",
        "12 2B 01 00 22 2B 02 00 32 2B 03 00", FALSE,
        "{\"records\":{\"power:max\":{\"value\":1,\"unit\":\"W\"},"
        "\"power:min\":{\"value\":2,\"unit\":\"W\"},"
        "\"power:error\":{\"value\":3,\"unit\":\"W\"}}}"
    );
    check_records("VIFE record error: no data available",
        "04 93 15 00 00 00 00", FALSE,
        "{\"records\":{\"volume\":{\"value\":null,\"unit\":\"m3\",\"error\":\"no data available\"}}}"
    );
    check_records("VIFE 3B and 3C: positive and negative accumulation",
        "04 93 3B 0A 00 00 00 04 93 3C 14 00 00 00", FALSE,
        "{\"records\":{\"volume_pos\":{\"value\":0.01,\"unit\":\"m3\"},"
        "\"volume_neg\":{\"value\":0.02,\"unit\":\"m3\"}}}"
    );
    check_records("VIFE 75: multiplicative correction 10^-1",
        "04 93 75 D2 04 00 00", FALSE,
        "{\"records\":{\"volume\":{\"value\":0.1234,\"unit\":\"m3\"}}}"
    );
    check_records("VIFE not interpreted: kept in the id, unit dropped",
        "04 93 22 05 00 00 00", FALSE,
        "{\"records\":{\"volume~22\":{\"value\":0.005}}}"
    );
    check_records("VIFE 6F: the date of the maximum",
        "94 10 DA 6F 32 14 7A 18", FALSE,
        "{\"records\":{\"flow_temperature:max@t1~6f\":{\"value\":\"2011-08-26T20:50\"}}}"
    );
    check_records("type G date",
        "02 6C 57 3A", FALSE,
        "{\"records\":{\"date\":{\"value\":\"2026-10-23\"}}}"
    );
    check_records("type F date and time",
        "04 6D 05 0E 57 3A", FALSE,
        "{\"records\":{\"datetime\":{\"value\":\"2026-10-23T14:05\"}}}"
    );
    check_records("type F marked invalid: no value",
        "04 6D 85 0E 57 3A", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("type G never set: no value",
        "02 6C 00 00", FALSE,
        "{\"records\":{\"date\":{\"value\":null}}}"
    );
    check_records("type G with year 127 (0-99): no value",
        "02 6C E1 F1", FALSE,
        "{\"records\":{\"date\":{\"value\":null}}}"
    );
    check_records("type F with hour 31 and minute 63: no value",
        "04 6D 3F 1F 21 01", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("type F with hour 24: no value",
        "04 6D 00 18 57 3A", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("type F with year 100 (0-99): no value",
        "04 6D 00 00 81 C1", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("type I date and time with seconds",
        "06 6D 1E 05 0E 57 3A 00", FALSE,
        "{\"records\":{\"datetime\":{\"value\":\"2026-10-23T14:05:30\"}}}"
    );
    check_records("type I with second 60: no value",
        "06 6D 3C 05 0E 57 3A 00", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("type J time",
        "03 6D 1E 05 0E", FALSE,
        "{\"records\":{\"datetime\":{\"value\":\"14:05:30\"}}}"
    );
    check_records("type J with minute 60: no value",
        "03 6D 00 3C 0E", FALSE,
        "{\"records\":{\"datetime\":{\"value\":null}}}"
    );
    check_records("FD 25: storage interval in minutes comes out in s",
        "01 FD 25 0F", FALSE,
        "{\"records\":{\"storage_interval\":{\"value\":900,\"unit\":\"s\"}}}"
    );
    check_records("FD 28: storage interval in months is another quantity",
        "01 FD 28 03", FALSE,
        "{\"records\":{\"storage_interval_months\":{\"value\":3,\"unit\":\"month\"}}}"
    );
    check_records("FD 29: storage interval in years comes out in months",
        "01 FD 29 02", FALSE,
        "{\"records\":{\"storage_interval_months\":{\"value\":24,\"unit\":\"month\"}}}"
    );
    check_records("FD 39: tariff period in years comes out in months",
        "01 FD 39 01", FALSE,
        "{\"records\":{\"tariff_period_months\":{\"value\":12,\"unit\":\"month\"}}}"
    );
    check_records("FD 6A: since last cumulation in months",
        "01 FD 6A 05", FALSE,
        "{\"records\":{\"since_last_cumulation_months\":{\"value\":5,\"unit\":\"month\"}}}"
    );
    check_records("FD 6D: battery operating time in days comes out in s",
        "01 FD 6D 02", FALSE,
        "{\"records\":{\"battery_operating_time\":{\"value\":172800,\"unit\":\"s\"}}}"
    );
    check_records("FD 6F: battery operating time in years comes out in months",
        "01 FD 6F 03", FALSE,
        "{\"records\":{\"battery_operating_time_months\":{\"value\":36,\"unit\":\"month\"}}}"
    );
    check_records("the same id twice: the second is #2",
        "01 2B 01 01 2B 02", FALSE,
        "{\"records\":{\"power\":{\"value\":1,\"unit\":\"W\"},\"power#2\":{\"value\":2,\"unit\":\"W\"}}}"
    );
    check_records("idle fillers are skipped",
        "2F 2F 01 2B 07 2F", FALSE,
        "{\"records\":{\"power\":{\"value\":7,\"unit\":\"W\"}}}"
    );
    check_records("DIF 0F: manufacturer data up to the end",
        "01 2B 07 0F AB CD", FALSE,
        "{\"records\":{\"power\":{\"value\":7,\"unit\":\"W\"}},\"mfr_data\":\"ABCD\"}"
    );
    check_records("DIF 1F: more records follow",
        "01 2B 07 1F", TRUE,
        "{\"records\":{\"power\":{\"value\":7,\"unit\":\"W\"}}}"
    );
    check_records("no data (DIF 00)",
        "00 13", FALSE,
        "{\"records\":{\"volume\":{\"value\":null,\"unit\":\"m3\"}}}"
    );
}

/***************************************************************************
 *  Headers, and what the decoder must refuse
 ***************************************************************************/
PRIVATE void test_headers_and_refusals(void)
{
    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    uint8_t data[MBUS_FRAME_MAX_LENGTH];
    size_t n, len;

    n = hex_to_bytes("0A 30 00 00 01 2B 07", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_RESP_SHORT_HEADER, data, n);
    check_decode("CI 7A: short header", bf, len, FALSE,
        "{\"header\":{\"access_no\":10,\"status\":48},"
        "\"records\":{\"power\":{\"value\":7,\"unit\":\"W\"}}}"
    );

    n = hex_to_bytes("17 58 85 06 2D 2C 08 04 04 00 00 05 01 2B 07", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_RESP_VARIABLE, data, n);
    check_decode("CI 72 with security mode 5: refused", bf, len, FALSE, NULL);

    n = hex_to_bytes("17 58 85 06 2D 2C 08 04 04 00 00 1F 01 2B 07", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_RESP_VARIABLE, data, n);
    check_decode("CI 72 with signature 1F00 (not a mode): decoded", bf, len, FALSE,
        "{\"header\":{\"id\":\"06855817\",\"manufacturer\":\"KAM\",\"version\":8,"
        "\"medium\":\"heat\",\"medium_code\":4,\"access_no\":4,\"status\":0},"
        "\"records\":{\"power\":{\"value\":7,\"unit\":\"W\"}}}"
    );

    n = hex_to_bytes("17 58 85 06 2D 2C", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_RESP_VARIABLE, data, n);
    check_decode("CI 72 shorter than its header", bf, len, FALSE, NULL);

    n = hex_to_bytes("12", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, MBUS_CI_APPLICATION_ERROR, data, n);
    check_decode("CI 70: application error", bf, len, FALSE, "{\"application_error\":18}");

    n = hex_to_bytes("00 00", data, sizeof(data));
    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, 1, 0x73, data, n);
    check_decode("CI 73, fixed data: refused", bf, len, FALSE, NULL);

    check_records("reserved DIF 3F: refused", "3F 13 00", FALSE, NULL);
    check_records("record shorter than its data: refused", "04 13 01 00", FALSE, NULL);
    check_records("telegram ends before the VIF: refused", "04", FALSE, NULL);
    check_records("telegram ends inside the VIFEs: refused", "04 93", FALSE, NULL);
    check_records("eleven DIFEs: refused", "84 80 80 80 80 80 80 80 80 80 80 00 13 00 00 00 00", FALSE, NULL);
    check_records("plain text VIF longer than the telegram: refused", "01 7C 09 61 62", FALSE, NULL);
    check_records("reserved LVAR FB: refused", "0D 13 FB 00", FALSE, NULL);

    uint8_t ack[1] = {0xE5};
    mbus_frame_t frame;
    char why[256] = "";
    mbus_parse_frame(ack, 1, &frame, why, sizeof(why));
    BOOL more;
    json_t *jn = mbus_decode_user_data(&frame, &more, why, sizeof(why));
    if(jn) {
        fail("user data of an ACK", "decoded");
        JSON_DECREF(jn)
    } else {
        printf("ok   %-56s refused: %s\n", "user data of an ACK", why);
    }
}

/***************************************************************************
 *  Real meters
 ***************************************************************************/
PRIVATE void test_real_meters(void)
{
    struct {
        const char *title;
        const char *hex;
        BOOL more;
        const char *expected;
    } cases[] = {
#include "real_telegrams.inc"
    };

    for(size_t i=0; i<ARRAY_SIZE(cases); i++) {
        uint8_t bf[MBUS_FRAME_MAX_LENGTH];
        size_t n = hex_to_bytes(cases[i].hex, bf, sizeof(bf));
        check_decode(cases[i].title, bf, n, cases[i].more, cases[i].expected);
    }
}

/***************************************************************************
 *  Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    test_frames();
    test_builders();
    test_records();
    test_headers_and_refusals();
    test_real_meters();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free\n");
        failed++;
    }

    printf("\n%s\n\n", failed? "FAILED": "all passed");
    return failed? -1: 0;
}
