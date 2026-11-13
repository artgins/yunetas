/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_controlcenter_scenarios
 *          Tests the scenarios of the control center, and the routing of
 *          what its agents send back (c_controlcenter.c)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <yunetas.h>
#include "c_controlcenter.h"
#include "c_test_cc_peer.h"
#include "c_test_cc.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_controlcenter_scenarios"
#define APP_DOC         "Test the scenarios of the control center"
#define ASSERTION_KEY_FILE "/tmp/test_c_controlcenter_scenarios_assertion.jwk"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'timeout_periodic': 100,                                    \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': '__top_side__',                                 \n\
            'gclass': 'C_TEST_CC_PEER',                             \n\
            'autostart': true                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_TEST_CC_PEER',                             \n\
            'autostart': true                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_TEST_CC_PEER',                             \n\
            'autostart': true                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'treedb_authzs',                                \n\
            'gclass': 'C_TEST_CC_PEER',                             \n\
            'autostart': true                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'controlcenter',                                \n\
            'gclass': 'C_CONTROLCENTER',                            \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'operator_assertion_key': '" ASSERTION_KEY_FILE "'  \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'test_cc',                                      \n\
            'gclass': 'C_TEST_CC',                                  \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  The private key the control center signs its operator assertions with
 *  (operator_assertion_key), written for the run: a test key, good for
 *  nothing else. Its public half is in c_test_cc.c.
 ***************************************************************************/
PRIVATE const char *ASSERTION_KEY_JWK = "{\"kid\":\"controlcenter\",\"kty\":\"EC\",\"crv\":\"P-256\",\"alg\":\"ES256\",\"use\":\"sig\",\"x\":\"nXhUxejW5sVCIMMt5VPATGJS_nsEIvIHbw_xYLdpa1Q\",\"y\":\"EmyPj6BtdnzvQTYMrLgqtRxTZig5KqIS1hh2YEB2kW8\",\"d\":\"As0_L5bAc2Ga27EKaeu0KhDW5IjXIHxKfgXw2nFDKPo\"}";

PRIVATE int write_assertion_key(void)
{
    unlink(ASSERTION_KEY_FILE);
    FILE *file = fopen(ASSERTION_KEY_FILE, "w");
    if(!file) {
        printf("cannot write %s\n", ASSERTION_KEY_FILE);
        return -1;
    }
    fputs(ASSERTION_KEY_JWK, file);
    fclose(file);
    return 0;
}

/***************************************************************************
 *  Every command of the control center asks a permission. With no checker
 *  installed the default one is C_AUTHZ's, which this test yuno does not
 *  run. This test is about what the commands do, not about who may ask:
 *  everybody may do everything, but "viewer", who may only read the
 *  scenario "scn", and "writer", who may read and write "scn", "mr", "wr",
 *  "mrw" and "mrc" but run nothing -- the
 *  scope a role's `parameters` gives in C_AUTHZ
 *  (tests/c/authz_scoped_permissions), which is what the control center
 *  asks per scenario (case 14).
 ***************************************************************************/
static BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    BOOL allow = TRUE;
    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    BOOL on_scn = strcmp(kw_get_str(gobj, kw, "scenario_id", "", 0), "scn")==0;
    if(strcmp(username, "viewer")==0) {
        allow = strcmp(authz, "read-scenarios")==0 && on_scn;
    } else if(strcmp(username, "writer")==0) {
        const char *id = kw_get_str(gobj, kw, "scenario_id", "", 0);
        BOOL on_mine = on_scn || strcmp(id, "mr")==0 || strcmp(id, "wr")==0 ||
            strcmp(id, "mrw")==0 || strcmp(id, "mrc")==0;
        allow = (strcmp(authz, "read-scenarios")==0 || strcmp(authz, "write-scenarios")==0) &&
            on_mine;
    }
    KW_DECREF(kw)
    return allow;
}

/***************************************************************************
 *  Freed memory that stays freed. A pointer read after its owner was
 *  freed usually reads what it read before: the block is handed out again
 *  at once, and often to a copy of the very same string. So every block
 *  freed is filled with POISON_BYTE and held in a quarantine of
 *  QUARANTINE_SIZE blocks before it is given back, and a read after the
 *  free reads the poison. Built over the allocators gbmem had, installed
 *  before anything is allocated (json included: the entry point hands
 *  jansson the allocators it finds).
 ***************************************************************************/
#define POISON_BYTE     0x5A
#define QUARANTINE_SIZE 4096

typedef struct {
    size_t size;
    size_t pad;     // keeps the block 16-aligned
} poison_hdr_t;

PRIVATE sys_malloc_fn_t base_malloc;
PRIVATE sys_realloc_fn_t base_realloc;
PRIVATE sys_calloc_fn_t base_calloc;
PRIVATE sys_free_fn_t base_free;
PRIVATE poison_hdr_t *quarantine[QUARANTINE_SIZE];
PRIVATE size_t quarantine_idx = 0;
PRIVATE BOOL quarantine_closed = FALSE;   // from the end of the run: freed at once

PRIVATE void *poison_malloc(size_t size)
{
    poison_hdr_t *hdr = base_malloc(sizeof(poison_hdr_t) + size);
    if(!hdr) {
        return NULL;    // Error already logged
    }
    memset(hdr + 1, 0, size);
    hdr->size = size;
    return hdr + 1;
}

PRIVATE void poison_free(void *p)
{
    if(!p) {
        return;
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    memset(p, POISON_BYTE, hdr->size);
    if(quarantine_closed) {
        base_free(hdr);
        return;
    }
    poison_hdr_t *old = quarantine[quarantine_idx];
    quarantine[quarantine_idx] = hdr;
    quarantine_idx = (quarantine_idx + 1) % QUARANTINE_SIZE;
    if(old) {
        base_free(old);
    }
}

PRIVATE void *poison_realloc(void *p, size_t size)
{
    if(!p) {
        return poison_malloc(size);
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    void *np = poison_malloc(size);
    if(!np) {
        return NULL;    // Error already logged
    }
    memcpy(np, p, hdr->size < size? hdr->size : size);
    poison_free(p);
    return np;
}

PRIVATE void *poison_calloc(size_t n, size_t size)
{
    return poison_malloc(n * size);
}

PRIVATE void install_poison_allocators(void)
{
    gbmem_get_allocators(&base_malloc, &base_realloc, &base_calloc, &base_free);
    gbmem_set_allocators(poison_malloc, poison_realloc, poison_calloc, poison_free);
}

/*
 *  The blocks held are freed, and every block freed from now on too. It runs
 *  in cleaning(), before the entry point's memory check (print_track_mem()):
 *  the held blocks are freed memory, and counted as not free they printed a
 *  false leak report on every run, which would hide a real one.
 */
PRIVATE void release_quarantine(void)
{
    quarantine_closed = TRUE;
    for(size_t i=0; i<QUARANTINE_SIZE; i++) {
        if(quarantine[i]) {
            base_free(quarantine[i]);
            quarantine[i] = NULL;
        }
    }
}

/***************************************************************************
 *  Strict FIFO: every log of info level and above the run emits, in order.
 *  The control center opens two treedbs: __system__ (seven topics) and its
 *  own treedb_controlcenter (its two, and the treedb's four: __snaps__,
 *  __graphs__, __assets__, __icons__).
 ***************************************************************************/
PRIVATE const char *expected_msgs[] = {
    "Starting yuno",
    "Playing yuno",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating __timeranger2__.json",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",       // nodes (7.30.10)
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 22: uuid-keyed
    "Node key decided",                                                             // 22: accept-node
    "Agent let in with a node key that is not the one accepted (enforce_node_keys is off)", // 22: KEY_B, recording
    "Agent refused: its node key is not the one accepted",                          // 22: KEY_B, enforcing
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-stranger, refused: not written
    "Agent refused: its node key is not the one accepted",                          // 22: uuid-stranger, pending
    "Node key decided",                                                             // 22: refuse-node
    "A session of the node dropped: its key is not accepted now",                   // 22: refuse-node drops it
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-auth, refused: not written
    "Agent refused: its node key is not the one accepted",                          // 22: uuid-auth, pending
    "Node key decided",                                                             // 22: accept uuid-auth
    "Agent not authenticated: its node key is not the one accepted",                // 22: uuid-auth, KEY_B
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-new
    "Node key decided",                                                             // 22: accept uuid-new
    "Node key decided",                                                             // 22: accept uuid-unseen
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-k1
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-k2
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 22: uuid-k3
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 23: A
    "Node key decided",                                                             // 23: accept A
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 23: B
    "Node key decided",                                                             // 23: accept B
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 23: A gives B's
    "write-tty: the agent_id names more than one node, nothing written",           // 23
    "write-tty without agent_id, nothing written",                                 // 23
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 23: no key, B's
    "Node not accepted: its key is accepted for another node||||WARNING",          // 24
    "Node key decided",                                                             // 24: A, its own key
    "Agent not authenticated: its identity card is not valid||||WARNING",          // 25: id not a string
    "Agent not authenticated: its identity card is not valid||||WARNING",          // 25: host not a string
    "Agent not authenticated: its identity card is not valid||||WARNING",          // 25: a backtick in the id
    "Agent not authenticated: its identity card is not valid||||WARNING",          // 25: an id of 300
    "Agent not authenticated: its identity card is not valid||||WARNING",          // 25: a host too long
    "Agent not authenticated: its node key is not the one accepted|||The node is refused|WARNING", // 26
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 28: R
    "Node key decided",                                                             // 28: accept R
    "Node key decided",                                                             // 28: accept R, same key
    "Node key decided",                                                             // 28: accept R, new key
    "A session of the node dropped: its key is not accepted now",                   // 28
    "Node key decided",                                                             // 28: refuse R
    "A session of the node dropped: its key is not accepted now",                   // 28
    "Node key decided",                                                             // 28: accept R, enforcing
    "Node key decided",                                                             // 28: accept R, the old key
    "A session of the node dropped: its key is not accepted now",                   // 28
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 30: C
    "Node key decided",                                                             // 30: accept C
    "Agent refused: its node key is not the one accepted|||The agent of an accepted node presents no key|WARNING", // 30: a token claims C
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 30: a token gives host-c
    "Agent let in with a node key that is not the one accepted (enforce_node_keys is off)",  // 31: A by a token
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 31: B, A's name
    "Agent refused: its node key is not the one accepted|||The node is not accepted (pending)|WARNING", // 32: a token with a key claims P
    "Two agents with the same node uuid on other hosts: a copy of a node, give each its own uuid (yuneta-node-uuid)||||WARNING", // 32: a token with no key claims P
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 32: another uuid gives P's name
    "write-tty: the agent_id names more than one node, nothing written",           // 32
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 33: B13
    "Node key decided",                                                             // 33: accept B13
    "A node knocking: its agent presents a key not accepted (accept-node with the key read on the node)", // 33: a token with a key, refused: not written
    "Agent refused: its node key is not the one accepted|||The node is not accepted (pending)|WARNING", // 33
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 37: B
    "Node key decided",                                                             // 37: accept B
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 37: P
    "Node key decided",                                                             // 37: accept P
    "A node to accept: its agent presents a key never accepted (accept-node)",     // 37: R
    "Node key decided",                                                             // 37: accept R
    "Agent let in with a node key that is not the one accepted (enforce_node_keys is off)",  // 34: a token claims A
    "Agent refused: its identity card is not valid|||its id is not a string|WARNING",       // 34
    "Agent refused: its identity card is not valid|||its host is not a string|WARNING",     // 34
    "Two agents with the same node uuid on other hosts: a copy of a node, give each its own uuid (yuneta-node-uuid)||||WARNING",   // 0: the copy, not the same host
    "Not in session||||WARNING",                            // 47: command-agent, the agent's link closing
    "Not in session||||WARNING",                            // 47: stats-agent
    "Not in session||||WARNING",                            // 47: the start's first step
    "Not in session||||WARNING",                            // 47: the stop's first step
    "Not in session||||WARNING",                            // 47: the stop's second step
    "Not in session||||WARNING",                            // 47: write-tty
    "event of an agent not from the agents' side, dropped",   // once a minute: the other seven counted
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "stream for a web client that is gone, dropped",          // the first of two frames
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "requester gone before its answer, answer dropped",     // 52: an honest agent, its client gone (EV_READ_FILE)
    "requester gone before its answer, answer dropped",     // 52: (EV_MT_COMMAND_ANSWER)
    "requester gone before its answer, answer dropped",     // 53: an EV_TTY_OPEN for a client that left
    "answer of an agent for a web client that did not ask that agent, dropped",  // 49: 39 forged, said once
    "web client that does not take what is relayed to it, dropped",            // 50
    "web client that does not take what is relayed to it, dropped",            // 51: in bytes
    "answer of an agent for no requester of this control center, dropped",
    "PTY output of an agent for no requester of this control center, dropped",  // two frames, said once
    "answer of an agent for no requester of this control center, dropped",
    "answer of an agent for no requester of this control center, dropped",  // a link that did not ask
    "answer of an agent for no requester of this control center, dropped",  // the link, after the agent's close
    "event of the users store not sent by the store, ignored||||WARNING",  // 15: three forged, said once
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 35: host-amb
    "stopping a scenario: its max_run is reached",          // 16
    "a step of a scenario run was not answered",            // 17: one step, said once
    "Two agents of different nodes give one hostname: reach them by their node uuid||||WARNING", // 36: host-mra
    "stopping a scenario: its max_run is reached",          // 36: mra, its name two nodes'
    "the stop of a scenario's max_run did nothing, tried again in a minute",    // 36
    "stopping a scenario: its max_run is reached",          // 19: mrx, its node away
    "the stop of a scenario's max_run did nothing, tried again when a node of it connects",
    "stopping a scenario: its max_run is reached",          // 19: its node connected
    "stopping a scenario: its max_run is reached",          // 20: mrn, its node connected
    "the stop of a scenario's max_run did nothing, tried again in a minute",    // 20: refused there
    "stopping a scenario: its max_run is reached",          // 38: mrp, a node of it away
    "the stop of a scenario's max_run did not do every step, the rest tried again when a node of it connects", // 38
    "stopping a scenario: its max_run is reached",          // 38: the rest, its node back
    "stopping a scenario: its max_run is reached",          // 41: mrw, a node of it away
    "the stop of a scenario's max_run did not do every step, the rest tried again when a node of it connects", // 41
    "stopping a scenario: its max_run is reached",          // 41: its node back
    "the stop of a scenario's max_run cannot run, tried again in a minute",    // 41: none of the rest in it
    "stopping a scenario: its max_run is reached",          // 42: mrg, its yuno not found
    "the stop of a scenario's max_run did nothing, tried again in a minute",    // 42
    "stopping a scenario: its max_run is reached",          // 43: mrc, its last try
    "the stop of a scenario's max_run did nothing in its last tries: not tried again",  // 43
    "stopping a scenario: its max_run is reached",          // 46: mrb, its stop gone
    "the stop of a scenario's max_run cannot run: not tried again",    // 46
    /*  11: a 300 ms window (C_TIMER ticks every 100 ms here, the
     *  timeout_periodic above). "msg|when|dropped": fields checked too  */
    "stream for a web client that is gone, dropped|its window ended|1",
    "PTY output of an agent for no requester of this control center, dropped|its window ended|1",
    "event of an agent not from the agents' side, dropped|its window ended|12",
    "answer of an agent for a web client that did not ask that agent, dropped|its window ended|43",  // 49: 38, 54: 5
    "yuno stats for a web client that is gone, dropped (the agent's watch expires)||1",
    "yuno stats for a web client that is gone, dropped (the agent's watch expires)|its window ended|3",
    "All controlcenter scenarios tests PASSED",
    "Exit to die",
    "Exit to die",
    "Pausing yuno",
    "Yuno stopped, gobj end",
    0
};

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------------------------*
     *  The store of the control center,
     *  inside the directory the test wipes
     *  when it starts (see C_TEST_CC)
     *--------------------------------------*/
    char root_dir[PATH_MAX];
    build_path(root_dir, sizeof(root_dir), getenv("HOME"), "tests_yuneta", NULL);
    register_yuneta_environment(root_dir, "c_controlcenter_scenarios", 02770, 0660);

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_controlcenter();
    result += register_c_test_cc_peer();
    result += register_c_test_cc();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *jn_expected = json_array();
    for(int i=0; expected_msgs[i]; i++) {
        /*
         *  "msg" alone, or "msg|when|dropped|refusal|priority": an empty
         *  field is not checked
         */
        int n = 0;
        const char **parts = split3(expected_msgs[i], "|", &n);
        json_t *jn_msg = json_pack("{s:s}", "msg", n > 0? parts[0] : "");
        if(n > 1 && !empty_string(parts[1])) {
            json_object_set_new(jn_msg, "when", json_string(parts[1]));
        }
        if(n > 2 && !empty_string(parts[2])) {
            json_object_set_new(jn_msg, "dropped", json_integer(atoll(parts[2])));
        }
        if(n > 3 && !empty_string(parts[3])) {
            json_object_set_new(jn_msg, "refusal", json_string(parts[3]));
        }
        if(n > 4 && !empty_string(parts[4])) {
            json_object_set_new(jn_msg, "__priority__", json_string(parts[4]));
        }
        split_free3(parts);
        json_array_append_new(jn_expected, jn_msg);
    }
    set_expected_results(
        APP_NAME,
        jn_expected,
        NULL,   // expected
        NULL,   // ignore_keys
        1       // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);
    release_quarantine();
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Freed memory is poisoned
     *------------------------------*/
    install_poison_allocators();

    if(write_assertion_key() < 0) {
        return -1;
    }

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        test_authz_checker,
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );
    unlink(ASSERTION_KEY_FILE);
    release_quarantine();   // an entry point that ended before cleaning()
    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result<0?-1:0;
}
