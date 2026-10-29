/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_MODBUS_M master against a raw Modbus TCP slave.
 *
 *          The server is raw (C_PROT_RAW) and answers Modbus TCP by hand.
 *          The master polls two slaves:
 *          - slave 1: block A (holding 0, 2 registers: "a", uint32), a
 *            block whose address is out of range (disabled at start), and
 *            block B (holding 10, 1 register: "b", uint16);
 *          - slave 2: block C (holding 0, 1 register: "c", uint16).
 *
 *          A cycle starts at the request of block A. In cycles 1 and 2,
 *          slave 2 answers exception 11, and block B exception 2 in cycle 1
 *          and the echo of a write (06) in cycle 2; in cycle 3 every block
 *          answers. So:
 *          - cycle 1 publishes {slave_id 1, a} and nothing of slave 2, and
 *            ends at once. Up to 7.27.7 it also published b and c as 0 (the
 *            cells were never read), and over TCP each exception waited for
 *            a CRC that TCP does not carry: the cycle took timeout_response
 *            per exception;
 *          - each exception is logged once, when the block starts failing,
 *            and "Modbus block answered again" when it answers (cycle 3);
 *          - on the message of cycle 1 a write is queued: the slave must see
 *            it once, and the polling must go on after it. Up to 7.27.7 the
 *            write was released while the queue still held it (BAD
 *            json_decref()) and the polling stopped.
 *          - with it, four more requests (REQUESTS, below): a write of three
 *            registers (function 16), a read of them back, a write the slave
 *            refuses with exception 1, and a coil (function 05, ON is
 *            0xFF00). Each must be answered with EV_MODBUS_ANSWER, in order,
 *            with what the slave said. Up to 7.29.2 no request was answered,
 *            a multiple write went out without its quantity and byte count,
 *            and a coil went out as the value given.
 *          - a request sent before the master connects is answered at once,
 *            as failed.
 *          - the traffic trace of the master and of its C_TCP is armed: a
 *            write value (two of them, 06 and 16, made to be found) must
 *            never show in it. The echo of an 06 carries its value back,
 *            and the transport's dump of what it RECEIVES cannot hide it
 *            (nobody can mark received bytes): only that line may.
 *          - (REV-CLOUD V45) the requests go out at once on the message of
 *            cycle 1, not a timeout_polling later; a request whose numbers
 *            are not numbers ("abc", null, "010", 20.9, "12ab"), or that
 *            passes 65535, is refused, and the refusal of a value that
 *            does not fit does not show it; a read answered after its
 *            timeout_response, when the next request is already out, is
 *            dropped by its transaction id instead of answering the next
 *            one; and an answer with an MBAP of length 3 fails its request
 *            as a bad answer (a warning), not as an internal error.
 *          - (REV-CLOUD V46) an exception of another function (0x84 to a
 *            03) fails its read as a bad answer, not as the exception; the
 *            late echo of a write (06, its value made to be found), which
 *            comes while a read is in flight, is dumped hidden by the
 *            master's own traffic trace; the half frame that follows an
 *            answer in the same bytes is kept as the start of the next one,
 *            which is read whole and dropped, and the answer after it is
 *            taken; an MBAP of length 65535 drops the connection instead of
 *            swallowing every answer after it; and a block answered with a
 *            write function (06, in cycle 2) fails, not "answered again".
 *          - (REV-CLOUD V46) the disabled block is reported once, at start,
 *            and every cycle skips it: no log, and no timeout_polling wait
 *            between block A and block B. Up to the fix each cycle logged
 *            "cur_map_ DISABLED" and waited timeout_polling there, so no
 *            cycle ended within CYCLE_MAX_MS.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_modbus_m.h>
#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SLAVE_URL       "tcp://127.0.0.1:7759"
#define CYCLE_MAX_MS    800     // a cycle answered at once; an exception that waits costs 1 s
#define WRITE_ADDRESS   20
#define WRITE_VALUE     7
#define MULTI_ADDRESS   30      // function 16, three registers, then read back
#define REFUSED_ADDRESS 40      // the slave answers exception 1
#define COIL_ADDRESS    5       // function 05
#define LATE_ADDRESS    50      // answered late: after the next request went out
#define AFTER_ADDRESS   51      // the next request: the late answer of 50 comes first
#define EMPTY_ADDRESS   60      // answered with an MBAP of length 3 and no registers
#define SECRET_VALUE    "70000" // a write value no log nor comment may show
#define LATE_VALUE      111
#define AFTER_VALUE     "222"   // in the answers table
#define AFTER_NUMBER    222
#define REQUEST_MAX_MS  500     // a request sent on the message of a cycle goes out at once
#define SECRET6_ADDRESS 70      // function 06 with a secret value: 0x7E5F
#define SECRET16_ADDRESS 72     // function 16 with a secret value: 0x5A3C, 0x6B4D
#define X84_ADDRESS     64      // a read answered with an exception of another function
#define LW6_ADDRESS     74      // function 06 with the secret value, answered late
#define LA2_ADDRESS     75      // the next read: the late echo of 74 comes first
#define HLF_ADDRESS     62      // answered, and half of a frame after it in the same bytes
#define NXT_ADDRESS     63      // the next read: the other half comes first
#define MBP_ADDRESS     65      // a frame with an MBAP length of 65535 comes first
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
    /*
     *  Refused at once, so answered before the queued ones: a number of a
     *  request is a json integer or its text, as the command parser reads
     *  one -- no "abc" as 0, no "010" as 8, no 20.9 as 20 -- and it fits.
     *  Up to the fix of REV-CLOUD V45.2 every one of them was sent
     */
    "{'tag': 'a_txt', 'type': 'holding_register', 'address': 'abc', 'value': 1}",
    "{'tag': 'a_null', 'type': 'holding_register', 'address': null, 'value': 1}",
    "{'tag': 'a_oct', 'operation': 'read', 'type': 'holding_register', 'address': '010'}",
    "{'tag': 'a_real', 'operation': 'read', 'type': 'holding_register', 'address': 20.9}",
    "{'tag': 'id_txt', 'id': 'x', 'type': 'holding_register', 'address': 20, 'value': 1}",
    "{'tag': 'v_txt', 'type': 'holding_register', 'address': 20, 'value': '12ab'}",
    "{'tag': 'v_big', 'type': 'holding_register', 'address': 20, 'value': " SECRET_VALUE "}",
    "{'tag': 'a_end', 'operation': 'read', 'type': 'holding_register', 'address': 65535, 'size': 2}",
    "{'tag': 'w_end', 'type': 'holding_register', 'address': 65534, 'value': [1, 2, 3]}",
    /*
     *  Queued, and sent one after another
     */
    "{'tag': 'w06', 'id': 1, 'type': 'holding_register', 'address': '20', 'value': '7'}",
    "{'tag': 'w16', 'id': 1, 'type': 'holding_register', 'address': 30, 'value': [11, 22, 33]}",
    "{'tag': 'r03', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 30, 'size': 3}",
    "{'tag': 'x40', 'id': 1, 'type': 'holding_register', 'address': 40, 'value': 1}",
    "{'tag': 'c05', 'id': 1, 'type': 'coil', 'address': 5, 'value': 1}",
    "{'tag': 'pw6', 'id': 1, 'type': 'holding_register', 'address': 70, 'value': 32351}",
    "{'tag': 'p16', 'id': 1, 'type': 'holding_register', 'address': 72, 'value': [23100, 27469]}",
    "{'tag': 'lat', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 50}",
    "{'tag': 'aft', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 51}",
    "{'tag': 'm03', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 60}",
    "{'tag': 'x84', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 64}",
    "{'tag': 'lw6', 'id': 1, 'type': 'holding_register', 'address': 74, 'value': 32351}",
    "{'tag': 'la2', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 75}",
    "{'tag': 'hlf', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 62}",
    "{'tag': 'nxt', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 63}",
    "{'tag': 'mbp', 'operation': 'read', 'id': 1, 'type': 'holding_register', 'address': 65}",
    0
};
PRIVATE const char *answers[] = {
    "{'tag': 'a_txt', 'result': -1, 'error_code': 0}",
    "{'tag': 'a_null', 'result': -1, 'error_code': 0}",
    "{'tag': 'a_oct', 'result': -1, 'error_code': 0}",
    "{'tag': 'a_real', 'result': -1, 'error_code': 0}",
    "{'tag': 'id_txt', 'result': -1, 'error_code': 0}",
    "{'tag': 'v_txt', 'result': -1, 'error_code': 0}",
    "{'tag': 'v_big', 'result': -1, 'error_code': 0}",
    "{'tag': 'a_end', 'result': -1, 'error_code': 0}",
    "{'tag': 'w_end', 'result': -1, 'error_code': 0}",
    "{'tag': 'w06', 'result': 0, 'error_code': 0}",
    "{'tag': 'w16', 'result': 0, 'error_code': 0}",
    "{'tag': 'r03', 'result': 0, 'error_code': 0, 'values': [11, 22, 33]}",
    "{'tag': 'x40', 'result': -1, 'error_code': 1}",
    "{'tag': 'c05', 'result': 0, 'error_code': 0}",
    "{'tag': 'pw6', 'result': 0, 'error_code': 0}",
    "{'tag': 'p16', 'result': 0, 'error_code': 0}",
    "{'tag': 'lat', 'result': -1, 'error_code': 0}",        // timeout_response
    "{'tag': 'aft', 'result': 0, 'error_code': 0, 'values': [" AFTER_VALUE "]}",
    "{'tag': 'm03', 'result': -1, 'error_code': 0}",        // an answer with no registers
    "{'tag': 'x84', 'result': -1, 'error_code': 0}",        // a bad answer, not exception 2
    "{'tag': 'lw6', 'result': -1, 'error_code': 0}",        // timeout_response
    "{'tag': 'la2', 'result': 0, 'error_code': 0, 'values': [333]}",
    "{'tag': 'hlf', 'result': 0, 'error_code': 0, 'values': [444]}",
    "{'tag': 'nxt', 'result': 0, 'error_code': 0, 'values': [555]}",
    "{'tag': 'mbp', 'result': -1, 'error_code': 0}",        // the connection dropped
    0
};

PRIVATE char slaves_config[] = "\
[                                                                   \n\
    {                                                               \n\
        'id': 1,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 2},   \n\
            {'type': 'holding_register', 'address': 70000, 'size': 1}, \n\
            {'type': 'holding_register', 'address': 10, 'size': 1}   \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'a', 'type': 'holding_register', 'address': 0, 'format': 'uint32'}, \n\
            {'id': 'b', 'type': 'holding_register', 'address': 10, 'format': 'uint16'} \n\
        ]                                                           \n\
    },                                                              \n\
    {                                                               \n\
        'id': 2,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1}    \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'c', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
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
    uint64_t t_cycle;           // CYCLE_MAX_MS from the request of block A
    int writes_seen;            // at the slave: must be 1
    uint16_t multi[3];          // at the slave: what the write of three registers left
    BOOL write_queued;
    int answers_seen;           // answers of the queued requests, in order
    uint64_t t_requests;        // REQUEST_MAX_MS from the requests, to the first answer of a queued one
    uint8_t late_t_id[2];       // the transaction id of the request answered late
    BOOL late_held;
    uint8_t lw6_req[12];        // the write answered late: its echo comes with the next read
    BOOL lw6_held;
    BOOL mbap_sent;             // the bad MBAP: the master drops the connection, and comes back
    BOOL early_answered;        // the request sent before connecting
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
 *  Send an answer from the slave
 ***************************************************************************/
PRIVATE void slave_send(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  An exception answer to a request
 ***************************************************************************/
PRIVATE void slave_exception(hgobj gobj, const uint8_t *req, int code)
{
    uint8_t rsp[9];
    memcpy(rsp, req, 2);                // transaction id
    rsp[2] = 0;                         // protocol
    rsp[3] = 0;
    rsp[4] = 0;                         // length: unit, function, code
    rsp[5] = 3;
    rsp[6] = req[6];                    // unit
    rsp[7] = (uint8_t)(req[7] | 0x80);  // function with the exception bit
    rsp[8] = (uint8_t)code;
    slave_send(gobj, rsp, sizeof(rsp));
}

/***************************************************************************
 *  The frame of the registers of a read, in rsp (9 + 2*n bytes)
 ***************************************************************************/
PRIVATE size_t registers_frame(uint8_t *rsp, const uint8_t *req, const uint16_t *regs, int n)
{
    memcpy(rsp, req, 2);
    rsp[2] = 0;
    rsp[3] = 0;
    rsp[4] = 0;
    rsp[5] = (uint8_t)(3 + 2*n);
    rsp[6] = req[6];
    rsp[7] = req[7];
    rsp[8] = (uint8_t)(2*n);
    for(int i=0; i<n; i++) {
        rsp[9 + 2*i] = (uint8_t)(regs[i] >> 8);
        rsp[10 + 2*i] = (uint8_t)(regs[i] & 0xFF);
    }
    return (size_t)(9 + 2*n);
}

/***************************************************************************
 *  The registers of a read
 ***************************************************************************/
PRIVATE void slave_registers(hgobj gobj, const uint8_t *req, const uint16_t *regs, int n)
{
    uint8_t rsp[9 + 2*8];
    memcpy(rsp, req, 2);
    rsp[2] = 0;
    rsp[3] = 0;
    rsp[4] = 0;
    rsp[5] = (uint8_t)(3 + 2*n);
    rsp[6] = req[6];
    rsp[7] = req[7];
    rsp[8] = (uint8_t)(2*n);
    for(int i=0; i<n; i++) {
        rsp[9 + 2*i] = (uint8_t)(regs[i] >> 8);
        rsp[10 + 2*i] = (uint8_t)(regs[i] & 0xFF);
    }
    slave_send(gobj, rsp, (size_t)(9 + 2*n));
}

/***************************************************************************
 *  One request at the slave
 ***************************************************************************/
PRIVATE void slave_request(hgobj gobj, const uint8_t *req)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int unit = req[6];
    int function = req[7];
    int address = (req[8] << 8) | req[9];
    int quantity = (req[10] << 8) | req[11];   // the value, in a write

    if((function == 6 && address == SECRET6_ADDRESS && quantity == 0x7E5F) ||
            (function == 16 && address == SECRET16_ADDRESS && quantity == 2 && req[12] == 4 &&
             req[13] == 0x5A && req[14] == 0x3C && req[15] == 0x6B && req[16] == 0x4D)) {
        uint8_t rsp[12];
        memcpy(rsp, req, 12);   // the echo: of 06 its value, of 16 its quantity
        rsp[4] = 0;
        rsp[5] = 6;
        slave_send(gobj, rsp, sizeof(rsp));
        return;
    }

    /*
     *  A frame that is not the answer of anything: 11 bytes, its MBAP says
     *  5 (unit, function, byte count, one register), its t_id is no request's
     */
    static const uint8_t stray[11] = {0xBE, 0xEF, 0, 0, 0, 5, 1, 3, 2, 0, 42};

    if(function == 6 && address == LW6_ADDRESS && quantity == 0x7E5F) {
        memcpy(priv->lw6_req, req, 12);     // its echo comes with the next read, too late
        priv->lw6_held = TRUE;
        return;
    }

    if(function == 3 && unit == 1 && address == LA2_ADDRESS && quantity == 1) {
        if(!priv->lw6_held) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The write answered late did not reach the slave first",
                NULL
            );
        }
        /*
         *  The late echo of the write, with its value: the master's traffic
         *  trace must hide it although a read is in flight
         */
        uint8_t echo[12];
        memcpy(echo, priv->lw6_req, 12);
        echo[4] = 0;
        echo[5] = 6;
        slave_send(gobj, echo, sizeof(echo));
        uint16_t regs[1] = {333};
        slave_registers(gobj, req, regs, 1);
        return;
    }

    if(function == 3 && unit == 1 && address == X84_ADDRESS && quantity == 1) {
        uint8_t other[12];
        memcpy(other, req, 12);
        other[7] = 4;                       // an exception of function 04 to a 03
        slave_exception(gobj, other, 2);
        return;
    }

    if(function == 3 && unit == 1 && address == HLF_ADDRESS && quantity == 1) {
        uint8_t rsp[9 + 2 + 4];
        uint16_t regs[1] = {444};
        size_t len = registers_frame(rsp, req, regs, 1);
        memcpy(rsp + len, stray, 4);        // and half of a frame, in the same bytes
        slave_send(gobj, rsp, len + 4);
        return;
    }

    if(function == 3 && unit == 1 && address == NXT_ADDRESS && quantity == 1) {
        uint8_t rsp[7 + 9 + 2];
        memcpy(rsp, stray + 4, 7);          // the other half first
        uint16_t regs[1] = {555};
        size_t len = registers_frame(rsp + 7, req, regs, 1);
        slave_send(gobj, rsp, 7 + len);
        return;
    }

    if(function == 3 && unit == 1 && address == MBP_ADDRESS && quantity == 1) {
        /*
         *  A head with another t_id and an MBAP length of 65535, then the
         *  answer: nothing says where a frame ends after such a head
         */
        uint8_t rsp[9 + 9 + 2] = {0xAB, 0xCD, 0, 0, 0xFF, 0xFF, 1, 3, 2};
        uint16_t regs[1] = {666};
        size_t len = registers_frame(rsp + 9, req, regs, 1);
        priv->mbap_sent = TRUE;
        slave_send(gobj, rsp, 9 + len);
        return;
    }

    if(function == 6 && address == REFUSED_ADDRESS) {
        slave_exception(gobj, req, 1);          // illegal function: a meter that is locked
        return;
    }

    if(function == 16) {
        if(address != MULTI_ADDRESS || quantity != 3 || req[12] != 6) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The write of three registers did not reach the slave as sent",
                "address",      "%d", address,
                "quantity",     "%d", quantity,
                "byte_count",   "%d", req[12],
                NULL
            );
            slave_exception(gobj, req, 3);
            return;
        }
        for(int i=0; i<3; i++) {
            priv->multi[i] = (uint16_t)((req[13 + 2*i] << 8) | req[14 + 2*i]);
        }
        uint8_t rsp[12];
        memcpy(rsp, req, 12);   // the echo of a multiple write: address and quantity
        rsp[4] = 0;
        rsp[5] = 6;
        slave_send(gobj, rsp, sizeof(rsp));
        return;
    }

    if(function == 5) {
        if(address != COIL_ADDRESS || quantity != 0xFF00) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The coil did not reach the slave as ON (0xFF00)",
                "address",      "%d", address,
                "value",        "%d", quantity,
                NULL
            );
            slave_exception(gobj, req, 3);
            return;
        }
        slave_send(gobj, req, 12);  // a write is answered with its echo
        return;
    }

    if(function == 3 && unit == 1 && address == MULTI_ADDRESS && quantity == 3) {
        slave_registers(gobj, req, priv->multi, 3);
        return;
    }

    if(function == 3 && unit == 1 && address == LATE_ADDRESS && quantity == 1) {
        memcpy(priv->late_t_id, req, 2);    // answered with the next request, too late
        priv->late_held = TRUE;
        return;
    }

    if(function == 3 && unit == 1 && address == AFTER_ADDRESS && quantity == 1) {
        if(!priv->late_held) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The request answered late did not reach the slave first",
                NULL
            );
        }
        /*
         *  The late answer of the request that timed out, with its own
         *  transaction id and the same slave, function and size: the master
         *  must not take it as the answer of this one
         */
        uint8_t late[2 + 7];
        memcpy(late, req, sizeof(late));
        memcpy(late, priv->late_t_id, 2);
        uint16_t late_value[1] = {LATE_VALUE};
        slave_registers(gobj, late, late_value, 1);

        uint16_t regs[1] = {AFTER_NUMBER};
        slave_registers(gobj, req, regs, 1);
        return;
    }

    if(function == 3 && unit == 1 && address == EMPTY_ADDRESS && quantity == 1) {
        uint8_t rsp[9];
        memcpy(rsp, req, 2);
        rsp[2] = 0;
        rsp[3] = 0;
        rsp[4] = 0;
        rsp[5] = 3;             // unit, function, byte count: no registers
        rsp[6] = req[6];
        rsp[7] = req[7];
        rsp[8] = 0;
        slave_send(gobj, rsp, sizeof(rsp));
        return;
    }

    if(function == 6) {
        priv->writes_seen++;
        if(address != WRITE_ADDRESS || quantity != WRITE_VALUE || priv->writes_seen != 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The write did not reach the slave once, as sent",
                "address",      "%d", address,
                "value",        "%d", quantity,
                "writes_seen",  "%d", priv->writes_seen,
                NULL
            );
        }
        slave_send(gobj, req, 12);  // a write is answered with its echo
        return;
    }

    if(function != 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected function at the slave",
            "modbus_function", "%d", function,
            NULL
        );
        slave_exception(gobj, req, 1);
        return;
    }

    if(unit == 1 && address == 0 && quantity == 2) {
        priv->cycle++;
        priv->t_cycle = start_msectimer(CYCLE_MAX_MS);
        uint16_t regs[2] = {0, 0};
        switch(priv->cycle) {
            case 1:
                regs[0] = 0x0001;
                regs[1] = 0x0002;
                break;
            case 2:
                regs[1] = 3;
                break;
            default:
                regs[1] = 4;
                break;
        }
        slave_registers(gobj, req, regs, 2);

    } else if(unit == 1 && address == 10 && quantity == 1) {
        if(priv->cycle == 1) {
            slave_exception(gobj, req, 2);      // illegal data address
        } else if(priv->cycle == 2) {
            uint8_t rsp[12];                    // a write echo (06) to a read
            memcpy(rsp, req, 12);
            rsp[4] = 0;
            rsp[5] = 6;
            rsp[7] = 6;
            slave_send(gobj, rsp, sizeof(rsp));
        } else {
            uint16_t regs[1] = {9};
            slave_registers(gobj, req, regs, 1);
        }

    } else if(unit == 2 && address == 0 && quantity == 1) {
        if(priv->cycle < 3) {
            slave_exception(gobj, req, 11);     // gateway target device failed to respond
        } else {
            uint16_t regs[1] = {5};
            slave_registers(gobj, req, regs, 1);
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected read at the slave",
            "unit",         "%d", unit,
            "address",      "%d", address,
            "quantity",     "%d", quantity,
            NULL
        );
        slave_exception(gobj, req, 2);
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

    while(priv->rx_len >= 6) {
        size_t frame_len = 6 + (size_t)((priv->rx[4] << 8) | priv->rx[5]);
        if(frame_len < 12 || frame_len > 260) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request of an unexpected length",
                "frame_len",    "%lu", (unsigned long)frame_len,
                NULL
            );
            priv->rx_len = 0;
            return;
        }
        if(priv->rx_len < frame_len) {
            return;
        }
        slave_request(gobj, priv->rx);
        memmove(priv->rx, priv->rx + frame_len, priv->rx_len - frame_len);
        priv->rx_len -= frame_len;
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
 *  A message of the master must be what is expected
 ***************************************************************************/
PRIVATE BOOL check_message(hgobj gobj, json_t *kw, const char *expected_)
{
    json_t *expected = string2json(expected_, TRUE);
    BOOL ok = json_equal(kw, expected)? TRUE : FALSE;
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published what the cycle did not read",
            "published",    "%j", kw,
            "expected",     "%j", expected?expected:json_null(),
            NULL
        );
    }
    JSON_DECREF(expected)
    return ok;
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

    if(test_msectimer(priv->t_cycle)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A cycle with exceptions did not end at once",
            "cycle",        "%d", priv->cycle,
            "max_ms",       "%d", CYCLE_MAX_MS,
            NULL
        );
    }

    int slave_id = (int)kw_get_int(gobj, kw, "slave_id", 0, KW_REQUIRED);

    switch(priv->cycle) {
        case 1:
            check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 65538}");
            if(!priv->write_queued) {
                priv->write_queued = TRUE;
                priv->t_requests = start_msectimer(REQUEST_MAX_MS);
                for(int i=0; requests[i]; i++) {
                    gobj_send_event(priv->gobj_master, EV_SEND_MESSAGE, qjson(requests[i]), gobj);
                }
            }
            break;

        case 2:
            check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 3}");
            break;

        default:
            if(slave_id == 1) {
                check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 4, \"b\": 9}");
            } else {
                check_message(gobj, kw, "{\"slave_id\": 2, \"c\": 5}");
                if(priv->writes_seen != 1) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "The queued write did not reach the slave once",
                        "writes_seen",  "%d", priv->writes_seen,
                        NULL
                    );
                }
                if(answers[priv->answers_seen] != NULL || !priv->early_answered) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "Not every request was answered",
                        "answers_seen", "%d", priv->answers_seen,
                        "early_answered", "%d", priv->early_answered,
                        NULL
                    );
                }
                priv->done = TRUE;
                set_yuno_must_die();
            }
            break;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The server listens: the master is created, and connects
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
            json_pack("{s:s, s:s, s:i, s:i, s:o}",
                "modbus_protocol", "TCP",
                "url", SLAVE_URL,
                "timeout_polling", 1000,
                "timeout_response", 1,
                "slaves", jn_slaves
            ),
            gobj
        );
        gobj_start(priv->gobj_master);

        /*
         *  The traffic of the master and of its transport, to check that no
         *  write value shows in it (main_test1.c scans every line)
         */
        gobj_set_gobj_trace(priv->gobj_master, "traffic", TRUE, 0);
        gobj_set_gobj_trace(gobj_bottom_gobj(priv->gobj_master), "traffic", TRUE, 0);

        /*
         *  Not connected yet: answered at once, as failed
         */
        gobj_send_event(priv->gobj_master, EV_SEND_MESSAGE,
            qjson("{'tag': 'early', 'operation': 'read', 'type': 'holding_register', 'address': 0}"),
            gobj
        );
        if(!priv->early_answered) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request sent before connecting was not answered at once",
                NULL
            );
        }
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

    if(strcmp(tag, "early")==0) {
        int result = (int)kw_get_int(gobj, kw, "result", 0, KW_REQUIRED);
        if(result != -1 || strcmp(kw_get_str(gobj, kw, "comment", "", 0), "not connected")!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request sent before connecting was not refused",
                "answer",       "%j", kw,
                NULL
            );
        }
        priv->early_answered = TRUE;
        JSON_DECREF(kw)
        return 0;
    }

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

    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(strstr(comment, SECRET_VALUE)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The answer of a write shows its value in the comment",
            "tag",          "%s", tag,
            NULL
        );
    }
    if(strcmp(tag, "mbp")==0 && strcmp(comment, "the connection dropped before its answer")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The read after a bad MBAP was not failed by the drop",
            "comment",      "%s", comment,
            NULL
        );
    }
    if(strcmp(tag, "w06")==0 && test_msectimer(priv->t_requests)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request sent on the message of a cycle waited for the next cycle",
            "max_ms",       "%d", REQUEST_MAX_MS,
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

    if(!priv->done && !priv->mbap_sent && !gobj_is_shutdowning()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The connection closed before the third cycle",
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
GOBJ_DEFINE_GCLASS(C_TEST1);

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
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
