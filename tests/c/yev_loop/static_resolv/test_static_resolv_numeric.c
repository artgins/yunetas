/****************************************************************************
 *  test_static_resolv_numeric.c
 *
 *  A numeric host is never sent to DNS by the NSS-free static resolver,
 *  whatever the family asked for -- as glibc's getaddrinfo() does:
 *
 *    A. A numeric address of the family asked for is answered directly.
 *    B. A numeric address of the OTHER family ("::1" in AF_INET,
 *       "127.0.0.1" in AF_INET6) answers EAI_ADDRFAMILY at once. Before,
 *       it fell through to a DNS query: bind_src_url() of "[::1]" for the
 *       IPv4 address of "localhost" stalled the event loop 3 s on a node
 *       with a slow nameserver (test_yevent_connect_src_url, case E).
 *    C. AI_NUMERICHOST with a name answers EAI_NONAME, no lookup.
 *
 *  Queries are routed to an unprivileged port (-DYUNETA_DNS_PORT) where
 *  nothing answers, so a query that escapes costs a timeout: every case
 *  must answer the right code AND fast.
 *
 *  Self-contained: libc only, #includes static_resolv.c.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

/* Pull in the unit under test, with the static helpers visible. */
#include "static_resolv.c"

#ifndef YUNETA_DNS_PORT
#error "build this test with -DYUNETA_DNS_PORT=<unprivileged port>"
#endif

#define FAST_MSEC   200     // a query that escapes waits a timeout, far above this

static int check(const char *what, const char *node, int family, int flags, int expected)
{
    struct addrinfo hints = {
        .ai_family = family,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = flags,
    };
    struct addrinfo *res = NULL;

    uint64_t t0 = monotonic_msec();
    int ret = yuneta_getaddrinfo(node, "0", &hints, &res);
    uint64_t msec = monotonic_msec() - t0;
    if(res) {
        yuneta_freeaddrinfo(res);
    }

    int ok = (ret == expected && msec < FAST_MSEC);
    printf("%s: %s family %d -> %d (%s) in %llu ms, expected %d ... %s\n",
        what, node, family, ret, ret? gai_strerror(ret):"ok",
        (unsigned long long)msec, expected, ok? "PASS":"FAIL"
    );
    return ok? 0:1;
}

int main(void)
{
    alarm(30);  /* watchdog: never hang the suite */

    int fail = 0;
    fail += check("A", "127.0.0.1", AF_INET, 0, 0);
    fail += check("A", "::1", AF_INET6, 0, 0);
    fail += check("A", "::1", AF_UNSPEC, 0, 0);
    fail += check("B", "::1", AF_INET, AI_PASSIVE, EAI_ADDRFAMILY);
    fail += check("B", "127.0.0.1", AF_INET6, AI_PASSIVE, EAI_ADDRFAMILY);
    fail += check("C", "test.example", AF_UNSPEC, AI_NUMERICHOST, EAI_NONAME);

    if(fail == 0) {
        printf("test_static_resolv_numeric: PASS\n");
        return 0;
    }
    printf("test_static_resolv_numeric: FAIL\n");
    return 1;
}
