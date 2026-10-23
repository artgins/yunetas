/****************************************************************************
 *          MBUS_TELEGRAM.C
 *
 *          M-Bus telegrams: frames (EN 13757-2) and the variable data
 *          structure (EN 13757-3), decoded to json. See mbus_telegram.h.
 *
 *          A record of the variable data structure is
 *
 *              DIF [DIFE...] VIF [VIFE...] data
 *
 *          DIF: bits 0-3 the coding and length of the data, bits 4-5 the
 *          function (instantaneous, max, min, during error), bit 6 the
 *          lowest bit of the storage number, bit 7 "a DIFE follows". Each
 *          DIFE adds 4 bits of storage number, 2 of tariff and 1 of
 *          subunit.
 *
 *          VIF: the quantity, its unit and its decimal exponent (0x13 is a
 *          volume in 10^-3 m3). 0xFB and 0xFD name a second and a third
 *          table in the byte that follows; 0x7C/0xFC give the unit as text;
 *          0x7F/0xFF are the manufacturer's. Bit 7 "a VIFE follows". The
 *          VIFEs that follow qualify the record (an error code, a
 *          correction factor, "only negative contributions"...).
 *
 *          Where the texts disagree, this follows the decoding that has
 *          been checked against real meters: libmbus (rscada) and its
 *          collection of telegrams. One case matters: an F in the most
 *          significant digit of a BCD is the minus sign (F321 is -321), as
 *          in EN 13757-3, not the 16's complement the 1998 m-bus.com text
 *          describes (-679). And a plain text VIF is read as libmbus reads
 *          it: the length and the text right after the VIF, the VIFEs
 *          after the text.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "mbus_telegram.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define FRAME_ACK_START     0xE5
#define FRAME_SHORT_START   0x10
#define FRAME_LONG_START    0x68
#define FRAME_STOP          0x16

#define MAX_DATA_LENGTH     252     // L is 255 at most, and C, A and CI take 3
#define MAX_DIFE            10
#define MAX_VIFE            10

#define DIF_EXTENSION       0x80
#define VIF_EXTENSION       0x80
#define DIF_MFR_DATA        0x0F    // manufacturer data up to the end
#define DIF_MORE_FOLLOWS    0x1F    // the same, and more records in the next telegram
#define DIF_IDLE_FILLER     0x2F

#define LONG_HEADER_SIZE    12
#define SHORT_HEADER_SIZE   4

/*
 *  Room for an id: a quantity name, the qualifiers, MAX_VIFE "~xx" and a
 *  "#n". The longest quantity is under 32 bytes.
 */
#define MBUS_RECORD_ID_MAX  128

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    VALUE_NUMBER = 0,
    VALUE_DATE,             // a time point: type G, F, I or J by the data field
    VALUE_TEXT,             // the number of a fabrication, a customer...: kept as read
} value_kind_t;

/*
 *  What a VIF (and the VIFEs that change it) says of a record.
 *  value = raw * 10^exponent * factor + offset
 */
typedef struct {
    char quantity[MBUS_RECORD_ID_MAX];
    const char *unit;
    char text_unit[MBUS_RECORD_ID_MAX];     // the unit of a plain text VIF
    int exponent;
    double factor;
    double offset;
    value_kind_t kind;
    int record_error;                       // a VIFE E000 xxxx from the slave, 0 none
    char qualifiers[MBUS_RECORD_ID_MAX];    // the "~xx" of the VIFEs not interpreted
} vif_meaning_t;

typedef struct {
    int code;
    const char *name;
} medium_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE const medium_t medium_table[] = {
    {0x00, "other"},
    {0x01, "oil"},
    {0x02, "electricity"},
    {0x03, "gas"},
    {0x04, "heat"},                     // heat meter, outlet
    {0x05, "steam"},
    {0x06, "warm_water"},               // 30-90 C
    {0x07, "water"},
    {0x08, "heat_cost_allocator"},
    {0x09, "compressed_air"},
    {0x0A, "cooling_outlet"},
    {0x0B, "cooling_inlet"},
    {0x0C, "heat_inlet"},
    {0x0D, "heat_cooling"},
    {0x0E, "bus_system"},
    {0x0F, "unknown"},
    {0x15, "hot_water"},                // 90 C and over
    {0x16, "cold_water"},
    {0x17, "dual_water"},
    {0x18, "pressure"},
    {0x19, "ad_converter"},
    {0x1A, "smoke_detector"},
    {0x1B, "room_sensor"},
    {0x1C, "gas_detector"},
    {0x20, "breaker"},
    {0x21, "valve"},
    {0x25, "customer_unit"},
    {0x28, "waste_water"},
    {0x29, "garbage"},
    {0x30, "service_unit"},
    {0x36, "radio_converter_system"},
    {0x37, "radio_converter_meter"},
    {-1, 0}
};

PRIVATE const char *record_error_names[0x20] = {
    [0x00] = 0,
    [0x01] = "too many DIFE",
    [0x02] = "storage number not implemented",
    [0x03] = "unit number not implemented",
    [0x04] = "tariff number not implemented",
    [0x05] = "function not implemented",
    [0x06] = "data class not implemented",
    [0x07] = "data size not implemented",
    [0x0B] = "too many VIFE",
    [0x0C] = "illegal VIF group",
    [0x0D] = "illegal VIF exponent",
    [0x0E] = "VIF/DIF mismatch",
    [0x0F] = "unimplemented action",
    [0x15] = "no data available",
    [0x16] = "data overflow",
    [0x17] = "data underflow",
    [0x18] = "data error",
    [0x1C] = "premature end of record",
};

/*
 *  Durations coded nn (s, min, h, d) and pp (h, d, months, years), in s
 */
PRIVATE const int duration_nn_s[4] = {1, 60, 3600, 86400};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void set_why(char *why, size_t why_size, const char *fmt, ...) JANSSON_ATTRS((format(printf, 3, 4)));

/***************************************************************************
 *  Write the reason of a failure for the caller to log
 ***************************************************************************/
PRIVATE void set_why(char *why, size_t why_size, const char *fmt, ...)
{
    if(!why || why_size == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, why_size, fmt, ap);
    va_end(ap);
}

/***************************************************************************
 *  The checksum of the link layer: the sum of the bytes, modulo 256
 ***************************************************************************/
PRIVATE uint8_t checksum(const uint8_t *bf, size_t len)
{
    uint8_t cs = 0;
    for(size_t i=0; i<len; i++) {
        cs = (uint8_t)(cs + bf[i]);
    }
    return cs;
}

/***************************************************************************
 *  Find the frame at the start of bf
 ***************************************************************************/
PUBLIC int mbus_parse_frame(
    const uint8_t *bf,
    size_t len,
    mbus_frame_t *frame,
    char *why,
    size_t why_size
) {
    memset(frame, 0, sizeof(*frame));

    if(len == 0) {
        return 0;
    }

    switch(bf[0]) {
        case FRAME_ACK_START:
            frame->type = MBUS_FRAME_ACK;
            return 1;

        case FRAME_SHORT_START:
            if(len < 5) {
                return 0;
            }
            if(bf[4] != FRAME_STOP) {
                set_why(why, why_size, "short frame: stop byte is 0x%02X, not 0x16", bf[4]);
                return -1;
            }
            if(checksum(bf+1, 2) != bf[3]) {
                set_why(why, why_size, "short frame: bad checksum 0x%02X, expected 0x%02X",
                    bf[3], checksum(bf+1, 2)
                );
                return -1;
            }
            frame->type = MBUS_FRAME_SHORT;
            frame->c = bf[1];
            frame->a = bf[2];
            return 5;

        case FRAME_LONG_START:
            {
                if(len < 4) {
                    if(len >= 3 && bf[1] != bf[2]) {
                        set_why(why, why_size, "long frame: the two lengths differ, 0x%02X and 0x%02X",
                            bf[1], bf[2]
                        );
                        return -1;
                    }
                    return 0;
                }
                size_t l = bf[1];
                if(bf[1] != bf[2]) {
                    set_why(why, why_size, "long frame: the two lengths differ, 0x%02X and 0x%02X",
                        bf[1], bf[2]
                    );
                    return -1;
                }
                if(bf[3] != FRAME_LONG_START) {
                    set_why(why, why_size, "long frame: fourth byte is 0x%02X, not 0x68", bf[3]);
                    return -1;
                }
                if(l < 3) {
                    set_why(why, why_size, "long frame: length %d, less than C, A and CI", (int)l);
                    return -1;
                }
                size_t total = l + 6;
                if(len < total) {
                    return 0;
                }
                if(bf[total-1] != FRAME_STOP) {
                    set_why(why, why_size, "long frame: stop byte is 0x%02X, not 0x16", bf[total-1]);
                    return -1;
                }
                uint8_t cs = checksum(bf+4, l);
                if(cs != bf[4+l]) {
                    set_why(why, why_size, "long frame: bad checksum 0x%02X, expected 0x%02X",
                        bf[4+l], cs
                    );
                    return -1;
                }
                frame->type = (l == 3)? MBUS_FRAME_CONTROL : MBUS_FRAME_LONG;
                frame->c = bf[4];
                frame->a = bf[5];
                frame->ci = bf[6];
                frame->data = bf + 7;
                frame->data_len = l - 3;
                return (int)total;
            }

        default:
            set_why(why, why_size, "no frame starts with 0x%02X", bf[0]);
            return -1;
    }
}

/***************************************************************************
 *  Build a short frame
 ***************************************************************************/
PUBLIC size_t mbus_build_short_frame(uint8_t *bf, size_t size, uint8_t c, uint8_t a)
{
    if(size < 5) {
        return 0;
    }
    bf[0] = FRAME_SHORT_START;
    bf[1] = c;
    bf[2] = a;
    bf[3] = (uint8_t)(c + a);
    bf[4] = FRAME_STOP;
    return 5;
}

/***************************************************************************
 *  Build a long frame
 ***************************************************************************/
PUBLIC size_t mbus_build_long_frame(
    uint8_t *bf,
    size_t size,
    uint8_t c,
    uint8_t a,
    uint8_t ci,
    const uint8_t *data,
    size_t data_len
) {
    if(data_len > MAX_DATA_LENGTH) {
        return 0;
    }
    size_t total = data_len + 9;
    if(size < total) {
        return 0;
    }
    bf[0] = FRAME_LONG_START;
    bf[1] = (uint8_t)(data_len + 3);
    bf[2] = (uint8_t)(data_len + 3);
    bf[3] = FRAME_LONG_START;
    bf[4] = c;
    bf[5] = a;
    bf[6] = ci;
    if(data_len > 0) {
        memcpy(bf + 7, data, data_len);
    }
    bf[7 + data_len] = checksum(bf + 4, data_len + 3);
    bf[8 + data_len] = FRAME_STOP;
    return total;
}

/***************************************************************************
 *  A hex digit of a secondary address: 0-9, or F (wildcard)
 ***************************************************************************/
PRIVATE int secondary_digit(char ch)
{
    if(ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if(ch == 'F' || ch == 'f') {
        return 0x0F;
    }
    return -1;
}

/***************************************************************************
 *  Encode a secondary address
 ***************************************************************************/
PUBLIC int mbus_build_secondary_address(
    uint8_t out[MBUS_SECONDARY_ADDRESS_SIZE],
    const char *id,
    const char *manufacturer,
    int version,
    int medium,
    char *why,
    size_t why_size
) {
    if(!id || strlen(id) != 8) {
        set_why(why, why_size, "secondary address id must have 8 digits: '%s'", id?id:"");
        return -1;
    }
    int d[8];
    for(int i=0; i<8; i++) {
        d[i] = secondary_digit(id[i]);
        if(d[i] < 0) {
            set_why(why, why_size, "secondary address id must be digits or F: '%s'", id);
            return -1;
        }
    }
    /*
     *  BCD, least significant byte first
     */
    out[0] = (uint8_t)((d[6] << 4) | d[7]);
    out[1] = (uint8_t)((d[4] << 4) | d[5]);
    out[2] = (uint8_t)((d[2] << 4) | d[3]);
    out[3] = (uint8_t)((d[0] << 4) | d[1]);

    if(empty_string(manufacturer)) {
        out[4] = 0xFF;
        out[5] = 0xFF;
    } else {
        if(strlen(manufacturer) != 3) {
            set_why(why, why_size, "manufacturer must have 3 letters: '%s'", manufacturer);
            return -1;
        }
        int m = 0;
        for(int i=0; i<3; i++) {
            char ch = manufacturer[i];
            if(ch >= 'a' && ch <= 'z') {
                ch = (char)(ch - 'a' + 'A');
            }
            if(ch < 'A' || ch > 'Z') {
                set_why(why, why_size, "manufacturer must have 3 letters: '%s'", manufacturer);
                return -1;
            }
            m = (m << 5) | (ch - 64);
        }
        out[4] = (uint8_t)(m & 0xFF);
        out[5] = (uint8_t)((m >> 8) & 0xFF);
    }

    if(version < -1 || version > 255) {
        set_why(why, why_size, "version must be 0-255, or -1 for any: %d", version);
        return -1;
    }
    out[6] = (version < 0)? 0xFF : (uint8_t)version;

    if(medium < -1 || medium > 255) {
        set_why(why, why_size, "medium must be 0-255, or -1 for any: %d", medium);
        return -1;
    }
    out[7] = (medium < 0)? 0xFF : (uint8_t)medium;

    return 0;
}

/***************************************************************************
 *  Medium names
 ***************************************************************************/
PUBLIC const char *mbus_medium_name(int medium)
{
    for(const medium_t *m = medium_table; m->name; m++) {
        if(m->code == medium) {
            return m->name;
        }
    }
    return NULL;
}

PUBLIC int mbus_medium_code(const char *name)
{
    if(empty_string(name)) {
        return -1;
    }
    for(const medium_t *m = medium_table; m->name; m++) {
        if(strcmp(m->name, name) == 0) {
            return m->code;
        }
    }
    return -1;
}

/***************************************************************************
 *  Append text to a fixed buffer, truncating (an id is never cut short:
 *  MBUS_RECORD_ID_MAX holds the longest one)
 ***************************************************************************/
PRIVATE void append_text(char *bf, size_t size, const char *fmt, ...) JANSSON_ATTRS((format(printf, 3, 4)));
PRIVATE void append_text(char *bf, size_t size, const char *fmt, ...)
{
    size_t used = strlen(bf);
    if(used + 1 >= size) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(bf + used, size - used, fmt, ap);
    va_end(ap);
}

/***************************************************************************
 *  Name and scale of a quantity
 ***************************************************************************/
PRIVATE void set_quantity(
    vif_meaning_t *m,
    const char *quantity,
    const char *unit,
    int exponent,
    double factor
) {
    snprintf(m->quantity, sizeof(m->quantity), "%s", quantity);
    m->unit = unit;
    m->exponent = exponent;
    m->factor = factor;
}

PRIVATE void set_unknown(vif_meaning_t *m, const char *table, uint8_t code)
{
    snprintf(m->quantity, sizeof(m->quantity), "%s_%02x", table, code);
    m->unit = NULL;
    m->exponent = 0;
    m->factor = 1;
}

/***************************************************************************
 *  The primary VIF table (VIF without its extension bit)
 ***************************************************************************/
PRIVATE void decode_primary_vif(vif_meaning_t *m, uint8_t vif)
{
    int nnn = vif & 0x07;
    int nn = vif & 0x03;

    switch(vif & 0x78) {
        case 0x00:  // E000 0nnn energy 10^(nnn-3) Wh
            set_quantity(m, "energy", "Wh", nnn - 3, 1);
            return;
        case 0x08:  // E000 1nnn energy 10^nnn J
            set_quantity(m, "energy", "Wh", nnn, 1.0/3600.0);
            return;
        case 0x10:  // E001 0nnn volume 10^(nnn-6) m3
            set_quantity(m, "volume", "m3", nnn - 6, 1);
            return;
        case 0x18:  // E001 1nnn mass 10^(nnn-3) kg
            set_quantity(m, "mass", "kg", nnn - 3, 1);
            return;
        case 0x28:  // E010 1nnn power 10^(nnn-3) W
            set_quantity(m, "power", "W", nnn - 3, 1);
            return;
        case 0x30:  // E011 0nnn power 10^nnn J/h
            set_quantity(m, "power", "W", nnn, 1.0/3600.0);
            return;
        case 0x38:  // E011 1nnn volume flow 10^(nnn-6) m3/h
            set_quantity(m, "volume_flow", "m3/h", nnn - 6, 1);
            return;
        case 0x40:  // E100 0nnn volume flow 10^(nnn-7) m3/min
            set_quantity(m, "volume_flow", "m3/h", nnn - 7, 60);
            return;
        case 0x48:  // E100 1nnn volume flow 10^(nnn-9) m3/s
            set_quantity(m, "volume_flow", "m3/h", nnn - 9, 3600);
            return;
        case 0x50:  // E101 0nnn mass flow 10^(nnn-3) kg/h
            set_quantity(m, "mass_flow", "kg/h", nnn - 3, 1);
            return;
        default:
            break;
    }

    switch(vif & 0x7C) {
        case 0x20:  // E010 00nn on time
            set_quantity(m, "on_time", "s", 0, duration_nn_s[nn]);
            return;
        case 0x24:  // E010 01nn operating time
            set_quantity(m, "operating_time", "s", 0, duration_nn_s[nn]);
            return;
        case 0x58:  // E101 10nn flow temperature 10^(nn-3) C
            set_quantity(m, "flow_temperature", "C", nn - 3, 1);
            return;
        case 0x5C:  // E101 11nn return temperature
            set_quantity(m, "return_temperature", "C", nn - 3, 1);
            return;
        case 0x60:  // E110 00nn temperature difference 10^(nn-3) K
            set_quantity(m, "temperature_difference", "K", nn - 3, 1);
            return;
        case 0x64:  // E110 01nn external temperature
            set_quantity(m, "external_temperature", "C", nn - 3, 1);
            return;
        case 0x68:  // E110 10nn pressure 10^(nn-3) bar
            set_quantity(m, "pressure", "bar", nn - 3, 1);
            return;
        case 0x70:  // E111 00nn averaging duration
            set_quantity(m, "averaging_duration", "s", 0, duration_nn_s[nn]);
            return;
        case 0x74:  // E111 01nn actuality duration
            set_quantity(m, "actuality_duration", "s", 0, duration_nn_s[nn]);
            return;
        default:
            break;
    }

    switch(vif) {
        case 0x6C:  // E110 1100 time point, date (type G)
            set_quantity(m, "date", NULL, 0, 1);
            m->kind = VALUE_DATE;
            return;
        case 0x6D:  // E110 1101 time point, date and time (type F, I or J)
            set_quantity(m, "datetime", NULL, 0, 1);
            m->kind = VALUE_DATE;
            return;
        case 0x6E:  // E110 1110 units for heat cost allocator
            set_quantity(m, "hca_units", NULL, 0, 1);
            return;
        case 0x78:  // E111 1000 fabrication number
            set_quantity(m, "fabrication_number", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x79:  // E111 1001 (enhanced) identification
            set_quantity(m, "identification", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x7A:  // E111 1010 bus address
            set_quantity(m, "bus_address", NULL, 0, 1);
            return;
        case 0x7F:  // E111 1111 manufacturer specific
            set_quantity(m, "mfr_specific", NULL, 0, 1);
            return;
        default:    // 6F reserved, 7E any VIF (a request, not an answer)
            set_unknown(m, "vif", vif);
            return;
    }
}

/***************************************************************************
 *  The first extension table, VIF FB (code without its extension bit)
 ***************************************************************************/
PRIVATE void decode_fb_vife(vif_meaning_t *m, uint8_t code)
{
    int n = code & 0x01;
    int nn = code & 0x03;
    int nnn = code & 0x07;

    switch(code & 0x7E) {
        case 0x00:  // E000 000n energy 10^(n-1) MWh
            set_quantity(m, "energy", "Wh", n - 1 + 6, 1);
            return;
        case 0x08:  // E000 100n energy 10^(n-1) GJ
            set_quantity(m, "energy", "Wh", n - 1 + 9, 1.0/3600.0);
            return;
        case 0x10:  // E001 000n volume 10^(n+2) m3
            set_quantity(m, "volume", "m3", n + 2, 1);
            return;
        case 0x18:  // E001 100n mass 10^(n+2) t
            set_quantity(m, "mass", "kg", n + 2 + 3, 1);
            return;
        case 0x28:  // E010 100n power 10^(n-1) MW
            set_quantity(m, "power", "W", n - 1 + 6, 1);
            return;
        case 0x30:  // E011 000n power 10^(n-1) GJ/h
            set_quantity(m, "power", "W", n - 1 + 9, 1.0/3600.0);
            return;
        default:
            break;
    }

    switch(code & 0x7C) {
        case 0x58:  // E101 10nn flow temperature 10^(nn-3) F
            set_quantity(m, "flow_temperature", "C", nn - 3, 5.0/9.0);
            m->offset = -32.0*5.0/9.0;
            return;
        case 0x5C:  // E101 11nn return temperature F
            set_quantity(m, "return_temperature", "C", nn - 3, 5.0/9.0);
            m->offset = -32.0*5.0/9.0;
            return;
        case 0x60:  // E110 00nn temperature difference F
            set_quantity(m, "temperature_difference", "K", nn - 3, 5.0/9.0);
            return;
        case 0x64:  // E110 01nn external temperature F
            set_quantity(m, "external_temperature", "C", nn - 3, 5.0/9.0);
            m->offset = -32.0*5.0/9.0;
            return;
        case 0x70:  // E111 00nn cold/warm temperature limit F
            set_quantity(m, "temperature_limit", "C", nn - 3, 5.0/9.0);
            m->offset = -32.0*5.0/9.0;
            return;
        case 0x74:  // E111 01nn cold/warm temperature limit C
            set_quantity(m, "temperature_limit", "C", nn - 3, 1);
            return;
        default:
            break;
    }

    if((code & 0x78) == 0x78) {  // E111 1nnn cumulative count of max power 10^(nnn-3) W
        set_quantity(m, "max_power_count", "W", nnn - 3, 1);
        return;
    }

    set_unknown(m, "fb", code);  // reserved, and the US volume units
}

/***************************************************************************
 *  The second extension table, VIF FD (code without its extension bit)
 ***************************************************************************/
PRIVATE void decode_fd_vife(vif_meaning_t *m, uint8_t code)
{
    int nn = code & 0x03;

    switch(code & 0x7C) {
        case 0x00:  // E000 00nn credit 10^(nn-3) of the local currency
            set_quantity(m, "credit", NULL, nn - 3, 1);
            return;
        case 0x04:  // E000 01nn debit
            set_quantity(m, "debit", NULL, nn - 3, 1);
            return;
        case 0x24:  // E010 01nn storage interval
            set_quantity(m, "storage_interval", "s", 0, duration_nn_s[nn]);
            return;
        case 0x2C:  // E010 11nn duration since last readout
            set_quantity(m, "since_last_readout", "s", 0, duration_nn_s[nn]);
            return;
        case 0x34:  // E011 01nn period of tariff
            set_quantity(m, "tariff_period", "s", 0, duration_nn_s[nn]);
            return;
        case 0x68:  // E110 10pp duration since last cumulation (h, d, months, years)
            if(nn < 2) {
                set_quantity(m, "since_last_cumulation", "s", 0, nn == 0? 3600 : 86400);
            } else {
                set_quantity(m, "since_last_cumulation", nn == 2? "month" : "year", 0, 1);
            }
            return;
        case 0x6C:  // E110 11pp operating time of the battery
            if(nn < 2) {
                set_quantity(m, "battery_operating_time", "s", 0, nn == 0? 3600 : 86400);
            } else {
                set_quantity(m, "battery_operating_time", nn == 2? "month" : "year", 0, 1);
            }
            return;
        default:
            break;
    }

    if((code & 0x70) == 0x40) {  // E100 nnnn voltage 10^(nnnn-9) V
        set_quantity(m, "voltage", "V", (code & 0x0F) - 9, 1);
        return;
    }
    if((code & 0x70) == 0x50) {  // E101 nnnn current 10^(nnnn-12) A
        set_quantity(m, "current", "A", (code & 0x0F) - 12, 1);
        return;
    }
    if(code >= 0x31 && code <= 0x33) {  // E011 00nn duration of tariff (nn 1-3: min to days)
        set_quantity(m, "tariff_duration", "s", 0, duration_nn_s[nn]);
        return;
    }

    switch(code) {
        case 0x08:
            set_quantity(m, "access_number", NULL, 0, 1);
            return;
        case 0x09:
            set_quantity(m, "medium", NULL, 0, 1);
            return;
        case 0x0A:
            set_quantity(m, "manufacturer", NULL, 0, 1);
            return;
        case 0x0B:
            set_quantity(m, "parameter_set_id", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x0C:
            set_quantity(m, "model_version", NULL, 0, 1);
            return;
        case 0x0D:
            set_quantity(m, "hardware_version", NULL, 0, 1);
            return;
        case 0x0E:
            set_quantity(m, "firmware_version", NULL, 0, 1);
            return;
        case 0x0F:
            set_quantity(m, "software_version", NULL, 0, 1);
            return;
        case 0x10:
            set_quantity(m, "customer_location", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x11:
            set_quantity(m, "customer", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x16:
            set_quantity(m, "password", NULL, 0, 1);
            m->kind = VALUE_TEXT;
            return;
        case 0x17:
            set_quantity(m, "error_flags", NULL, 0, 1);
            return;
        case 0x18:
            set_quantity(m, "error_mask", NULL, 0, 1);
            return;
        case 0x1A:
            set_quantity(m, "digital_output", NULL, 0, 1);
            return;
        case 0x1B:
            set_quantity(m, "digital_input", NULL, 0, 1);
            return;
        case 0x1C:
            set_quantity(m, "baudrate", NULL, 0, 1);
            return;
        case 0x1D:
            set_quantity(m, "response_delay", NULL, 0, 1);
            return;
        case 0x1E:
            set_quantity(m, "retry", NULL, 0, 1);
            return;
        case 0x20:
            set_quantity(m, "first_storage", NULL, 0, 1);
            return;
        case 0x21:
            set_quantity(m, "last_storage", NULL, 0, 1);
            return;
        case 0x22:
            set_quantity(m, "storage_block_size", NULL, 0, 1);
            return;
        case 0x28:
            set_quantity(m, "storage_interval", "month", 0, 1);
            return;
        case 0x29:
            set_quantity(m, "storage_interval", "year", 0, 1);
            return;
        case 0x30:
            set_quantity(m, "tariff_start", NULL, 0, 1);
            m->kind = VALUE_DATE;
            return;
        case 0x38:
            set_quantity(m, "tariff_period", "month", 0, 1);
            return;
        case 0x39:
            set_quantity(m, "tariff_period", "year", 0, 1);
            return;
        case 0x3A:
            set_quantity(m, "dimensionless", NULL, 0, 1);
            return;
        case 0x60:
            set_quantity(m, "reset_counter", NULL, 0, 1);
            return;
        case 0x61:
            set_quantity(m, "cumulation_counter", NULL, 0, 1);
            return;
        case 0x62:
            set_quantity(m, "control_signal", NULL, 0, 1);
            return;
        case 0x63:
            set_quantity(m, "day_of_week", NULL, 0, 1);
            return;
        case 0x64:
            set_quantity(m, "week_number", NULL, 0, 1);
            return;
        case 0x65:
            set_quantity(m, "day_change", NULL, 0, 1);
            m->kind = VALUE_DATE;
            return;
        case 0x66:
            set_quantity(m, "parameter_activation", NULL, 0, 1);
            return;
        case 0x67:
            set_quantity(m, "supplier_information", NULL, 0, 1);
            return;
        case 0x70:
            set_quantity(m, "battery_change", NULL, 0, 1);
            m->kind = VALUE_DATE;
            return;
        default:
            break;
    }

    set_unknown(m, "fd", code);  // access codes, reserved
}

/***************************************************************************
 *  A VIFE after the VIF (the combinable table, EN 13757-3).
 *  Return FALSE when the VIFEs that follow are the manufacturer's (7F).
 ***************************************************************************/
PRIVATE BOOL apply_vife(vif_meaning_t *m, uint8_t vife)
{
    uint8_t code = vife & 0x7F;

    if(code <= 0x1F) {  // E00x xxxx a record error, slave to master
        m->record_error = code;
        return TRUE;
    }
    if(code == 0x3B) {  // E011 1011 accumulation only of positive contributions
        append_text(m->quantity, sizeof(m->quantity), "_pos");
        return TRUE;
    }
    if(code == 0x3C) {  // E011 1100 accumulation of the absolute value only of negative ones
        append_text(m->quantity, sizeof(m->quantity), "_neg");
        return TRUE;
    }
    if((code & 0x78) == 0x70) {  // E111 0nnn multiplicative correction 10^(nnn-6)
        m->exponent += (code & 0x07) - 6;
        return TRUE;
    }
    if(code == 0x7D) {  // E111 1101 multiplicative correction 10^3
        m->exponent += 3;
        return TRUE;
    }
    if(code == 0x39 || (code & 0x72) == 0x42 || (code & 0x7A) == 0x6A) {
        /*
         *  "date (/time) of": the start of, the begin or end of a limit
         *  exceed, ... The value is a time point, and the record keeps the
         *  VIFE in its id because it is not the quantity itself.
         */
        m->kind = VALUE_DATE;
    }

    /*
     *  A VIFE kept as a qualifier can change what the number counts (a
     *  number of limit exceeds, a duration, "per hour"), so the unit of the
     *  VIF stays only where the VIFE leaves the quantity as it is: an
     *  uncorrected unit, a lower or upper limit value, a future value.
     */
    if(code != 0x3A && code != 0x40 && code != 0x48 && code != 0x7E) {
        m->unit = NULL;
    }
    append_text(m->qualifiers, sizeof(m->qualifiers), "~%02x", code);
    return code != 0x7F;
}

/***************************************************************************
 *  A text of the telegram, least significant byte first, to a string json
 *  can hold (a byte that is not printable ASCII becomes '?')
 ***************************************************************************/
PRIVATE void copy_reversed_text(char *dst, size_t size, const uint8_t *src, size_t len)
{
    size_t n = 0;
    while(len > 0 && n + 1 < size) {
        uint8_t ch = src[--len];
        dst[n++] = (ch >= 0x20 && ch < 0x7F)? (char)ch : '?';
    }
    dst[n] = 0;
}

/***************************************************************************
 *  10^n as a double, without libm (static yunos have none)
 ***************************************************************************/
PRIVATE double power_of_ten(int n)
{
    double p = 1;
    int a = n < 0? -n : n;
    for(int i=0; i<a; i++) {
        p *= 10;
    }
    return p;
}

/***************************************************************************
 *  The value of a number with its scale: an integer when it stays one
 ***************************************************************************/
PRIVATE json_t *scaled_number(const vif_meaning_t *m, int64_t raw, double rawf, BOOL is_real)
{
    if(!is_real && m->factor == 1 && m->offset == 0 && m->exponent >= 0 && m->exponent <= 9) {
        int64_t mul = (int64_t)power_of_ten(m->exponent);
        if(raw == 0 || (raw > 0 && raw <= INT64_MAX / mul) || (raw < 0 && raw >= INT64_MIN / mul)) {
            return json_integer((json_int_t)(raw * mul));
        }
    }
    if(!is_real && m->factor != 1 && m->offset == 0 && m->exponent == 0) {
        double f = m->factor;
        if(f == (double)(int64_t)f) {  // a duration: s, min, h or d to s
            int64_t mul = (int64_t)f;
            if(raw == 0 || (raw > 0 && raw <= INT64_MAX / mul) || (raw < 0 && raw >= INT64_MIN / mul)) {
                return json_integer((json_int_t)(raw * mul));
            }
        }
    }

    double v = is_real? rawf : (double)raw;
    if(m->exponent < 0) {
        v = v / power_of_ten(m->exponent);  // a division keeps 561.08 as 561.08
    } else {
        v = v * power_of_ten(m->exponent);
    }
    v = v * m->factor + m->offset;
    return json_real(v);
}

/***************************************************************************
 *  A signed integer of len bytes, least significant first
 ***************************************************************************/
PRIVATE int64_t decode_integer(const uint8_t *p, size_t len)
{
    uint64_t u = 0;
    for(size_t i=len; i>0; i--) {
        u = (u << 8) | p[i-1];
    }
    if(len < 8 && (p[len-1] & 0x80)) {
        u |= ~(uint64_t)0 << (8*len);
    }
    return (int64_t)u;
}

/***************************************************************************
 *  A BCD of len bytes, least significant first.
 *  Return 0, or -1 when a digit is not one (a BCD that says it is not
 *  available, overflowed or in error).
 ***************************************************************************/
PRIVATE int decode_bcd(const uint8_t *p, size_t len, int64_t *value)
{
    int64_t v = 0;
    BOOL negative = FALSE;
    for(size_t i=len; i>0; i--) {
        int hi = p[i-1] >> 4;
        int lo = p[i-1] & 0x0F;
        if(i == len && hi == 0x0F) {
            negative = TRUE;
            hi = 0;
        }
        if(hi > 9 || lo > 9) {
            return -1;
        }
        v = v * 100 + hi * 10 + lo;
    }
    *value = negative? -v : v;
    return 0;
}

/***************************************************************************
 *  A time point: type G (2 bytes, date), J (3, time), F (4, date and
 *  time) or I (6, date and time with seconds). NULL when the meter marks
 *  it invalid, or the length is none of those.
 ***************************************************************************/
PRIVATE json_t *decode_time_point(const uint8_t *p, size_t len)
{
    switch(len) {
        case 2: // type G
            {
                if(p[0] == 0xFF && p[1] == 0xFF) {
                    return NULL;
                }
                int day = p[0] & 0x1F;
                int month = p[1] & 0x0F;
                int year = ((p[0] & 0xE0) >> 5) | ((p[1] & 0xF0) >> 1);
                if(day == 0 || month == 0 || month > 12) {
                    return NULL;  // a date never set
                }
                return json_sprintf("%04d-%02d-%02d", 2000 + year, month, day);
            }
        case 3: // type J
            return json_sprintf("%02d:%02d:%02d", p[2] & 0x1F, p[1] & 0x3F, p[0] & 0x3F);
        case 4: // type F
            {
                if(p[0] & 0x80) {
                    return NULL;  // IV: invalid
                }
                int minute = p[0] & 0x3F;
                int hour = p[1] & 0x1F;
                int day = p[2] & 0x1F;
                int month = p[3] & 0x0F;
                int year = ((p[2] & 0xE0) >> 5) | ((p[3] & 0xF0) >> 1);
                if(day == 0 || month == 0 || month > 12) {
                    return NULL;
                }
                int century = (p[1] & 0x60) >> 5;
                if(century == 0 && year <= 80) {
                    century = 1;  // old meters count the year in two digits
                }
                return json_sprintf("%04d-%02d-%02dT%02d:%02d",
                    1900 + 100*century + year, month, day, hour, minute
                );
            }
        case 6: // type I
            {
                if(p[1] & 0x80) {
                    return NULL;  // IV: invalid
                }
                int second = p[0] & 0x3F;
                int minute = p[1] & 0x3F;
                int hour = p[2] & 0x1F;
                int day = p[3] & 0x1F;
                int month = p[4] & 0x0F;
                int year = ((p[3] & 0xE0) >> 5) | ((p[4] & 0xF0) >> 1);
                if(day == 0 || month == 0 || month > 12) {
                    return NULL;
                }
                return json_sprintf("%04d-%02d-%02dT%02d:%02d:%02d",
                    2000 + year, month, day, hour, minute, second
                );
            }
        default:
            return NULL;
    }
}

/***************************************************************************
 *  Bytes in hex: in the order they came (manufacturer data), or the most
 *  significant first (a binary number, which travels least significant
 *  first)
 ***************************************************************************/
PRIVATE json_t *hex_string(const uint8_t *p, size_t len, BOOL reversed)
{
    char bf[2*MBUS_FRAME_MAX_LENGTH + 1];
    size_t n = 0;
    for(size_t i=0; i<len && n + 2 < sizeof(bf); i++) {
        snprintf(bf + n, sizeof(bf) - n, "%02X", reversed? p[len-1-i] : p[i]);
        n += 2;
    }
    bf[n] = 0;
    return json_string(bf);
}

/***************************************************************************
 *  The value of a record. `*len` gets the bytes its data takes (the LVAR
 *  byte included). Return the json, json_null() when the record carries
 *  no usable value, or NULL with the reason in `why` when the data does
 *  not fit in what is left of the telegram.
 ***************************************************************************/
PRIVATE json_t *decode_record_value(
    const vif_meaning_t *m,
    uint8_t coding,
    const uint8_t *p,
    size_t left,
    size_t *len,
    char *why,
    size_t why_size
) {
    static const uint8_t data_length[16] = {
        0, 1, 2, 3, 4, 4, 6, 8, 0, 1, 2, 3, 4, 0, 6, 0
    };

    size_t n = data_length[coding];
    size_t skip = 0;    // the LVAR byte

    if(coding == 0x0D) {
        if(left < 1) {
            set_why(why, why_size, "record ends before its LVAR");
            return NULL;
        }
        uint8_t lvar = p[0];
        skip = 1;
        if(lvar <= 0xBF) {
            n = lvar;
        } else if(lvar >= 0xC0 && lvar <= 0xCF) {
            n = (size_t)(lvar - 0xC0);
        } else if(lvar >= 0xD0 && lvar <= 0xDF) {
            n = (size_t)(lvar - 0xD0);
        } else if(lvar >= 0xE0 && lvar <= 0xEF) {
            n = (size_t)(lvar - 0xE0);
        } else if(lvar >= 0xF0 && lvar <= 0xF4) {
            n = (size_t)(lvar - 0xEC) * 4;
        } else if(lvar == 0xF5) {
            n = 48;
        } else if(lvar == 0xF6) {
            n = 64;
        } else {
            set_why(why, why_size, "reserved LVAR 0x%02X", lvar);
            return NULL;
        }
        if(skip + n > left) {
            set_why(why, why_size, "record data (%d bytes) longer than the telegram left (%d)",
                (int)(skip + n), (int)left
            );
            return NULL;
        }
        *len = skip + n;
        const uint8_t *d = p + skip;

        if(lvar <= 0xBF) {
            char text[MAX_DATA_LENGTH + 1];
            copy_reversed_text(text, sizeof(text), d, n);
            return json_string(text);
        }
        if(lvar <= 0xDF) {
            int64_t v;
            if(n == 0 || n > 9 || decode_bcd(d, n, &v) < 0) {
                return json_null();
            }
            if(lvar >= 0xD0) {
                v = -v;
            }
            return scaled_number(m, v, 0, FALSE);
        }
        if(lvar <= 0xEF && n >= 1 && n <= 8) {
            return scaled_number(m, decode_integer(d, n), 0, FALSE);
        }
        return hex_string(d, n, TRUE);
    }

    if(n > left) {
        set_why(why, why_size, "record data (%d bytes) longer than the telegram left (%d)",
            (int)n, (int)left
        );
        return NULL;
    }
    *len = n;

    if(n == 0) {
        return json_null();  // 0: no data; 8: selection for readout
    }

    if(m->kind == VALUE_DATE && (coding == 0x02 || coding == 0x03 || coding == 0x04 || coding == 0x06)) {
        json_t *jn = decode_time_point(p, n);
        return jn? jn : json_null();
    }

    switch(coding) {
        case 0x05:  // real, 32 bits
            {
                uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
                float f;
                memcpy(&f, &u, sizeof(f));
                if(f != f) {
                    return json_null();  // NaN
                }
                return scaled_number(m, 0, (double)f, TRUE);
            }
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0E:
            {
                int64_t v;
                if(decode_bcd(p, n, &v) < 0) {
                    return json_null();
                }
                if(m->kind == VALUE_TEXT) {
                    return json_sprintf("%0*lld", (int)(2*n), (long long)v);  // "06855817"
                }
                return scaled_number(m, v, 0, FALSE);
            }
        default:
            if(m->kind == VALUE_TEXT) {
                return json_sprintf("%lld", (long long)decode_integer(p, n));
            }
            return scaled_number(m, decode_integer(p, n), 0, FALSE);
    }
}

/***************************************************************************
 *  The header of CI 72: identification, manufacturer, version, medium,
 *  access number, status, signature
 ***************************************************************************/
PRIVATE json_t *decode_long_header(const uint8_t *d)
{
    json_t *jn_header = json_object();

    char id[9];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X", d[3], d[2], d[1], d[0]);
    json_object_set_new(jn_header, "id", json_string(id));

    int m = d[4] | (d[5] << 8);
    char man[4] = {
        (char)(((m >> 10) & 0x1F) + 64),
        (char)(((m >> 5) & 0x1F) + 64),
        (char)((m & 0x1F) + 64),
        0
    };
    for(int i=0; i<3; i++) {
        if(man[i] < 'A' || man[i] > 'Z') {
            man[i] = '?';  // 0 or 27-31 in a letter: not a manufacturer code
        }
    }
    json_object_set_new(jn_header, "manufacturer", json_string(man));
    json_object_set_new(jn_header, "version", json_integer(d[6]));

    const char *medium = mbus_medium_name(d[7]);
    if(medium) {
        json_object_set_new(jn_header, "medium", json_string(medium));
    }
    json_object_set_new(jn_header, "medium_code", json_integer(d[7]));
    json_object_set_new(jn_header, "access_no", json_integer(d[8]));
    json_object_set_new(jn_header, "status", json_integer(d[9]));
    return jn_header;
}

/***************************************************************************
 *  The records of the variable data structure, from d[0] to d[len-1]
 ***************************************************************************/
PRIVATE int decode_records(
    json_t *jn_data,
    const uint8_t *d,
    size_t len,
    BOOL *more_follows,
    char *why,
    size_t why_size
) {
    json_t *jn_records = json_object();
    json_object_set_new(jn_data, "records", jn_records);

    size_t i = 0;
    int idx = 0;
    while(i < len) {
        size_t record_start = i;
        uint8_t dif = d[i];

        if(dif == DIF_IDLE_FILLER) {
            i++;
            continue;
        }
        if(dif == DIF_MFR_DATA || dif == DIF_MORE_FOLLOWS) {
            if(dif == DIF_MORE_FOLLOWS) {
                *more_follows = TRUE;
            }
            i++;
            if(i < len) {
                json_object_set_new(jn_data, "mfr_data", hex_string(d + i, len - i, FALSE));
            }
            break;
        }
        if((dif & 0x0F) == 0x0F) {
            set_why(why, why_size, "record %d: reserved DIF 0x%02X at byte %d", idx, dif, (int)i);
            return -1;
        }

        uint8_t coding = dif & 0x0F;
        int function = (dif >> 4) & 0x03;
        uint64_t storage = (dif >> 6) & 0x01;
        unsigned tariff = 0;
        unsigned subunit = 0;
        i++;

        int ndife = 0;
        uint8_t last = dif;
        while(last & DIF_EXTENSION) {
            if(ndife >= MAX_DIFE) {
                set_why(why, why_size, "record %d: more than %d DIFE", idx, MAX_DIFE);
                return -1;
            }
            if(i >= len) {
                set_why(why, why_size, "record %d: telegram ends inside the DIFEs", idx);
                return -1;
            }
            uint8_t dife = d[i++];
            storage |= (uint64_t)(dife & 0x0F) << (1 + 4*ndife);
            tariff |= (unsigned)((dife >> 4) & 0x03) << (2*ndife);
            subunit |= (unsigned)((dife >> 6) & 0x01) << ndife;
            ndife++;
            last = dife;
        }

        if(i >= len) {
            set_why(why, why_size, "record %d: telegram ends before the VIF", idx);
            return -1;
        }

        vif_meaning_t m;
        memset(&m, 0, sizeof(m));
        m.factor = 1;

        uint8_t vif = d[i++];
        uint8_t vif7 = vif & 0x7F;
        BOOL more_vife = (vif & VIF_EXTENSION)? TRUE : FALSE;
        BOOL interpret = TRUE;

        if(vif7 == 0x7C) {
            /*
             *  Plain text VIF: the unit is the text that follows
             */
            if(i >= len || i + 1 + d[i] > len) {
                set_why(why, why_size, "record %d: telegram ends inside the plain text VIF", idx);
                return -1;
            }
            size_t tl = d[i];
            copy_reversed_text(m.text_unit, sizeof(m.text_unit), d + i + 1, tl);
            i += 1 + tl;
            set_quantity(&m, "custom", m.text_unit, 0, 1);

        } else if(vif == 0xFB || vif == 0xFD) {
            if(i >= len) {
                set_why(why, why_size, "record %d: telegram ends after the VIF 0x%02X", idx, vif);
                return -1;
            }
            uint8_t ext = d[i++];
            if(vif == 0xFB) {
                decode_fb_vife(&m, ext & 0x7F);
            } else {
                decode_fd_vife(&m, ext & 0x7F);
            }
            more_vife = (ext & VIF_EXTENSION)? TRUE : FALSE;

        } else {
            decode_primary_vif(&m, vif7);
            if(vif7 == 0x7F) {
                interpret = FALSE;
            }
        }

        int nvife = 0;
        while(more_vife) {
            if(nvife >= MAX_VIFE) {
                set_why(why, why_size, "record %d: more than %d VIFE", idx, MAX_VIFE);
                return -1;
            }
            if(i >= len) {
                set_why(why, why_size, "record %d: telegram ends inside the VIFEs", idx);
                return -1;
            }
            uint8_t vife = d[i++];
            if(interpret) {
                interpret = apply_vife(&m, vife);
            } else {
                append_text(m.qualifiers, sizeof(m.qualifiers), "~%02x", vife & 0x7F);
            }
            more_vife = (vife & VIF_EXTENSION)? TRUE : FALSE;
            nvife++;
        }

        size_t data_len = 0;
        json_t *jn_value = decode_record_value(&m, coding, d + i, len - i, &data_len, why, why_size);
        if(!jn_value) {
            char reason[256];
            snprintf(reason, sizeof(reason), "%s", why? why : "");
            set_why(why, why_size, "record %d at byte %d: %s", idx, (int)record_start, reason);
            return -1;
        }
        i += data_len;

        /*
         *  The id
         */
        char id[MBUS_RECORD_ID_MAX + 16];   // and room for the "#n" of a repeated id
        snprintf(id, sizeof(id), "%s", m.quantity);
        static const char *function_suffix[4] = {"", ":max", ":min", ":error"};
        append_text(id, sizeof(id), "%s", function_suffix[function]);
        if(storage) {
            append_text(id, sizeof(id), "@s%llu", (unsigned long long)storage);
        }
        if(tariff) {
            append_text(id, sizeof(id), "@t%u", tariff);
        }
        if(subunit) {
            append_text(id, sizeof(id), "@u%u", subunit);
        }
        append_text(id, sizeof(id), "%s", m.qualifiers);

        if(json_object_get(jn_records, id)) {
            char base[MBUS_RECORD_ID_MAX];
            snprintf(base, sizeof(base), "%.*s", (int)sizeof(base) - 1, id);
            for(int k=2; ; k++) {
                snprintf(id, sizeof(id), "%s#%d", base, k);
                if(!json_object_get(jn_records, id)) {
                    break;
                }
            }
        }

        json_t *jn_record = json_object();
        if(m.record_error && !json_is_null(jn_value)) {
            JSON_DECREF(jn_value)
            jn_value = json_null();
        }
        json_object_set_new(jn_record, "value", jn_value);
        if(m.unit && m.kind == VALUE_NUMBER) {
            json_object_set_new(jn_record, "unit", json_string(m.unit));
        }
        if(m.record_error) {
            const char *name = record_error_names[m.record_error];
            if(name) {
                json_object_set_new(jn_record, "error", json_string(name));
            } else {
                json_object_set_new(jn_record, "error", json_sprintf("record error 0x%02x", m.record_error));
            }
        }
        json_object_set_new(jn_records, id, jn_record);
        idx++;
    }

    return 0;
}

/***************************************************************************
 *  Decode the user data of a slave's answer
 ***************************************************************************/
PUBLIC json_t *mbus_decode_user_data(
    const mbus_frame_t *frame,
    BOOL *more_follows,
    char *why,
    size_t why_size
) {
    BOOL more = FALSE;
    if(more_follows) {
        *more_follows = FALSE;
    }

    if(frame->type != MBUS_FRAME_LONG && frame->type != MBUS_FRAME_CONTROL) {
        set_why(why, why_size, "not a long frame: no user data");
        return NULL;
    }

    const uint8_t *d = frame->data;
    size_t len = frame->data_len;
    json_t *jn_data = json_object();
    unsigned signature = 0;

    switch(frame->ci) {
        case MBUS_CI_RESP_VARIABLE:
            if(len < LONG_HEADER_SIZE) {
                set_why(why, why_size, "CI 72: %d bytes, less than its 12-byte header", (int)len);
                JSON_DECREF(jn_data)
                return NULL;
            }
            json_object_set_new(jn_data, "header", decode_long_header(d));
            signature = (unsigned)(d[10] | (d[11] << 8));
            d += LONG_HEADER_SIZE;
            len -= LONG_HEADER_SIZE;
            break;

        case MBUS_CI_RESP_SHORT_HEADER:
            {
                if(len < SHORT_HEADER_SIZE) {
                    set_why(why, why_size, "CI 7A: %d bytes, less than its 4-byte header", (int)len);
                    JSON_DECREF(jn_data)
                    return NULL;
                }
                json_t *jn_header = json_object();
                json_object_set_new(jn_header, "access_no", json_integer(d[0]));
                json_object_set_new(jn_header, "status", json_integer(d[1]));
                json_object_set_new(jn_data, "header", jn_header);
                signature = (unsigned)(d[2] | (d[3] << 8));
                d += SHORT_HEADER_SIZE;
                len -= SHORT_HEADER_SIZE;
            }
            break;

        case MBUS_CI_RESP_NO_HEADER:
            break;

        case MBUS_CI_APPLICATION_ERROR:
            json_object_set_new(jn_data, "application_error", json_integer(len > 0? d[0] : 0));
            return jn_data;

        case MBUS_CI_ALARM_STATUS:
            json_object_set_new(jn_data, "alarm_status", len > 0? hex_string(d, len, FALSE) : json_string(""));
            return jn_data;

        default:
            set_why(why, why_size, "CI 0x%02X is not a variable data answer (fixed data, MSB first or a request)",
                frame->ci
            );
            JSON_DECREF(jn_data)
            return NULL;
    }

    /*
     *  The signature is the configuration field of OMS: bits 8-12 are the
     *  security mode. Only the modes OMS defines for encryption refuse the
     *  records: older wired meters put other values there (31, 22 in the
     *  telegrams of libmbus) and send their records in the clear.
     */
    unsigned mode = (signature >> 8) & 0x1F;
    if(mode == 5 || mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 13) {
        set_why(why, why_size, "records encrypted (security mode %u): not decoded", mode);
        JSON_DECREF(jn_data)
        return NULL;
    }

    if(decode_records(jn_data, d, len, &more, why, why_size) < 0) {
        JSON_DECREF(jn_data)
        return NULL;
    }

    if(more_follows) {
        *more_follows = more;
    }
    return jn_data;
}
