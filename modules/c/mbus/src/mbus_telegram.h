/****************************************************************************
 *          MBUS_TELEGRAM.H
 *
 *          M-Bus telegrams (EN 13757-2 link layer, EN 13757-3 application
 *          layer): the frames, and the variable data structure a meter
 *          answers with, decoded to json.
 *
 *          No gobj and no state: a buffer in, a frame or a json out. The
 *          master gclass (C_PROT_MBUS_M) is built on top of it, and a test
 *          feeds it captured telegrams one by one.
 *
 *          NOTHING HERE LOGS. A function that fails returns -1 or NULL and
 *          writes the reason into `why`; the caller logs it, because only
 *          the caller knows which peer sent the bytes and how bad that is
 *          (a meter that answers garbage is a warning, not an error).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <stdint.h>
#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  Link layer: control field (C) of a master request
 */
#define MBUS_C_SND_NKE      0x40    // link reset, answered with E5
#define MBUS_C_SND_UD       0x53    // send user data (FCB 0x20 may be added)
#define MBUS_C_REQ_UD2      0x5B    // request class 2 data (FCB 0x20 may be added)
#define MBUS_C_FCB          0x20    // frame count bit, toggled by the master
#define MBUS_C_RSP_UD       0x08    // a slave's answer (ACD 0x20, DFC 0x10 may be set)

/*
 *  Link layer: address field (A)
 */
#define MBUS_ADDRESS_MIN            1
#define MBUS_ADDRESS_MAX            250
#define MBUS_ADDRESS_SELECTED       0xFD    // the slave selected by its secondary address
#define MBUS_ADDRESS_BROADCAST      0xFE    // broadcast, slaves answer (one slave only on the bus)
#define MBUS_ADDRESS_BROADCAST_NR   0xFF    // broadcast, no answer

/*
 *  Application layer: control information field (CI)
 */
#define MBUS_CI_DATA_SEND           0x51    // master to slave: data records
#define MBUS_CI_SELECT_SECONDARY    0x52    // master to slave: select by secondary address
#define MBUS_CI_APPLICATION_ERROR   0x70    // slave to master: general application error
#define MBUS_CI_ALARM_STATUS        0x71    // slave to master: alarm status
#define MBUS_CI_RESP_VARIABLE       0x72    // slave to master: variable data, 12-byte header
#define MBUS_CI_RESP_NO_HEADER      0x78    // slave to master: variable data, no header
#define MBUS_CI_RESP_SHORT_HEADER   0x7A    // slave to master: variable data, 4-byte header

#define MBUS_FRAME_MAX_LENGTH       261     // 68 L L 68 + L (255 at most) + CS 16
#define MBUS_SECONDARY_ADDRESS_SIZE 8       // id (4 BCD), manufacturer (2), version, medium

/***************************************************************
 *              Structures
 ***************************************************************/
typedef enum {
    MBUS_FRAME_ACK = 1,         // E5
    MBUS_FRAME_SHORT,           // 10 C A CS 16
    MBUS_FRAME_CONTROL,         // 68 03 03 68 C A CI CS 16
    MBUS_FRAME_LONG,            // 68 L L 68 C A CI data CS 16
} mbus_frame_type_t;

typedef struct {
    mbus_frame_type_t type;
    uint8_t c;
    uint8_t a;
    uint8_t ci;
    const uint8_t *data;        // points into the parsed buffer: the bytes after CI
    size_t data_len;
} mbus_frame_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Find the frame that starts at bf[0].
 *  Return its length (> 0) and fill `frame`, 0 when bf holds the start of
 *  a frame and more bytes are needed, or -1 when bf does not start with a
 *  valid frame (bad start, lengths, checksum or stop byte), with the
 *  reason in `why`. A caller that gets -1 drops what it holds: the link
 *  layer has no way to resynchronise inside a broken frame.
 */
PUBLIC int mbus_parse_frame(
    const uint8_t *bf,
    size_t len,
    mbus_frame_t *frame,
    char *why,
    size_t why_size
);

/*
 *  Build a short frame (10 C A CS 16) into bf.
 *  Return its length (5), or 0 when bf is smaller than that.
 */
PUBLIC size_t mbus_build_short_frame(uint8_t *bf, size_t size, uint8_t c, uint8_t a);

/*
 *  Build a long frame (68 L L 68 C A CI data CS 16) into bf.
 *  Return its length, or 0 when data_len is more than a frame carries
 *  (252 bytes) or bf is too small.
 */
PUBLIC size_t mbus_build_long_frame(
    uint8_t *bf,
    size_t size,
    uint8_t c,
    uint8_t a,
    uint8_t ci,
    const uint8_t *data,
    size_t data_len
);

/*
 *  Encode a secondary address, the 8 bytes of a selection (CI 52).
 *      id              8 digits; an 'F' in a digit is a wildcard ("1234FFFF")
 *      manufacturer    3 letters ("KAM"), or NULL/"" for any
 *      version         0-255, or -1 for any
 *      medium          0-255, or -1 for any
 *  Return 0, or -1 with the reason in `why`.
 */
PUBLIC int mbus_build_secondary_address(
    uint8_t out[MBUS_SECONDARY_ADDRESS_SIZE],
    const char *id,
    const char *manufacturer,
    int version,
    int medium,
    char *why,
    size_t why_size
);

/*
 *  Decode the user data of a slave's long frame (RSP_UD).
 *  CI 72, 7A and 78 (variable data) are decoded to
 *
 *      {
 *          "header": {                     // CI 72; CI 7A carries the last
 *              "id": "12345678",           // four fields only, CI 78 none
 *              "manufacturer": "KAM",
 *              "version": 8,
 *              "medium": "heat",
 *              "medium_code": 4,
 *              "access_no": 4,
 *              "status": 0
 *          },
 *          "records": {
 *              "energy":           {"value": 37351000, "unit": "Wh"},
 *              "volume@s1":        {"value": 560.5, "unit": "m3"},
 *              "flow_temperature": {"value": 101.69, "unit": "C"}
 *          },
 *          "mfr_data": "0001..."           // after DIF 0F/1F, in hex
 *      }
 *
 *  and CI 70 (application error) to {"application_error": <code>}.
 *  `*more_follows` is set when the telegram ends with DIF 1F: the slave
 *  has more records, to be asked for with the FCB toggled.
 *
 *  The id of a record says what it is, and is the same reading after
 *  reading:
 *      <quantity>[:max|:min|:error][@s<n>][@t<n>][@u<n>][~<vife>...][#<n>]
 *  :max/:min/:error is the function (instantaneous has none), @s the
 *  storage number, @t the tariff, @u the subunit (none when 0), ~ a VIFE
 *  this decoder does not interpret, in hex, and #<n> the n-th record of
 *  the telegram with the same id.
 *
 *  Values are scaled and given in ONE unit per quantity: energy in Wh,
 *  power in W, volume in m3, volume flow in m3/h, mass in kg, durations
 *  in s, temperatures in C (a difference in K), pressure in bar. So a
 *  meter that counts in GJ, MWh or J/h comes out in Wh or W, and a
 *  consumer that reads "energy" never has to ask which unit it is in.
 *  A duration counted in months or years has no fixed number of seconds:
 *  it is another quantity, "<quantity>_months", always in months
 *  ("storage_interval" in s, "storage_interval_months" in month). The one
 *  exception is "custom", a plain text VIF, whose unit is the meter's text.
 *
 *  A value that is not one has "value": null: a NaN or +-Inf real, a BCD
 *  with a digit in error, a date marked invalid or out of range (an hour
 *  25, a year 127), a record with a record-error VIFE.
 *
 *  Return the json (yours to decref), or NULL with the reason in `why`.
 */
PUBLIC json_t *mbus_decode_user_data(
    const mbus_frame_t *frame,
    BOOL *more_follows,
    char *why,
    size_t why_size
);

/*
 *  The name of a medium code ("water", "gas", "heat"...), or NULL when
 *  the code has none.
 */
PUBLIC const char *mbus_medium_name(int medium);

/*
 *  The code of a medium name, or -1.
 */
PUBLIC int mbus_medium_code(const char *name);

#ifdef __cplusplus
}
#endif
