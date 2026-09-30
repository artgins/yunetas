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
 *    D. As glibc: an IPv4 address in AF_INET6 with AI_V4MAPPED answers its
 *       v4-mapped IPv6 address ("::ffff:127.0.0.1"), and a v4-mapped IPv6
 *       address in AF_INET answers its IPv4 address, flags or not. Up to
 *       7.25.20 both answered EAI_ADDRFAMILY.
 *    E. The IPv4 forms glibc takes as numeric (inet_aton, exact: no blank
 *       before or after): shorthand ("127.1", "10.1.2"), a single number
 *       ("2130706433"), hex ("0x7f.1") and octal ("0177.0.0.1") parts --
 *       answered as glibc answers them, in every family, with and without
 *       AI_NUMERICHOST. Up to 7.25.20 only the dotted quad was numeric:
 *       the others went to DNS (and waited its timeout here), and
 *       AI_NUMERICHOST refused them (EAI_NONAME). What glibc does not
 *       take ("1.2.3.4x", "256.1", "08.1", "127.0.0.1 ") stays a name.
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
#include <arpa/inet.h>

/* Pull in the unit under test, with the static helpers visible. */
#include "static_resolv.c"

#ifndef YUNETA_DNS_PORT
#error "build this test with -DYUNETA_DNS_PORT=<unprivileged port>"
#endif

#define FAST_MSEC   200     // a query that escapes waits a timeout, far above this

static int check_addr(const char *what, const char *node, int family, int flags, const char *expected)
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

    char got[INET6_ADDRSTRLEN] = "";
    int n = 0;
    for(struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        n++;
        if(ai->ai_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr, got, sizeof(got));
        } else if(ai->ai_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, got, sizeof(got));
        }
    }
    int family_ok = res && (family == AF_UNSPEC || res->ai_family == family);
    if(res) {
        yuneta_freeaddrinfo(res);
    }

    int ok = (ret == 0 && n == 1 && family_ok && strcmp(got, expected)==0 && msec < FAST_MSEC);
    printf("%s: %s family %d flags 0x%x -> %d (%s) %s in %llu ms, expected %s ... %s\n",
        what, node, family, flags, ret, ret? gai_strerror(ret):"ok", got,
        (unsigned long long)msec, expected, ok? "PASS":"FAIL"
    );
    return ok? 0:1;
}

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
    fail += check_addr("D", "127.0.0.1", AF_INET6, AI_V4MAPPED, "::ffff:127.0.0.1");
    fail += check_addr("D", "127.0.0.1", AF_INET6, AI_V4MAPPED|AI_PASSIVE, "::ffff:127.0.0.1");
    fail += check_addr("D", "::ffff:1.2.3.4", AF_INET, 0, "1.2.3.4");
    fail += check_addr("D", "::ffff:1.2.3.4", AF_INET, AI_V4MAPPED, "1.2.3.4");
    fail += check_addr("D", "127.0.0.1", AF_INET, AI_V4MAPPED, "127.0.0.1");
    fail += check("D", "::1", AF_INET, AI_V4MAPPED, EAI_ADDRFAMILY);
    fail += check("D", "::1", AF_INET, AI_NUMERICHOST, EAI_ADDRFAMILY);

    /*
     *  E. What glibc's getaddrinfo() answers for each form (checked on
     *  glibc 2.43, probe of 2026-10-01)
     */
    const struct {
        const char *node;
        const char *v4;
    } shorthand[] = {
        {"127.1",       "127.0.0.1"},
        {"10.1.2",      "10.1.0.2"},
        {"0x7f.1",      "127.0.0.1"},
        {"0177.0.0.1",  "127.0.0.1"},
        {"2130706433",  "127.0.0.1"},
        {"0x7f000001",  "127.0.0.1"},
        {0}
    };
    for(int i=0; shorthand[i].node; i++) {
        const char *node = shorthand[i].node;
        char mapped[INET6_ADDRSTRLEN];
        snprintf(mapped, sizeof(mapped), "::ffff:%s", shorthand[i].v4);
        for(int numeric=0; numeric<2; numeric++) {
            int nf = numeric? AI_NUMERICHOST : 0;
            fail += check_addr("E", node, AF_INET, nf, shorthand[i].v4);
            fail += check_addr("E", node, AF_UNSPEC, nf, shorthand[i].v4);
            fail += check_addr("E", node, AF_INET6, nf|AI_V4MAPPED, mapped);
            fail += check("E", node, AF_INET6, nf, EAI_ADDRFAMILY);
        }
    }
    const char *not_numeric[] = {"1.2.3.4x", "256.1", "08.1", "127.0.0.1 ", " 127.0.0.1", "1 2", 0};
    for(int i=0; not_numeric[i]; i++) {
        fail += check("E", not_numeric[i], AF_INET, AI_NUMERICHOST, EAI_NONAME);
        fail += check("E", not_numeric[i], AF_UNSPEC, AI_NUMERICHOST, EAI_NONAME);
    }

    if(fail == 0) {
        printf("test_static_resolv_numeric: PASS\n");
        return 0;
    }
    printf("test_static_resolv_numeric: FAIL\n");
    return 1;
}
