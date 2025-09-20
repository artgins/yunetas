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


 *          Copyright (c) 2021-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
    uint8_t byte_count; // TODO en las funciones WRITE no viene byte_count (1), viene addr or value (2 bytes)
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

    // information of frame
    size_t payload_length;
} FRAME_HEAD;

typedef enum {
    WAIT_HEAD=0,
    WAIT_PAYLOAD,
} state_t;

#define RESET_MACHINE() \
    ISTREAM_DESTROY(priv->istream_payload);                     \
    if(priv->istream_head) istream_clear(priv->istream_head);   \
    memset(&priv->frame_head,0,sizeof(priv->frame_head));       \
    priv->modbus_function = -1;                                 \
    priv->st = WAIT_HEAD;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int print_slave_data(hgobj gobj);
PRIVATE slave_data_t *get_slave_data(hgobj gobj, int slave_id, BOOL verbose);
PRIVATE const char *modbus_function_name(int modbus_function);
PRIVATE modbus_object_type_t get_object_type(hgobj gobj, const char *type);
PRIVATE int build_slave_data(hgobj gobj);
PRIVATE int free_slave_data(hgobj gobj);
PRIVATE int load_modbus_config(hgobj gobj);
PRIVATE int load_slave_mapping(hgobj gobj);
PRIVATE int store_modbus_response_data(hgobj gobj, uint8_t *bf, int len);
PRIVATE endian_format_t get_endian_format(hgobj gobj, const char *format);
PRIVATE variable_format_t get_variable_format(hgobj gobj, const char *format);
PRIVATE int build_message_to_publish(hgobj gobj);
PRIVATE int check_conversion_variables(hgobj gobj);

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
SDATAPM (DTP_STRING,    "address",      0,              0,          "Address"),
SDATAPM (DTP_STRING,    "size",         0,              0,          "Size (-1 all data)"),
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
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "modbus_protocol",  SDF_RD,         "TCP",      "Modbus protocol: TCP,RTU,ASCII"),
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url or device"),
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

    int inform_on_close;

    FRAME_HEAD frame_head;
    istream_h istream_head;
    istream_h istream_payload;
    state_t st;
    int modbus_function;

    json_t *jn_current_request;
    json_t *jn_request_queue;
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
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->jn_request_queue = json_array();

    load_modbus_config(gobj);
    build_slave_data(gobj);
    check_conversion_variables(gobj);

    if(gobj_trace_level(gobj) & TRACE_DECODE) {
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
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    RESET_MACHINE();
    ISTREAM_DESTROY(priv->istream_head);

    clear_timeout(priv->gobj_timer);

    JSON_DECREF(priv->jn_request_queue);
    JSON_DECREF(priv->jn_current_request);
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Modbus protocol NOT IMPLEMENTED",
                "protocol",     "%s", priv->modbus_protocol,
                NULL
            );
            break;
    } SWITCHS_END;

    priv->modbus_function = modbus_function;

    if(gobj_trace_level(gobj) & TRACE_DECODE) {
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
 *
 ***************************************************************************/
PRIVATE gbuffer_t *build_modbus_request_write_message(hgobj gobj, json_t *jn_request)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj); // WARNING must be used only for t_id

    uint8_t req[12] = {0};
    uint8_t slave_id = (uint8_t)kw_get_int(gobj, jn_request, "id", 1, 0);
    uint16_t address = kw_get_int(gobj, jn_request, "address", 0, KW_REQUIRED|KW_WILD_NUMBER);
    uint16_t size = kw_get_int(gobj, jn_request, "size", 1, KW_WILD_NUMBER);
    if(size == 0 || size > MODBUS_MAX_WRITE_REGISTERS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Modbus Too many discrete values to write or zero",
            "size",         "%d", size,
            NULL
        );
        return 0;
    }
    json_t *jn_value = kw_get_dict_value(gobj, jn_request, "value", 0, KW_REQUIRED);
    if(!jn_value) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Modbus write needs a value",
            NULL
        );
        return 0;
    }

    uint16_t value;
    if(json_is_integer(jn_value) || json_is_string(jn_value)) {
        value = (uint16_t)jn2integer(jn_value);
    } else if(json_is_array(jn_value)) {
        // TODO not tested
        if(json_array_size(jn_value) != size) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Modbus write array size not match",
                "size",         "%d", size,
                "json_size",    "%d", json_array_size(jn_value),
                NULL
            );
            gobj_trace_json(gobj, jn_value, "Modbus write array size not match");
            return 0;
        }
        value = (uint16_t) jn2integer(json_array_get(jn_value, 0));
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Modbus json value type not valid",
            NULL
        );
        return 0;
    }

    const char *type = kw_get_str(gobj, jn_request, "type", "", KW_REQUIRED);
    modbus_object_type_t object_type = get_object_type(gobj, type);

    uint8_t modbus_function = 0;
    switch(object_type) {
        case TYPE_COIL:
            if(size == 1) {
                modbus_function = MODBUS_FC_WRITE_SINGLE_COIL;
            } else {
                modbus_function = MODBUS_FC_WRITE_MULTIPLE_COILS;
            }
            break;

        case TYPE_HOLDING_REGISTER:
            if(size == 1) {
                modbus_function = MODBUS_FC_WRITE_SINGLE_REGISTER;
            } else {
                modbus_function = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Modbus object type NOT SUPPORTED to write",
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
            int mbap_length = 10 - 6 + size * 2;

            req[4] = mbap_length >> 8;
            req[5] = mbap_length & 0x00FF;
            req[6] = slave_id;
            req[7] = modbus_function;
            req[8] = address >> 8;
            req[9] = address & 0x00ff;
            req[10] = value >> 8;           // Add the first value, always come
            req[11] = value & 0x00ff;
            gbuffer_append(gbuf, req, 12);

            for(int i=1; i<size; i++) { // TODO No tested!!!
                value = (uint16_t) jn2integer(json_array_get(jn_value, i));
                req[0] = value >> 8;
                req[1] = value & 0x00ff;
                gbuffer_append(gbuf, req, 2);
                size--;
            }
            break;

        CASES("RTU")
        CASES("ASCII")
// TODO            req[0] = slave_id;
//             req[1] = modbus_function;
//             req[2] = address >> 8;
//             req[3] = address & 0x00ff;
//             req[4] = size >> 8;
//             req[5] = size & 0x00ff;
//
//             uint16_t crc = crc16_tx(req, 6);
//             req[7] = crc >> 8;
//             req[8] = crc & 0x00FF;
//             gbuffer_append(gbuf, req, 9);
//             break;

        DEFAULTS
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Modbus protocol UNKNOWN",
                "protocol",     "%s", priv->modbus_protocol,
                NULL
            );
            break;
    } SWITCHS_END;

    priv->modbus_function = modbus_function;

    if(gobj_trace_level(gobj) & TRACE_DECODE) {
        gobj_trace_msg(gobj, "🍅🍅⏩ func: %d %s, slave_id: %d, addr: %d (0x%04X), value: %d",
            modbus_function,
            modbus_function_name(modbus_function),
            slave_id,
            address,
            address,
            value
        );
    }

    return gbuf;
}




            /***************************
             *      Local Methods
             ***************************/




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
PRIVATE modbus_object_type_t get_object_type(hgobj gobj, const char *type_)
{
    char type[64];
    snprintf(type, sizeof(type), "%s", type_);
    strntolower(type, strlen(type));

    modbus_object_type_t object_type = 0;
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
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Modbus object type UNKNOWN",
                "type",         "%s", type,
                NULL
            );
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "slaves_ NULL",
            NULL
        );
        return -1;
    }
    if(!priv->max_slaves) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
        pslv->slave_id = slave_id;
        pslv->x_control = json_object();

        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_COIL), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_DISCRETE_INPUT), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_INPUT_REGISTER), json_object());
        json_object_set_new(pslv->x_control, get_object_type_name(TYPE_HOLDING_REGISTER), json_object());

        json_t *jn_mapping = kw_get_list(gobj, jn_slave, "mapping", 0, KW_REQUIRED);
        size_t idx_map; json_t *jn_map;
        json_array_foreach(jn_mapping, idx_map, jn_map) {
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                //cell_control_t *cell_control = &pslv->control[object_type][address+i]; XXX
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
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    priv->slaves_ = gobj_read_json_attr(gobj, "slaves");
    priv->idx_slaves = 0;
    priv->max_slaves = json_array_size(priv->slaves_);

    load_slave_mapping(gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int load_slave_mapping(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cur_slave_ = json_array_get(priv->slaves_, priv->idx_slaves);
    if(!priv->cur_slave_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cur_slave_ NULL",
            "idx_slaves",   "%d", priv->idx_slaves,
            "slaves_",      "%j", priv->slaves_?priv->slaves_:json_null(),
            NULL
        );
        priv->idx_mapping = 0;
        priv->max_mapping = 0;
        return -1;
    }
    priv->mapping_ = kw_get_list(gobj, priv->cur_slave_, "mapping", 0, KW_REQUIRED);
    priv->idx_mapping = -1;
    priv->max_mapping = (int)json_array_size(priv->mapping_);
    if(priv->max_mapping == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "slave without mapping",
            "idx_slaves",   "%d", priv->idx_slaves,
            "cur_slave_",   "%j", priv->cur_slave_,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Prepare next poll
 *  If end of cycle then publish and return -1
 ***************************************************************************/
PRIVATE int next_map(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cur_map_ = 0; // Reset cur map

    if(priv->idx_mapping < 0) {
        priv->idx_mapping = 0;
    } else {
        priv->idx_mapping++;
    }
    if(priv->idx_mapping < priv->max_mapping) {
        // Next map in current slave
        if(gobj_trace_level(gobj) & TRACE_POLLING) {
            gobj_trace_msg(gobj, "🔊🔊🔊🔊⏩ next map  : idx slave %d, idx map %d",
                priv->idx_slaves, priv->idx_mapping
            );
        }
        return 0; // do polling
    }

    priv->idx_slaves++;
    if(priv->idx_slaves < priv->max_slaves) {
        load_slave_mapping(gobj);
        priv->idx_mapping = 0;;
        if(gobj_trace_level(gobj) & TRACE_POLLING) {
            gobj_trace_msg(gobj, "🔊🔊🔊🔊🔊🔊🔊🔊⏩ next slave: idx slave %d, idx map %d",
                priv->idx_slaves, priv->idx_mapping
            );
        }
        return 0; // do polling
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
    load_slave_mapping(gobj);

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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    gbuffer_t *gbuf = build_modbus_request_read_message(gobj, priv->cur_slave_, priv->cur_map_);
    send_data(gobj, gbuf);

    // Change state
    gobj_change_state(gobj, ST_WAIT_RESPONSE);

    // Set response timeout
    set_timeout(priv->gobj_timer, priv->timeout_response*1000);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL send_request(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_array_size(priv->jn_request_queue)==0) {
        return FALSE;
    }

    json_t *jn_current_request = json_array_get(priv->jn_request_queue, 0);
    if(gobj_trace_level(gobj) & TRACE_SEND) {
        gobj_trace_json(gobj, jn_current_request, "sending to %s",
           gobj_read_str_attr(gobj_bottom_gobj(gobj), "url")
        );
    }
    gbuffer_t *gbuf = build_modbus_request_write_message(gobj, jn_current_request);
    JSON_DECREF(jn_current_request);
    send_data(gobj, gbuf);

    // Change state
    gobj_change_state(gobj, ST_WAIT_RESPONSE);

    // Set response timeout
    set_timeout(priv->gobj_timer, priv->timeout_response*1000);

    return TRUE;
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
 ***************************************************************************/
PRIVATE int framehead_consume(hgobj gobj, FRAME_HEAD *frame, istream_h istream, char *bf, int len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int total_consumed = 0;
    int consumed;

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

        SWITCHS(priv->modbus_protocol) {
            CASES("TCP")
                head_tcp_t *head = (head_tcp_t *)istream_extract_matched_data(istream, 0);
                frame->function = head->function;
                frame->slave_id = head->slave_id;
                frame->byte_count = head->byte_count;
                head->length = ntohs(head->length);
                frame->payload_length = head->length - 3;
                break;

            CASES("RTU")
            CASES("ASCII")
                head_rtu_t *head = (head_rtu_t *)istream_extract_matched_data(istream, 0);
                frame->function = head->function;
                frame->slave_id = head->slave_id;
                frame->byte_count = head->byte_count;
                frame->payload_length = head->byte_count + sizeof(uint16_t); // + crc
                break;

            DEFAULTS
                break;
        } SWITCHS_END
    }

    if(frame->function & 0x80) {
        frame->error_code = priv->frame_head.byte_count;
        frame->payload_length = sizeof(uint16_t); // + crc
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL_ERROR,
            "msg",              "%s", "modbus exception",
            "error_code",       "%d", frame->error_code,
            "error_name",       "%s", modbus_exception_name(frame->error_code),
            "cur_map_",         "%j", priv->cur_map_,
            NULL
        );
    } else {
        if(gobj_trace_level(gobj) & TRACE_DECODE) {
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
 *  Process the completed frame
 ***************************************************************************/
PRIVATE int frame_completed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = istream_get_gbuffer(priv->istream_payload);

    SWITCHS(priv->modbus_protocol) {
        CASES("TCP")
            int len = gbuffer_leftbytes(gbuf);
            uint8_t *bf = gbuffer_get(gbuf, len);
            if(!priv->frame_head.error_code) {
                store_modbus_response_data(gobj, bf, len);
            }
            break;

        CASES("RTU")
            int len = gbuffer_leftbytes(gbuf);
            uint8_t *bf = gbuffer_get(gbuf, len);
             if (len < 2 || !bf) {
                 gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_PROTOCOL_ERROR,
                    "msg",              "%s", "Not enough data",
                    "len",              "%d", len,
                    NULL
                 );
                 return -1;
             }
            int crc_calculated = crc16_rx(&priv->frame_head, bf, len - 2);
            int crc_received = (bf[len - 2] << 8) | bf[len - 1];

             /* Check CRC of msg */
             if (crc_calculated != crc_received) {
                 gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_PROTOCOL_ERROR,
                    "msg",              "%s", "CRC error",
                    "crc received",     "%d", crc_received,
                    "crc calculated",   "%d", crc_calculated,
                    NULL
                 );
                 return -1;
             }
             if(!priv->frame_head.error_code) {
                 store_modbus_response_data(gobj, bf, len - 2);
             }
             break;

        CASES("ASCII")
        DEFAULTS
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Protocol modbus NOT IMPLEMENTED",
                NULL
            );
            break;
    } SWITCHS_END

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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
    //cell_control_t *cell_control = &pslv->control[object_type][address]; XXX
    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return -1;
    }
    cell_control->control.bit_value = value?1:0;
    cell_control->control.updated = 1;

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
    //cell_control_t *cell_control = &pslv->control[object_type][address]; XXX
    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return -1;
    }

    switch(object_type) {
        case TYPE_INPUT_REGISTER:
            //memmove(&pslv->input_register[address], bf, 2); XXX
            memmove(&cell_control->input_register, bf, 2);
            cell_control->control.updated = 1;
            break;
        case TYPE_HOLDING_REGISTER:
            //memmove(&pslv->holding_register[address], bf, 2); XXX
            memmove(&cell_control->holding_register, bf, 2);
            cell_control->control.updated = 1;
            break;
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
 *
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
    ) { // TODO not sure
        if(byte_count != len) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "byte_count != len",
                "byte_count",   "%d", byte_count,
                "len",          "%d", len,
                NULL
            );
            return -1;
        }
    } else {
        // TODO by now write functions not checked
        return 0;
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
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "slave_id NOT MATCH",
            "req_slave_id", "%d", req_slave_id,
            "slave_id",     "%d", slave_id,
            NULL
        );
        return -1;
    }

    if(priv->modbus_function != modbus_function) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL_ERROR,
            "msg",              "%s", "modbus function NOT MATCH",
            "function esperada","%s", modbus_function_name(priv->modbus_function),
            "function recibida","%s", modbus_function_name(modbus_function),
            NULL
        );
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
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL_ERROR,
            "msg",              "%s", "Quantity not corresponding to the request",
            "function",         "%s", modbus_function_name(modbus_function),
            "rsp_nb_value",     "%d", rsp_nb_value,
            "req_nb_value",     "%d", req_nb_value,
            NULL
        );
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
                "msgset",           "%s", MSGSET_INTERNAL_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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

    //cell_control_t *cell_control = &pslv->control[object_type][address]; XXX
    cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address, FALSE);
    if(!cell_control) {
        // Error already logged
        return jn_value;
    }
    cell_control->control.updated = 0;

    switch(variable_format) {
        case FORMAT_BOOL:
            {
                switch(object_type) {
                    case TYPE_COIL:
                    case TYPE_DISCRETE_INPUT:
                        jn_value = cell_control->control.bit_value?json_true():json_false();
                        break;
                    case TYPE_INPUT_REGISTER:
                        //jn_value = pslv->input_register[address]?json_true():json_false(); XXX
                        jn_value = cell_control->input_register?json_true():json_false();
                        break;
                    case TYPE_HOLDING_REGISTER:
                        //jn_value = pslv->holding_register[address]?json_true():json_false(); XXX
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

        case FORMAT_INT32:
        case FORMAT_UINT32:
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
                    if(variable_format == FORMAT_INT32) {
                        int32_t v = endian_32(endian_format, (uint8_t *)pv);
                        if(multiplier < 1.0 &&  multiplier > 0.0) {
                            float v_ = (float)v * multiplier;
                            jn_value = json_real(v_);
                        } else {
                            v = v*multiplier;
                            jn_value = json_integer(v);
                        }
                    } else {
                        uint32_t v = endian_32(endian_format, (uint8_t *)pv);
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

        case FORMAT_INT64:
        case FORMAT_UINT64:
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
                    if(variable_format == FORMAT_INT64) {
                        int64_t v = endian_64(endian_format, (uint8_t *)pv);
                        if(multiplier < 1.0 &&  multiplier > 0.0) {
                            double v_ = (double)v * multiplier;
                            jn_value = json_real(v_);
                        } else {
                            v = v*multiplier;
                            jn_value = json_integer(v);
                        }
                    } else {
                        uint64_t v = endian_64(endian_format, (uint8_t *)pv);
                        if(multiplier < 1.0 &&  multiplier > 0.0) {
                            double v_ = (double)v * multiplier;
                            jn_value = json_real(v_);
                        } else {
                            v = v*multiplier;
                            jn_value = json_integer(v);
                        }
                    }
                }
            }
            break;

        case FORMAT_FLOAT:
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
                    float v = endian_float(endian_format, (uint8_t *)pv);
                    v = v*multiplier;
                    jn_value = json_real(v);
                }
            }
            break;

        case FORMAT_DOUBLE:
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
                    double v = endian_double(endian_format, (uint8_t *)pv);
                    v = v*multiplier;
                    jn_value = json_real(v);
                }
            }
            break;

        case FORMAT_STRING:
            {
                int size = (int)kw_get_int(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
                gbuffer_t *gbuf_string = gbuffer_create(size*2, size*2);

                for(int i=0; i<size; i++) {
                    uint16_t *pv = 0;
                    cell_control_t *cell2 = get_cell_control(gobj, pslv, object_type, address+i, FALSE);
                    switch(object_type) {
                        case TYPE_INPUT_REGISTER:
                            pv = &cell2->input_register;
                            break;
                        case TYPE_HOLDING_REGISTER:
                            pv = &cell2->holding_register;
                            break;
                        default:
                            break;
                    }

                    uint32_t word = endian_16(endian_format, (uint8_t *)pv);
                    uint8_t b1 = (uint8_t)(word >> 8); // get the higher byte;
                    uint8_t b2 = (uint8_t)(word & 0xFF); // get the lower byte
                    // convert the nulls into space
                    if(b1==0) {
                        b1 = ' ';
                    }
                    if(b2==0) {
                        b2 = ' ';
                    }
                    gbuffer_append(gbuf_string, &b1, 1);
                    gbuffer_append(gbuf_string, &b2, 1);
                }
                left_justify(gbuffer_cur_rd_pointer(gbuf_string));
                jn_value = json_string(gbuffer_cur_rd_pointer(gbuf_string));
                GBUFFER_DECREF(gbuf_string)
            }
            break;
    }

    return jn_value;
}

/***************************************************************************
 *
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

        size_t idx_conversion; json_t *jn_variable;
        json_array_foreach(jn_conversion, idx_conversion, jn_variable) {
            BOOL disabled = kw_get_bool(gobj, jn_variable, "disabled", 0, 0);
            if(disabled) {
                continue;
            }
            const char *variable_id = kw_get_str(gobj, jn_variable, "id", "", KW_REQUIRED);
            json_t *jn_value = get_variable_value(gobj, pslv, jn_variable);
            json_object_set_new(kw_data, variable_id, jn_value);
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
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",           "%s", MSGSET_PARAMETER_ERROR,
            "msg",              "%s", "Conversion: endian format UNKNOWN",
            "slave_id",         "%d", slave_id,
            "variable",         "%j", jn_variable,
            NULL
        );
        json_object_set_new(jn_variable, "disabled", json_true());
        return -1;
    }

    int compound_value = 0;
    switch(variable_format) {
        case FORMAT_INT16:
            compound_value = 1;
            break;
        case FORMAT_UINT16:
            compound_value = 1;
            break;
        case FORMAT_BOOL:
            compound_value = 1;
            break;
        case FORMAT_INT32:
            compound_value = 2;
            break;
        case FORMAT_UINT32:
            compound_value = 2;
            break;
        case FORMAT_INT64:
            compound_value = 4;
            break;
        case FORMAT_UINT64:
            compound_value = 4;
            break;
        case FORMAT_FLOAT:
            compound_value = 2;
            break;
        case FORMAT_DOUBLE:
            compound_value = 4;
            break;
        case FORMAT_STRING:
            compound_value = (int)kw_get_int(gobj, jn_variable, "multiplier", 1, KW_WILD_NUMBER);
            break;
    }

    for(int i=0; i<compound_value; i++) {
        //cell_control_t *cell_control = &pslv->control[object_type][address+i]; XXX
        cell_control_t *cell_control = get_cell_control(gobj, pslv, object_type, address+i, FALSE);
        if(!cell_control || !cell_control->control.value_busy) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
    // // cambia el método, json_integer_value_pointer() is not available
    // json_t *jn_value = json_object_get(jn_type, saddress);
    // cell_control_t *cell_control = (cell_control_t *)(uintptr_t)json_integer_value_pointer(jn_value);
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

    gobj_change_state(gobj, ST_CONNECTED);

    priv->inform_on_close = TRUE;
    gobj_publish_event(gobj, EV_ON_OPEN, 0);

    set_timeout(priv->gobj_timer, 1*1000);

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

    clear_timeout(priv->gobj_timer);

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
     *
     *---------------------------------------------*/
    BOOL response_completed = FALSE;
    int lnn;
    BOOL fin = FALSE;
    while(!fin && (lnn=(int)gbuffer_leftbytes(gbuf))>0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);

        switch(priv->st) {
        case WAIT_HEAD:
            {
                int n = framehead_consume(gobj, &priv->frame_head, priv->istream_head, bf, lnn);
                if (n <= 0) {
                    // Some error in parsing
                    // on error do break the connection
                    fin = TRUE;
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                        "msg",          "%s", "modbus framehead_consume() FAILED",
                        NULL
                    );
                    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                    break;
                } else if (n > 0) {
                    gbuffer_get(gbuf, n);  // take out the bytes consumed
                }

                if(priv->frame_head.header_complete) {
                    if(priv->frame_head.payload_length <= 0) {
                        // Error already logged. Can be an exception
                        response_completed = TRUE;
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
                            "msgset",       "%s", MSGSET_MEMORY_ERROR,
                            "msg",          "%s", "no memory for istream_payload",
                            "payload_length", "%d", priv->frame_head.payload_length,
                            NULL
                        );
                        response_completed = TRUE;
                        break;
                    }
                    istream_read_until_num_bytes(
                        priv->istream_payload,
                        priv->frame_head.payload_length,
                        0
                    );
                    priv->st = WAIT_PAYLOAD;
                    break;
                }
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
                        gobj_trace_dump_gbuf(gobj,
                           istream_get_gbuffer(priv->istream_payload), "%s", gobj_short_name(src)
                       );
                    }
                    frame_completed(gobj);
                    response_completed = TRUE;
                    fin = TRUE;
                }
            }
            break;
        }
    }

    /*---------------------------*
     *      Next map
     *---------------------------*/
    if(response_completed) {
        if(gbuffer_leftbytes(gbuf)>0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "Modbus: response too large",
                NULL
            );
            gobj_trace_dump_full_gbuf(gobj, gbuf, "Modbus: response too large");
        }

        /*---------------------------------------------*
         *   Reset response gobj_timer
         *---------------------------------------------*/
        clear_timeout(priv->gobj_timer);

        RESET_MACHINE()
        gobj_change_state(gobj, ST_CONNECTED);

        if(!send_request(gobj)) {
            if(next_map(gobj)<0) {
                /*
                 *  NO map or end of cycle, wait timeout_polling
                 */
                set_timeout(priv->gobj_timer, priv->timeout_polling);
            } else {
                if(poll_modbus(gobj)<0) {
                    /*
                     *  Problemas con el query actual, pasa al siguiente después del timeout
                     */
                    next_map(gobj);
                    set_timeout(priv->gobj_timer, priv->timeout_polling);
                } else {
                    // timeout set by poll_modbus
                }
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/********************************************************************
 *  Message from high level, enqueue and send after poll cycle
 ********************************************************************/
PRIVATE int ac_enqueue_tx_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // TODO enqueue the message and send after poll cycle or between responses
    // format output message or directly send
    //gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw, gobj);

    json_array_append_new(priv->jn_request_queue, json_deep_copy(kw));

    KW_DECREF(kw)
    return 0;
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

    /*
     *  Next map
     */
    if(!send_request(gobj)) {
        if(next_map(gobj)<0) {
            /*
             *  NO map or end of cycle, wait timeout_polling
             */
            set_timeout(priv->gobj_timer, priv->timeout_polling);
        } else {
            if(poll_modbus(gobj)<0) {
                /*
                 *  Problemas con el query actual, pasa al siguiente después del timeout
                 */
                next_map(gobj);
                set_timeout(priv->gobj_timer, priv->timeout_polling);
            } else {
                // timeout set by poll_modbus
            }
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_response(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
        "msg",          "%s", "Modbus Timeout",
        NULL
    );

    RESET_MACHINE()
    gobj_change_state(gobj, ST_CONNECTED);

    /*
     *  Next map
     */
    if(next_map(gobj)<0) {
        /*
         *  NO map or end of cycle, wait timeout_polling
         */
        set_timeout(priv->gobj_timer, priv->timeout_response*1000);
    } else {
        if(poll_modbus(gobj)<0) {
            /*
             *  Problemas con el query actual, pasa al siguiente después del timeout
             */
            next_map(gobj);
            set_timeout(priv->gobj_timer, priv->timeout_response*1000);
        } else {
            // timeout set by poll_modbus
        }
    }

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




                    /***************************
                     *          FSM
                     ***************************/




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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
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
