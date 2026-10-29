/***********************************************************************
 *          C_PROT_MODBUS_M.C
 *          Prot_modbus_master GClass.
 *
 *          Modbus protocol (master side)

Object type         Access      Size        Address Space   Other names         PLC address
=================   ==========  =========   =============== =================== ===========
"Coil"              Read-write  1 bit       0x0000 – 0xFFFF "coil_status"       0xxxx
"Discrete input"    Only-read   1 bit       0x0000 – 0xFFFF "input_status"      1xxxx
"Input register"    Only-read   16 bits     0x0000 – 0xFFFF "input_register"    2xxxx
"Holding register"  Read-write  16 bits     0x0000 – 0xFFFF "holding_register"  3xxxx


Some description of protocol
https://www.fernhillsoftware.com/help/drivers/modbus/modbus-protocol.html
https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf

Example of modbus configuration:

"kw": {
    "modbus_protocol": "TCP",
    "slaves": [
        {
            "id": 3,
            "mapping": [
                {
                    "type": "input_register",
                    "address": "4096",
                    "size": 16
                }
            ],
            "conversion": [
                {
                    "id": "counter1",
                    "type": "input_register",
                    "format": "int64",
                    "address": "4096",
                    "multiplier": 1
                },
                {
                    "id": "counter2",
                    "type": "input_register",
                    "format": "int64",
                    "address": 4104,
                    "multiplier": 1
                }
            ]
        }
    ]
},


REQUESTS: a write, or a read outside the polling map

The gobj above sends EV_SEND_MESSAGE with one request:

    {
        "operation": "write",               # "write" (default) or "read"
        "id": 1,                            # slave id, default 1
        "type": "holding_register",         # write: holding_register, coil
                                            # read: also input_register,
                                            #   input_status
        "address": 46208,                   # a number, or its text: a JSON
                                            #   integer, whole ("010", "12ab",
                                            #   20.5 and null are refused)
        "size": 2,                          # registers/coils; a write takes
                                            #   it from the array of values
        "value": [1, 0]                     # write: a number, or an array
        ...                                 # anything else travels back
    }

and gets one EV_MODBUS_ANSWER per request, always -- also when it fails:

    {
        "request": {...},                   # the request, its defaults
                                            #   filled in, value an array
        "result": 0,                        # 0 done, -1 failed
        "comment": "",                      # why it failed
        "error_code": 0,                    # the modbus exception, if any
        "values": [1, 0]                    # read: the registers, or the
                                            #   coils/inputs as 0/1
    }

A request waits for the block of the cycle in flight and goes out before
the next one; one that arrives while the connection is idle -- or on the
message that ends a cycle -- goes out at once, and the wait between cycles
comes after it. They go one at a time, in the order they came: a request
that needs another before it in the same connection (a meter password and
the setup it unlocks) is just sent after it. A write is checked against the
echo the slave answers with. A refusal, a log or a trace never shows the
values of a write: they can be a secret. The frame of a write is marked
secret (gbuffer_set_secret()), so the traffic trace of this gclass and of
its transport dump it as "<N bytes hidden>". This gclass's own dumps of what
it receives hide a frame by ITS function -- a write (05, 06, 15, 16) or its
exception -- or while a write is in flight: the late echo of a write comes
while a read is in flight. Two limits: the transport's dump of what it
RECEIVES shows the echo of a single write (05, 06), which carries the value
(received bytes nobody can mark); and the global `ev_kw` trace shows the
request's kw, "value" included (it masks by key name, and "value" is none).

An answer is taken only for what is in flight. Over TCP it must carry the
transaction id of the request sent and protocol 0: the late answer of a
request that timed out (same slave, function and size as the next one) is
dropped with a warning instead of answering the next. RTU carries no
transaction id: there a frame is dropped when nothing is in flight, or when
its slave or its function (an exception's without its 0x80 bit) is not the
one sent -- the late exception of a read is not the answer of the write sent
after it. An RTU answer is checked against its CRC before anything in it is
taken, an exception included. Over TCP an answer of another function, an
exception included, fails what is in flight. A broken answer fails what is
in flight, as a warning: the bytes are the peer's. Over TCP, bytes that
come after an answer in the same read came before the next request went
out: the frame they start is read whole and dropped, and the one after it
is read. An MBAP length outside 3-254 (unit and PDU: 253 bytes at most)
tells nowhere a frame ends: the connection is dropped. So are bytes that
are no frame (TCP loses nothing: they come from a broken slave or
gateway): the head read across them has a bad MBAP length, and a new
connection is the one resync that cannot take data for a head. Over RTU
the bytes after an answer in the same read are dropped, with a warning: a
noise byte when the slave releases the line, taken as the head of a frame,
would swallow the next answer. Over RTU the slave ids 248-255 are
reserved, and 0 is the broadcast that no slave answers: a request to
248-255 is refused, and a polled slave with one of them, or with 0, is not
polled (its blocks are disabled, logged when the gobj starts).

The map (`slaves`) is read at start. Written while the gobj runs, it takes
effect at the next start, with a warning: the cycle keeps its own
reference to the map it started with. A block whose answer does not fit
what it asked (its quantity, its byte count) fails, logged when it starts
failing, not every cycle.

A request is valid for the connection it was queued in: when the
connection drops, the one in flight and every queued one are answered as
failed, and none of them is sent on the next connection -- whatever
unlocked them there, a password, is gone with the old one. A request sent
while disconnected is answered at once.


 *          Copyright (c) 2021-2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <arpa/inet.h>

#include "istream.h"
#include "msg_ievent.h"
#include "g_ev_kernel.h"
#include "g_st_kernel.h"
#include "c_uart.h"
#include "c_timer.h"
#include "c_tcp.h"
#include "c_prot_modbus_m.h"

#ifdef ESP_PLATFORM
#include "c_esp_transport.h"
#endif

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
Object type         Access      Size        Address Space   Other names         PLC address
=================   ==========  =========   =============== =================== ===========
Coil                Read-write  1 bit       0x0000 – 0xFFFF "coil_status"       0xxxx
Discrete input      Only-read   1 bit       0x0000 – 0xFFFF "input_status"      1xxxx
Input register      Only-read   16 bits     0x0000 – 0xFFFF "input_register"    2xxxx
Holding register    Read-write  16 bits     0x0000 – 0xFFFF "holding_register"  3xxxx
*/

typedef enum {
    TYPE_COIL               = 0,
    TYPE_DISCRETE_INPUT     = 1,
    TYPE_INPUT_REGISTER     = 2,
    TYPE_HOLDING_REGISTER   = 3,
    TYPE_UNKNOWN            = -1
} modbus_object_type_t;

typedef enum {
    FORMAT_BIG_ENDIAN               = 0,
    FORMAT_LITTLE_ENDIAN            = 1,
    FORMAT_BIG_ENDIAN_BYTE_SWAP     = 2,
    FORMAT_LITTLE_ENDIAN_BYTE_SWAP  = 3,
} endian_format_t;

typedef enum {
    FORMAT_INT16        = 0,
    FORMAT_UINT16       = 1,
    FORMAT_BOOL         = 2,
    FORMAT_INT32        = 3,
    FORMAT_UINT32       = 4,
    FORMAT_INT64        = 5,
    FORMAT_UINT64       = 6,
    FORMAT_FLOAT        = 7,
    FORMAT_DOUBLE       = 8,
    FORMAT_STRING       = 9,
} variable_format_t;

/* Modbus function codes */
typedef enum {
    MODBUS_FC_READ_COILS                = 0x01,
    MODBUS_FC_READ_DISCRETE_INPUTS      = 0x02,
    MODBUS_FC_READ_HOLDING_REGISTERS    = 0x03,
    MODBUS_FC_READ_INPUT_REGISTERS      = 0x04,
    MODBUS_FC_WRITE_SINGLE_COIL         = 0x05,
    MODBUS_FC_WRITE_SINGLE_REGISTER     = 0x06,
    MODBUS_FC_READ_EXCEPTION_STATUS     = 0x07,
    MODBUS_FC_WRITE_MULTIPLE_COILS      = 0x0F,
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS  = 0x10,
    MODBUS_FC_REPORT_SLAVE_ID           = 0x11,
    MODBUS_FC_MASK_WRITE_REGISTER       = 0x16,
    MODBUS_FC_WRITE_AND_READ_REGISTERS  = 0x17,
} modbus_function_code_t;


/* Modbus_Application_Protocol_V1_1b.pdf (chapter 6 section 1 page 12)
 * Quantity of Coils to read (2 bytes): 1 to 2000 (0x7D0)
 * (chapter 6 section 11 page 29)
 * Quantity of Coils to write (2 bytes): 1 to 1968 (0x7B0)
 */
#define MODBUS_MAX_READ_BITS              2000
#define MODBUS_MAX_WRITE_BITS             1968

/* Modbus_Application_Protocol_V1_1b.pdf (chapter 6 section 3 page 15)
 * Quantity of Registers to read (2 bytes): 1 to 125 (0x7D)
 * (chapter 6 section 12 page 31)
 * Quantity of Registers to write (2 bytes) 1 to 123 (0x7B)
 * (chapter 6 section 17 page 38)
 * Quantity of Registers to write in R/W registers (2 bytes) 1 to 121 (0x79)
 */
#define MODBUS_MAX_READ_REGISTERS          125
#define MODBUS_MAX_WRITE_REGISTERS         123
#define MODBUS_MAX_WR_WRITE_REGISTERS      121
#define MODBUS_MAX_WR_READ_REGISTERS       125

/* The size of the MODBUS PDU is limited by the size constraint inherited from
 * the first MODBUS implementation on Serial Line network (max. RS485 ADU = 256
 * bytes). Therefore, MODBUS PDU for serial line communication = 256 - Server
 * address (1 byte) - CRC (2 bytes) = 253 bytes.
 */
#define MODBUS_MAX_PDU_LENGTH              253

/* Consequently:
 * - RTU MODBUS ADU = 253 bytes + Server address (1 byte) + CRC (2 bytes) = 256
 *   bytes.
 * - TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
 * so the maximum of both backend in 260 bytes. This size can used to allocate
 * an array of bytes to store responses and it will be compatible with the two
 * backends.
 */
#define MODBUS_MAX_ADU_LENGTH              260

/* Protocol exceptions */
enum {
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE,
    MODBUS_EXCEPTION_ACKNOWLEDGE,
    MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY,
    MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE,
    MODBUS_EXCEPTION_MEMORY_PARITY,
    MODBUS_EXCEPTION_NOT_DEFINED,
    MODBUS_EXCEPTION_GATEWAY_PATH,
    MODBUS_EXCEPTION_GATEWAY_TARGET,
    MODBUS_EXCEPTION_MAX
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

#pragma pack(1)

/*
    Tabla de control: 4 * uint8_t[0xFFFF+1]     -> 262144
    Tabla de datos:   2 * uint16_t[0xFFFF+1]    -> 262144
    Total por nodo slave: 524288 bytes (1/2 mega)
*/
typedef struct { /* 1 word: 2 bytes */
    struct {
        uint16_t bit_value: 1;  // Valor para las variables bit (nos cabe en la palabra de control)
        uint16_t updated: 1;    // Indica si el valor ha sido actualizado (reseteado cuando se publique)
        uint16_t compound_value: 1;
        uint16_t to_write: 1;

        uint16_t free2: 3;
        uint16_t value_busy: 1;     // Si la celda (bit o word) está ocupada. Tamaño celdas: 0xFFFF
    } control;
    uint16_t input_register;
    uint16_t holding_register;
} cell_control_t;

typedef struct {
//cell_control_t control[4][0xFFFF+1];    // 0x00000 control coil
//                                        // 0x10000 control discrete input
//                                        // 0x20000 control input register
//                                        // 0x30000 control holding register
//uint16_t input_register[0xFFFF+1];      // 0x40000 data input register
//uint16_t holding_register[0xFFFF+0];    // 0x60000 data holding register HACK last addr to slave_id
    json_t *x_control;
    uint16_t slave_id;
} slave_data_t;

typedef struct {
    uint8_t slave_id;
    uint8_t function;
    uint8_t byte_count;
} head_rtu_t;

typedef struct {
    uint16_t t_id;   // transaction id
    uint16_t protocol;
    uint16_t length;
    uint8_t slave_id;
    uint8_t function;
    uint8_t byte_count; // In the echo of a write it is the high byte of the address
} head_tcp_t;

#pragma pack()

typedef struct _FRAME_HEAD {
    // Common head
    unsigned slave_id;
    unsigned function;
    unsigned byte_count;
    int error_code;

    // state of frame
    char busy;              // in half of header
    char header_complete;   // Set True when header is completed

    // must do
    char must_read_payload_data;    // (payload length) bytes
    char discard;                   // not the answer awaited: read whole, then dropped
    char stale;                     // began before the request in flight went out: not its answer

    // information of frame
    size_t payload_length;
} FRAME_HEAD;

typedef enum {
    WAIT_HEAD=0,
    WAIT_PAYLOAD,
} state_t;

/*
 *  The frame being read is forgotten; the function of the request in flight
 *  too (reset_frame() alone keeps it: a frame dropped is not its answer)
 */
#define RESET_MACHINE() \
    reset_frame(gobj);                                          \
    priv->modbus_function = -1;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE const char *get_object_type_name(modbus_object_type_t object_type);
PRIVATE int print_slave_data(hgobj gobj);
PRIVATE slave_data_t *get_slave_data(hgobj gobj, int slave_id, BOOL verbose);
PRIVATE const char *modbus_function_name(int modbus_function);
PRIVATE modbus_object_type_t get_object_type(hgobj gobj, const char *type);
PRIVATE modbus_object_type_t object_type_of(const char *type);
PRIVATE int build_slave_data(hgobj gobj);
PRIVATE int free_slave_data(hgobj gobj);
PRIVATE int load_modbus_config(hgobj gobj);
PRIVATE int load_slave_mapping(hgobj gobj);
PRIVATE BOOL map_is_pollable(hgobj gobj);
PRIVATE int store_modbus_response_data(hgobj gobj, uint8_t *bf, int len);
PRIVATE endian_format_t get_endian_format(hgobj gobj, const char *format);
PRIVATE variable_format_t get_variable_format(hgobj gobj, const char *format);
PRIVATE int variable_cells(hgobj gobj, json_t *jn_variable, variable_format_t variable_format);
PRIVATE BOOL variable_updated(hgobj gobj, slave_data_t *pslv, json_t *jn_variable);
PRIVATE int clear_updated_cells(slave_data_t *pslv);
PRIVATE int build_message_to_publish(hgobj gobj);
PRIVATE int check_conversion_variables(hgobj gobj);
PRIVATE int note_block_failed(hgobj gobj, const char *msg, int error_code);
PRIVATE int note_block_answered(hgobj gobj);
PRIVATE json_t *request_to_log(json_t *jn_request);
PRIVATE json_t *requests_to_log(json_t *jn_requests);
PRIVATE json_t *check_request(hgobj gobj, json_t *jn_request);
PRIVATE int answer_request(
    hgobj gobj,
    json_t *jn_request,
    int result,
    int error_code,
    json_t *jn_values,
    json_t *jn_comment
);
PRIVATE int set_current_answer(
    hgobj gobj,
    int result,
    int error_code,
    json_t *jn_values,
    json_t *jn_comment
);
PRIVATE int answer_current_request(hgobj gobj);
PRIVATE int answer_queued_requests(hgobj gobj, const char *why);
PRIVATE int read_request_answer(hgobj gobj, uint8_t *bf, int len);
PRIVATE int continue_polling(hgobj gobj, int wait_ms);
PRIVATE int reset_frame(hgobj gobj);
PRIVATE BOOL write_in_flight(hgobj gobj);
PRIVATE BOOL frame_is_hidden(hgobj gobj);
PRIVATE int drop_bytes_after_answer(hgobj gobj, gbuffer_t *gbuf, BOOL hidden);
PRIVATE int finish_answer(hgobj gobj);

PRIVATE cell_control_t *get_cell_control(
    hgobj gobj,
    slave_data_t *pslv,
    modbus_object_type_t object_type,
    int32_t address,
    BOOL create
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/* Table of CRC values for high-order byte */
static const uint8_t table_crc_hi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
static const uint8_t table_crc_lo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dump_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_poll_timeout(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE const sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_dump_data[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "slave_id",     0,              0,          "Slave id (-1 all slaves)"),
SDATAPM (DTP_JSON,      "address",      0,              0,          "Address: a number, or its text"),
SDATAPM (DTP_JSON,      "size",         0,              0,          "Size (-1 all data): a number, or its text"),
SDATA_END()
};
PRIVATE const sdata_desc_t pm_timeout[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "timeout",      0,              "1000",      "Polling timeout in milliseconds"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE const sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "dump_data",        0,          pm_dump_data,   cmd_dump_data,  "Dump slave data"),
SDATACM (DTP_SCHEMA,    "set-poll-timeout", 0,          pm_timeout,     cmd_set_poll_timeout, "Set polling timeout (in milliseconds)"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "modbus_protocol",  SDF_RD,         "TCP",      "Modbus protocol: TCP,RTU,ASCII"),
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url or device"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certificate, PEM format"),
SDATA (DTP_JSON,    "slaves",           SDF_WR,         "[]",       "Modbus configuration"),
SDATA (DTP_INTEGER, "timeout_polling",  SDF_PERSIST,    "1000",     "Polling modbus time in milliseconds"),
SDATA (DTP_INTEGER, "timeout_response", SDF_PERSIST,    "10",       "Timeout response in seconds"),
SDATA (DTP_POINTER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
    TRACE_TRAFFIC   = 0x0002,
    TRACE_POLLING   = 0x0004,
    TRACE_DECODE    = 0x0008,
    TRACE_SEND      = 0x0010,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{"traffic",         "Trace dump traffic"},
{"polling",         "Trace polling"},
{"decode",          "Trace decoding"},
{"send",            "Trace send"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int timeout_polling;
    int timeout_response;
    const char *url;
    hgobj gobj_timer;
    const char *modbus_protocol;

    json_t *slaves_;
    int idx_slaves;
    int max_slaves;
    json_t *cur_slave_;

    json_t *mapping_;
    int idx_mapping;
    int max_mapping;
    json_t *cur_map_;

    /* Extract from MODBUS Messaging on TCP/IP Implementation Guide V1.0b
       (page 23/46):
       The transaction identifier is used to associate the future response
       with the request. This identifier is unique on each TCP connection. */
    uint16_t t_id;

    slave_data_t *slave_data;

    BOOL inform_on_close;

    FRAME_HEAD frame_head;
    istream_h istream_head;
    istream_h istream_payload;
    state_t st;
    int modbus_function;
    int slave_id_sent;          // RTU has no t_id: the answer must come from the slave asked

    json_t *jn_current_request;
    json_t *jn_current_answer;  // what the answer of jn_current_request said, while it is parsed
    BOOL waiting;               // the timer counts the wait between cycles: a request can go at once
    int resume_wait_ms;         // requests went out instead of that wait: it comes after them
    json_t *jn_request_queue;

    json_t *jn_failing_blocks;  // "<slave id>:<block index>" -> why it fails
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(modbus_protocol,       gobj_read_str_attr)
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(timeout_polling,       (int)gobj_read_integer_attr)
    SET_PRIV(timeout_response,      (int)gobj_read_integer_attr)

}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_polling,         (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_response,      (int)gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(url,                   gobj_read_str_attr)
        if(gobj_bottom_gobj(gobj)) {
            gobj_write_str_attr(gobj_bottom_gobj(gobj), "url", priv->url);
        }
    END_EQ_SET_PRIV()

    if(strcmp(path, "slaves")==0 && gobj_is_running(gobj)) {
        /*
         *  The map is read at start: the running cycle keeps its own
         *  reference to the one it started with (load_modbus_config())
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Modbus slaves written while the master runs: they take effect at the next start",
            NULL
        );
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->jn_request_queue = json_array();
    priv->jn_failing_blocks = json_object();

    load_modbus_config(gobj);
    build_slave_data(gobj);
    check_conversion_variables(gobj);

    if(gobj_trace_level(gobj)) {
        gobj_trace_json(gobj, priv->slaves_, "slaves_ max: %d", priv->max_slaves);
        gobj_trace_json(gobj, priv->mapping_, "mapping_ max: %d", priv->max_mapping);
        print_slave_data(gobj);
    }

    SWITCHS(priv->modbus_protocol) {
        CASES("TCP")
            priv->istream_head = istream_create(
                gobj,
                sizeof(head_tcp_t),
                sizeof(head_tcp_t)
            );
            if(!priv->istream_head) {
                gobj_log_critical(0,0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "istream_create(head) FAILED",
                    NULL
                );
            }
            break;

        CASES("RTU")
        CASES("ASCII")
            priv->istream_head = istream_create(
                gobj,
                sizeof(head_rtu_t),
                sizeof(head_rtu_t)
            );
            if(!priv->istream_head) {
                gobj_log_critical(0,0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "istream_create(head) FAILED",
                    NULL
                );
            }
            break;

        DEFAULTS
            break;
    } SWITCHS_END;

    /*
     *  The bottom must be a C_TCP or C_UART (it has manual start/stop!).
     *  If it's a client then start to begin the connection.
     */
    {
        /*
         *  Client side
         */
        hgobj bottom_gobj = gobj_bottom_gobj(gobj);
        if(!bottom_gobj) {
            json_t *kw = json_pack("{s:s, s:s}",
                "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
                "url", gobj_read_str_attr(gobj, "url")
            );

#ifdef ESP_PLATFORM
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
#endif
#ifdef __linux__
            bottom_gobj = gobj_create_pure_child(
                gobj_name(gobj),
                strcasecmp(priv->modbus_protocol, "TCP")==0? C_TCP: C_UART,
                kw,
                gobj
            );
#endif
            gobj_set_bottom_gobj(gobj, bottom_gobj);
            //gobj_write_str_attr(bottom_gobj, "tx_ready_event_name", 0);
        }

        if(!empty_string(priv->url)) {
            gobj_write_str_attr(gobj_bottom_gobj(gobj), "url", priv->url);
        }

        gobj_start(bottom_gobj);
    }



    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    free_slave_data(gobj);
    JSON_DECREF(priv->slaves_)
    priv->cur_slave_ = NULL;    // they pointed into slaves_
    priv->mapping_ = NULL;
    priv->cur_map_ = NULL;

    RESET_MACHINE();
    ISTREAM_DESTROY(priv->istream_head);

    clear_timeout(priv->gobj_timer);

    if(priv->jn_current_request) {
        json_t *jn_log = request_to_log(priv->jn_current_request);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Modbus request NOT confirmed, the gobj stopped before its answer",
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
    }
    if(json_array_size(priv->jn_request_queue) > 0) {
        json_t *jn_log = requests_to_log(priv->jn_request_queue);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Modbus requests NOT sent, the gobj stopped",
            "requests",     "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
    }
    JSON_DECREF(priv->jn_request_queue);
    JSON_DECREF(priv->jn_current_request);
    JSON_DECREF(priv->jn_current_answer);
    JSON_DECREF(priv->jn_failing_blocks);

    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(bottom_gobj) {
        gobj_stop(bottom_gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_dump_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int slave_id = (int)kw_get_int(gobj, kw, "slave_id", -1, KW_WILD_NUMBER);  // -1 all slaves
    int address = (int)kw_get_int(gobj, kw, "address", 0, KW_WILD_NUMBER);
    int size = (int)kw_get_int(gobj, kw, "size", 0, KW_WILD_NUMBER);           // -1 all data

    if(address > 0xFFFF) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Address out of range"),
            0,
            0,
            kw  // owned
        );
    }

    if(slave_id == -1 && size == -1) {
        gobj_trace_dump(
            gobj,
            (const char *)priv->slave_data,
            priv->max_slaves * sizeof(slave_data_t),
            "slaves"
        );
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("FULL data dumped in log file"),
            0,
            0,
            kw  // owned
        );
    }

    if(size == -1) {
        size = 0xFFFF + 1;
    }
    if(address+size > 0xFFFF+1) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Size out of range"),
            0,
            0,
            kw  // owned
        );
    }

    if(slave_id != -1) {
        slave_data_t *pslv = get_slave_data(gobj, slave_id, FALSE);
        if(!pslv) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Slave id not found"),
                0,
                0,
                kw  // owned
            );
        }

        gobj_trace_dump(gobj, (const char *)pslv + 0x00000 + address, size,
            "%d: Control Coil", slave_id
        );
        gobj_trace_dump(gobj, (const char *)pslv + 0x10000 + address, size,
            "%d: Control Discrete input", slave_id
        );
        gobj_trace_dump(gobj, (const char *)pslv + 0x20000 + address, size,
            "%d: Control Input register", slave_id
        );
        gobj_trace_dump(gobj, (const char *)pslv + 0x30000 + address, size,
            "%d: Control Holding register", slave_id
        );
        gobj_trace_dump(gobj, (const char *)pslv + 0x40000 + address*2, size*2,
            "%d: Data Input register", slave_id
        );
        gobj_trace_dump(gobj, (const char *)pslv + 0x60000 + address*2, size*2,
            "%d: Data Holding register", slave_id
        );

    } else {
        slave_data_t *pslv = priv->slave_data;
        for(int i=0; i<priv->max_slaves; i++) {
            slave_id = pslv->slave_id;
            gobj_trace_dump(gobj, (const char *)pslv + 0x00000 + address, size,
                "%d: Control Coil", slave_id
            );
            gobj_trace_dump(gobj, (const char *)pslv + 0x10000 + address, size,
                "%d: Control Discrete input", slave_id
            );
            gobj_trace_dump(gobj, (const char *)pslv + 0x20000 + address, size,
                "%d: Control Input register", slave_id
            );
            gobj_trace_dump(gobj, (const char *)pslv + 0x30000 + address, size,
                "%d: Control Holding register", slave_id
            );
            gobj_trace_dump(gobj, (const char *)pslv + 0x40000 + address*2, size*2,
                "%d: Data Input register", slave_id
            );
            gobj_trace_dump(gobj, (const char *)pslv + 0x60000 + address*2, size*2,
                "%d: Data Holding register", slave_id
            );
            // Next slave
            pslv++;
        }
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Data dumped in log file"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_poll_timeout(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    int timeout = kw_get_int(gobj, kw, "timeout", 1000, KW_WILD_NUMBER);

    gobj_write_integer_attr(gobj, "timeout_polling", timeout);
    gobj_save_persistent_attrs(gobj, json_string("timeout_polling"));

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Poll timeout = %d milliseconds", timeout),
        0,
        0,
        kw  // owned
    );
}




                    /***************************
                     *      Modbus
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *modbus_function_name(int modbus_function)
{
    switch(modbus_function) {
        case MODBUS_FC_READ_COILS:
            return "READ_COILS";
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            return "READ_DISCRETE_INPUTS";
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            return "READ_HOLDING_REGISTERS";
        case MODBUS_FC_READ_INPUT_REGISTERS:
            return "READ_INPUT_REGISTERS";
        case MODBUS_FC_WRITE_SINGLE_COIL:
            return "WRITE_SINGLE_COIL";
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            return "WRITE_SINGLE_REGISTER";
        case MODBUS_FC_READ_EXCEPTION_STATUS:
            return "READ_EXCEPTION_STATUS";
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            return "WRITE_MULTIPLE_COILS";
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return "WRITE_MULTIPLE_REGISTERS";
        case MODBUS_FC_REPORT_SLAVE_ID:
            return "REPORT_SLAVE_ID";
        case MODBUS_FC_MASK_WRITE_REGISTER:
            return "MASK_WRITE_REGISTER";
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            return "WRITE_AND_READ_REGISTERS";
        default:
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Modbus function UNKNOWN",
                "function",     "%d", modbus_function,
                NULL
            );
            return "???";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *modbus_exception_name(int exception_name)
{
    switch(exception_name) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
            return "ILLEGAL_FUNCTION";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
            return "ILLEGAL_DATA_ADDRESS";
        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
            return "ILLEGAL_DATA_VALUE";
        case MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE:
            return "SLAVE_OR_SERVER_FAILURE";
        case MODBUS_EXCEPTION_ACKNOWLEDGE:
            return "ACKNOWLEDGE";
        case MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY:
            return "SLAVE_OR_SERVER_BUSY";
        case MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE:
            return "NEGATIVE_ACKNOWLEDGE";
        case MODBUS_EXCEPTION_MEMORY_PARITY:
            return "MEMORY_PARITY";
        case MODBUS_EXCEPTION_NOT_DEFINED:
            return "NOT_DEFINED";
        case MODBUS_EXCEPTION_GATEWAY_PATH:
            return "GATEWAY_PATH";
        case MODBUS_EXCEPTION_GATEWAY_TARGET:
            return "GATEWAY_TARGET";
        default:
            return "???";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_data(hgobj gobj, gbuffer_t *gbuf)
{
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ==> %s",
            gobj_short_name(gobj),
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_send, gobj);
}

/***************************************************************************
 *  Calculate crc for tx messages
 ***************************************************************************/
PRIVATE uint16_t crc16_tx(uint8_t *buffer, uint16_t buffer_length)
{
    uint8_t crc_hi = 0xFF; /* high CRC byte initialized */
    uint8_t crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i; /* will index into CRC lookup */

    /* pass through message buffer */
    while (buffer_length--) {
        i = crc_hi ^ *buffer++; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    return (crc_hi << 8 | crc_lo);
}

/***************************************************************************
 *  Calculate crc for rx messages
 ***************************************************************************/
PRIVATE uint16_t crc16_rx(FRAME_HEAD *frame, uint8_t *buffer, uint16_t buffer_length)
{
    uint8_t crc_hi = 0xFF; /* high CRC byte initialized */
    uint8_t crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i; /* will index into CRC lookup */

    {
        i = crc_hi ^ frame->slave_id; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    {
        i = crc_hi ^ frame->function; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    {
        i = crc_hi ^ frame->byte_count; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    /* pass through message buffer */
    while (buffer_length--) {
        i = crc_hi ^ *buffer++; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    return (crc_hi << 8 | crc_lo);
}

/***************************************************************************
 * - HEADER_LENGTH_TCP (7) + function (1) + address (2) + number (2)
 * - HEADER_LENGTH_RTU (1) + function (1) + address (2) + number (2) + CRC (2)
 *
 *
 * <------------------------ MODBUS TCP/IP ADU(1) ------------------------->
 *              <----------- MODBUS PDU (1') ---------------->
 *  +-----------+---------------+------------------------------------------+
 *  | TID | PID | Length | UID  |Code | Data                               |
 *  +-----------+---------------+------------------------------------------+
 *  |     |     |        |      |
 * (2)   (3)   (4)      (5)    (6)
 *
 * (2)  ... MB_TCP_TID          = 0 (Transaction Identifier - 2 Byte)
 * (3)  ... MB_TCP_PID          = 2 (Protocol Identifier - 2 Byte)
 * (4)  ... MB_TCP_LEN          = 4 (Number of bytes - 2 Byte)
 * (5)  ... MB_TCP_UID          = 6 (Unit Identifier - 1 Byte)
 * (6)  ... MB_TCP_FUNC         = 7 (Modbus Function Code)
 *
 * (1)  ... Modbus TCP/IP Application Data Unit
 * (1') ... Modbus Protocol Data Unit
 ***************************************************************************/
PRIVATE gbuffer_t *build_modbus_request_read_message(hgobj gobj, json_t *jn_slave, json_t *jn_map)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj); // WARNING must be used only for t_id

    uint8_t req[12] = {0};
    uint8_t slave_id = (uint8_t)kw_get_int(gobj, jn_slave, "id", 0, KW_REQUIRED);
    uint16_t address = (uint16_t)kw_get_int(gobj, jn_map, "address", 0, KW_REQUIRED|KW_WILD_NUMBER);
    uint16_t size = (uint16_t)kw_get_int(gobj, jn_map, "size", 0, KW_REQUIRED|KW_WILD_NUMBER);
    const char *id = kw_get_str(gobj, jn_map, "id", "", 0);
    const char *type = kw_get_str(gobj, jn_map, "type", "", KW_REQUIRED);
    modbus_object_type_t object_type = get_object_type(gobj, type);

    uint8_t modbus_function = 0;
    switch(object_type) {
        case TYPE_COIL:
            modbus_function = MODBUS_FC_READ_COILS;
            if(size > MODBUS_MAX_READ_BITS) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus Too many coils requested",
                    "type",         "%s", type,
                    "size",         "%d", size,
                    NULL
                );
                size = MODBUS_MAX_READ_BITS;
            }

            break;
        case TYPE_DISCRETE_INPUT:
            modbus_function = MODBUS_FC_READ_DISCRETE_INPUTS;
            if(size > MODBUS_MAX_READ_BITS) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus Too many discrete inputs requested",
                    "size",         "%d", size,
                    NULL
                );
                size = MODBUS_MAX_READ_BITS;
            }
            break;

        case TYPE_INPUT_REGISTER:
            modbus_function = MODBUS_FC_READ_INPUT_REGISTERS;
            if(size > MODBUS_MAX_READ_REGISTERS) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus Too many discrete inputs requested",
                    "size",         "%d", size,
                    NULL
                );
                size = MODBUS_MAX_READ_REGISTERS;
            }
            break;
        case TYPE_HOLDING_REGISTER:
            modbus_function = MODBUS_FC_READ_HOLDING_REGISTERS;
            if(size > MODBUS_MAX_READ_REGISTERS) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus Too many discrete inputs requested",
                    "size",         "%d", size,
                    NULL
                );
                size = MODBUS_MAX_READ_REGISTERS;
            }
            break;
        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Modbus object type UNKNOWN",
                "type",         "%s", type,
                NULL
            );
            return 0;
    }

    gbuffer_t *gbuf = gbuffer_create(32, 32);

    SWITCHS(priv->modbus_protocol) {
        CASES("TCP")
            /* Increase transaction ID */
            if (priv->t_id < UINT16_MAX)
                priv->t_id++;
            else
                priv->t_id = 0;

            req[0] = priv->t_id >> 8;
            req[1] = priv->t_id & 0x00ff;

            /* Protocol Modbus */
            req[2] = 0;
            req[3] = 0;

            /* Subtract the header length to the message length */
            int mbap_length = 12 - 6;

            req[4] = mbap_length >> 8;
            req[5] = mbap_length & 0x00FF;
            req[6] = slave_id;
            req[7] = modbus_function;
            req[8] = address >> 8;
            req[9] = address & 0x00ff;
            req[10] = size >> 8;
            req[11] = size & 0x00ff;
            gbuffer_append(gbuf, req, 12);
            break;

        CASES("RTU")
            req[0] = slave_id;
            req[1] = modbus_function;
            req[2] = address >> 8;
            req[3] = address & 0x00ff;
            req[4] = size >> 8;
            req[5] = size & 0x00ff;

            uint16_t crc = crc16_tx(req, 6);
            req[6] = crc >> 8;
            req[7] = crc & 0x00FF;
            gbuffer_append(gbuf, req, 8);
            break;

        CASES("ASCII")
        DEFAULTS
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Modbus protocol NOT IMPLEMENTED",
                "protocol",     "%s", priv->modbus_protocol,
                NULL
            );
            break;
    } SWITCHS_END;

    priv->modbus_function = modbus_function;
    priv->slave_id_sent = slave_id;

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_msg(gobj, "🍅🍅⏩ func: %d %s, slave_id: %d, addr: %d (0x%04X), size: %d, id: %s",
            modbus_function,
            modbus_function_name(modbus_function),
            slave_id,
            address,
            address,
            size,
            id // ??? TODO revisa qué es
        );
    }

    return gbuf;
}

/***************************************************************************
 *  The frame of a request, a write or a read, already checked and filled
 *  in by check_request(). The PDU is the same over TCP and RTU; only its
 *  envelope changes.
 ***************************************************************************/
PRIVATE gbuffer_t *build_modbus_request_message(hgobj gobj, json_t *jn_request)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *operation = kw_get_str(gobj, jn_request, "operation", "", KW_REQUIRED);
    BOOL is_write = strcmp(operation, "write")==0? TRUE : FALSE;
    uint8_t slave_id = (uint8_t)kw_get_int(gobj, jn_request, "id", 1, KW_REQUIRED);
    uint16_t address = (uint16_t)kw_get_int(gobj, jn_request, "address", 0, KW_REQUIRED);
    uint16_t size = (uint16_t)kw_get_int(gobj, jn_request, "size", 0, KW_REQUIRED);
    const char *type = kw_get_str(gobj, jn_request, "type", "", KW_REQUIRED);
    modbus_object_type_t object_type = get_object_type(gobj, type);
    json_t *jn_values = is_write? kw_get_list(gobj, jn_request, "value", 0, KW_REQUIRED) : NULL;

    BOOL tcp = strcasecmp(priv->modbus_protocol, "TCP")==0? TRUE : FALSE;
    if(!tcp && strcasecmp(priv->modbus_protocol, "RTU")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Modbus request NOT IMPLEMENTED for this protocol",
            "protocol",     "%s", priv->modbus_protocol,
            NULL
        );
        return 0;
    }
    if(is_write && (!jn_values || (int)json_array_size(jn_values) != size)) {
        json_t *jn_log = request_to_log(jn_request);
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Modbus write NOT checked before framing it",
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        return 0;
    }

    uint8_t frame[MODBUS_MAX_ADU_LENGTH];
    memset(frame, 0, sizeof(frame));
    uint8_t *pdu = frame + (tcp? 7 : 1);   // after the MBAP header, or the slave id
    int n = 0;
    uint8_t modbus_function = 0;

    if(!is_write) {
        switch(object_type) {
            case TYPE_COIL:
                modbus_function = MODBUS_FC_READ_COILS;
                break;
            case TYPE_DISCRETE_INPUT:
                modbus_function = MODBUS_FC_READ_DISCRETE_INPUTS;
                break;
            case TYPE_INPUT_REGISTER:
                modbus_function = MODBUS_FC_READ_INPUT_REGISTERS;
                break;
            case TYPE_HOLDING_REGISTER:
                modbus_function = MODBUS_FC_READ_HOLDING_REGISTERS;
                break;
            default:
                // Error already logged
                return 0;
        }
        pdu[n++] = modbus_function;
        pdu[n++] = (uint8_t)(address >> 8);
        pdu[n++] = (uint8_t)(address & 0xFF);
        pdu[n++] = (uint8_t)(size >> 8);
        pdu[n++] = (uint8_t)(size & 0xFF);

    } else if(object_type == TYPE_COIL) {
        modbus_function = size == 1? MODBUS_FC_WRITE_SINGLE_COIL : MODBUS_FC_WRITE_MULTIPLE_COILS;
        pdu[n++] = modbus_function;
        pdu[n++] = (uint8_t)(address >> 8);
        pdu[n++] = (uint8_t)(address & 0xFF);
        if(size == 1) {
            // A single coil is ON with 0xFF00, OFF with 0x0000: any value but 0 is ON
            pdu[n++] = json_integer_value(json_array_get(jn_values, 0))? 0xFF : 0x00;
            pdu[n++] = 0x00;
        } else {
            int byte_count = (size + 7) / 8;
            pdu[n++] = (uint8_t)(size >> 8);
            pdu[n++] = (uint8_t)(size & 0xFF);
            pdu[n++] = (uint8_t)byte_count;
            for(int i=0; i<size; i++) {
                if(json_integer_value(json_array_get(jn_values, (size_t)i))) {
                    pdu[n + i/8] |= (uint8_t)(1 << (i%8));
                }
            }
            n += byte_count;
        }

    } else if(object_type == TYPE_HOLDING_REGISTER) {
        modbus_function = size == 1?
            MODBUS_FC_WRITE_SINGLE_REGISTER : MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
        pdu[n++] = modbus_function;
        pdu[n++] = (uint8_t)(address >> 8);
        pdu[n++] = (uint8_t)(address & 0xFF);
        if(size > 1) {
            pdu[n++] = (uint8_t)(size >> 8);
            pdu[n++] = (uint8_t)(size & 0xFF);
            pdu[n++] = (uint8_t)(2*size);
        }
        for(int i=0; i<size; i++) {
            uint16_t value = (uint16_t)json_integer_value(json_array_get(jn_values, (size_t)i));
            pdu[n++] = (uint8_t)(value >> 8);
            pdu[n++] = (uint8_t)(value & 0xFF);
        }

    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Modbus object type NOT SUPPORTED to write",
            "type",         "%s", type,
            NULL
        );
        return 0;
    }

    int len;
    if(tcp) {
        /* Increase transaction ID */
        if(priv->t_id < UINT16_MAX) {
            priv->t_id++;
        } else {
            priv->t_id = 0;
        }
        frame[0] = (uint8_t)(priv->t_id >> 8);
        frame[1] = (uint8_t)(priv->t_id & 0xFF);
        frame[2] = 0;   // Protocol Modbus
        frame[3] = 0;
        frame[4] = (uint8_t)((n + 1) >> 8);     // The length counts the slave id and the PDU
        frame[5] = (uint8_t)((n + 1) & 0xFF);
        frame[6] = slave_id;
        len = 7 + n;
    } else {
        frame[0] = slave_id;
        uint16_t crc = crc16_tx(frame, (uint16_t)(1 + n));
        frame[1 + n] = (uint8_t)(crc >> 8);
        frame[2 + n] = (uint8_t)(crc & 0xFF);
        len = 3 + n;
    }

    gbuffer_t *gbuf = gbuffer_create((size_t)len, (size_t)len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_create() FAILED",
            NULL
        );
        return 0;
    }
    gbuffer_append(gbuf, frame, (size_t)len);
    if(is_write) {
        /*
         *  A write can carry a secret (a meter password): its frame is
         *  dumped as "<N bytes hidden>", here and by the transport
         */
        gbuffer_set_secret(gbuf, TRUE);
    }

    priv->modbus_function = modbus_function;
    priv->slave_id_sent = slave_id;

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_msg(gobj, "🍅🍅⏩ func: %d %s, slave_id: %d, addr: %d (0x%04X), size: %d (request)",
            modbus_function,
            modbus_function_name(modbus_function),
            slave_id,
            address,
            address,
            size
        );
    }

    return gbuf;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Gather 2*words bytes from consecutive registers starting at address.
 ***************************************************************************/
PRIVATE int collect_register_bytes(
    hgobj gobj,
    slave_data_t *pslv,
    modbus_object_type_t ot,
    int address,
    int words,
    uint8_t *out /* >= 2*words */
) {
    for(int i=0; i<words; i++) {
        cell_control_t *c = get_cell_control(gobj, pslv, ot, address+i, FALSE);
        if(!c) { return -1; }
        uint8_t *p = 0;
        switch(ot) {
            case TYPE_INPUT_REGISTER:
                p = (uint8_t *)&c->input_register;
                break;

            case TYPE_HOLDING_REGISTER:
                p = (uint8_t *)&c->holding_register;
                break;

            default:
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "type NOT implemented or UNKNOWN",
                    "type",         "%s", get_object_type_name(ot),
                    NULL
                );
            return -1;
        }
        /* store_modbus_response_data() kept wire order: p[0]=MSB, p[1]=LSB */
        out[(i<<1)+0] = p[0];
        out[(i<<1)+1] = p[1];
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_object_type_name(modbus_object_type_t object_type)
{
    switch(object_type) {
        case TYPE_COIL:
            return "coil";
        case TYPE_DISCRETE_INPUT:
            return "discrete_input";
        case TYPE_INPUT_REGISTER:
            return "input_register";
        case TYPE_HOLDING_REGISTER:
            return "holding_register";

        case TYPE_UNKNOWN:
        default:
            return NULL;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE modbus_object_type_t get_object_type(hgobj gobj, const char *type)
{
    modbus_object_type_t object_type = object_type_of(type);
    if(object_type == TYPE_UNKNOWN) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Modbus object type UNKNOWN",
            "type",         "%s", type,
            NULL
        );
    }
    return object_type;
}

/***************************************************************************
 *  The object type of its name, with no log: TYPE_UNKNOWN when it is none.
 *  A request says its type, and its refusal is its answer.
 ***************************************************************************/
PRIVATE modbus_object_type_t object_type_of(const char *type_)
{
    char type[64];
    snprintf(type, sizeof(type), "%s", type_? type_ : "");
    strntolower(type, strlen(type));

    modbus_object_type_t object_type = TYPE_UNKNOWN;
    SWITCHS(type) {
        CASES("coil")
        CASES("coil_status")
            object_type = TYPE_COIL;
            break;
        CASES("discrete input")
        CASES("input_status")
            object_type = TYPE_DISCRETE_INPUT;
            break;
        CASES("input register")
        CASES("input_register")
            object_type = TYPE_INPUT_REGISTER;
            break;
        CASES("holding register")
        CASES("holding_register")
            object_type = TYPE_HOLDING_REGISTER;
            break;
        DEFAULTS
            object_type = TYPE_UNKNOWN;
    } SWITCHS_END;

    return object_type;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_slave_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->slaves_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "slaves_ NULL",
            NULL
        );
        return -1;
    }
    if(!priv->max_slaves) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "NO slave defined",
            NULL
        );
        return -1;
    }

    /*
     *  Alloc memory
     */
    int array_size = priv->max_slaves * sizeof(slave_data_t);
    priv->slave_data = GBMEM_MALLOC(array_size);
    if(!priv->slave_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for slave_data",
            "size",         "%d", array_size,
            NULL
        );
        return -1;
    }
    char temp[128];
    char nice[64];
    nice_size(nice, sizeof(nice), (uint64_t)array_size, FALSE);
    snprintf(temp, sizeof(temp), "Allocating Modbus Array of %s (%d) bytes, %d slaves",
        nice,
        array_size,
        priv->max_slaves
    );
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", temp,
        NULL
    );

    /*
     *  Fill data
     */
    slave_data_t *pslv = priv->slave_data;
    size_t idx_slaves; json_t *jn_slave;
    json_array_foreach(priv->slaves_, idx_slaves, jn_slave) {
        int slave_id = (int)kw_get_int(gobj, jn_slave, "id", 0, KW_REQUIRED);

        /*
         *  A slave that cannot be asked is not polled: its blocks are
         *  disabled. Over RTU 248-255 are reserved, and 0 is the
         *  broadcast, which no slave answers: polled, it would wait
         *  timeout_response in every cycle.
         */
        const char *refused = NULL;
        if(slave_id < 0 || slave_id > 255) {
            refused = "Modbus slave id OUT OF RANGE";
        } else if(slave_id > 247 && strcasecmp(priv->modbus_protocol, "RTU")==0) {
            refused = "Modbus slave id RESERVED in RTU";
        } else if(slave_id == 0 && strcasecmp(priv->modbus_protocol, "RTU")==0) {
            refused = "Modbus slave id 0 is the BROADCAST in RTU: no slave answers it";
        }
        if(refused) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", refused,
                "slave_id",     "%d", slave_id,
                "valid",        "%s", strcasecmp(priv->modbus_protocol, "RTU")==0? "1-247" : "0-255",
                NULL
            );
            json_object_set_new(jn_slave, "disabled", json_true());
        }
        pslv->slave_id = (uint16_t)slave_id;
        pslv->x_control = json_object();

        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_COIL), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_DISCRETE_INPUT), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_INPUT_REGISTER), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_HOLDING_REGISTER), json_object());

        json_t *jn_mapping = kw_get_list(gobj, jn_slave, "mapping", 0, KW_REQUIRED);
        if(!refused && json_array_size(jn_mapping) == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Modbus slave without blocks: not polled",
                "slave_id",     "%d", slave_id,
                NULL
            );
        }
        size_t idx_map; json_t *jn_map;
        json_array_foreach(jn_mapping, idx_map, jn_map) {
            if(refused) {
                // Error already logged
                json_object_set_new(jn_map, "disabled", json_true());
                continue;
            }
            const char *type = kw_get_str(gobj, jn_map, "type", "", KW_REQUIRED);
            modbus_object_type_t object_type = get_object_type(gobj, type);
            if(object_type < 0) {
                json_object_set_new(jn_map, "disabled", json_true());
                continue;
            };

            int32_t address = (int32_t)kw_get_int(gobj, jn_map, "address", -1, KW_REQUIRED|KW_WILD_NUMBER);
            int32_t size = (int32_t)kw_get_int(gobj, jn_map, "size", -1, KW_REQUIRED|KW_WILD_NUMBER);

            if(address < 0 || address > 0xFFFF) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus object address OUT OF RANGE",
                    "type",         "%s", type,
                    "object_type",  "%d", object_type,
                    "address",      "%d", address,
                    "map",          "%j", jn_map,
                    NULL
                );
                json_object_set_new(jn_map, "disabled", json_true());
                continue;
            }
            if(size < 0 || size > 0xFFFF) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus object size OUT OF RANGE",
                    "type",         "%s", type,
                    "object_type",  "%d", object_type,
                    "address",      "%d", address,
                    "size",         "%d", size,
                    "map",          "%j", jn_map,
                    NULL
                );
                json_object_set_new(jn_map, "disabled", json_true());
                continue;
            }

            if((address + size) >= 0xFFFF) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "Modbus object data OUT OF RANGE",
                    "type",         "%s", type,
                    "object_type",  "%d", object_type,
                    "address",      "%d", address,
                    "size",         "%d", size,
                    "map",          "%j", jn_map,
                    NULL
                );
                json_object_set_new(jn_map, "disabled", json_true());
                continue;
            }

            for(int i=0; i<size; i++) {
                cell_control_t *cell_control = get_cell_control(
                    gobj,
                    pslv,
                    object_type,
                    address+i,
                    TRUE
                );
                if(!cell_control) {
                    // Error already logged
                    continue;
                }
                if(cell_control->control.value_busy) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "Map OVERRIDE",
                        "type",         "%s", type,
                        "object_type",  "%d", object_type,
                        "address",      "%d", address,
                        "size",         "%d", size,
                        "i",            "%d", i,
                        "map",          "%j", jn_map,
                        NULL
                    );
                    json_object_set_new(jn_map, "disabled", json_true());
                    break;
                }
                cell_control->control.value_busy = 1;
                cell_control->input_register = 0;
                cell_control->holding_register = 0;
            }
        }

        // Next slave
        pslv++;
    }

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Data filled",
        NULL
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int free_slave_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    slave_data_t *pslv = priv->slave_data;
    if(!pslv) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "slave data NULL",
            NULL
        );
        return 0;
    }
    for(int i=0; i<priv->max_slaves; i++) {
        const char *key; json_t *jn_v;
        json_object_foreach(pslv->x_control, key, jn_v) {
            const char *key2; json_t *jn_v2;
            json_object_foreach(jn_v, key2, jn_v2) {
                cell_control_t *cell_control = (cell_control_t *)(uintptr_t)json_integer_value(jn_v2);
                if(cell_control) {
                    GBMEM_FREE(cell_control);
                }
            }
        }

        JSON_DECREF(pslv->x_control)
        pslv++;
    }

    GBMEM_FREE(priv->slave_data);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int print_slave_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    slave_data_t *pslv = priv->slave_data;
    if(!pslv) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "slave data NULL",
            NULL
        );
        return -1;
    }
    for(int i=0; i<priv->max_slaves; i++) {
        // Next slave
        gobj_trace_json(gobj, pslv->x_control, "x_control of slave data %d", pslv->slave_id);
        const char *key; json_t *jn_v;
        json_object_foreach(pslv->x_control, key, jn_v) {
            const char *key2; json_t *jn_v2;
            json_object_foreach(jn_v, key2, jn_v2) {
                cell_control_t *cell_control = (cell_control_t *)(uintptr_t)json_integer_value(jn_v2);
                trace_msg0("slave %d, '%s', '%s':", pslv->slave_id, key, key2);
                if(cell_control) {
                    trace_msg0("    input_register: 0x%02X", cell_control->input_register);
                    trace_msg0("    holding_register: 0x%02X", cell_control->holding_register);
                    trace_msg0("    control.bit_value: %d", cell_control->control.bit_value);
                    trace_msg0("    control.updated: %d", cell_control->control.updated);
                    trace_msg0("    control.compound_value: %d", cell_control->control.compound_value);
                    trace_msg0("    control.to_write: %d", cell_control->control.to_write);
                    trace_msg0("    control.value_busy: %d", cell_control->control.value_busy);
                }
            }
        }
        pslv++;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int load_modbus_config(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Its own reference: `slaves` is writable, and a write while it runs
     *  would free the map that the cycle, its blocks and its cells point
     *  into. Released at stop.
     */
    JSON_DECREF(priv->slaves_)
    priv->slaves_ = json_incref(gobj_read_json_attr(gobj, "slaves"));
    priv->idx_slaves = 0;
    priv->max_slaves = json_array_size(priv->slaves_);

    if(priv->max_slaves > 0) {
        load_slave_mapping(gobj);
    }

    return 0;
}

/***************************************************************************
 *  The blocks of the slave idx_slaves. A slave without blocks was reported
 *  at start (build_slave_data()), and next_map() skips it: nothing to log
 *  here, on every cycle.
 ***************************************************************************/
PRIVATE int load_slave_mapping(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cur_slave_ = json_array_get(priv->slaves_, priv->idx_slaves);
    if(!priv->cur_slave_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cur_slave_ NULL",
            "idx_slaves",   "%d", priv->idx_slaves,
            "slaves_",      "%j", priv->slaves_?priv->slaves_:json_null(),
            NULL
        );
        priv->idx_mapping = 0;
        priv->max_mapping = 0;
        return -1;
    }
    priv->mapping_ = kw_get_list(gobj, priv->cur_slave_, "mapping", 0, 0);
    priv->idx_mapping = -1;
    priv->max_mapping = (int)json_array_size(priv->mapping_);
    return 0;
}

/***************************************************************************
 *  TRUE when the block idx_mapping of the current slave can be polled. A
 *  block disabled at start (a bad type, address or size, an override, a
 *  slave id that cannot be asked) was logged there, once.
 ***************************************************************************/
PRIVATE BOOL map_is_pollable(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_map = json_array_get(priv->mapping_, (size_t)priv->idx_mapping);
    if(!jn_map) {
        return FALSE;
    }
    return kw_get_bool(gobj, jn_map, "disabled", 0, 0)? FALSE : TRUE;
}

/***************************************************************************
 *  Prepare next poll: the next block that can be polled, of this slave or
 *  of the next ones. A disabled block, and a slave without one, are skipped
 *  here, without a log (they were logged at start) and without spending a
 *  wait of timeout_polling on them.
 *  If end of cycle then publish and return -1
 ***************************************************************************/
PRIVATE int next_map(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cur_map_ = 0; // Reset cur map

    while(priv->idx_slaves < priv->max_slaves) {
        if(priv->idx_mapping < 0) {
            priv->idx_mapping = 0;
        } else {
            priv->idx_mapping++;
        }
        while(priv->idx_mapping < priv->max_mapping && !map_is_pollable(gobj)) {
            priv->idx_mapping++;
        }
        if(priv->idx_mapping < priv->max_mapping) {
            if(gobj_trace_level(gobj) & TRACE_POLLING) {
                gobj_trace_msg(gobj, "🔊🔊🔊🔊⏩ next map  : idx slave %d, idx map %d",
                    priv->idx_slaves, priv->idx_mapping
                );
            }
            return 0; // do polling
        }

        priv->idx_slaves++;
        if(priv->idx_slaves < priv->max_slaves) {
            load_slave_mapping(gobj);   // idx_mapping -1: its first block next
            if(gobj_trace_level(gobj) & TRACE_POLLING) {
                gobj_trace_msg(gobj, "🔊🔊🔊🔊🔊🔊🔊🔊⏩ next slave: idx slave %d",
                    priv->idx_slaves
                );
            }
        }
    }

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_msg(gobj, "🔊🔊🔊🔊⏩⏩⏩ end cycle: idx slave %d, idx map %d",
            priv->idx_slaves, priv->idx_mapping
        );
    }

    /*
     *  End of cycle, publish variables
     */
    build_message_to_publish(gobj);

    priv->idx_slaves = 0; // Begin cycle
    if(priv->max_slaves > 0) {
        load_slave_mapping(gobj);
    }

    return -1; // End of cycle
}

/***************************************************************************
 *  Send current map
 *  If success return 0 and set response timeout
 *  If fails return -1 and don't set timeout
 ***************************************************************************/
PRIVATE int poll_modbus(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->cur_map_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cur_map_ ALREADY loaded",
            "idx_slaves",   "%d", priv->idx_slaves,
            "idx_mapping",  "%d", priv->idx_mapping,
            "cur_map_",     "%j", priv->cur_map_?priv->cur_map_:json_null(),
            NULL
        );
    }

    priv->cur_map_ = json_array_get(priv->mapping_, priv->idx_mapping);
    if(!priv->cur_map_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cur_map_ NULL",
            "idx_slaves",   "%d", priv->idx_slaves,
            "idx_mapping",  "%d", priv->idx_mapping,
            "mapping_",     "%j", priv->mapping_?priv->mapping_:json_null(),
            NULL
        );
        // Don't set timeout
        return -1;
    }

    if(kw_get_bool(gobj, priv->cur_map_, "disabled", 0, 0)) {
        // next_map() skips a disabled block: reaching one here is a bug
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "cur_map_ DISABLED",
            "idx_slaves",   "%d", priv->idx_slaves,
            "idx_mapping",  "%d", priv->idx_mapping,
            "cur_map_",     "%j", priv->cur_map_,
            NULL
        );
        // Don't set timeout
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_json(gobj, priv->cur_map_, "polling");
    }

    // Change state
    gobj_change_state(gobj, ST_WAIT_RESPONSE);
    priv->waiting = FALSE;

    // Set response timeout
    set_timeout(priv->gobj_timer, priv->timeout_response*1000);

    gbuffer_t *gbuf = build_modbus_request_read_message(gobj, priv->cur_slave_, priv->cur_map_);
    send_data(gobj, gbuf);

    return 0;
}

/***************************************************************************
 *  Send the first queued request, if any.
 *  Return TRUE when one is in flight and its answer is awaited.
 *
 *  A request is sent once: it leaves the queue here, and the one in flight
 *  is kept in jn_current_request until it is answered (its response, its
 *  timeout or the end of the connection). While one is in flight nothing
 *  else is sent: its answer sends the next. A request that cannot be
 *  framed is answered as failed (the builder logged why) and the next one
 *  is tried.
 ***************************************************************************/
PRIVATE BOOL send_request(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The answer of a request that cannot be framed is published here, and
     *  its subscriber can send another right there: one that goes out
     *  inside that answer leaves this loop with a request in flight
     */
    while(!priv->jn_current_request && json_array_size(priv->jn_request_queue) > 0) {
        json_t *jn_request = json_incref(json_array_get(priv->jn_request_queue, 0));
        json_array_remove(priv->jn_request_queue, 0);

        if(gobj_trace_level(gobj) & TRACE_SEND) {
            json_t *jn_log = request_to_log(jn_request);
            gobj_trace_json(gobj, jn_log, "sending to %s",
               gobj_read_str_attr(gobj_bottom_gobj(gobj), "url")
            );
            JSON_DECREF(jn_log)
        }

        gbuffer_t *gbuf = build_modbus_request_message(gobj, jn_request);
        if(!gbuf) {
            // Error already logged
            answer_request(gobj, jn_request, -1, 0, NULL,
                json_string("the request could not be framed, see the log")
            );
            continue;
        }

        priv->jn_current_request = jn_request;
        JSON_DECREF(priv->jn_current_answer)

        // Change state
        gobj_change_state(gobj, ST_WAIT_RESPONSE);
        priv->waiting = FALSE;

        // Set response timeout
        set_timeout(priv->gobj_timer, priv->timeout_response*1000);

        send_data(gobj, gbuf);
    }

    return priv->jn_current_request? TRUE : FALSE;
}

/***************************************************************************
 *  What comes after an answer or a timeout: a queued request first; else,
 *  when requests went out instead of the wait between two cycles, back to
 *  that wait; else the next block of the cycle, or the wait of wait_ms when
 *  the cycle ended or its block cannot be polled.
 *
 *  The end of a cycle publishes what it read, and its subscriber can send a
 *  request right there, while nothing waits: it goes out at once, before the
 *  wait between cycles, which comes after it. Its timeout must not be
 *  replaced by that wait.
 ***************************************************************************/
PRIVATE int continue_polling(hgobj gobj, int wait_ms)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(send_request(gobj)) {
        return 0;
    }

    if(priv->resume_wait_ms > 0) {
        set_timeout(priv->gobj_timer, priv->resume_wait_ms);
        priv->resume_wait_ms = 0;
        priv->waiting = TRUE;
        return 0;
    }

    if(next_map(gobj) == 0) {
        if(poll_modbus(gobj) == 0) {
            return 0;   // timeout set by poll_modbus
        }
        next_map(gobj);     // a block that cannot be polled: the next one, after the wait
    }

    /*
     *  NO map, end of cycle, or a block that cannot be polled: wait. A
     *  request sent on the message of the cycle goes first.
     */
    if(send_request(gobj)) {
        priv->resume_wait_ms = wait_ms;
        return 0;
    }
    set_timeout(priv->gobj_timer, wait_ms);
    priv->waiting = TRUE;
    return 0;
}

/***************************************************************************
 *  The answer of what was in flight was read: the next request, or the
 *  next block
 ***************************************************************************/
PRIVATE int finish_answer(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->gobj_timer);

    RESET_MACHINE()
    gobj_change_state(gobj, ST_CONNECTED);

    /*
     *  The request in flight, if it was one, is answered. Its subscriber
     *  may send another on the answer: send_request() then finds it in
     *  flight, and the polling waits for it.
     */
    if(priv->jn_current_request) {
        answer_current_request(gobj);
    }

    return continue_polling(gobj, priv->timeout_polling);
}

/***************************************************************************
 *  Reset variables for a new read.
 ***************************************************************************/
PRIVATE int framehead_prepare_new_frame(FRAME_HEAD *frame)
{
    frame->function = -1;
    frame->slave_id = -1;

    /*
     *  state of frame
     */
    frame->busy = 1;    //in half of header
    frame->header_complete = 0; // Set True when header is completed
    frame->error_code = 0;
    frame->discard = 0;

    /*
     *  must do
     */
    frame->must_read_payload_data = 0;

    frame->payload_length = 0;

    return 0;
}

/***************************************************************************
 *  Consume input data to get and analyze the frame header.
 *  Return the consumed size.
 *  `stale`: the bytes came before the request in flight went out (they
 *  followed an answer in the same read), so the frame they belong to is not
 *  its answer. Only over TCP: over RTU those bytes are dropped.
 ***************************************************************************/
PRIVATE int framehead_consume(
    hgobj gobj,
    FRAME_HEAD *frame,
    istream_h istream,
    char *bf,
    int len,
    BOOL stale
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int total_consumed = 0;
    int consumed;

    if(stale) {
        frame->stale = TRUE;
    }

    /*
     *
     */
    if (!frame->busy) {
        /*
         * waiting the common head
         */
        SWITCHS(priv->modbus_protocol) {
            CASES("TCP")
                istream_read_until_num_bytes(istream, sizeof(head_tcp_t), 0); // idempotent
                break;

            CASES("RTU")
            CASES("ASCII")
                istream_read_until_num_bytes(istream, sizeof(head_rtu_t), 0); // idempotent
                break;

            DEFAULTS
                break;
        } SWITCHS_END

        consumed = istream_consume(istream, bf, len);
        total_consumed += consumed;
        bf += consumed;
        len -= consumed;
        if(!istream_is_completed(istream)) {
            return total_consumed;  // wait more data
        }

        /*
         *  we've got common head! Start a new frame
         */
        framehead_prepare_new_frame(frame);  // `busy` flag is set.

        /*
         *  Only the request in flight, a block or a request, is answered:
         *  a frame that comes while nothing is in flight, or that began
         *  before it went out, is not an answer
         */
        BOOL in_flight = (gobj_in_this_state(gobj, ST_WAIT_RESPONSE) && !frame->stale)?
            TRUE : FALSE;

        SWITCHS(priv->modbus_protocol) {
            CASES("TCP")
                head_tcp_t *head = (head_tcp_t *)istream_extract_matched_data(istream, 0);
                frame->function = head->function;
                frame->slave_id = head->slave_id;
                frame->byte_count = head->byte_count;
                uint16_t t_id = ntohs(head->t_id);
                uint16_t protocol = ntohs(head->protocol);
                uint16_t length = ntohs(head->length);
                if(length < 3 || length > MODBUS_MAX_PDU_LENGTH + 1) {
                    /*
                     *  MBAP length must cover at least unit_id + function +
                     *  byte_count. length < 3 underflows the size_t
                     *  payload_length below (length==2 -> SIZE_MAX, which sizes
                     *  a malloc(0)-backed gbuffer that data_size still records as
                     *  SIZE_MAX -> the response memmove overruns it: heap
                     *  overflow). And it is at most the unit and a PDU, 254:
                     *  a frame of another t_id with a length of 65535 would be
                     *  "read whole" by swallowing every answer after it.
                     *  Nothing tells where the next frame starts: the caller
                     *  drops the connection on a negative return.
                     */
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL,
                        "msg",          "%s", "Modbus TCP: bad MBAP length, the connection is dropped",
                        "length",       "%d", (int)length,
                        NULL
                    );
                    gobj_trace_dump(gobj, (const char *)head, sizeof(head_tcp_t), "MBAP");
                    return -1;
                }
                frame->payload_length = length - 3;

                /*
                 *  The answer of the request in flight carries its
                 *  transaction id: the late answer of a request that timed
                 *  out has the same slave, function and size as the next
                 *  one, and would be taken for it
                 */
                if(!in_flight || t_id != priv->t_id || protocol != 0) {
                    frame->discard = TRUE;
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL,
                        "msg",          "%s", "Modbus frame dropped: not the answer awaited",
                        "in_flight",    "%d", (int)in_flight,
                        "t_id",         "%d", (int)t_id,
                        "t_id_sent",    "%d", (int)priv->t_id,
                        "protocol",     "%d", (int)protocol,
                        "slave_id",     "%d", (int)frame->slave_id,
                        "modbus_function", "%d", (int)frame->function,
                        "length",       "%d", (int)length,
                        NULL
                    );
                }
                break;

            CASES("RTU")
            CASES("ASCII")
                head_rtu_t *head = (head_rtu_t *)istream_extract_matched_data(istream, 0);
                frame->function = head->function;
                frame->slave_id = head->slave_id;
                frame->byte_count = head->byte_count;
                if(head->function & 0x80) {
                    /*
                     *  An exception ends at its code, then the CRC
                     */
                    frame->payload_length = sizeof(uint16_t);
                } else {
                    switch(head->function) {
                        case MODBUS_FC_WRITE_SINGLE_COIL:
                        case MODBUS_FC_WRITE_SINGLE_REGISTER:
                        case MODBUS_FC_WRITE_MULTIPLE_COILS:
                        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                            /*
                             *  The echo of a write has no byte count: address
                             *  and value (or quantity), 4 bytes, of which the
                             *  head already took the first
                             */
                            frame->payload_length = 3 + sizeof(uint16_t); // + crc
                            break;
                        default:
                            frame->payload_length = head->byte_count + sizeof(uint16_t); // + crc
                            break;
                    }
                }

                /*
                 *  RTU carries no transaction id: a frame is dropped when
                 *  nothing is in flight, or when its slave or its function
                 *  (an exception's, without the 0x80 bit) is not the one
                 *  sent. The late exception of a read that timed out is
                 *  not the answer of the write sent after it: taken, it
                 *  failed the write, whose echo then came with nothing in
                 *  flight and was dropped.
                 */
                if(!in_flight ||
                        (int)(head->function & 0x7F) != priv->modbus_function ||
                        (int)head->slave_id != priv->slave_id_sent) {
                    frame->discard = TRUE;
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL,
                        "msg",          "%s", "Modbus frame dropped: not the answer awaited",
                        "in_flight",    "%d", (int)in_flight,
                        "slave_id",     "%d", (int)frame->slave_id,
                        "slave_id_sent", "%d", priv->slave_id_sent,
                        "modbus_function", "%d", (int)frame->function,
                        "modbus_function_sent", "%d", priv->modbus_function,
                        NULL
                    );
                }
                break;

            DEFAULTS
                break;
        } SWITCHS_END
    }

    /*
     *  An exception ends at its code. RTU closes the frame with a CRC; TCP
     *  carries none, and the MBAP length (3: unit, function, code) leaves
     *  payload_length at 0. It is taken when the frame is complete
     *  (frame_completed()): over RTU only after its CRC was checked.
     */
    if(frame->function & 0x80) {
        frame->error_code = frame->byte_count;
    } else {
        if(gobj_trace_level(gobj) & TRACE_POLLING) {
            gobj_trace_msg(gobj, "🍅🍅⏪ func: %d %s, slave_id: %d, count: %d",
                frame->function,
                modbus_function_name(frame->function),
                frame->slave_id,
                frame->byte_count
            );
        }
    }

    frame->header_complete = TRUE;
    return total_consumed;
}

/***************************************************************************
 *  TRUE when the request in flight is a write: what comes back is its echo,
 *  and the echo of a single write (05, 06) carries its value.
 ***************************************************************************/
PRIVATE BOOL write_in_flight(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->jn_current_request) {
        return FALSE;
    }
    json_t *jn_operation = json_object_get(priv->jn_current_request, "operation");
    return (json_is_string(jn_operation) &&
        strcmp(json_string_value(jn_operation), "write")==0)? TRUE : FALSE;
}

/***************************************************************************
 *  TRUE when the bytes of the frame being read are dumped hidden, like the
 *  frame of a write: its function is a write (05, 06, 15, 16) or the
 *  exception of one, or a write is in flight. Decided by the frame first:
 *  the late echo of a write comes while a read is in flight. And by what is
 *  in flight too: a broken frame may not say its function right.
 ***************************************************************************/
PRIVATE BOOL frame_is_hidden(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->frame_head.function & 0x7F) {
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return TRUE;
        default:
            break;
    }
    return write_in_flight(gobj);
}

/***************************************************************************
 *  Over RTU, the bytes that follow an answer in the same read are dropped:
 *  without a length in the frame, noise taken as the head of a frame would
 *  swallow the next answer. Dumped hidden when the answer was, or when a
 *  write is in flight now.
 ***************************************************************************/
PRIVATE int drop_bytes_after_answer(hgobj gobj, gbuffer_t *gbuf, BOOL hidden)
{
    size_t len = gbuffer_leftbytes(gbuf);

    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "Modbus: bytes after the answer, dropped",
        "len",          "%d", (int)len,
        NULL
    );
    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        if(hidden || write_in_flight(gobj)) {
            gobj_trace_msg(gobj, "Modbus: bytes after the answer: <%d bytes hidden>", (int)len);
        } else {
            gobj_trace_dump(gobj,
                gbuffer_cur_rd_pointer(gbuf),
                MIN(len, 64),
                "Modbus: bytes after the answer"
            );
        }
    }
    gbuffer_get(gbuf, len);
    return 0;
}

/***************************************************************************
 *  Forget the frame being read, to read the next one from its head. The
 *  function of the request in flight is kept: see RESET_MACHINE().
 ***************************************************************************/
PRIVATE int reset_frame(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ISTREAM_DESTROY(priv->istream_payload);
    if(priv->istream_head) {
        istream_clear(priv->istream_head);
    }
    memset(&priv->frame_head, 0, sizeof(priv->frame_head));
    priv->st = WAIT_HEAD;
    return 0;
}

/***************************************************************************
 *  The answer awaited is an exception: it fails the request in flight, or
 *  the block of the cycle
 ***************************************************************************/
PRIVATE int frame_exception(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int error_code = priv->frame_head.error_code;

    if(priv->jn_current_request) {
        json_t *jn_log = request_to_log(priv->jn_current_request);
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL,
            "msg",              "%s", "modbus exception",
            "slave_id",         "%d", (int)priv->frame_head.slave_id,
            "error_code",       "%d", error_code,
            "error_name",       "%s", modbus_exception_name(error_code),
            "request",          "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        set_current_answer(gobj, -1, error_code, NULL,
            json_sprintf("modbus exception %d %s",
                error_code,
                modbus_exception_name(error_code)
            )
        );
    } else {
        note_block_failed(gobj, "modbus exception", error_code);
    }
    return 0;
}

/***************************************************************************
 *  The answer awaited came broken (a peer's bytes, not ours): it fails the
 *  request in flight, or the block of the cycle. Its bytes are dumped only
 *  with the traffic trace: the echo of a write carries its value.
 ***************************************************************************/
PRIVATE int frame_failed(hgobj gobj, const char *msg, uint8_t *bf, int len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_current_request) {
        json_t *jn_log = request_to_log(priv->jn_current_request);
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL,
            "msg",              "%s", msg,
            "slave_id",         "%d", (int)priv->frame_head.slave_id,
            "modbus_function",  "%d", (int)priv->frame_head.function,
            "modbus_function_sent", "%d", priv->modbus_function,
            "request",          "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        set_current_answer(gobj, -1, 0, NULL, json_string(msg));
    } else {
        note_block_failed(gobj, msg, 0);
    }

    if((gobj_trace_level(gobj) & TRACE_TRAFFIC) && bf && len > 0) {
        if(frame_is_hidden(gobj)) {
            gobj_trace_msg(gobj, "%s: <%d bytes hidden>", msg, len);
        } else {
            gobj_trace_dump(gobj, (const char *)bf, (size_t)(len < 64? len : 64), "%s", msg);
        }
    }
    return 0;
}

/***************************************************************************
 *  Process the completed frame: the answer of what is in flight (a frame
 *  that is not was dropped before). Its payload can be empty: a TCP
 *  exception, or an MBAP of length 3.
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t *bf = NULL;
    int len = 0;
    if(priv->istream_payload) {
        gbuffer_t *gbuf = istream_get_gbuffer(priv->istream_payload);
        len = gbuf? (int)gbuffer_leftbytes(gbuf) : 0;
        if(len > 0) {
            bf = gbuffer_get(gbuf, (size_t)len);
        }
    }

    SWITCHS(priv->modbus_protocol) {
        CASES("TCP")
            break;

        CASES("RTU")
            if(len < 2 || !bf) {
                frame_failed(gobj, "Modbus answer without its CRC", bf, len);
                return -1;
            }
            uint16_t crc_calculated = crc16_rx(&priv->frame_head, bf, (uint16_t)(len - 2));
            uint16_t crc_received = (uint16_t)((bf[len - 2] << 8) | bf[len - 1]);
            if(crc_calculated != crc_received) {
                // No CRC in the log: with the address, it gives the value of a write echo
                frame_failed(gobj, "Modbus answer with a bad CRC", bf, len);
                return -1;
            }
            len -= 2;
            break;

        CASES("ASCII")
        DEFAULTS
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Protocol modbus NOT IMPLEMENTED",
                NULL
            );
            return -1;
    } SWITCHS_END

    /*
     *  An answer, an exception included, is of the function sent: a 0x84 is
     *  not the exception of a 03, nor the echo of a write the answer of a
     *  block. Over RTU such a frame was dropped as not the answer awaited;
     *  over TCP it carries the t_id sent, and is a broken answer.
     */
    if((int)(priv->frame_head.function & 0x7F) != priv->modbus_function) {
        frame_failed(gobj, "Modbus answer of another function", bf, len);
        return -1;
    }

    if(priv->frame_head.function & 0x80) {
        return frame_exception(gobj);
    }

    if(priv->jn_current_request) {
        read_request_answer(gobj, bf, len);
    } else if(store_modbus_response_data(gobj, bf, len)==0) {
        note_block_answered(gobj);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE slave_data_t *get_slave_data(hgobj gobj, int slave_id, BOOL verbose)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    slave_data_t *pslv = priv->slave_data;
    if(!pslv) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "slave data NULL",
            "slave_id",     "%d", slave_id,
            NULL
        );
        return 0;
    }
    for(int i=0; i<priv->max_slaves; i++) {
        if(pslv->slave_id == slave_id) {
            return pslv;
        }
        // Next slave
        pslv++;
    }

    if(verbose) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "slave data NOT FOUND",
            "slave_id",     "%d", slave_id,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int store_slave_bit(
    hgobj gobj,
    int slave_id,
    modbus_object_type_t object_type,
    int address,
    int value
) {
    slave_data_t *pslv = get_slave_data(gobj, slave_id, TRUE);
    if(!pslv) {
        // Error already logged
        return -1;
    }
    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return -1;
    }
    cell_control->control.bit_value = value?1:0;
    cell_control->control.updated = 1;

    if(gobj_trace_level(gobj) & TRACE_DECODE) {
        gobj_trace_msg(gobj,
            "store bit in %s x%0X %d, value: x%0X",
            get_object_type_name(object_type),
            address,
            address,
            cell_control->control.bit_value
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int store_slave_word(
    hgobj gobj,
    int slave_id,
    modbus_object_type_t object_type,
    int address,
    uint8_t *bf
) {
    slave_data_t *pslv = get_slave_data(gobj, slave_id, TRUE);
    if(!pslv) {
        // Error already logged
        return -1;
    }
    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return -1;
    }

    switch(object_type) {
        case TYPE_INPUT_REGISTER:
            memmove(&cell_control->input_register, bf, 2);
            cell_control->control.updated = 1;

            if(gobj_trace_level(gobj) & TRACE_DECODE) {
                gobj_trace_dump(gobj,
                    (const char *)bf, 2, "store word in %s x%0X %d, value: x%0X",
                    get_object_type_name(object_type),
                    address,
                    address,
                    cell_control->input_register
                );
            }

            break;

        case TYPE_HOLDING_REGISTER:
            memmove(&cell_control->holding_register, bf, 2);
            cell_control->control.updated = 1;

            if(gobj_trace_level(gobj) & TRACE_DECODE) {
                gobj_trace_dump(gobj,
                    (const char *)bf, 2, "store word in %s x%0X %d, value: x%0X",
                    get_object_type_name(object_type),
                    address,
                    address,
                    cell_control->holding_register
                );
            }

            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "BAD object type",
                "slave_id",     "%d", slave_id,
                "object_type",  "%d", object_type,
                NULL
            );
            return -1;
    }

    return 0;
}

/***************************************************************************
 *  The answer of the block in flight, into its cells. An answer that does
 *  not fit what the block asked fails it: logged when the block starts
 *  failing, or fails another way (note_block_failed()), not every cycle.
 ***************************************************************************/
PRIVATE int store_modbus_response_data(hgobj gobj, uint8_t *bf, int len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int slave_id = priv->frame_head.slave_id;
    int modbus_function = priv->frame_head.function;
    int byte_count = priv->frame_head.byte_count;

    if(modbus_function == MODBUS_FC_READ_COILS ||
        modbus_function == MODBUS_FC_READ_DISCRETE_INPUTS ||
        modbus_function == MODBUS_FC_READ_HOLDING_REGISTERS ||
        modbus_function == MODBUS_FC_READ_INPUT_REGISTERS ||
        modbus_function == MODBUS_FC_WRITE_AND_READ_REGISTERS
    ) {
        if(byte_count != len) {
            note_block_failed(gobj, "byte_count != len", 0);
            return -1;
        }
    } else {
        /*
         *  A block is a read: frame_completed() fails an answer of another
         *  function before it comes here
         */
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "A block answered with a function that is not a read",
            "modbus_function",  "%s", modbus_function_name(modbus_function),
            NULL
        );
        return -1;
    }

    uint8_t req_slave_id = (uint8_t)kw_get_int(gobj, priv->cur_slave_, "id", 0, KW_REQUIRED);
    uint16_t req_address = (uint16_t)kw_get_int(
        gobj, priv->cur_map_, "address", 0, KW_REQUIRED|KW_WILD_NUMBER
    );
    uint16_t req_size = (uint16_t)kw_get_int(gobj, priv->cur_map_, "size", 0, KW_REQUIRED|KW_WILD_NUMBER);

    /*------------------------------*
     *      Check protocol
     *------------------------------*/
    if(req_slave_id != slave_id) {
        note_block_failed(gobj, "slave_id NOT MATCH", 0);
        return -1;
    }

    if(priv->modbus_function != modbus_function) {
        note_block_failed(gobj, "modbus function NOT MATCH", 0);
        return -1;
    }

    /* Check the number of values is corresponding to the request */
    int req_nb_value=-1;
    int rsp_nb_value=-2;

    switch (modbus_function) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        /*
         * Read functions, 8 values in a byte (nb
         * of values in the request and byte count in
         * the response.
         */
        req_nb_value = req_size;
        req_nb_value = (req_nb_value / 8) + ((req_nb_value % 8) ? 1 : 0);
        rsp_nb_value = byte_count;
        break;

    case MODBUS_FC_WRITE_AND_READ_REGISTERS:
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
        /*
         *  Read functions 1 value = 2 bytes
         */
        req_nb_value = req_size;
        rsp_nb_value = byte_count/2;
        break;

    case MODBUS_FC_WRITE_MULTIPLE_COILS:
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        /*
         *  N Write functions
         * TODO no comprobado
         */
        req_nb_value = req_size;
        rsp_nb_value = byte_count;
        break;

    case MODBUS_FC_REPORT_SLAVE_ID:
        /*
         *  Report slave ID (bytes received)
         *  TODO no comprobado
         */
        req_nb_value = rsp_nb_value = byte_count;
        break;

    default:
        /* 1 Write functions & others */
        req_nb_value = rsp_nb_value = 1;
    }

    if(req_nb_value != rsp_nb_value) {
        note_block_failed(gobj, "Quantity not corresponding to the request", 0);
        return -1;
    }

    switch(modbus_function) {
        case MODBUS_FC_READ_COILS:
            {
                int pos = 0;
                for(int i = 0; i < len; i++) {
                    /* Shift reg hi_byte to temp */
                    int temp = bf[i];

                    for (int bit = 0x01; (bit & 0xff) && (pos < req_size);) {
                        BOOL value = (temp & bit) ? TRUE : FALSE;
                        store_slave_bit(
                            gobj,
                            slave_id,
                            TYPE_COIL,
                            req_address + pos,
                            value
                        );
                        pos++;
                        bit = bit << 1;
                    }
                }
            }
            break;

        case MODBUS_FC_READ_DISCRETE_INPUTS:
            {
                int pos = 0;
                for(int i = 0; i < len; i++) {
                    /* Shift reg hi_byte to temp */
                    int temp = bf[i];

                    for (int bit = 0x01; (bit & 0xff) && (pos < req_size);) {
                        BOOL value = (temp & bit) ? TRUE : FALSE;
                        store_slave_bit(
                            gobj,
                            slave_id,
                            TYPE_DISCRETE_INPUT,
                            req_address + pos,
                            value
                        );
                        pos++;
                        bit = bit << 1;
                    }
                }
            }
            break;

        case MODBUS_FC_READ_HOLDING_REGISTERS:
            {
                for(int i = 0; i < len/2; i++) {
                    store_slave_word(
                        gobj,
                        slave_id,
                        TYPE_HOLDING_REGISTER,
                        req_address + i,
                        bf + (i << 1)
                    );
                }
            }
            break;

        case MODBUS_FC_READ_INPUT_REGISTERS:
            {
                for(int i = 0; i < len/2; i++) {
                    /* shift reg hi_byte to temp OR with lo_byte */
                    store_slave_word(
                        gobj,
                        slave_id,
                        TYPE_INPUT_REGISTER,
                        req_address + i,
                        bf + (i << 1)
                    );
                }
            }
            break;

        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_READ_EXCEPTION_STATUS:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        case MODBUS_FC_REPORT_SLAVE_ID:
        case MODBUS_FC_MASK_WRITE_REGISTER:
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
        default:
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INTERNAL,
                "msg",              "%s", "Function NOT IMPLEMENTED",
                "function",         "%s", modbus_function_name(modbus_function),
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE endian_format_t get_endian_format(hgobj gobj, const char *format_)
{
    endian_format_t endian_format = 0;

    char format[64];
    snprintf(format, sizeof(format), "%s", format_);
    strntolower(format, strlen(format));

    SWITCHS(format) {
        CASES("big_endian")
        CASES("big endian")
            endian_format = FORMAT_BIG_ENDIAN;
            break;
        CASES("little_endian")
        CASES("little endian")
            endian_format = FORMAT_LITTLE_ENDIAN;
            break;
        CASES("big_endian_byte_swap")
        CASES("big endian byte swap")
            endian_format = FORMAT_BIG_ENDIAN_BYTE_SWAP;
            break;
        CASES("little_endian_byte_swap")
        CASES("little endian byte swap")
            endian_format = FORMAT_LITTLE_ENDIAN_BYTE_SWAP;
            break;
        DEFAULTS
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "endian format UNKNOWN",
                "format",       "%s", format_,
                NULL
            );
            endian_format = -1;
    } SWITCHS_END;

    return endian_format;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE variable_format_t get_variable_format(hgobj gobj, const char *format_)
{
    variable_format_t variable_format = 0;

    char format[64];
    snprintf(format, sizeof(format), "%s", format_);
    strntolower(format, strlen(format));

    SWITCHS(format) {
        CASES("int16")
            variable_format = FORMAT_INT16;
            break;
        CASES("uint16")
            variable_format = FORMAT_UINT16;
            break;
        CASES("bool")
            variable_format = FORMAT_BOOL;
            break;
        CASES("int32")
            variable_format = FORMAT_INT32;
            break;
        CASES("uint32")
            variable_format = FORMAT_UINT32;
            break;
        CASES("int64")
            variable_format = FORMAT_INT64;
            break;
        CASES("uint64")
            variable_format = FORMAT_UINT64;
            break;
        CASES("float")
            variable_format = FORMAT_FLOAT;
            break;
        CASES("double")
            variable_format = FORMAT_DOUBLE;
            break;
        CASES("str")
        CASES("string")
            variable_format = FORMAT_STRING;
            break;
        DEFAULTS
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "variable format UNKNOWN",
                "format",       "%s", format_,
                NULL
            );
            variable_format = -1;
    } SWITCHS_END;

    return variable_format;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint16_t endian_16(endian_format_t endian_format, uint8_t *pv)
{
    uint16_t v = 0;

    switch(endian_format) {
        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
        case FORMAT_BIG_ENDIAN:
            v = pv[1] + (((uint16_t)pv[0]) << 8);
            break;
        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
        case FORMAT_LITTLE_ENDIAN:
            v = pv[0] + (((uint16_t)pv[1]) << 8);
            break;
    }
    return v;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint32_t endian_32(endian_format_t endian_format, uint8_t *pv)
{
    uint32_t v = 0;

    switch(endian_format) {
        case FORMAT_BIG_ENDIAN:
            v = (((uint32_t)pv[3]) << 8*0) +
                (((uint32_t)pv[2]) << 8*1) +
                (((uint32_t)pv[1]) << 8*2) +
                (((uint32_t)pv[0]) << 8*3);
            break;
        case FORMAT_LITTLE_ENDIAN:
            v = (((uint32_t)pv[0]) << 8*0) +
                (((uint32_t)pv[1]) << 8*1) +
                (((uint32_t)pv[2]) << 8*2) +
                (((uint32_t)pv[3]) << 8*3);
            break;
        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
            v = (((uint32_t)pv[2]) << 8*0) +
                (((uint32_t)pv[3]) << 8*1) +
                (((uint32_t)pv[0]) << 8*2) +
                (((uint32_t)pv[1]) << 8*3);
            break;
        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
            v = (((uint32_t)pv[1]) << 8*0) +
                (((uint32_t)pv[0]) << 8*1) +
                (((uint32_t)pv[3]) << 8*2) +
                (((uint32_t)pv[2]) << 8*3);
            break;
    }

    return v;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint64_t endian_64(endian_format_t endian_format, uint8_t *pv)
{
    uint64_t v = 0;

    switch(endian_format) {
        case FORMAT_BIG_ENDIAN:
            v =
                (((uint64_t)pv[7]) << 8*0) +
                (((uint64_t)pv[6]) << 8*1) +
                (((uint64_t)pv[5]) << 8*2) +
                (((uint64_t)pv[4]) << 8*3) +
                (((uint64_t)pv[3]) << 8*4) +
                (((uint64_t)pv[2]) << 8*5) +
                (((uint64_t)pv[1]) << 8*6) +
                (((uint64_t)pv[0]) << 8*7);
            break;
        case FORMAT_LITTLE_ENDIAN:
            v =
                (((uint64_t)pv[0]) << 8*0) +
                (((uint64_t)pv[1]) << 8*1) +
                (((uint64_t)pv[2]) << 8*2) +
                (((uint64_t)pv[3]) << 8*3) +
                (((uint64_t)pv[4]) << 8*4) +
                (((uint64_t)pv[5]) << 8*5) +
                (((uint64_t)pv[6]) << 8*6) +
                (((uint64_t)pv[7]) << 8*7);
            break;
        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
            v =
                (((uint64_t)pv[6]) << 8*0) +
                (((uint64_t)pv[7]) << 8*1) +
                (((uint64_t)pv[4]) << 8*2) +
                (((uint64_t)pv[5]) << 8*3) +
                (((uint64_t)pv[2]) << 8*4) +
                (((uint64_t)pv[3]) << 8*5) +
                (((uint64_t)pv[0]) << 8*6) +
                (((uint64_t)pv[1]) << 8*7);
            break;
        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
            v =
                (((uint64_t)pv[1]) << 8*0) +
                (((uint64_t)pv[0]) << 8*1) +
                (((uint64_t)pv[3]) << 8*2) +
                (((uint64_t)pv[2]) << 8*3) +
                (((uint64_t)pv[5]) << 8*4) +
                (((uint64_t)pv[4]) << 8*5) +
                (((uint64_t)pv[7]) << 8*6) +
                (((uint64_t)pv[6]) << 8*7);
            break;
    }

    return v;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE float endian_float(endian_format_t endian_format, uint8_t *pv)
{
    uint32_t v = 0;

    switch(endian_format) {
        case FORMAT_BIG_ENDIAN:
            v = (((uint32_t)pv[3]) << 8*0) +
                (((uint32_t)pv[2]) << 8*1) +
                (((uint32_t)pv[1]) << 8*2) +
                (((uint32_t)pv[0]) << 8*3);
            break;
        case FORMAT_LITTLE_ENDIAN:
            v = (((uint32_t)pv[0]) << 8*0) +
                (((uint32_t)pv[1]) << 8*1) +
                (((uint32_t)pv[2]) << 8*2) +
                (((uint32_t)pv[3]) << 8*3);
            break;
        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
            v = (((uint32_t)pv[2]) << 8*0) +
                (((uint32_t)pv[3]) << 8*1) +
                (((uint32_t)pv[0]) << 8*2) +
                (((uint32_t)pv[1]) << 8*3);
            break;
        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
            v = (((uint32_t)pv[1]) << 8*0) +
                (((uint32_t)pv[0]) << 8*1) +
                (((uint32_t)pv[3]) << 8*2) +
                (((uint32_t)pv[2]) << 8*3);
            break;
    }

    float f;
    memcpy(&f, &v, sizeof(float));

    return f;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE double endian_double(endian_format_t endian_format, uint8_t *pv)
{
    uint64_t v = 0;

    switch(endian_format) {
        case FORMAT_BIG_ENDIAN:
            v =
                (((uint64_t)pv[7]) << 8*0) +
                (((uint64_t)pv[6]) << 8*1) +
                (((uint64_t)pv[5]) << 8*2) +
                (((uint64_t)pv[4]) << 8*3) +
                (((uint64_t)pv[3]) << 8*4) +
                (((uint64_t)pv[2]) << 8*5) +
                (((uint64_t)pv[1]) << 8*6) +
                (((uint64_t)pv[0]) << 8*7);
            break;
        case FORMAT_LITTLE_ENDIAN:
            v =
                (((uint64_t)pv[0]) << 8*0) +
                (((uint64_t)pv[1]) << 8*1) +
                (((uint64_t)pv[2]) << 8*2) +
                (((uint64_t)pv[3]) << 8*3) +
                (((uint64_t)pv[4]) << 8*4) +
                (((uint64_t)pv[5]) << 8*5) +
                (((uint64_t)pv[6]) << 8*6) +
                (((uint64_t)pv[7]) << 8*7);
            break;
        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
            v =
                (((uint64_t)pv[6]) << 8*0) +
                (((uint64_t)pv[7]) << 8*1) +
                (((uint64_t)pv[4]) << 8*2) +
                (((uint64_t)pv[5]) << 8*3) +
                (((uint64_t)pv[2]) << 8*4) +
                (((uint64_t)pv[3]) << 8*5) +
                (((uint64_t)pv[0]) << 8*6) +
                (((uint64_t)pv[1]) << 8*7);
            break;
        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
            v =
                (((uint64_t)pv[1]) << 8*0) +
                (((uint64_t)pv[0]) << 8*1) +
                (((uint64_t)pv[3]) << 8*2) +
                (((uint64_t)pv[2]) << 8*3) +
                (((uint64_t)pv[5]) << 8*4) +
                (((uint64_t)pv[4]) << 8*5) +
                (((uint64_t)pv[7]) << 8*6) +
                (((uint64_t)pv[6]) << 8*7);
            break;
    }

    double f;
    memcpy(&f, &v, sizeof(double));

    return f;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_variable_value(hgobj gobj, slave_data_t *pslv, json_t *jn_variable)
{
    json_t *jn_value = json_null();

    /*
     *  These values are been checked in check_conversion_variable()
     */
    const char *type = kw_get_str(gobj, jn_variable, "type", "", KW_REQUIRED);
    modbus_object_type_t object_type = get_object_type(gobj, type);
    if(object_type < 0) {
        return jn_value;
    }

    int address = (int)kw_get_int(gobj, jn_variable, "address", -1, KW_REQUIRED|KW_WILD_NUMBER);
    float multiplier = (float)kw_get_real(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
    if(multiplier == 0.0) {
        multiplier = 1.0;
    }

    const char *format = kw_get_str(gobj, jn_variable, "format", "", KW_REQUIRED);
    variable_format_t variable_format = get_variable_format(gobj, format);

    const char *endian = kw_get_str(gobj, jn_variable, "endian", "big endian", 0);
    endian_format_t endian_format = get_endian_format(gobj, endian);

    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return jn_value;
    }

    switch(variable_format) {
        case FORMAT_BOOL:
            {
                switch(object_type) {
                    case TYPE_COIL:
                    case TYPE_DISCRETE_INPUT:
                        jn_value = cell_control->control.bit_value?json_true():json_false();
                        break;
                    case TYPE_INPUT_REGISTER:
                        jn_value = cell_control->input_register?json_true():json_false();
                        break;
                    case TYPE_HOLDING_REGISTER:
                        jn_value = cell_control->holding_register?json_true():json_false();
                        break;
                    default:
                        break;
                }
            }
            break;

        case FORMAT_INT16:
        case FORMAT_UINT16:
            {
                uint16_t *pv = 0;
                switch(object_type) {
                    case TYPE_COIL:
                    case TYPE_DISCRETE_INPUT:
                        jn_value = cell_control->control.bit_value?json_integer(1):json_integer(0);
                        break;
                    case TYPE_INPUT_REGISTER:
                        pv = &cell_control->input_register;
                        break;
                    case TYPE_HOLDING_REGISTER:
                        pv = &cell_control->holding_register;
                        break;
                    default:
                        break;
                }

                if(pv) {
                    if(variable_format == FORMAT_INT16) {
                        int16_t v = endian_16(endian_format, (uint8_t *)pv);
                        if(multiplier < 1.0 &&  multiplier > 0.0) {
                            float v_ = (float)v * multiplier;
                            jn_value = json_real(v_);
                        } else {
                            v = v*multiplier;
                            jn_value = json_integer(v);
                        }
                    } else {
                        uint16_t v = endian_16(endian_format, (uint8_t *)pv);
                        if(multiplier < 1.0 &&  multiplier > 0.0) {
                            float v_ = (float)v * multiplier;
                            jn_value = json_real(v_);
                        } else {
                            v = v*multiplier;
                            jn_value = json_integer(v);
                        }
                    }
                }
            }
            break;

        // case FORMAT_INT32:
        // case FORMAT_UINT32:
        //     {
        //         uint16_t *pv = 0;
        //         switch(object_type) {
        //             case TYPE_COIL:
        //             case TYPE_DISCRETE_INPUT:
        //                 jn_value = cell_control->control.bit_value?json_integer(1):json_integer(0);
        //                 break;
        //             case TYPE_INPUT_REGISTER:
        //                 pv = &cell_control->input_register;
        //                 break;
        //             case TYPE_HOLDING_REGISTER:
        //                 pv = &cell_control->holding_register;
        //                 break;
        //             default:
        //                 break;
        //         }
        //
        //         if(pv) {
        //             if(variable_format == FORMAT_INT32) {
        //                 int32_t v = endian_32(endian_format, (uint8_t *)pv);
        //                 if(multiplier < 1.0 &&  multiplier > 0.0) {
        //                     float v_ = (float)v * multiplier;
        //                     jn_value = json_real(v_);
        //                 } else {
        //                     v = v*multiplier;
        //                     jn_value = json_integer(v);
        //                 }
        //             } else {
        //                 uint32_t v = endian_32(endian_format, (uint8_t *)pv);
        //                 if(multiplier < 1.0 &&  multiplier > 0.0) {
        //                     float v_ = (float)v * multiplier;
        //                     jn_value = json_real(v_);
        //                 } else {
        //                     v = v*multiplier;
        //                     jn_value = json_integer(v);
        //                 }
        //             }
        //         }
        //     }
        //     break;

        case FORMAT_INT32:
        case FORMAT_UINT32:
            {
                uint8_t b[4];
                if(collect_register_bytes(gobj, pslv, object_type, address, 2, b)==0) {
                    if(variable_format==FORMAT_INT32) {
                        int32_t v = (int32_t)endian_32(endian_format, b);
                        jn_value = (multiplier>0.0 && multiplier<1.0)
                            ? json_real((float)v * multiplier)
                            : json_integer((json_int_t)((int64_t)v * (int64_t)multiplier));
                    } else {
                        uint32_t v = endian_32(endian_format, b);
                        jn_value = (multiplier>0.0 && multiplier<1.0)
                            ? json_real((float)v * multiplier)
                            : json_integer((json_int_t)((uint64_t)v * (uint64_t)multiplier));
                    }
                }
            }
            break;

        // case FORMAT_INT64:
        // case FORMAT_UINT64:
        //     {
        //         uint16_t *pv = 0;
        //         switch(object_type) {
        //             case TYPE_COIL:
        //             case TYPE_DISCRETE_INPUT:
        //                 jn_value = cell_control->control.bit_value?json_integer(1):json_integer(0);
        //                 break;
        //             case TYPE_INPUT_REGISTER:
        //                 pv = &cell_control->input_register;
        //                 break;
        //             case TYPE_HOLDING_REGISTER:
        //                 pv = &cell_control->holding_register;
        //                 break;
        //             default:
        //                 break;
        //         }
        //
        //         if(pv) {
        //             if(variable_format == FORMAT_INT64) {
        //                 int64_t v = endian_64(endian_format, (uint8_t *)pv);
        //                 if(multiplier < 1.0 &&  multiplier > 0.0) {
        //                     double v_ = (double)v * multiplier;
        //                     jn_value = json_real(v_);
        //                 } else {
        //                     v = v*multiplier;
        //                     jn_value = json_integer(v);
        //                 }
        //             } else {
        //                 uint64_t v = endian_64(endian_format, (uint8_t *)pv);
        //                 if(multiplier < 1.0 &&  multiplier > 0.0) {
        //                     double v_ = (double)v * multiplier;
        //                     jn_value = json_real(v_);
        //                 } else {
        //                     v = v*multiplier;
        //                     jn_value = json_integer(v);
        //                 }
        //             }
        //         }
        //     }
        //     break;

        case FORMAT_INT64:
        case FORMAT_UINT64:
            {
                uint8_t b[8];
                if(collect_register_bytes(gobj, pslv, object_type, address, 4, b)==0) {
                    if(variable_format==FORMAT_INT64) {
                        int64_t v = (int64_t)endian_64(endian_format, b);
                        jn_value = (multiplier>0.0 && multiplier<1.0)
                            ? json_real((double)v * multiplier)
                            : json_integer((json_int_t)(v * (int64_t)multiplier));
                    } else {
                        uint64_t v = endian_64(endian_format, b);
                        jn_value = (multiplier>0.0 && multiplier<1.0)
                            ? json_real((double)v * multiplier)
                            : json_integer((json_int_t)(v * (uint64_t)multiplier));
                    }
                }
            }
            break;

        // case FORMAT_FLOAT:
        //     {
        //         uint16_t *pv = 0;
        //         switch(object_type) {
        //             case TYPE_COIL:
        //             case TYPE_DISCRETE_INPUT:
        //                 jn_value = cell_control->control.bit_value?json_integer(1):json_integer(0);
        //                 break;
        //             case TYPE_INPUT_REGISTER:
        //                 pv = &cell_control->input_register;
        //                 break;
        //             case TYPE_HOLDING_REGISTER:
        //                  pv = &cell_control->holding_register;
        //                 break;
        //             default:
        //                 break;
        //         }
        //
        //         if(pv) {
        //             float v = endian_float(endian_format, (uint8_t *)pv);
        //             v = v*multiplier;
        //             jn_value = json_real(v);
        //         }
        //     }
        //     break;

        case FORMAT_FLOAT:
            {
                uint8_t b[4];
                if(collect_register_bytes(gobj, pslv, object_type, address, 2, b)==0) {
                    float v = endian_float(endian_format, b);
                    jn_value = json_real(v * multiplier);
                }
            }
            break;

        // case FORMAT_DOUBLE:
        //     {
        //         uint16_t *pv = 0;
        //         switch(object_type) {
        //             case TYPE_COIL:
        //             case TYPE_DISCRETE_INPUT:
        //                 jn_value = cell_control->control.bit_value?json_integer(1):json_integer(0);
        //                 break;
        //             case TYPE_INPUT_REGISTER:
        //                 pv = &cell_control->input_register;
        //                 break;
        //             case TYPE_HOLDING_REGISTER:
        //                 pv = &cell_control->holding_register;
        //                 break;
        //             default:
        //                 break;
        //         }
        //
        //         if(pv) {
        //             double v = endian_double(endian_format, (uint8_t *)pv);
        //             v = v*multiplier;
        //             jn_value = json_real(v);
        //         }
        //     }
        //     break;

        case FORMAT_DOUBLE:
            {
                uint8_t b[8];
                if(collect_register_bytes(gobj, pslv, object_type, address, 4, b)==0) {
                    double v = endian_double(endian_format, b);
                    jn_value = json_real(v * multiplier);
                }
            }
            break;

        // case FORMAT_STRING:
        //     {
        //         int size = (int)kw_get_int(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
        //         gbuffer_t *gbuf_string = gbuffer_create(size*2, size*2);
        //
        //         for(int i=0; i<size; i++) {
        //             uint16_t *pv = 0;
        //             cell_control_t *cell2 = get_cell_control(gobj, pslv, object_type, address+i, FALSE);
        //             switch(object_type) {
        //                 case TYPE_INPUT_REGISTER:
        //                     pv = &cell2->input_register;
        //                     break;
        //                 case TYPE_HOLDING_REGISTER:
        //                     pv = &cell2->holding_register;
        //                     break;
        //                 default:
        //                     break;
        //             }
        //
        //             uint32_t word = endian_16(endian_format, (uint8_t *)pv);
        //             uint8_t b1 = (uint8_t)(word >> 8); // get the higher byte;
        //             uint8_t b2 = (uint8_t)(word & 0xFF); // get the lower byte
        //             // convert the nulls into space
        //             if(b1==0) {
        //                 b1 = ' ';
        //             }
        //             if(b2==0) {
        //                 b2 = ' ';
        //             }
        //             gbuffer_append(gbuf_string, &b1, 1);
        //             gbuffer_append(gbuf_string, &b2, 1);
        //         }
        //         left_justify(gbuffer_cur_rd_pointer(gbuf_string));
        //         jn_value = json_string(gbuffer_cur_rd_pointer(gbuf_string));
        //         GBUFFER_DECREF(gbuf_string)
        //     }
        //     break;

        case FORMAT_STRING:
            {
                /*
                 *  Prefer 'length_bytes', fallback to historic 'multiplier' (=bytes).
                 */
                int bytes = (int)kw_get_int(gobj, jn_variable, "length_bytes", -1, KW_WILD_NUMBER);
                if(bytes <= 0) {
                    bytes = (int)kw_get_int(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
                }
                if(bytes <= 0) {
                    bytes = 1;
                }

                int words = (bytes + 1) / 2;                /* registers needed */
                gbuffer_t *gbuf = gbuffer_create(bytes, bytes);
                if(!gbuf) break;

                /* Gather raw bytes (MSB,LSB per register). */
                uint8_t tmp[2*words];
                if(collect_register_bytes(gobj, pslv, object_type, address, words, tmp) != 0) {
                    GBUFFER_DECREF(gbuf);
                    break;
                }

                /*
                 *  Apply endian to string:
                   - Within each register (word) swap if little-endian variants.
                   - If *_BYTE_SWAP, also swap adjacent 16-bit words in each 32-bit block (CDAB).
                   This mirrors your numeric semantics for CDAB/BADC.
                */
                for(int w=0; w<words; w++) {
                    uint8_t hi = tmp[(w<<1)+0]; /* MSB */
                    uint8_t lo = tmp[(w<<1)+1]; /* LSB */

                    switch(endian_format) {
                        case FORMAT_BIG_ENDIAN:
                        case FORMAT_BIG_ENDIAN_BYTE_SWAP:
                            tmp[(w<<1)+0] = hi; tmp[(w<<1)+1] = lo; /* keep */
                            break;
                        case FORMAT_LITTLE_ENDIAN:
                        case FORMAT_LITTLE_ENDIAN_BYTE_SWAP:
                            tmp[(w<<1)+0] = lo; tmp[(w<<1)+1] = hi; /* swap bytes inside word */
                            break;
                    }
                }
                if(endian_format==FORMAT_BIG_ENDIAN_BYTE_SWAP ||
                    endian_format==FORMAT_LITTLE_ENDIAN_BYTE_SWAP
                ) {
                    for(int i=0; i+3 < 2*words; i+=4) {
                        /* swap word0 <-> word1 in each 4-byte block: [0,1,2,3] -> [2,3,0,1] */
                        uint8_t a=tmp[i], b=tmp[i+1], c=tmp[i+2], d=tmp[i+3];
                        tmp[i]=c; tmp[i+1]=d; tmp[i+2]=a; tmp[i+3]=b;
                    }
                }

                /*
                 *  Emit exactly 'bytes' bytes (trim the padding byte if odd).
                 */
                gbuffer_append(gbuf, tmp, bytes);

                /*
                 *  Optional: treat NULs as spaces like your legacy code
                 *  (toggle via a flag if you want).
                 */
                uint8_t *s = (uint8_t *)gbuffer_cur_rd_pointer(gbuf);
                for(int i=0; i<bytes; i++) {
                    if(s[i]==0) s[i]=' ';
                }

                left_justify((char *)s);               /* your existing helper */
                jn_value = json_string((char *)s);
                GBUFFER_DECREF(gbuf);
            }
            break;
    }

    return jn_value;
}

/***************************************************************************
 *  How many cells a variable of this format spans: registers, or one bit
 ***************************************************************************/
PRIVATE int variable_cells(hgobj gobj, json_t *jn_variable, variable_format_t variable_format)
{
    switch(variable_format) {
        case FORMAT_BOOL:
        case FORMAT_INT16:
        case FORMAT_UINT16:
            return 1;

        case FORMAT_INT32:
        case FORMAT_UINT32:
        case FORMAT_FLOAT:
            return 2;

        case FORMAT_INT64:
        case FORMAT_UINT64:
        case FORMAT_DOUBLE:
            return 4;

        case FORMAT_STRING:
            {
                /*
                 *  Prefer 'length_bytes', fallback to historic 'multiplier' (=bytes).
                 */
                int bytes = (int)kw_get_int(gobj, jn_variable, "length_bytes", -1, KW_WILD_NUMBER);
                if(bytes <= 0) {
                    bytes = (int)kw_get_int(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
                }
                if(bytes <= 0) {
                    bytes = 1;
                }
                return (bytes + 1) / 2;
            }
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "variable format UNKNOWN",
        "format",       "%d", (int)variable_format,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  TRUE when every cell of the variable was stored after the last
 *  publication, that is, its read of this cycle was answered
 ***************************************************************************/
PRIVATE BOOL variable_updated(hgobj gobj, slave_data_t *pslv, json_t *jn_variable)
{
    const char *type = kw_get_str(gobj, jn_variable, "type", "", KW_REQUIRED);
    modbus_object_type_t object_type = get_object_type(gobj, type);
    const char *format = kw_get_str(gobj, jn_variable, "format", "", KW_REQUIRED);
    variable_format_t variable_format = get_variable_format(gobj, format);
    if(object_type < 0 || variable_format < 0) {
        // Error already logged
        return FALSE;
    }
    int address = (int)kw_get_int(gobj, jn_variable, "address", -1, KW_REQUIRED|KW_WILD_NUMBER);

    int cells = variable_cells(gobj, jn_variable, variable_format);
    if(cells <= 0) {
        // Error already logged
        return FALSE;
    }
    for(int i=0; i<cells; i++) {
        cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address+i, FALSE);
        if(!cell_control) {
            // Error already logged
            return FALSE;
        }
        if(!cell_control->control.updated) {
            return FALSE;
        }
    }
    return TRUE;
}

/***************************************************************************
 *  The cells of the slave wait for a new read
 ***************************************************************************/
PRIVATE int clear_updated_cells(slave_data_t *pslv)
{
    const char *key; json_t *jn_type;
    json_object_foreach(pslv->x_control, key, jn_type) {
        const char *key2; json_t *jn_cell;
        json_object_foreach(jn_type, key2, jn_cell) {
            cell_control_t *cell_control = (cell_control_t *)(uintptr_t)json_integer_value(jn_cell);
            if(cell_control) {
                cell_control->control.updated = 0;
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  The key of the block in flight in jn_failing_blocks
 ***************************************************************************/
PRIVATE int current_block_key(hgobj gobj, char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int slave_id = (int)kw_get_int(gobj, priv->cur_slave_, "id", 0, 0);
    snprintf(bf, bfsize, "%d:%d", slave_id, priv->idx_mapping);
    return slave_id;
}

/***************************************************************************
 *  A block of the cycle was not answered: logged when it starts failing,
 *  or fails another way, not on every cycle. A unit that does not exist
 *  answers the same exception every cycle, and that is one fact.
 ***************************************************************************/
PRIVATE int note_block_failed(hgobj gobj, const char *msg, int error_code)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char key[64];
    int slave_id = current_block_key(gobj, key, sizeof(key));

    char why[64];
    snprintf(why, sizeof(why), "%s %d", msg, error_code);

    json_t *jn_why = json_object_get(priv->jn_failing_blocks, key);
    if(jn_why && strcmp(json_string_value(jn_why), why)==0) {
        return 0;
    }
    json_object_set_new(priv->jn_failing_blocks, key, json_string(why));

    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_PROTOCOL,
        "msg",              "%s", msg,
        "slave_id",         "%d", slave_id,
        "error_code",       "%d", error_code,
        "error_name",       "%s", error_code? modbus_exception_name(error_code) : "",
        "block",            "%j", priv->cur_map_?priv->cur_map_:json_null(),
        "note",             "%s", "logged again only when the block answers or fails another way",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  A block of the cycle was answered: if it was failing, say it is back
 ***************************************************************************/
PRIVATE int note_block_answered(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char key[64];
    int slave_id = current_block_key(gobj, key, sizeof(key));

    json_t *jn_why = json_object_get(priv->jn_failing_blocks, key);
    if(!jn_why) {
        return 0;
    }

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_PROTOCOL,
        "msg",              "%s", "Modbus block answered again",
        "slave_id",         "%d", slave_id,
        "failed_with",      "%s", json_string_value(jn_why),
        "block",            "%j", priv->cur_map_?priv->cur_map_:json_null(),
        NULL
    );
    json_object_del(priv->jn_failing_blocks, key);
    return 0;
}

/***************************************************************************
 *  A request as a log may show it: without the values of a write. A write
 *  can carry a secret -- the password a meter asks before its setup goes in
 *  a write like any other -- and the log goes to the logcenter. Returns a
 *  new reference.
 ***************************************************************************/
PRIVATE json_t *request_to_log(json_t *jn_request)
{
    if(!json_is_object(jn_request)) {
        return json_incref(jn_request? jn_request : json_null());
    }
    json_t *jn = json_copy(jn_request);
    if(json_object_get(jn, "value")) {
        json_object_set_new(jn, "value", json_string("(not logged)"));
    }
    return jn;
}

/***************************************************************************
 *  A list of requests as a log may show it. Returns a new reference.
 ***************************************************************************/
PRIVATE json_t *requests_to_log(json_t *jn_requests)
{
    json_t *jn = json_array();
    size_t idx; json_t *jn_request;
    json_array_foreach(jn_requests, idx, jn_request) {
        json_array_append_new(jn, request_to_log(jn_request));
    }
    return jn;
}

/***************************************************************************
 *  A number of a request, read as the command parser reads an integer
 *  parameter (command_parameter_from_json()): a json integer, a real with
 *  no decimals, a boolean as 1 or 0, or the WHOLE text of a JSON integer
 *  ("20", "-3"; not "abc", "010", "20.5", "0x14", " 20", "12ab"). FALSE
 *  when it is not one, or there is none (null, ""): the caller says why.
 ***************************************************************************/
PRIVATE BOOL request_integer(json_t *jn, json_int_t *value)
{
    const char *refused = NULL;
    json_t *jn_int = command_parameter_from_json(DTP_INTEGER, jn, &refused);
    if(!jn_int) {
        return FALSE;
    }
    *value = json_integer_value(jn_int);
    JSON_DECREF(jn_int)
    return TRUE;
}

/***************************************************************************
 *  A request is checked when it is queued, and filled in: operation, id,
 *  address and size as numbers, the value of a write as an array of them.
 *  Returns NULL when it can be sent, else why not (a json string, owned by
 *  the caller). Why never shows a value of a write: it goes to the log.
 ***************************************************************************/
PRIVATE json_t *check_request(hgobj gobj, json_t *jn_request)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!json_is_object(jn_request)) {
        return json_string("a request is a json object");
    }

    const char *operation = "write";
    json_t *jn_operation = json_object_get(jn_request, "operation");
    if(jn_operation) {
        if(!json_is_string(jn_operation)) {
            return json_string("operation is write or read");
        }
        operation = json_string_value(jn_operation);
    }
    BOOL is_write;
    if(strcmp(operation, "write")==0) {
        is_write = TRUE;
    } else if(strcmp(operation, "read")==0) {
        is_write = FALSE;
    } else {
        return json_sprintf("operation '%s' unknown: write or read", operation);
    }

    json_int_t slave_id = 1;
    json_t *jn_id = json_object_get(jn_request, "id");
    if(jn_id && !request_integer(jn_id, &slave_id)) {
        return json_string("the slave id is not a number");
    }
    if(slave_id < 0 || slave_id > 255) {
        return json_sprintf("slave id %lld out of 0-255", (long long)slave_id);
    }
    if(slave_id > 247 && strcasecmp(priv->modbus_protocol, "RTU")==0) {
        return json_sprintf("slave id %lld is reserved in Modbus RTU (248-255)", (long long)slave_id);
    }

    json_int_t address;
    json_t *jn_address = json_object_get(jn_request, "address");
    if(!jn_address) {
        return json_string("a request needs an address");
    }
    if(!request_integer(jn_address, &address)) {
        return json_string("the address is not a number");
    }
    if(address < 0 || address > 0xFFFF) {
        return json_sprintf("address %lld out of 0-65535", (long long)address);
    }

    json_t *jn_type = json_object_get(jn_request, "type");
    const char *type = json_is_string(jn_type)? json_string_value(jn_type) : "";
    modbus_object_type_t object_type = object_type_of(type);
    if(object_type == TYPE_UNKNOWN) {
        return json_sprintf("object type '%s' unknown", type);
    }

    json_int_t size = 1;
    json_t *jn_size = json_object_get(jn_request, "size");
    if(jn_size && !request_integer(jn_size, &size)) {
        return json_string("the size is not a number");
    }

    int max_size;
    if(is_write) {
        if(object_type == TYPE_COIL) {
            max_size = MODBUS_MAX_WRITE_BITS;
        } else if(object_type == TYPE_HOLDING_REGISTER) {
            max_size = MODBUS_MAX_WRITE_REGISTERS;
        } else {
            return json_sprintf("'%s' cannot be written: holding_register or coil", type);
        }

        /*
         *  The values are named by their position, never shown
         */
        json_t *jn_value = json_object_get(jn_request, "value");
        if(!jn_value || json_is_null(jn_value)) {
            return json_string("a write needs a value: a number, or an array of them");
        }
        json_t *jn_given = json_is_array(jn_value)? json_incref(jn_value) : json_pack("[O]", jn_value);
        json_t *jn_values = json_array();
        size_t idx; json_t *jn_v;
        json_array_foreach(jn_given, idx, jn_v) {
            json_int_t v;
            if(!request_integer(jn_v, &v)) {
                JSON_DECREF(jn_given)
                JSON_DECREF(jn_values)
                return json_sprintf("value %d of the write is not a number", (int)idx);
            }
            if(v < -32768 || v > 65535) {
                JSON_DECREF(jn_given)
                JSON_DECREF(jn_values)
                return json_sprintf(
                    "value %d of the write does not fit in a register (-32768..65535)", (int)idx
                );
            }
            json_array_append_new(jn_values, json_integer(v));
        }
        JSON_DECREF(jn_given)

        json_int_t n_values = (json_int_t)json_array_size(jn_values);
        if(jn_size && size != n_values) {
            JSON_DECREF(jn_values)
            return json_sprintf("size %lld but %lld values", (long long)size, (long long)n_values);
        }
        size = n_values;
        json_object_set_new(jn_request, "value", jn_values);

    } else {
        max_size = (object_type == TYPE_COIL || object_type == TYPE_DISCRETE_INPUT)?
            MODBUS_MAX_READ_BITS : MODBUS_MAX_READ_REGISTERS;
    }

    if(size < 1 || size > max_size) {
        return json_sprintf("size %lld out of 1-%d", (long long)size, max_size);
    }
    if(address + size - 1 > 0xFFFF) {
        return json_sprintf("address %lld and size %lld pass 65535", (long long)address, (long long)size);
    }

    json_object_set_new(jn_request, "operation", json_string(operation));
    json_object_set_new(jn_request, "id", json_integer(slave_id));
    json_object_set_new(jn_request, "address", json_integer(address));
    json_object_set_new(jn_request, "size", json_integer(size));
    return NULL;
}

/***************************************************************************
 *  Answer a request to the gobj above. jn_request, jn_values and
 *  jn_comment are owned.
 ***************************************************************************/
PRIVATE int answer_request(
    hgobj gobj,
    json_t *jn_request,
    int result,
    int error_code,
    json_t *jn_values,
    json_t *jn_comment
)
{
    json_t *kw = json_object();
    json_object_set_new(kw, "request", jn_request? jn_request : json_null());
    json_object_set_new(kw, "result", json_integer(result));
    json_object_set_new(kw, "comment", jn_comment? jn_comment : json_string(""));
    json_object_set_new(kw, "error_code", json_integer(error_code));
    if(jn_values) {
        json_object_set_new(kw, "values", jn_values);
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        json_t *jn_log = json_copy(kw);
        json_object_set_new(jn_log, "request", request_to_log(json_object_get(kw, "request")));
        gobj_trace_json(gobj, jn_log, "ANSWER %s", gobj_short_name(gobj));
        JSON_DECREF(jn_log)
    }
    return gobj_publish_event(gobj, EV_MODBUS_ANSWER, kw);
}

/***************************************************************************
 *  What the answer of the request in flight said, kept until the frame is
 *  done with. jn_values and jn_comment are owned.
 ***************************************************************************/
PRIVATE int set_current_answer(
    hgobj gobj,
    int result,
    int error_code,
    json_t *jn_values,
    json_t *jn_comment
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_current_answer)
    priv->jn_current_answer = json_object();
    json_object_set_new(priv->jn_current_answer, "result", json_integer(result));
    json_object_set_new(priv->jn_current_answer, "error_code", json_integer(error_code));
    json_object_set_new(priv->jn_current_answer, "comment",
        jn_comment? jn_comment : json_string("")
    );
    if(jn_values) {
        json_object_set_new(priv->jn_current_answer, "values", jn_values);
    }
    return 0;
}

/***************************************************************************
 *  Answer the request in flight with what was set for it, and forget it
 ***************************************************************************/
PRIVATE int answer_current_request(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_request = priv->jn_current_request;
    json_t *jn_answer = priv->jn_current_answer;
    priv->jn_current_request = NULL;
    priv->jn_current_answer = NULL;

    if(!jn_answer) {
        json_t *jn_log = request_to_log(jn_request? jn_request : json_null());
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Modbus request answered WITHOUT reading its answer",
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        return answer_request(gobj, jn_request, -1, 0, NULL,
            json_string("its answer could not be read, see the log")
        );
    }

    json_t *jn_values = json_object_get(jn_answer, "values");
    int ret = answer_request(
        gobj,
        jn_request,
        (int)json_integer_value(json_object_get(jn_answer, "result")),
        (int)json_integer_value(json_object_get(jn_answer, "error_code")),
        jn_values? json_incref(jn_values) : NULL,
        json_incref(json_object_get(jn_answer, "comment"))
    );
    JSON_DECREF(jn_answer)
    return ret;
}

/***************************************************************************
 *  Answer as failed every queued request, which will never be sent
 ***************************************************************************/
PRIVATE int answer_queued_requests(hgobj gobj, const char *why)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A new queue first: a subscriber that sends again on the answer
     *  must not land in the one being emptied. With no queue the gobj is
     *  stopped (mt_stop logged what it held): the transport disconnects
     *  after it.
     */
    json_t *jn_queue = priv->jn_request_queue;
    if(!jn_queue) {
        return 0;
    }
    priv->jn_request_queue = json_array();

    if(json_array_size(jn_queue) > 0) {
        json_t *jn_log = requests_to_log(jn_queue);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Modbus requests NOT sent, answered as failed",
            "why",          "%s", why,
            "requests",     "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
    }

    size_t idx; json_t *jn_request;
    json_array_foreach(jn_queue, idx, jn_request) {
        answer_request(gobj, json_incref(jn_request), -1, 0, NULL, json_string(why));
    }
    JSON_DECREF(jn_queue)
    return 0;
}

/***************************************************************************
 *  The response to the request in flight: the registers or bits of a read,
 *  the echo of a write. What it says is set as the answer of the request.
 ***************************************************************************/
PRIVATE int read_request_answer(hgobj gobj, uint8_t *bf, int len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_request = priv->jn_current_request;
    int slave_id = (int)priv->frame_head.slave_id;
    int modbus_function = (int)priv->frame_head.function;
    int byte_count = (int)priv->frame_head.byte_count;
    int req_slave_id = (int)kw_get_int(gobj, jn_request, "id", 0, KW_REQUIRED);
    int address = (int)kw_get_int(gobj, jn_request, "address", 0, KW_REQUIRED);
    int size = (int)kw_get_int(gobj, jn_request, "size", 0, KW_REQUIRED);

    json_t *jn_why = NULL;
    json_t *jn_values = NULL;

    if(slave_id != req_slave_id) {
        jn_why = json_sprintf("answered by slave %d, asked to slave %d", slave_id, req_slave_id);
    } else if(modbus_function != priv->modbus_function) {
        jn_why = json_sprintf("answered function %d %s, asked function %d %s",
            modbus_function, modbus_function_name(modbus_function),
            priv->modbus_function, modbus_function_name(priv->modbus_function)
        );
    } else {
        switch(modbus_function) {
            case MODBUS_FC_READ_COILS:
            case MODBUS_FC_READ_DISCRETE_INPUTS:
                if(byte_count != len || byte_count != (size + 7)/8) {
                    jn_why = json_sprintf("a read of %d bits answered with %d bytes", size, len);
                    break;
                }
                jn_values = json_array();
                for(int i=0; i<size; i++) {
                    json_array_append_new(jn_values, json_integer((bf[i/8] >> (i%8)) & 1));
                }
                break;

            case MODBUS_FC_READ_HOLDING_REGISTERS:
            case MODBUS_FC_READ_INPUT_REGISTERS:
                if(byte_count != len || byte_count != 2*size) {
                    jn_why = json_sprintf("a read of %d registers answered with %d bytes", size, len);
                    break;
                }
                jn_values = json_array();
                for(int i=0; i<size; i++) {
                    json_array_append_new(jn_values, json_integer((bf[2*i] << 8) | bf[2*i + 1]));
                }
                break;

            case MODBUS_FC_WRITE_SINGLE_COIL:
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                {
                    if(len != 3) {
                        jn_why = json_sprintf("the echo of a write has %d bytes, not 4", len + 1);
                        break;
                    }
                    int echo_address = (byte_count << 8) | bf[0];
                    int echo_value = (bf[1] << 8) | bf[2];
                    json_int_t first = json_integer_value(
                        json_array_get(kw_get_list(gobj, jn_request, "value", 0, KW_REQUIRED), 0)
                    );
                    int expected;
                    if(modbus_function == MODBUS_FC_WRITE_SINGLE_COIL) {
                        expected = first? 0xFF00 : 0x0000;
                    } else if(modbus_function == MODBUS_FC_WRITE_SINGLE_REGISTER) {
                        expected = (uint16_t)first;
                    } else {
                        expected = size;    // the echo of a multiple write is its quantity
                    }
                    if(echo_address != address || echo_value != expected) {
                        // No values: a write can carry a secret, and this goes to the log
                        jn_why = json_sprintf(
                            "the echo of the write does not match it (address %d, sent to %d)",
                            echo_address, address
                        );
                    }
                }
                break;

            default:
                jn_why = json_sprintf("function %d %s cannot answer a request",
                    modbus_function, modbus_function_name(modbus_function)
                );
                break;
        }
    }

    if(jn_why) {
        json_t *jn_log = request_to_log(jn_request);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Modbus answer of a request does not match it",
            "why",          "%s", json_string_value(jn_why),
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
            if(frame_is_hidden(gobj)) {
                gobj_trace_msg(gobj, "answer: <%d bytes hidden>", len);
            } else {
                gobj_trace_dump(gobj, (const char *)bf, (size_t)(len < 64? len : 64), "answer");
            }
        }
        JSON_DECREF(jn_values)
        set_current_answer(gobj, -1, 0, NULL, jn_why);
        return -1;
    }

    set_current_answer(gobj, 0, 0, jn_values, NULL);
    return 0;
}

/***************************************************************************
 *  Publish, slave by slave, what this cycle read
 *
 *  Only what it read. A cell whose read timed out or was answered with an
 *  exception keeps the value of an earlier cycle, or zero if none ever came,
 *  and would reach the subscriber as a new reading. A variable
 *  whose cells were not all stored since the last publication is left out,
 *  and a slave with none is not published at all: its failed reads were
 *  logged where they failed.
 *
 *  The cells are cleared after the whole slave was decoded, not variable by
 *  variable: two variables can share a cell.
 ***************************************************************************/
PRIVATE int build_message_to_publish(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t idx_slaves; json_t *jn_slave;
    json_array_foreach(priv->slaves_, idx_slaves, jn_slave) {
        int slave_id = (int)kw_get_int(gobj, jn_slave, "id", 0, KW_REQUIRED);
        slave_data_t *pslv = get_slave_data(gobj, slave_id, TRUE);
        if(!pslv) {
            continue;
        }
        json_t *jn_conversion = kw_get_list(gobj, jn_slave, "conversion", 0, KW_REQUIRED);
        if(!jn_conversion) {
            continue;
        }

        json_t *kw_data = json_object();
        json_object_set_new(kw_data, "slave_id", json_integer(slave_id));
        int n_read = 0;
        int n_not_read = 0;

        size_t idx_conversion; json_t *jn_variable;
        json_array_foreach(jn_conversion, idx_conversion, jn_variable) {
            BOOL disabled = kw_get_bool(gobj, jn_variable, "disabled", 0, 0);
            if(disabled) {
                continue;
            }
            if(!variable_updated(gobj, pslv, jn_variable)) {
                n_not_read++;
                continue;
            }
            const char *variable_id = kw_get_str(gobj, jn_variable, "id", "", KW_REQUIRED);
            json_t *jn_value = get_variable_value(gobj, pslv, jn_variable);
            json_object_set_new(kw_data, variable_id, jn_value);
            n_read++;
        }
        clear_updated_cells(pslv);

        if(n_not_read > 0 && (gobj_trace_level(gobj) & TRACE_MESSAGES)) {
            gobj_trace_msg(gobj, "slave %d: %d variables not read this cycle, left out",
                slave_id, n_not_read
            );
        }
        if(n_read == 0) {
            JSON_DECREF(kw_data)
            continue;
        }

        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_json(gobj, kw_data, "PUBLISH %s", gobj_short_name(gobj));
        }
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw_data);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_conversion_variable(hgobj gobj, slave_data_t *pslv, json_t *jn_variable)
{
    int slave_id = pslv->slave_id;

    const char *type = kw_get_str(gobj, jn_variable, "type", "", KW_REQUIRED);

    json_object_set_new(jn_variable, "disabled", json_false());

    modbus_object_type_t object_type = get_object_type(gobj, type);
    if(object_type < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Conversion: bad object type",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }
    int address = (int)kw_get_int(gobj, jn_variable, "address", -1, KW_REQUIRED|KW_WILD_NUMBER);
    if(address < 0 || address > 0xFFFF) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Conversion: bad address",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }
    const char *variable_id = kw_get_str(gobj, jn_variable, "id", "", 0);
    if(empty_string(variable_id)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Conversion: variable id empty",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }
    const char *format = kw_get_str(gobj, jn_variable, "format", "", KW_REQUIRED);
    variable_format_t variable_format = get_variable_format(gobj, format);
    if(variable_format < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Conversion: variable format UNKNOWN",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }

    const char *endian = kw_get_str(gobj, jn_variable, "endian", "big endian", 0);
    endian_format_t endian_format = get_endian_format(gobj, endian);
    if(endian_format < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Conversion: endian format UNKNOWN",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }

    int compound_value = variable_cells(gobj, jn_variable, variable_format);

    for(int i=0; i<compound_value; i++) {
        cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address+i, FALSE);
        if(!cell_control || !cell_control->control.value_busy) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Conversion: cell not defined",
                "slave_id",     "%d", slave_id,
                "type",         "%s", type,
                "object_type",  "%d", object_type,
                "address",      "%d", address,
                "i",            "%d", i,
                "variable",     "%j", jn_variable,
                NULL
            );
            json_object_set_new(jn_variable, "disabled", json_true());
            break;
        }
        if(cell_control->control.compound_value) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Conversion: OVERRIDE compound value",
                "slave_id",     "%d", slave_id,
                "type",         "%s", type,
                "object_type",  "%d", object_type,
                "address",      "%d", address,
                "i",            "%d", i,
                "variable",     "%j", jn_variable,
                NULL
            );
            json_object_set_new(jn_variable, "disabled", json_true());
            break;
        }
        if(compound_value > 1) {
            cell_control->control.compound_value = 1;
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_conversion_variables(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t idx_slaves; json_t *jn_slave;
    json_array_foreach(priv->slaves_, idx_slaves, jn_slave) {
        int slave_id = (int)kw_get_int(gobj, jn_slave, "id", 0, KW_REQUIRED);
        slave_data_t *pslv = get_slave_data(gobj, slave_id, TRUE);
        if(!pslv) {
            continue;
        }
        json_t *jn_conversion = kw_get_list(gobj, jn_slave, "conversion", 0, KW_REQUIRED);
        if(!jn_conversion) {
            continue;
        }

        size_t idx_conversion; json_t *jn_variable;
        if(kw_get_bool(gobj, jn_slave, "disabled", 0, 0)) {
            // Error already logged: a slave that is not polled has no cell to convert
            json_array_foreach(jn_conversion, idx_conversion, jn_variable) {
                json_object_set_new(jn_variable, "disabled", json_true());
            }
            continue;
        }
        json_array_foreach(jn_conversion, idx_conversion, jn_variable) {
            check_conversion_variable(gobj, pslv, jn_variable);
        }
    }

    return 0;
}

/***************************************************************************
 *  XXX
 ***************************************************************************/
PRIVATE cell_control_t *get_cell_control(
    hgobj gobj,
    slave_data_t *pslv,
    modbus_object_type_t object_type,
    int32_t address,
    BOOL create
) {
    json_t *jn_type = kw_get_dict(gobj, pslv->x_control, get_object_type_name(object_type), 0, KW_REQUIRED);
    if(!jn_type) {
        return NULL;
    }
    char saddress[32];
    snprintf(saddress, sizeof(saddress), "%04X", (int)address);
    if(create) {
        cell_control_t *new_cell_control = GBMEM_MALLOC(sizeof(cell_control_t));
        json_object_set_new(
            jn_type,
            saddress,
            json_integer((json_int_t)(uintptr_t)new_cell_control)
        );
    }
    cell_control_t *cell_control = (cell_control_t *)(uintptr_t)kw_get_int(
        gobj,
        jn_type,
        saddress,
        0,
        KW_REQUIRED
    );
    return cell_control;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    RESET_MACHINE();
    priv->resume_wait_ms = 0;

    gobj_change_state(gobj, ST_CONNECTED);

    priv->inform_on_close = TRUE;
    gobj_publish_event(gobj, EV_ON_OPEN, 0);

    set_timeout(priv->gobj_timer, 1*1000);
    priv->waiting = TRUE;

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    RESET_MACHINE();
    priv->resume_wait_ms = 0;
    priv->waiting = FALSE;

    clear_timeout(priv->gobj_timer);

    /*
     *  What the cycle read before the connection dropped is not a reading of
     *  the cycle that publishes after it comes back. An orderly stop has
     *  freed the slave data before stopping the transport: nothing to clear.
     */
    if(priv->slave_data) {
        slave_data_t *pslv = priv->slave_data;
        for(int i=0; i<priv->max_slaves; i++) {
            clear_updated_cells(pslv);
            pslv++;
        }
    }

    /*
     *  A request is valid for the connection it was queued in: whatever
     *  unlocked it there (a meter password) is gone with it
     */
    if(priv->jn_current_request) {
        json_t *jn_log = request_to_log(priv->jn_current_request);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Modbus request NOT confirmed, the connection dropped before its response",
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        set_current_answer(gobj, -1, 0, NULL,
            json_string("the connection dropped before its answer")
        );
        answer_current_request(gobj);
    }
    answer_queued_requests(gobj, "the connection dropped before it was sent");

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        gobj_publish_event(gobj, EV_ON_CLOSE, 0);
    }

    JSON_DECREF(kw)
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------------*
     *   Recupera el frame
     *---------------------------------------------*/
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    /*---------------------------------------------*
     *  Over TCP the bytes that follow an answer are the start of the next
     *  frame: dropped, they would leave the stream in the middle of one,
     *  and the answer after it lost. They came before the next request
     *  went out: the frame they start is not its answer (stale).
     *  Over RTU nothing says where a frame ends but its own head: a noise
     *  byte after an answer (the line released by the slave) taken as the
     *  start of a frame would swallow the next answer. There they are
     *  dropped, as the next answer starts a read of its own.
     *---------------------------------------------*/
    BOOL is_tcp = (strcasecmp(priv->modbus_protocol, "TCP")==0)? TRUE : FALSE;
    BOOL stale = FALSE;
    int lnn;
    BOOL fin = FALSE;
    while(!fin && (lnn=(int)gbuffer_leftbytes(gbuf))>0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        BOOL frame_complete = FALSE;

        switch(priv->st) {
        case WAIT_HEAD:
            {
                int n = framehead_consume(
                    gobj, &priv->frame_head, priv->istream_head, bf, lnn, stale
                );
                if(n < 0) {
                    // Error already logged: nothing says where the next frame starts
                    fin = TRUE;
                    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                    break;
                }
                if(n == 0) {
                    fin = TRUE;
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "modbus framehead_consume() consumed nothing",
                        "st",           "%d", (int)priv->st,
                        NULL
                    );
                    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                    break;
                }
                gbuffer_get(gbuf, (size_t)n);  // take out the bytes consumed

                if(!priv->frame_head.header_complete) {
                    break;  // wait more data
                }
                if(priv->frame_head.payload_length == 0) {
                    frame_complete = TRUE;  // a TCP exception, or an MBAP of length 3
                    break;
                }

                /*
                 *  Creat a new buffer for payload data
                 */
                ISTREAM_CREATE(
                    priv->istream_payload,
                    gobj,
                    priv->frame_head.payload_length,
                    priv->frame_head.payload_length
                );
                if(!priv->istream_payload) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY,
                        "msg",          "%s", "no memory for istream_payload",
                        "payload_length", "%d", (int)priv->frame_head.payload_length,
                        NULL
                    );
                    fin = TRUE;
                    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                    break;
                }
                istream_read_until_num_bytes(
                    priv->istream_payload,
                    priv->frame_head.payload_length,
                    0
                );
                priv->st = WAIT_PAYLOAD;
            }
            break;
        case WAIT_PAYLOAD:
            {
                int consumed = istream_consume(priv->istream_payload, bf, lnn);
                if(consumed > 0) {
                    gbuffer_get(gbuf, consumed);  // take out the bytes consumed
                }
                if(istream_is_completed(priv->istream_payload)) {
                    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
                        if(frame_is_hidden(gobj)) {
                            gbuffer_set_secret(istream_get_gbuffer(priv->istream_payload), TRUE);
                        }
                        gobj_trace_dump_gbuf(gobj,
                           istream_get_gbuffer(priv->istream_payload), "%s", gobj_short_name(src)
                       );
                    }
                    frame_complete = TRUE;
                }
            }
            break;
        }

        if(frame_complete) {
            if(priv->frame_head.discard) {
                /*
                 *  Not the answer awaited (logged): read whole and dropped,
                 *  and what is in flight goes on waiting for its own
                 */
                reset_frame(gobj);
            } else {
                BOOL hidden = frame_is_hidden(gobj);
                frame_completed(gobj);
                finish_answer(gobj);

                /*
                 *  Its subscriber may have stopped the gobj, or dropped the
                 *  connection, on the answer: nothing is read for it then
                 */
                if(!gobj_is_running(gobj) || gobj_in_this_state(gobj, ST_DISCONNECTED)) {
                    fin = TRUE;
                } else if(!is_tcp && gbuffer_leftbytes(gbuf) > 0) {
                    fin = TRUE;
                    drop_bytes_after_answer(gobj, gbuf, hidden);
                }
                stale = TRUE;
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/********************************************************************
 *  A request from above: checked, queued, and sent at once if the
 *  connection is idle, else after the block or request in flight
 ********************************************************************/
PRIVATE int ac_enqueue_tx_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_request = json_deep_copy(kw);
    KW_DECREF(kw)

    json_t *jn_why = check_request(gobj, jn_request);
    if(jn_why) {
        json_t *jn_log = request_to_log(jn_request);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Modbus request refused",
            "why",          "%s", json_string_value(jn_why),
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        answer_request(gobj, jn_request, -1, 0, NULL, jn_why);
        return -1;
    }

    json_array_append_new(priv->jn_request_queue, jn_request);

    if(priv->waiting && !priv->jn_current_request) {
        if(send_request(gobj)) {
            priv->resume_wait_ms = priv->timeout_polling;
        }
    }
    return 0;
}

/********************************************************************
 *  A request with no connection to send it on is answered at once
 ********************************************************************/
PRIVATE int ac_refuse_request(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    json_t *jn_request = json_deep_copy(kw);
    KW_DECREF(kw)

    json_t *jn_log = request_to_log(jn_request);
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "Modbus request refused: not connected",
        "url",          "%s", gobj_read_str_attr(gobj, "url"),
        "request",      "%j", jn_log,
        NULL
    );
    JSON_DECREF(jn_log)
    answer_request(gobj, jn_request, -1, 0, NULL, json_string("not connected"));
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_polling(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->waiting = FALSE;

    /*
     *  Next map
     */
    continue_polling(gobj, priv->timeout_polling);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_response(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->jn_current_request) {
        json_t *jn_log = request_to_log(priv->jn_current_request);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Modbus Timeout",
            "request",      "%j", jn_log,
            NULL
        );
        JSON_DECREF(jn_log)
        set_current_answer(gobj, -1, 0, NULL,
            json_sprintf("no answer in %d seconds", priv->timeout_response)
        );
    } else {
        note_block_failed(gobj, "Modbus Timeout", 0);
    }

    RESET_MACHINE()
    gobj_change_state(gobj, ST_CONNECTED);

    if(priv->jn_current_request) {
        answer_current_request(gobj);
    }

    /*
     *  A queued request, else the next map: after a timeout, the wait is
     *  timeout_response
     */
    continue_polling(gobj, priv->timeout_response*1000);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_MODBUS_M);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_MODBUS_ANSWER);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,       ST_CONNECTED},
        {EV_SEND_MESSAGE,       ac_refuse_request,  0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,             0}, // when close with pending rx, in re-open it receives some data
        {EV_SEND_MESSAGE,       ac_enqueue_tx_message,  0},
        {EV_TIMEOUT,            ac_timeout_polling,     0},
        {EV_TX_READY,           0,                      0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {0,0,0}
    };
    ev_action_t st_wait_response[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_SEND_MESSAGE,       ac_enqueue_tx_message,  0},
        {EV_TIMEOUT,            ac_timeout_response,    ST_CONNECTED},
        {EV_TX_READY,           0,                      0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {EV_DROP,               ac_drop,            0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_DISCONNECTED,   st_disconnected},
        {ST_CONNECTED,      st_connected},
        {ST_WAIT_RESPONSE,  st_wait_response},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     EVF_OUTPUT_EVENT},
        {EV_MODBUS_ANSWER,  EVF_OUTPUT_EVENT},
        {EV_ON_HEADER,      EVF_OUTPUT_EVENT},
        {EV_RX_DATA,        0},
        {EV_ON_OPEN,        EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,       EVF_OUTPUT_EVENT},

        {EV_TIMEOUT,        0},
        {EV_CONNECTED,      0},
        {EV_DISCONNECTED,   0},
        {EV_STOPPED,        0},
        {EV_TX_READY,       0},
        {EV_SEND_MESSAGE,   0},
        {EV_DROP,           0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_prot_modbus_m(void)
{
    return create_gclass(C_PROT_MODBUS_M);
}
