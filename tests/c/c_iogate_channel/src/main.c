/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_iogate_channel
 *          What C_IOGATE writes in a message (the channel it came in by),
 *          and the brake of "No channel FOUND to send"
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_iogate.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_iogate_channel"
#define APP_DOC         "Test the channel C_IOGATE writes in a message"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0       // use default
#define MEM_MAX_BLOCK           0       // use default
#define MEM_SUPERBLOCK          0       // use default
#define MEM_MAX_SYSTEM_MEMORY   0       // use default

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
            'name': 'test_iogate',                                  \n\
            'gclass': 'C_TEST_IOGATE',                              \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_iogate();

    /*--------------------------*
     *  Check all gclass' FSM
     *  (the core ones are registered by yuneta_entry_point())
     *--------------------------*/
    json_t *jn_gclasses = gclass_gclass_register();
    int idx; json_t *jn_gclass;
    json_array_foreach(jn_gclasses, idx, jn_gclass) {
        const char *gclass_name = kw_get_str(0, jn_gclass, "gclass", "", KW_REQUIRED);
        hgclass gclass = gclass_find_by_name(gclass_name);
        result += gclass_check_fsm(gclass);
    }
    json_decref(jn_gclasses);

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);
    gobj_set_global_no_trace("timer", TRUE);

    // Samples of traces
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    // gobj_set_gobj_trace(0, "create_delete", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    /*
     *  The errors, in order (see c_test_iogate.c): one per message whose
     *  __temp__ is not a dict, and one per 1000 sends with no channel
     */
    set_expected_results(
        APP_NAME, // test name
        json_pack("[{s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s},"
                  " {s:s, s:s}, {s:s, s:s},"
                  " {s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s},"
                  " {s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]", // errors_list
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_MESSAGE",
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_OPEN",
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_CLOSE",
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_MESSAGE",
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_MESSAGE",  // a string
            "msg", "__temp__ of a message is not a dict, replaced", "event", "EV_ON_MESSAGE",  // an integer
            "msg", "channel NOT FOUND", "repeated", "0",            // a drop before the outage
            "msg", "No channel FOUND to send", "repeated", "0",     // the outage, said at once
            "msg", "No channel FOUND to send", "repeated", "999",
            "msg", "No channel FOUND to send", "repeated", "999",
            "msg", "No channel FOUND to send", "repeated", "0",     // a new run: said at once
            "msg", "No channel FOUND to send", "repeated", "0",     // to all
            "msg", "No channel FOUND to send", "repeated", "999",
            "msg", "channel NOT FOUND", "repeated", "999"           // drops, in the run of the first
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    return result;
}

/***************************************************************************
 *  The end of each run of sends with no channel: an INFO, in order, with
 *  what the brake did not say
 ***************************************************************************/
typedef struct {
    const char *msg;
    const char *why;
    const char *without_channel;
    const char *not_said;
} run_end_t;
#define SENDS_ENDED "Sends with no channel ended"
#define DROPS_ENDED "Drops of a channel not found ended"
PRIVATE const run_end_t run_ends[] = {
    {SENDS_ENDED, "a channel found to send", "2500", "499"},    // not 2501: the drop is not a send
    {SENDS_ENDED, "a channel found to send", "1",    "0"},
    {SENDS_ENDED, "a channel found to send", "1500", "499"},    // a send to all
    {DROPS_ENDED, "the gate stops",          "1201", "200"},
    {0, 0, 0, 0}
};
int run_ends_seen = 0;
int run_ends_wrong = 0;

static BOOL line_has(const char *bf, size_t len, const char *key, const char *value, BOOL quoted)
{
    char pair[128];
    if(quoted) {
        snprintf(pair, sizeof(pair), "\"%s\": \"%s\"", key, value);
    } else {
        snprintf(pair, sizeof(pair), "\"%s\": %s", key, value);   // the log writes a number as one
    }
    size_t plen = strlen(pair);
    const char *p = memmem(bf, len, pair, plen);
    if(!p) {
        return FALSE;
    }
    size_t after = (size_t)(p - bf) + plen;
    return (after < len && bf[after] >= '0' && bf[after] <= '9')? FALSE:TRUE;  // 499, not 4990
}

static int run_end_log_write(void *v, int priority, const char *bf, size_t len)
{
    if(!memmem(bf, len, SENDS_ENDED, strlen(SENDS_ENDED)) &&
            !memmem(bf, len, DROPS_ENDED, strlen(DROPS_ENDED))) {
        return 0;
    }
    int i = run_ends_seen++;
    if(i >= (int)(sizeof(run_ends)/sizeof(run_ends[0])) - 1 ||
            priority != LOG_INFO ||
            !memmem(bf, len, run_ends[i].msg, strlen(run_ends[i].msg)) ||
            !line_has(bf, len, "why", run_ends[i].why, TRUE) ||
            !line_has(bf, len, "without_channel", run_ends[i].without_channel, FALSE) ||
            !line_has(bf, len, "not_said", run_ends[i].not_said, FALSE)) {
        printf("%sERROR --> %s: %.*s%s\n", On_Red BWhite,
            "the end of a run with no channel is not the expected", (int)len, bf, Color_Off);
        run_ends_wrong++;
    }
    return 0;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
static void cleaning(void)
{
    result += test_json(NULL);  // NULL: we want to check only the logs

    int expected = (int)(sizeof(run_ends)/sizeof(run_ends[0])) - 1;
    if(run_ends_wrong || run_ends_seen != expected) {
        printf("%sERROR --> %s: %d seen (expected %d), %d wrong%s\n", On_Red BWhite,
            "the ends of the runs with no channel", run_ends_seen, expected, run_ends_wrong,
            Color_Off);
        result += -1;
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Capture the logger output
     *------------------------------*/
    glog_init();

    /*
     *  Add all handlers very early
     */
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    gobj_log_register_handler(
        "run_end",          // handler_name
        0,                  // close_fn
        run_end_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("run_end", "run_end", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs, default internal dbsimple
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authentication_parser, default Monoclass C_AUTHZ
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
