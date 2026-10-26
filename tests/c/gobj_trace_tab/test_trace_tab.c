/****************************************************************************
 *          test_trace_tab.c
 *
 *          Unit test: tab(), the indentation of a machine trace line.
 *
 *          A trace line is indented by its nesting depth -- the `__inside__`
 *          counter gobj_send_event() bumps on the way in -- so an event sent
 *          from inside another one's action stands under it. The contract:
 *
 *            - four spaces per level, never less than one (depth 0),
 *            - never more than bflen-2,
 *            - a buffer smaller than 2 bytes logs an error and is not
 *              written beyond its size (tab() used to overrun it).
 *
 *          The depth is real, not faked: one action sends the same event to
 *          its own gobj until it reaches the depth asked for, and measures
 *          tab() at every level.
 *
 *          And trace_machine(), which builds the line around tab(): it used
 *          to take the indentation through a 40-byte buffer of its own, which
 *          capped it at depth 9 once a level was four spaces wide. At depth
 *          12 the line must carry the 48 spaces.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_trace_tab"

#define MAX_DEPTH 12

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;
PRIVATE int g_len[MAX_DEPTH + 1];       /* strlen(tab()) seen at each depth */
PRIVATE BOOL g_all_spaces = TRUE;       /* tab() wrote nothing but spaces */
PRIVATE int g_clamp6 = -1;              /* strlen(tab(bf, 6)) at depth 3 */
PRIVATE int g_clamp2 = -1;              /* strlen(tab(bf, 2)) at depth 3 */

PRIVATE char captured[2048];
PRIVATE size_t captured_len = 0;

GOBJ_DEFINE_GCLASS(C_TABTEST);
GOBJ_DEFINE_EVENT(EV_DIVE);

/***************************************************************************
 *  Capture handler: keeps the last record, as the handlers receive it
 ***************************************************************************/
PRIVATE int capture_write(void *h, int priority, const char *bf, size_t len)
{
    if(len >= sizeof(captured)) {
        len = sizeof(captured) - 1;
    }
    memcpy(captured, bf, len);
    captured[len] = 0;
    captured_len = len;
    return 0;
}

/***************************************************************************
 *  Measure tab() at this depth, and go one level deeper
 ***************************************************************************/
PRIVATE int ac_dive(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    int depth = (int)kw_get_int(gobj, kw, "depth", 0, KW_REQUIRED);
    char bf[256];

    tab(bf, sizeof(bf));
    if(depth >= 0 && depth <= MAX_DEPTH) {
        g_len[depth] = (int)strlen(bf);
    }
    if(strspn(bf, " ") != strlen(bf)) {
        g_all_spaces = FALSE;
    }

    if(depth == 3) {
        char small[6];
        tab(small, sizeof(small));
        g_clamp6 = (int)strlen(small);
        char tiny[2];
        tab(tiny, sizeof(tiny));
        g_clamp2 = (int)strlen(tiny);
    }

    if(depth == MAX_DEPTH) {
        captured_len = 0;
        trace_machine("DEEPEST %d", depth);
    }

    if(depth < MAX_DEPTH) {
        gobj_send_event(gobj, EV_DIVE, json_pack("{s:i}", "depth", depth + 1), gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass scaffolding
 ***************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE int register_tabtest(void)
{
    ev_action_t st_idle[] = {
        {EV_DIVE,   ac_dive,    0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_DIVE,   0},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_TABTEST,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        0,              // priv_size
        0,              // authz_table
        0,              // command_table
        0,              // trace_level
        0               // gclass_flag
    );
    return gc ? 0 : -1;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-52s got %d expected %d\n", name, got, expected);
        global_result += -1;
    } else {
        printf("ok   %-52s (%d)\n", name, got);
    }
}

PRIVATE void check_true(const char *name, BOOL ok)
{
    if(!ok) {
        printf("FAIL %s\n", name);
        global_result += -1;
    } else {
        printf("ok   %s\n", name);
    }
}

/***************************************************************************
 *  Depth 0, then 1..MAX_DEPTH through real nested events
 ***************************************************************************/
PRIVATE void test_depths(hgobj yuno)
{
    char bf[256];
    char label[80];

    tab(bf, sizeof(bf));
    check_int("depth 0: one space, never zero", (int)strlen(bf), 1);

    for(int d = 0; d <= MAX_DEPTH; d++) {
        g_len[d] = -1;
    }
    gobj_send_event(yuno, EV_DIVE, json_pack("{s:i}", "depth", 1), yuno);

    for(int d = 1; d <= MAX_DEPTH; d++) {
        snprintf(label, sizeof(label), "depth %d: four spaces per level", d);
        check_int(label, g_len[d], 4 * d);
    }
    check_true("tab() writes only spaces", g_all_spaces);
    check_int("depth 3, bflen 6: never more than bflen-2", g_clamp6, 4);
    check_int("depth 3, bflen 2: still one space", g_clamp2, 1);

    tab(bf, sizeof(bf));
    check_int("back at depth 0 after the dive", (int)strlen(bf), 1);
}

/***************************************************************************
 *  trace_machine() at depth MAX_DEPTH: "<timestamp> -" + 4*depth spaces
 ***************************************************************************/
PRIVATE void test_trace_machine_line(void)
{
    check_true("trace_machine() emitted the line", captured_len > 0);

    const char *mark = strstr(captured, " -");
    const char *text = strstr(captured, "DEEPEST");
    if(!mark || !text || text < mark) {
        printf("FAIL %-52s unexpected record '%s'\n", "trace_machine() line shape", captured);
        global_result += -1;
        return;
    }
    const char *indent = mark + 2;
    int spaces = (int)strspn(indent, " ");
    check_int("trace_machine() at depth 12: 48 spaces, no cap", spaces, 4 * MAX_DEPTH);
    check_true("trace_machine() text follows the indentation", indent + spaces == text);
}

/***************************************************************************
 *  A buffer too small is refused with an error, never overrun
 ***************************************************************************/
PRIVATE void test_small_buffers(void)
{
    char bf[4];

    memset(bf, 'x', sizeof(bf));
    captured_len = 0;
    tab(bf, 1);
    check_true("bflen 1: empty string", bf[0] == '\0');
    check_true("bflen 1: nothing written beyond it", bf[1] == 'x');
    check_true("bflen 1: error logged", strstr(captured, "at least 2 bytes") != NULL);

    memset(bf, 'x', sizeof(bf));
    captured_len = 0;
    tab(bf, 0);
    check_true("bflen 0: nothing written", bf[0] == 'x');
    check_true("bflen 0: error logged", strstr(captured, "at least 2 bytes") != NULL);

    captured_len = 0;
    check_true("NULL buffer: returns NULL", tab(NULL, 10) == NULL);
    check_true("NULL buffer: error logged", strstr(captured, "at least 2 bytes") != NULL);
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0};
    set_memory_check_list(memory_check_list);

    gobj_start_up(
        argc, argv,
        NULL,                   // jn_global_settings
        NULL,                   // persistent_attrs
        NULL,                   // global_command_parser
        NULL,                   // global_stats_parser
        NULL,                   // global_authz_checker
        NULL                    // global_authentication_parser
    );

    gobj_log_register_handler(
        "capture",
        NULL,           // close_fn
        capture_write,  // write_fn
        NULL            // fwrite_fn
    );
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    if(register_tabtest() != 0) {
        printf("%s: FAIL (gclass_create)\n", APP);
        gobj_end();
        return -1;
    }

    hgobj yuno = gobj_create_yuno("tabtest_yuno", C_TABTEST, 0);
    if(!yuno) {
        printf("%s: FAIL (gobj_create_yuno)\n", APP);
        gobj_end();
        return -1;
    }

    test_depths(yuno);
    test_trace_machine_line();
    test_small_buffers();

    gobj_end();

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
