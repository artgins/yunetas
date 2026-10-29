/****************************************************************************
 *          C_TEST2.C
 *
 *          A C_PROT_MODBUS_M master with RTU framing against a raw slave.
 *
 *          RTU goes over a serial line, but the framing does not depend on
 *          it: the master is given its own C_TCP (as a gate does), and the
 *          raw server (C_PROT_RAW) answers RTU frames by hand, CRC included.
 *          The master polls one slave: block A (holding 0, 1 register: "a").
 *          Its config has three more, never polled: slave 3, with no block,
 *          slave 250, a reserved id, and slave 0, the broadcast. Each is reported once, at start,
 *          and no cycle logs anything for them. Up to the fix of REV-CLOUD
 *          V46 every cycle logged "slave without mapping" and "cur_map_
 *          NULL" for slave 3, and "cur_map_ DISABLED" for slave 250.
 *
 *          On the message of cycle 1 it queues requests (REQUESTS, below),
 *          each of which must be answered with EV_MODBUS_ANSWER, in order:
 *          - a write of one register (06) and of three (16), a read of them
 *            back (03), one coil (05, ON is 0xFF00), nine coils (15) and a
 *            read of them back (01): the slave checks each frame, its CRC
 *            included. Up to 7.29.2 RTU refused every write;
 *          - a write the slave refuses with exception 1;
 *          - a write answered with an exception whose CRC does not match,
 *            and a read answered with registers whose CRC does not match:
 *            each fails its request as a bad answer, logged as a warning.
 *            Up to the fix of REV-CLOUD V45.2 the exception was taken
 *            before its CRC was checked, and a read with a bad CRC logged
 *            an internal error ("answered WITHOUT reading its answer").
 *          - (REV-CLOUD V46) a request to slave 248 is refused: 248-255 are
 *            reserved in RTU; so is the polled slave 250 of the config, whose
 *            block is disabled. A read the slave never answers times out,
 *            and its exception comes late, while the next request, a write,
 *            is in flight: it is dropped (its function is not the write's),
 *            and the echo that follows answers the write. Up to the fix the
 *            late exception failed the write that succeeded, and its echo
 *            was dropped.
 *          - (REV-CLOUD V47) two reads answered with noise after the answer,
 *            in the same bytes (0x00, and 0x00 0xFF): the noise is dropped,
 *            and the read sent after each is answered. With 226945f9c the
 *            noise began a frame, which swallowed the next answer.
 *
 *          (REV-CLOUD V47) Slave 1 has a block B (holding 10, 1 register)
 *          that the slave answers with 2 registers every cycle: it is logged
 *          once, not in every cycle. Slave 0, the RTU broadcast, is in the
 *          config with a block: it is reported once at start and never
 *          polled (before, every cycle waited timeout_response for it). And
 *          on the message of cycle 1 the slaves are written as [] while the
 *          master runs: a warning, and cycle 2 polls the map it started
 *          with. Before, the write freed the map the cycle pointed into.
 *
 *          (REV-CLOUD V47, follow-up) The answer of block A carries a noise
 *          byte in cycles 2 and 3 too: a line that spikes in every cycle is
 *          logged once, and once more ("a cycle came without bytes after
 *          its answers") when cycle 4 comes clean. Before, every noisy
 *          answer was a warning.
 *
 *          Cycle 4 ends the test.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_modbus_m.h>
#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SLAVE_URL       "tcp://127.0.0.1:7760"
#define WRITE_ADDRESS   20      // function 06
#define WRITE_VALUE     7
#define MULTI_ADDRESS   30      // function 16, three registers, then read back
#define REFUSED_ADDRESS 40      // the slave answers exception 1
#define BADEXC_ADDRESS  41      // the slave answers an exception with a bad CRC
#define BADREAD_ADDRESS 50      // the slave answers a read with a bad CRC
#define COIL_ADDRESS    5       // function 05
#define COILS_ADDRESS   8       // function 15, nine coils, then read back (01)
#define LATE_ADDRESS    60      // a read never answered: its exception comes late
#define AFTER_ADDRESS   21      // the next request, a write: the late exception comes first
#define AFTER_VALUE     9
#define NOISE1_ADDRESS  70      // answered with one noise byte after it, in the same bytes
#define NOISE2_ADDRESS  71      // answered with two noise bytes after it (0x00 0xFF)
#define LAST_CYCLE      4       // its message ends the test
#define BLOCKB_ADDRESS  10      // block B: 1 register asked, 2 answered, every cycle
#define RX_SIZE         512

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*
 *  The requests queued on the message of cycle 1, and the answer each must
 *  get, in this order
 */
PRIVATE const char *requests[] = {
    "{'tag': 'r248', 'id': 248, 'type': 'holding_register', 'address': 20, 'value': 7}",
    "{'tag': 'w06', 'type': 'holding_register', 'address': 20, 'value': 7}",
    "{'tag': 'w16', 'type': 'holding_register', 'address': 30, 'value': [11, 22, 33]}",
    "{'tag': 'r03', 'operation': 'read', 'type': 'holding_register', 'address': 30, 'size': 3}",
    "{'tag': 'x40', 'type': 'holding_register', 'address': 40, 'value': 1}",
    "{'tag': 'x41', 'type': 'holding_register', 'address': 41, 'value': 1}",
    "{'tag': 'r50', 'operation': 'read', 'type': 'holding_register', 'address': 50}",
    "{'tag': 'c05', 'type': 'coil', 'address': 5, 'value': 1}",
    "{'tag': 'c15', 'type': 'coil', 'address': 8, 'value': [1, 0, 1, 1, 0, 0, 0, 0, 1]}",
    "{'tag': 'r01', 'operation': 'read', 'type': 'coil', 'address': 8, 'size': 9}",
    "{'tag': 'lte', 'operation': 'read', 'type': 'holding_register', 'address': 60}",
    "{'tag': 'w21', 'type': 'holding_register', 'address': 21, 'value': 9}",
    "{'tag': 'nz1', 'operation': 'read', 'type': 'holding_register', 'address': 70}",
    "{'tag': 'af1', 'operation': 'read', 'type': 'holding_register', 'address': 30, 'size': 3}",
    "{'tag': 'nz2', 'operation': 'read', 'type': 'holding_register', 'address': 71}",
    "{'tag': 'af2', 'operation': 'read', 'type': 'holding_register', 'address': 30, 'size': 3}",
    0
};
PRIVATE const char *answers[] = {
    "{'tag': 'r248', 'result': -1, 'error_code': 0}",      // refused: reserved in RTU
    "{'tag': 'w06', 'result': 0, 'error_code': 0}",
    "{'tag': 'w16', 'result': 0, 'error_code': 0}",
    "{'tag': 'r03', 'result': 0, 'error_code': 0, 'values': [11, 22, 33]}",
    "{'tag': 'x40', 'result': -1, 'error_code': 1}",
    "{'tag': 'x41', 'result': -1, 'error_code': 0}",       // not an exception: a bad frame
    "{'tag': 'r50', 'result': -1, 'error_code': 0}",
    "{'tag': 'c05', 'result': 0, 'error_code': 0}",
    "{'tag': 'c15', 'result': 0, 'error_code': 0}",
    "{'tag': 'r01', 'result': 0, 'error_code': 0, 'values': [1, 0, 1, 1, 0, 0, 0, 0, 1]}",
    "{'tag': 'lte', 'result': -1, 'error_code': 0}",       // timeout_response
    "{'tag': 'w21', 'result': 0, 'error_code': 0}",        // not the late exception of lte
    "{'tag': 'nz1', 'result': 0, 'error_code': 0, 'values': [70]}",
    "{'tag': 'af1', 'result': 0, 'error_code': 0, 'values': [11, 22, 33]}",   // not lost to the noise
    "{'tag': 'nz2', 'result': 0, 'error_code': 0, 'values': [71]}",
    "{'tag': 'af2', 'result': 0, 'error_code': 0, 'values': [11, 22, 33]}",   // not lost to the noise
    0
};

PRIVATE char slaves_config[] = "\
[                                                                   \n\
    {                                                               \n\
        'id': 1,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1},   \n\
            {'type': 'holding_register', 'address': 10, 'size': 1}   \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'a', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
        ]                                                           \n\
    },                                                              \n\
    {                                                               \n\
        'id': 3,                                                    \n\
        'mapping': [],                                              \n\
        'conversion': []                                            \n\
    },                                                              \n\
    {                                                               \n\
        'id': 250,                                                  \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1}    \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'z', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
        ]                                                           \n\
    },                                                              \n\
    {                                                               \n\
        'id': 0,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1}    \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'y', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
        ]                                                           \n\
    }                                                               \n\
]                                                                   \n\
";

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj gobj_input_side;
    hgobj gobj_master;
    hgobj server_channel;       // the channel of the server, where the master connected

    uint8_t rx[RX_SIZE];        // what the slave received and did not answer yet
    size_t rx_len;

    int cycle;                  // counted at the slave, by the requests of block A
    uint16_t multi[3];          // at the slave: what the write of three registers left
    uint8_t coils[2];           // at the slave: what the write of nine coils left
    BOOL requests_queued;
    int messages;               // messages of the master: one per cycle
    int answers_seen;           // answers of the queued requests, in order
    BOOL late_held;             // the read never answered: its exception comes late
    int broadcast_polls;        // polls of slave 0, the RTU broadcast: none expected
    BOOL done;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_master && gobj_is_running(priv->gobj_master)) {
        gobj_stop(priv->gobj_master);
    }
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    set_timeout(priv->timer, priv->timeout); // then the master connects

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

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




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The CRC of Modbus RTU (CRC-16, polynomial 0xA001 reflected, from 0xFFFF),
 *  computed bit by bit: not the master's tables
 ***************************************************************************/
PRIVATE uint16_t rtu_crc(const uint8_t *bf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for(size_t i=0; i<len; i++) {
        crc ^= bf[i];
        for(int bit=0; bit<8; bit++) {
            if(crc & 1) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/***************************************************************************
 *  Send an answer from the slave: the CRC is appended, low byte first,
 *  and spoiled when `bad_crc`. Then `noise`, in the same bytes: what a
 *  RS485 line can carry when the slave releases it.
 ***************************************************************************/
PRIVATE void slave_send_noisy(
    hgobj gobj,
    const uint8_t *bf,
    size_t len,
    BOOL bad_crc,
    const uint8_t *noise,
    size_t noise_len
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint16_t crc = rtu_crc(bf, len);
    if(bad_crc) {
        crc ^= 0x5A5A;
    }
    gbuffer_t *gbuf = gbuffer_create(len + 2 + noise_len, len + 2 + noise_len);
    gbuffer_append(gbuf, (void *)bf, len);
    uint8_t c[2] = {(uint8_t)(crc & 0xFF), (uint8_t)(crc >> 8)};
    gbuffer_append(gbuf, c, 2);
    if(noise_len > 0) {
        gbuffer_append(gbuf, (void *)noise, noise_len);
    }
    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  Send an answer from the slave, alone
 ***************************************************************************/
PRIVATE void slave_send(hgobj gobj, const uint8_t *bf, size_t len, BOOL bad_crc)
{
    slave_send_noisy(gobj, bf, len, bad_crc, NULL, 0);
}

/***************************************************************************
 *  An exception answer to a request
 ***************************************************************************/
PRIVATE void slave_exception(hgobj gobj, const uint8_t *req, int code, BOOL bad_crc)
{
    uint8_t rsp[3];
    rsp[0] = req[0];                    // slave
    rsp[1] = (uint8_t)(req[1] | 0x80);  // function with the exception bit
    rsp[2] = (uint8_t)code;
    slave_send(gobj, rsp, sizeof(rsp), bad_crc);
}

/***************************************************************************
 *  The registers of a read
 ***************************************************************************/
PRIVATE void slave_registers(hgobj gobj, const uint8_t *req, const uint16_t *regs, int n, BOOL bad_crc)
{
    uint8_t rsp[3 + 2*8];
    rsp[0] = req[0];
    rsp[1] = req[1];
    rsp[2] = (uint8_t)(2*n);
    for(int i=0; i<n; i++) {
        rsp[3 + 2*i] = (uint8_t)(regs[i] >> 8);
        rsp[4 + 2*i] = (uint8_t)(regs[i] & 0xFF);
    }
    slave_send(gobj, rsp, (size_t)(3 + 2*n), bad_crc);
}

/***************************************************************************
 *  One whole request at the slave, its CRC already checked
 ***************************************************************************/
PRIVATE void slave_request(hgobj gobj, const uint8_t *req)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int unit = req[0];
    int function = req[1];
    int address = (req[2] << 8) | req[3];
    int quantity = (req[4] << 8) | req[5];     // the value, in a single write

    if(unit == 0) {
        /*
         *  The broadcast: no slave answers it
         */
        if(function == 3) {
            priv->broadcast_polls++;
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The master polled slave 0, the RTU broadcast",
                "polls",        "%d", priv->broadcast_polls,
                NULL
            );
        }
        return;
    }

    if(unit != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request to an unexpected slave",
            "unit",         "%d", unit,
            NULL
        );
        slave_exception(gobj, req, 11, FALSE);
        return;
    }

    switch(function) {
        case 6:
            if(address == AFTER_ADDRESS && quantity == AFTER_VALUE) {
                if(!priv->late_held) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "The read answered late did not reach the slave first",
                        NULL
                    );
                }
                /*
                 *  The late exception of the read (function 03), then the
                 *  echo of this write: only the echo answers it
                 */
                uint8_t late[6] = {req[0], 3, (uint8_t)(LATE_ADDRESS >> 8), LATE_ADDRESS & 0xFF, 0, 1};
                slave_exception(gobj, late, 2, FALSE);
                slave_send(gobj, req, 6, FALSE);
                return;
            }
            if(address == REFUSED_ADDRESS) {
                slave_exception(gobj, req, 1, FALSE);   // illegal function: a meter that is locked
                return;
            }
            if(address == BADEXC_ADDRESS) {
                slave_exception(gobj, req, 2, TRUE);
                return;
            }
            if(address != WRITE_ADDRESS || quantity != WRITE_VALUE) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "The write did not reach the slave as sent",
                    "address",      "%d", address,
                    "value",        "%d", quantity,
                    NULL
                );
                slave_exception(gobj, req, 3, FALSE);
                return;
            }
            slave_send(gobj, req, 6, FALSE);    // a write is answered with its echo
            return;

        case 16:
            if(address != MULTI_ADDRESS || quantity != 3 || req[6] != 6) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "The write of three registers did not reach the slave as sent",
                    "address",      "%d", address,
                    "quantity",     "%d", quantity,
                    "byte_count",   "%d", req[6],
                    NULL
                );
                slave_exception(gobj, req, 3, FALSE);
                return;
            }
            for(int i=0; i<3; i++) {
                priv->multi[i] = (uint16_t)((req[7 + 2*i] << 8) | req[8 + 2*i]);
            }
            slave_send(gobj, req, 6, FALSE);    // the echo: address and quantity
            return;

        case 5:
            if(address != COIL_ADDRESS || quantity != 0xFF00) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "The coil did not reach the slave as ON (0xFF00)",
                    "address",      "%d", address,
                    "value",        "%d", quantity,
                    NULL
                );
                slave_exception(gobj, req, 3, FALSE);
                return;
            }
            slave_send(gobj, req, 6, FALSE);
            return;

        case 15:
            // 1, 0, 1, 1, 0, 0, 0, 0 | 1: bit 0 first
            if(address != COILS_ADDRESS || quantity != 9 || req[6] != 2 ||
                    req[7] != 0x0D || req[8] != 0x01) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "The write of nine coils did not reach the slave as sent",
                    "address",      "%d", address,
                    "quantity",     "%d", quantity,
                    "byte_count",   "%d", req[6],
                    NULL
                );
                slave_exception(gobj, req, 3, FALSE);
                return;
            }
            priv->coils[0] = req[7];
            priv->coils[1] = req[8];
            slave_send(gobj, req, 6, FALSE);
            return;

        case 1:
            if(address != COILS_ADDRESS || quantity != 9) {
                break;
            }
            {
                uint8_t rsp[5] = {req[0], req[1], 2, priv->coils[0], priv->coils[1]};
                slave_send(gobj, rsp, sizeof(rsp), FALSE);
            }
            return;

        case 3:
            if(address == 0 && quantity == 1) {
                priv->cycle++;
                if(priv->cycle == 2 || priv->cycle == 3) {
                    /*
                     *  A line that spikes in every cycle: noise after the
                     *  answer of block A, in cycles 2 and 3
                     */
                    uint8_t rsp[5] = {req[0], req[1], 2, 0, (uint8_t)priv->cycle};
                    uint8_t noise[1] = {0x00};
                    slave_send_noisy(gobj, rsp, sizeof(rsp), FALSE, noise, sizeof(noise));
                    return;
                }
                uint16_t regs[1] = {(uint16_t)priv->cycle};
                slave_registers(gobj, req, regs, 1, FALSE);
                return;
            }
            if(address == MULTI_ADDRESS && quantity == 3) {
                slave_registers(gobj, req, priv->multi, 3, FALSE);
                return;
            }
            if(address == BLOCKB_ADDRESS && quantity == 1) {
                uint16_t regs[2] = {1, 2};  // one register asked, two answered
                slave_registers(gobj, req, regs, 2, FALSE);
                return;
            }
            if((address == NOISE1_ADDRESS || address == NOISE2_ADDRESS) && quantity == 1) {
                uint8_t rsp[5] = {req[0], req[1], 2, 0, (uint8_t)address};
                uint8_t noise[2] = {0x00, 0xFF};
                slave_send_noisy(gobj, rsp, sizeof(rsp), FALSE,
                    noise, address == NOISE1_ADDRESS? 1 : 2
                );
                return;
            }
            if(address == LATE_ADDRESS && quantity == 1) {
                priv->late_held = TRUE;     // never answered: its exception comes late
                return;
            }
            if(address == BADREAD_ADDRESS && quantity == 1) {
                uint16_t regs[1] = {99};
                slave_registers(gobj, req, regs, 1, TRUE);
                return;
            }
            break;

        default:
            break;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "Unexpected request at the slave",
        "modbus_function", "%d", function,
        "address",      "%d", address,
        "quantity",     "%d", quantity,
        NULL
    );
    slave_exception(gobj, req, 1, FALSE);
}

/***************************************************************************
 *  The length of the RTU request at the head of rx, CRC included; 0 when
 *  more bytes are needed to know it, -1 when it is not one this slave reads
 ***************************************************************************/
PRIVATE int rtu_request_length(const uint8_t *rx, size_t rx_len)
{
    if(rx_len < 2) {
        return 0;
    }
    switch(rx[1]) {
        case 1:
        case 3:
        case 5:
        case 6:
            return 8;
        case 15:
        case 16:
            if(rx_len < 7) {
                return 0;
            }
            return 9 + rx[6];
        default:
            return -1;
    }
}

/***************************************************************************
 *  Bytes at the slave: answer every whole request
 ***************************************************************************/
PRIVATE void slave_rx(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(!gbuf) {
        // Error already logged
        return;
    }
    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->rx_len + len > sizeof(priv->rx)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The slave got more than it can hold",
            "len",          "%lu", (unsigned long)len,
            NULL
        );
        return;
    }
    memcpy(priv->rx + priv->rx_len, gbuffer_cur_rd_pointer(gbuf), len);
    priv->rx_len += len;

    while(priv->rx_len > 0) {
        int frame_len = rtu_request_length(priv->rx, priv->rx_len);
        if(frame_len < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request with an unexpected function",
                "modbus_function", "%d", priv->rx[1],
                NULL
            );
            priv->rx_len = 0;
            return;
        }
        if(frame_len == 0 || priv->rx_len < (size_t)frame_len) {
            return;
        }
        uint16_t crc = rtu_crc(priv->rx, (size_t)frame_len - 2);
        if(priv->rx[frame_len - 2] != (crc & 0xFF) || priv->rx[frame_len - 1] != (crc >> 8)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request with a bad CRC at the slave",
                "modbus_function", "%d", priv->rx[1],
                NULL
            );
        } else {
            slave_request(gobj, priv->rx);
        }
        memmove(priv->rx, priv->rx + frame_len, priv->rx_len - (size_t)frame_len);
        priv->rx_len -= (size_t)frame_len;
    }
}

/***************************************************************************
 *  A json written with single quotes
 ***************************************************************************/
PRIVATE json_t *qjson(const char *text)
{
    char *bf = gbmem_strdup(text);
    helper_quote2doublequote(bf);
    json_t *jn = string2json(bf, TRUE);
    GBMEM_FREE(bf)
    return jn;
}

/***************************************************************************
 *  A message of the master, at the end of a cycle
 ***************************************************************************/
PRIVATE void master_message(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->done) {
        return;
    }

    json_int_t a = kw_get_int(gobj, kw, "a", -1, 0);
    if(a != priv->cycle) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published what the cycle did not read",
            "published",    "%j", kw,
            "cycle",        "%d", priv->cycle,
            NULL
        );
    }

    if(!priv->requests_queued) {
        priv->requests_queued = TRUE;
        for(int i=0; requests[i]; i++) {
            gobj_send_event(priv->gobj_master, EV_SEND_MESSAGE, qjson(requests[i]), gobj);
        }

        /*
         *  The slaves written while the master runs, inside its publication:
         *  the cycle goes on with the map it started with
         */
        json_t *jn_slaves = json_array();
        gobj_write_json_attr(priv->gobj_master, "slaves", jn_slaves);   // not owned
        JSON_DECREF(jn_slaves)
        return;
    }

    if(++priv->messages < LAST_CYCLE - 1) {
        return;
    }

    json_int_t noise = gobj_read_integer_attr(priv->gobj_master, "rx_noise_dropped");
    if(noise != 4) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master did not count each noisy answer",
            "rx_noise_dropped", "%lld", (long long)noise,
            "expected",     "%d", 4,
            NULL
        );
    }

    if(answers[priv->answers_seen] != NULL) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not every request was answered",
            "answers_seen", "%d", priv->answers_seen,
            NULL
        );
    }
    priv->done = TRUE;
    set_yuno_must_die();
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The server listens: the master is created over its own C_TCP, and
 *  connects
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_master) {
        helper_quote2doublequote(slaves_config);
        json_t *jn_slaves = string2json(slaves_config, TRUE);
        priv->gobj_master = gobj_create(
            "modbus",
            C_PROT_MODBUS_M,
            json_pack("{s:s, s:i, s:i, s:o}",
                "modbus_protocol", "RTU",
                "timeout_polling", 1000,
                "timeout_response", 1,
                "slaves", jn_slaves
            ),
            gobj
        );
        hgobj gobj_tcp = gobj_create_pure_child(
            "modbus",
            C_TCP,
            json_pack("{s:s}", "url", SLAVE_URL),
            priv->gobj_master
        );
        gobj_set_bottom_gobj(priv->gobj_master, gobj_tcp);
        gobj_start(priv->gobj_master);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer of a request: the next one expected, in order
 ***************************************************************************/
PRIVATE int ac_modbus_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *tag = kw_get_str(gobj, kw, "request`tag", "", KW_REQUIRED);

    const char *expected_ = answers[priv->answers_seen];
    if(!expected_) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "An answer more than the requests",
            "answer",       "%j", kw,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    priv->answers_seen++;

    if(strcmp(tag, "r248")==0 &&
            !strstr(kw_get_str(gobj, kw, "comment", "", 0), "reserved")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request to a reserved RTU slave id was not refused as reserved",
            "answer",       "%j", kw,
            NULL
        );
    }

    json_t *expected = qjson(expected_);
    json_t *got = json_pack("{s:s, s:O, s:O}",
        "tag", tag,
        "result", json_object_get(kw, "result"),
        "error_code", json_object_get(kw, "error_code")
    );
    if(json_object_get(kw, "values")) {
        json_object_set(got, "values", json_object_get(kw, "values"));
    }
    if(!json_equal(got, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request was not answered as expected",
            "got",          "%j", got,
            "expected",     "%j", expected? expected : json_null(),
            "answer",       "%j", kw,
            NULL
        );
    }
    JSON_DECREF(got)
    JSON_DECREF(expected)

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A message: at the server, the master's requests; from the master, what
 *  a cycle read
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        slave_rx(gobj, kw);
    } else if(src == priv->gobj_master) {
        master_message(gobj, kw);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        priv->rx_len = 0;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A close before the end is a failure: the master must not drop
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->done && !gobj_is_shutdowning()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The connection closed before the second cycle",
            "src",          "%s", gobj_short_name(src),
            "cycle",        "%d", priv->cycle,
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST2);

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
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_MODBUS_ANSWER,          ac_modbus_answer,           0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_MODBUS_ANSWER,          0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
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
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
