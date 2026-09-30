/****************************************************************************
 *          test_secret_traffic.c
 *
 *          Regression test for the secret gbuffer in the C_TCP `traffic`
 *          trace.
 *
 *          The trace dumps what C_TCP sends, in clear (before TLS). A frame
 *          sent with `"__secret__": true` in the kw of EV_TX_DATA carries a
 *          credential (an SMTP AUTH line): its bytes must never reach the
 *          trace, which prints "<N bytes hidden>" instead. A normal frame is
 *          still dumped.
 *
 *          Both tx paths are checked:
 *            - a frame QUEUED while the client connects (ac_tx_data_queued,
 *              written later by start_pending_writes): the flag must survive
 *              the queue;
 *            - a frame written at once on the connected client (ac_tx_data).
 *
 *          And the memory of a secret gbuffer is wiped when it is freed,
 *          and when it grows (the old block): the test wraps the gbmem
 *          allocators (gbmem_set_allocators) and looks into every block
 *          freed. A plain gbuffer with the same kind of content is the
 *          control that the look works. A secret appended to a gbuffer too
 *          small for it fails, and the part copied is flagged (hidden and
 *          wiped) too. A secret gbuffer is never serialized
 *          (gbuffer_serialize() answers NULL, and a kw carrying it crosses
 *          without it). What is RECEIVED cannot be marked: its dump masks
 *          what can be told in the bytes (mask_secrets_in_text(): an HTTP
 *          Cookie/Authorization header, a password=..., a json "token":).
 *
 *          Topology: the pepon server (C_IOGATE -> C_TCP_S + C_CHANNEL ->
 *          C_PROT_RAW -> C_TCP, no echo) and a pure C_TCP client, the only
 *          gobj with the `traffic` trace armed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>
#include <c_pepon.h>

#define APP             "test_secret_traffic"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "Secret gbuffers hidden from the C_TCP traffic trace"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define CLIENT_URL          "tcp://127.0.0.1:7797"
#define PLAIN_FRAME         "VISIBLE-FRAME-01\r\n"
#define SECRET_QUEUED       "HUNTER2-QUEUED-1\r\n"
#define SECRET_DIRECT       "HUNTER2-DIRECT-1\r\n"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */

PRIVATE BOOL s_capturing = FALSE;
PRIVATE int s_plain_seen = 0;
PRIVATE int s_secret_seen = 0;
PRIVATE int s_hidden_seen = 0;
PRIVATE int s_rx_secret_seen = 0;
PRIVATE int s_rx_masked_seen = 0;
PRIVATE int s_rx_plain_seen = 0;

/*
 *  Allocators wrapped: each block carries its size in front, so a free
 *  can look into it
 */
#define WRAP_HEADER     16
PRIVATE sys_malloc_fn_t  s_orig_malloc;
PRIVATE sys_realloc_fn_t s_orig_realloc;
PRIVATE sys_calloc_fn_t  s_orig_calloc;
PRIVATE sys_free_fn_t    s_orig_free;
PRIVATE int s_freed_with_secret = 0;
PRIVATE int s_freed_with_control = 0;

GOBJ_DEFINE_GCLASS(C_TEST_SECRET_TRAFFIC);

typedef enum {
    PHASE_INIT = 0,
    PHASE_CONNECTING,
    PHASE_CONNECTED,
    PHASE_DONE,
} phase_t;

typedef struct {
    hgobj   timer;
    hgobj   pepon;
    hgobj   clitcp;
    phase_t phase;
} PRIVATE_DATA;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'test_secret_traffic',                         \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'domain_dir': 'test_secret_traffic',                        \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcp://0.0.0.0:7797',                  \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '7797'                                \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'secret-traffic',                               \n\
            'gclass': 'C_TEST_SECRET_TRAFFIC',                      \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': '(^^__input_url__^^)',               \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,1],                                 \n\
                '__vars__': {},                                     \n\
                '__content__': {                                    \n\
                    'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-52s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-52s (%d)\n", name, got);
    }
}

PRIVATE void check_true(const char *name, BOOL got)
{
    check_int(name, got?1:0, 1);
}

/*
 *  A log handler that only looks: the traffic trace goes through the log
 *  handlers, so what it prints can be searched here. The dump splits the
 *  bytes in lines of 16, so each frame starts with a 16-byte marker.
 */
PRIVATE int capture_logs(void *h, int priority, const char *bf, size_t len)
{
    if(!s_capturing) {
        return 0;
    }
    if(strstr(bf, "VISIBLE-FRAME-01")) {
        s_plain_seen++;
    }
    if(strstr(bf, "HUNTER2")) {
        s_secret_seen++;
    }
    if(strstr(bf, "bytes hidden")) {
        s_hidden_seen++;
    }
    if(strstr(bf, "Zq7Xw9Y")) {
        s_rx_secret_seen++;
    }
    if(strstr(bf, "\"masked\":")) {
        s_rx_masked_seen++;
    }
    if(strstr(bf, "RX-PLAIN-FRAME-1")) {
        s_rx_plain_seen++;
    }
    return 0;
}

PRIVATE void look_into_freed_block(void *p, size_t size)
{
    if(memmem(p, size, "WIPE-SECRET", 11)) {
        s_freed_with_secret++;
    }
    if(memmem(p, size, "WIPE-CONTROL", 12)) {
        s_freed_with_control++;
    }
}

PRIVATE void *wrap_malloc(size_t size)
{
    char *p = s_orig_malloc(size + WRAP_HEADER);
    if(!p) {
        return NULL;
    }
    *(size_t *)p = size;
    return p + WRAP_HEADER;
}

PRIVATE void wrap_free(void *ptr)
{
    if(!ptr) {
        return;
    }
    char *p = (char *)ptr - WRAP_HEADER;
    look_into_freed_block(ptr, *(size_t *)p);
    s_orig_free(p);
}

PRIVATE void *wrap_realloc(void *ptr, size_t size)
{
    if(!ptr) {
        return wrap_malloc(size);
    }
    size_t old_size = *(size_t *)((char *)ptr - WRAP_HEADER);
    void *q = wrap_malloc(size);
    if(!q) {
        return NULL;
    }
    memcpy(q, ptr, old_size < size? old_size : size);
    wrap_free(ptr);     // the old block is freed as it is
    return q;
}

PRIVATE void *wrap_calloc(size_t n, size_t size)
{
    void *p = wrap_malloc(n*size);
    if(p) {
        memset(p, 0, n*size);
    }
    return p;
}

PRIVATE void check_gbuffer_wipe(void)
{
    s_freed_with_secret = 0;
    s_freed_with_control = 0;

    /*
     *  The control: a plain gbuffer is freed with its bytes in it
     */
    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "WIPE-CONTROL-01");
    GBUFFER_DECREF(gbuf)
    check_int("a plain gbuffer is freed with its bytes (control)", s_freed_with_control, 1);

    /*
     *  A secret gbuffer freed
     */
    gbuf = gbuffer_create(64, 64);
    gbuffer_set_secret(gbuf, TRUE);
    gbuffer_append_string(gbuf, "WIPE-SECRET-01");
    GBUFFER_DECREF(gbuf)

    /*
     *  A secret gbuffer that grows: the old block is freed on the way
     */
    gbuf = gbuffer_create(16, 4096);
    gbuffer_set_secret(gbuf, TRUE);
    gbuffer_append_string(gbuf, "WIPE-SECRET-02");
    gbuffer_append_string(gbuf, "-and-more-bytes-to-make-it-grow");
    check_true("the secret gbuffer grew", gbuffer_leftbytes(gbuf) > 16);
    GBUFFER_DECREF(gbuf)

    check_int("no secret gbuffer is freed with its bytes", s_freed_with_secret, 0);
}

/*
 *  What is RECEIVED cannot be marked secret by its sender: the traffic dump
 *  masks what can be told in the bytes, the length kept
 */
PRIVATE void check_received_dump(void)
{
    char text[] =
        "GET /x?user=bob&password=q-hunter2 HTTP/1.1\r\n"
        "Cookie: sid=c-hunter2; theme=dark\r\n"
        "Authorization: Bearer a-hunter2\r\n"
        "\r\n"
        "{\"client_secret\": \"j-hunter2\", \"pin\": 7, \"token\":42}";
    char expected[] =
        "GET /x?user=bob&password=********* HTTP/1.1\r\n"
        "Cookie: *************************\r\n"
        "Authorization: Bearer *********\r\n"
        "\r\n"
        "{\"client_secret\": \"*********\", \"pin\": 7, \"token\":**}";
    size_t masked = mask_secrets_in_text(text, strlen(text));
    check_true("the credentials of the bytes are masked, the rest kept",
        strcmp(text, expected)==0
    );
    if(strcmp(text, expected)!=0) {
        printf("     got      %s\n     expected %s\n", text, expected);
    }
    check_int("the bytes masked", (int)masked, 9+25+9+9+2);

    /*
     *  The dump of a received frame: the secret on a line of its own
     */
    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "RX-PLAIN-FRAME-1password=Zq7Xw9Y\r\n");
    s_rx_secret_seen = 0;
    s_rx_masked_seen = 0;
    s_rx_plain_seen = 0;
    s_capturing = TRUE;
    gobj_trace_dump_gbuf(0, gbuf, "a received frame");
    gobj_trace_dump(0, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), "raw bytes");
    s_capturing = FALSE;
    GBUFFER_DECREF(gbuf)
    check_true("the received frame is dumped", s_rx_plain_seen >= 2);
    check_int("its credential is not", s_rx_secret_seen, 0);
    check_true("the dump says how many bytes it masked", s_rx_masked_seen > 0);
}

/*
 *  A secret appended to a gbuffer too small for it: the append fails, and
 *  the part it copied is a secret too. And a secret gbuffer is never
 *  serialized: its base64 would go into a json the traces print.
 */
PRIVATE void check_gbuffer_append_and_serialize(void)
{
    s_freed_with_secret = 0;

    gbuffer_t *src = gbuffer_create(64, 64);
    gbuffer_set_secret(src, TRUE);
    gbuffer_append_string(src, "WIPE-SECRET-03-longer-than-its-destination");
    gbuffer_t *dst = gbuffer_create(16, 16);
    check_int("appending a secret to a small gbuffer fails", gbuffer_append_gbuf(dst, src), -1);
    check_true("the destination holds part of the secret",
        memmem(gbuffer_cur_rd_pointer(dst), gbuffer_leftbytes(dst), "WIPE-SECRET", 11)?TRUE:FALSE
    );
    check_true("the destination of a failed append is secret", gbuffer_is_secret(dst));
    GBUFFER_DECREF(dst)
    GBUFFER_DECREF(src)
    check_int("the destination of a failed append is freed wiped", s_freed_with_secret, 0);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "VISIBLE-SERIALIZED");
    json_t *jn = gbuffer_serialize(0, gbuf);
    check_true("a plain gbuffer is serialized (control)", jn?TRUE:FALSE);
    JSON_DECREF(jn)

    gbuffer_set_secret(gbuf, TRUE);
    jn = gbuffer_serialize(0, gbuf);
    check_true("a secret gbuffer is not serialized", jn?FALSE:TRUE);
    JSON_DECREF(jn)

    json_t *kw = json_pack("{s:s, s:I}",
        "note", "visible",
        "gbuffer", (json_int_t)(uintptr_t)gbuf     // the kw takes the reference
    );
    kw = kw_serialize(0, kw);
    check_true("a serialized kw carries no secret gbuffer",
        !json_object_get(kw, "__gbuffer___") && !json_object_get(kw, "gbuffer")
    );
    check_true("the rest of the kw is serialized", json_object_get(kw, "note")?TRUE:FALSE);
    JSON_DECREF(kw)
    check_int("the secret gbuffer is freed wiped", s_freed_with_secret, 0);
}

PRIVATE void send_frame(hgobj gobj, const char *frame, BOOL secret)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, frame);
    json_t *kw_tx = json_pack("{s:I, s:b}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf,
        "__secret__", secret
    );
    gobj_send_event(priv->clitcp, EV_TX_DATA, kw_tx, gobj);
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->pepon = gobj_create_pure_child(
        "server", C_PEPON, json_pack("{s:b}", "do_echo", 0), gobj
    );

    /*
     *  timeout_inactivity > 0: a frame sent while connecting is QUEUED
     */
    priv->clitcp = gobj_create_pure_child(
        "clitcp",
        C_TCP,
        json_pack("{s:s, s:I, s:b}",
            "url", CLIENT_URL,
            "timeout_inactivity", (json_int_t)60000,
            "no_tx_ready_event", 1
        ),
        gobj
    );
    gobj_set_gobj_trace(priv->clitcp, "traffic", TRUE, 0);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->pepon);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    if(gobj_is_running(priv->clitcp)) {
        gobj_stop(priv->clitcp);
    }
    gobj_stop(priv->pepon);
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_play(priv->pepon);
    set_timeout(priv->timer, 500);  // let the server listen
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case PHASE_INIT:
            check_gbuffer_wipe();
            check_gbuffer_append_and_serialize();
            check_received_dump();
            priv->phase = PHASE_CONNECTING;
            s_capturing = TRUE;
            gobj_start(priv->clitcp);
            send_frame(gobj, SECRET_QUEUED, TRUE);
            send_frame(gobj, PLAIN_FRAME, FALSE);
            set_timeout(priv->timer, 3000);     // the connection must come
            break;

        case PHASE_CONNECTED:
            s_capturing = FALSE;
            priv->phase = PHASE_DONE;
            check_true("the plain frame is in the traffic trace", s_plain_seen > 0);
            check_int("no secret frame is in the traffic trace", s_secret_seen, 0);
            check_int("both secret frames are dumped as hidden", s_hidden_seen, 2);
            set_yuno_must_die();
            break;

        default:
            printf("FAIL the client did not connect\n");
            s_result += -1;
            priv->phase = PHASE_DONE;
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_CONNECTING) {
        priv->phase = PHASE_CONNECTED;
        send_frame(gobj, SECRET_DIRECT, TRUE);
        set_timeout(priv->timer, 300);  // let the writes complete
    }

    KW_DECREF(kw)
    return 0;
}

PRIVATE int ac_ignore(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
    SDATA_END()
};

PRIVATE int register_c_test_secret_traffic(void)
{
    static const GMETHODS gmt = {
        .mt_create = mt_create,
        .mt_start = mt_start,
        .mt_stop = mt_stop,
        .mt_play = mt_play,
        .mt_pause = mt_pause,
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_CONNECTED,      0},
        {EV_DISCONNECTED,   0},
        {EV_RX_DATA,        0},
        {EV_TX_READY,       0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,        ac_timeout,     0},
        {EV_CONNECTED,      ac_connected,   0},
        {EV_DISCONNECTED,   ac_ignore,      0},
        {EV_RX_DATA,        ac_ignore,      0},
        {EV_TX_READY,       ac_ignore,      0},
        {EV_STOPPED,        ac_ignore,      0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,   st_idle},
        {0, 0}
    };

    hgclass gc = gclass_create(
        C_TEST_SECRET_TRAFFIC,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    int result = 0;
    result += register_c_pepon();
    result += register_c_test_secret_traffic();
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_register_handler("capture_logs", 0, capture_logs, 0);
    gobj_log_add_handler("capture_logs", "capture_logs", LOG_OPT_ALL, 0);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gbmem_get_allocators(&s_orig_malloc, &s_orig_realloc, &s_orig_calloc, &s_orig_free);
    gbmem_set_allocators(wrap_malloc, wrap_realloc, wrap_calloc, wrap_free);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    yuneta_setup(
        NULL,                   // persistent_attrs, default internal dbsimple
        NULL,                   // command_parser
        NULL,                   // stats_parser
        NULL,                   // authz_checker
        NULL,                   // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    int result = yuneta_entry_point(
        argc, argv,
        APP, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL                    // cleaning
    );

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
