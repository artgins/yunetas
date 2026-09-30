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

#include <yunetas.h>
#include "c_controlcenter.h"
#include "c_test_cc_peer.h"
#include "c_test_cc.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_controlcenter_scenarios"
#define APP_DOC         "Test the scenarios of the control center"

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
            'name': 'controlcenter',                                \n\
            'gclass': 'C_CONTROLCENTER',                            \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
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
 *  Every command of the control center asks a permission. With no checker
 *  installed the default one is C_AUTHZ's, which this test yuno does not
 *  run. This test is about what the commands do, not about who may ask.
 ***************************************************************************/
static BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
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

PRIVATE void release_quarantine(void)
{
    for(size_t i=0; i<QUARANTINE_SIZE; i++) {
        if(quarantine[i]) {
            base_free(quarantine[i]);
            quarantine[i] = NULL;
        }
    }
}

/***************************************************************************
 *  Strict FIFO: every log of info level and above the run emits, in order.
 *  The control center opens two treedbs: __system__ (six topics) and its
 *  own treedb_controlcenter (its two, and the treedb's three: __snaps__,
 *  __graphs__, __assets__).
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
    "Creating __timeranger2__.json",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "event of an agent not from the agents' side, dropped",   // once a minute: the other six counted
    "event of an agent not from the agents' side, dropped",   // the six, said when the agent's connection closes
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "stream for a web client that is gone, dropped",          // the first of two frames
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "stream for a web client that is gone, dropped",          // the second, said when the agent's connection closes
    "answer for a web client that is gone, dropped: its channel holds another connection now",
    "answer of an agent for no requester of this control center, dropped",
    "PTY output of an agent for no requester of this control center, dropped",  // two frames, said once
    "answer of an agent for no requester of this control center, dropped",
    "All controlcenter scenarios tests PASSED",
    "Exit to die",
    "Exit to die",
    "Pausing yuno",
    "PTY output of an agent for no requester of this control center, dropped",  // the second frame, said when the control center stops
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
        json_array_append_new(jn_expected, json_pack("{s:s}", "msg", expected_msgs[i]));
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
    release_quarantine();

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
