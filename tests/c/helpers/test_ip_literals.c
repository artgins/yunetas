/****************************************************************************
 *          test_ip_literals.c
 *
 *          The IPv4 addresses of webstats' mail body, written as [a.b.c.d]:
 *          yunos/c/webstats/src/ip_literals.c, compiled into this test.
 *
 *          OVH's outbound relay reads a bare "34.140.132.132" as a phone
 *          number and drops the mail (7.25.19). Up to 7.25.20 an address
 *          followed by a sentence dot, by a port, or written in its
 *          IPv4-mapped IPv6 form stayed bare, because '.' and ':' counted
 *          as a neighbour that glues the address to a word.
 *
 *          Cases:
 *          1. what 7.25.19 already bracketed, and what it rightly leaves
 *             alone (versions, tags, five numbers, a glued word);
 *          2. an address at the end of a sentence: [a.b.c.d].
 *          3. an address with its port: [a.b.c.d]:443
 *          4. an IPv6 address that ends in an IPv4, bracketed whole:
 *             [::ffff:a.b.c.d], [64:ff9b::a.b.c.d]
 *          5. a colon or a dash that separates (client:[a.b.c.d],
 *             [a.b.c.d]:x, a range [a.b.c.d]-[e.f.g.h]), and the versions
 *             and times that must stay as they are (nginx-1.25.3.1,
 *             1.2.3.4-beta, 12:30:45.123)
 *          6. a NULL source: NULL, logged
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>
#include "ip_literals.h"

#define APP     "test_ip_literals"

PRIVATE int global_result = 0;

/***************************************************************************
 *  Bracket `text`, compare with `expected`
 ***************************************************************************/
PRIVATE void check_bracket(const char *text, const char *expected)
{
    gbuffer_t *src = gbuffer_create(strlen(text) + 1, strlen(text) + 1);
    gbuffer_append_string(src, text);

    gbuffer_t *dst = bracket_ip_literals(src);
    GBUFFER_DECREF(src)
    if(!dst) {
        printf("FAIL %-40s -> NULL\n", text);
        global_result += -1;
        return;
    }

    size_t len = gbuffer_leftbytes(dst);
    const char *p = gbuffer_cur_rd_pointer(dst);
    if(len == strlen(expected) && memcmp(p, expected, len) == 0) {
        printf("ok   %-40s -> %.*s\n", text, (int)len, p);
    } else {
        printf("FAIL %-40s -> %.*s (expected %s)\n", text, (int)len, p, expected);
        global_result += -1;
    }
    GBUFFER_DECREF(dst)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_brackets(void)
{
    /*
     *  1. What was already right
     */
    check_bracket("34.140.132.132", "[34.140.132.132]");
    check_bracket("<td>34.140.132.132</td>", "<td>[34.140.132.132]</td>");
    check_bracket("from 10.0.0.1 and 10.0.0.2, banned", "from [10.0.0.1] and [10.0.0.2], banned");
    check_bracket("Chrome/142.0.0.0 Safari", "Chrome/142.0.0.0 Safari");
    check_bracket("<a href=\"http://1.2.3.4/\">x</a>", "<a href=\"http://1.2.3.4/\">x</a>");
    check_bracket("1.2.3.4.5", "1.2.3.4.5");
    check_bracket("v1.2.3.4", "v1.2.3.4");
    check_bracket("1.2.3.4a", "1.2.3.4a");
    check_bracket("256.1.1.1", "256.1.1.1");
    check_bracket("[1.2.3.4]", "[1.2.3.4]");
    check_bracket("1.2.3", "1.2.3");

    /*
     *  2. End of a sentence: the dot stays outside
     */
    check_bracket("client 34.140.132.132.", "client [34.140.132.132].");
    check_bracket("client 34.140.132.132. Next", "client [34.140.132.132]. Next");
    check_bracket("<td>34.140.132.132.</td>", "<td>[34.140.132.132].</td>");

    /*
     *  3. With a port: the port stays outside
     */
    check_bracket("upstream 34.140.132.132:443 failed", "upstream [34.140.132.132]:443 failed");
    check_bracket("34.140.132.132:8080.", "[34.140.132.132]:8080.");
    check_bracket("34.140.132.132:x", "[34.140.132.132]:x");
    check_bracket("34.140.132.132:443x", "[34.140.132.132]:443x");

    /*
     *  4. IPv4-mapped IPv6
     */
    check_bracket("client ::ffff:34.140.132.132 banned", "client [::ffff:34.140.132.132] banned");
    check_bracket("<td>::FFFF:34.140.132.132</td>", "<td>[::FFFF:34.140.132.132]</td>");
    check_bracket("::ffff:34.140.132.132.", "[::ffff:34.140.132.132].");
    check_bracket("a::ffff:34.140.132.132", "[a::ffff:34.140.132.132]");
    check_bracket("nat64 64:ff9b::34.1.2.3 x", "nat64 [64:ff9b::34.1.2.3] x");
    check_bracket("0:0:0:0:0:ffff:34.1.2.3", "[0:0:0:0:0:ffff:34.1.2.3]");
    check_bracket("::34.1.2.3", "[::34.1.2.3]");
    check_bracket("x64:ff9b::34.1.2.3", "x64:[ff9b::34.1.2.3]");   // a colon separates, as in client:[a.b.c.d]

    /*
     *  5. A colon or a dash that separates; versions and times stay
     */
    check_bracket("client:34.1.2.3", "client:[34.1.2.3]");
    check_bracket("34.1.2.3: connection refused", "[34.1.2.3]: connection refused");
    check_bracket("range 10.0.0.1-10.0.0.9 banned", "range [10.0.0.1]-[10.0.0.9] banned");
    check_bracket(" -34.1.2.3", " -[34.1.2.3]");
    check_bracket("-34.1.2.3", "-[34.1.2.3]");
    check_bracket("34.1.2.3- x", "[34.1.2.3]- x");
    check_bracket("nginx-1.25.3.1", "nginx-1.25.3.1");
    check_bracket("1.2.3.4-beta", "1.2.3.4-beta");
    check_bracket("7.25.20.1-1", "7.25.20.1-1");
    check_bracket("at 12:30:45.123", "at 12:30:45.123");
    check_bracket("10:20:30", "10:20:30");
    check_bracket("2026-09-30 10:20", "2026-09-30 10:20");
}

/***************************************************************************
 *  6. A NULL source
 ***************************************************************************/
PRIVATE void test_null(void)
{
    gbuffer_t *dst = bracket_ip_literals(NULL);
    if(dst) {
        printf("FAIL bracket_ip_literals(NULL) -> not NULL\n");
        GBUFFER_DECREF(dst)
        global_result += -1;
    } else {
        printf("ok   bracket_ip_literals(NULL) -> NULL\n");
    }
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_brackets();
    test_null();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
