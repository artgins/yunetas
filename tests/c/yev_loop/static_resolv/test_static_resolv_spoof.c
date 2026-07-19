/****************************************************************************
 *  test_static_resolv_spoof.c
 *
 *  Regression test for F-001 (DNS response spoofing in the NSS-free static
 *  resolver). dns_query() must only accept a UDP response from the exact
 *  nameserver it queried. The fix connect()s the UDP socket, so the kernel
 *  drops datagrams whose source address/port is not the nameserver — closing
 *  the "any source delivers" property an on/off-path attacker used to forge
 *  answers and redirect a yuno's outbound connection.
 *
 *  This test drives the REAL dns_query() (reached by #including the .c so the
 *  static helpers are visible) against a localhost fake nameserver, and asserts:
 *
 *    A. A fully-valid response delivered from the WRONG source port is dropped
 *       by the kernel  ->  dns_query() receives nothing and times out (n <= 0).
 *       Before the fix (unconnected socket) this datagram was delivered and
 *       parsed, so a revert makes this subtest fail.
 *
 *    B. The same response delivered from the RIGHT source (the queried
 *       nameserver) is accepted  ->  dns_query() returns n > 0 and
 *       dns_parse_response() extracts the A record. Confirms the fix did not
 *       break normal resolution.
 *
 *  Self-contained: libc only, no io_uring, no kernel libs, no root. The fake
 *  nameserver binds an unprivileged port supplied via -DYUNETA_DNS_PORT, which
 *  the same macro routes dns_query() to.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Pull in the unit under test, with the static helpers visible. */
#include "static_resolv.c"

#ifndef YUNETA_DNS_PORT
#error "build this test with -DYUNETA_DNS_PORT=<unprivileged port>"
#endif

#define TEST_ID        0xABCD
#define TEST_HOST      "test.example"
#define EXPECTED_A     "1.2.3.4"

/* Build a minimal valid DNS A-record response that echoes the question from
 * `query` and answers TEST_HOST -> EXPECTED_A. Returns response length. */
static int build_response(const uint8_t *query, size_t qlen, uint8_t *out, size_t outsz)
{
    /* Locate end of the question section in the query (after qname + qtype + qclass). */
    size_t i = 12;
    while(i < qlen && query[i] != 0) {
        i += (size_t)query[i] + 1;
    }
    i += 1;     /* skip the root label */
    i += 4;     /* skip QTYPE + QCLASS */
    if(i > qlen) return -1;
    size_t qsection = i - 12;

    size_t need = 12 + qsection + 16;
    if(need > outsz) return -1;

    memset(out, 0, 12);
    out[0] = query[0]; out[1] = query[1];   /* echo transaction id */
    out[2] = 0x81;                          /* QR=1, RD=1 */
    out[3] = 0x80;                          /* RA=1, RCODE=0 */
    out[4] = 0x00; out[5] = 0x01;           /* QDCOUNT=1 */
    out[6] = 0x00; out[7] = 0x01;           /* ANCOUNT=1 */

    memcpy(out + 12, query + 12, qsection); /* echo the question verbatim */

    uint8_t *a = out + 12 + qsection;
    a[0] = 0xC0; a[1] = 0x0C;               /* name: pointer to qname at offset 12 */
    a[2] = 0x00; a[3] = 0x01;               /* TYPE = A */
    a[4] = 0x00; a[5] = 0x01;               /* CLASS = IN */
    a[6] = 0x00; a[7] = 0x00; a[8] = 0x00; a[9] = 0x3C;  /* TTL = 60 */
    a[10] = 0x00; a[11] = 0x04;             /* RDLENGTH = 4 */
    a[12] = 1; a[13] = 2; a[14] = 3; a[15] = 4;          /* RDATA = 1.2.3.4 */

    return (int)need;
}

static int udp_bind_local(uint16_t port)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s < 0) return -1;
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    if(bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

/*
 *  Run one subtest in a forked child so the blocking dns_query() is isolated.
 *  `reply_from_wrong_source`: TRUE  -> server answers from YUNETA_DNS_PORT+1
 *                             FALSE -> server answers from the queried port.
 *  Returns 0 if dns_query() behaved as the fix requires.
 */
static int run_subtest(const uint8_t *query, int qlen, int reply_from_wrong_source)
{
    int srv = udp_bind_local(YUNETA_DNS_PORT);
    if(srv < 0) { fprintf(stderr, "bind nameserver port failed: %s\n", strerror(errno)); return 1; }
    int bad = -1;
    if(reply_from_wrong_source) {
        bad = udp_bind_local(YUNETA_DNS_PORT + 1);
        if(bad < 0) { fprintf(stderr, "bind spoof port failed: %s\n", strerror(errno)); close(srv); return 1; }
    }

    pid_t pid = fork();
    if(pid < 0) { perror("fork"); close(srv); if(bad>=0) close(bad); return 1; }

    if(pid == 0) {
        /* Child = the resolver client. Only dns_query()'s own socket should exist here. */
        close(srv);
        if(bad >= 0) close(bad);
        uint8_t resp[4096];
        int n = dns_query("127.0.0.1", query, (size_t)qlen, resp, sizeof(resp),
                          reply_from_wrong_source ? 700 : 2000);
        if(reply_from_wrong_source) {
            /* Spoof from wrong source must be dropped by the kernel -> timeout. */
            _exit(n <= 0 ? 0 : 10);
        }
        /* Legit reply from the queried nameserver must be accepted and parsed. */
        if(n <= 0) _exit(11);
        struct in_addr a4[16]; int n4 = 0, d6 = 0;
        uint32_t ttl = 0;
        dns_parse_response(resp, (size_t)n, TEST_ID, a4, &n4, NULL, &d6, 16, &ttl);
        if(n4 != 1) _exit(12);
        struct in_addr want; inet_pton(AF_INET, EXPECTED_A, &want);
        _exit(a4[0].s_addr == want.s_addr ? 0 : 13);
    }

    /* Parent = fake nameserver. Receive the query, learn the client's source,
     * then answer from either the right or the wrong source socket. */
    uint8_t qbuf[1500];
    struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
    ssize_t qn = recvfrom(srv, qbuf, sizeof(qbuf), 0, (struct sockaddr *)&cli, &clilen);
    if(qn > 0) {
        uint8_t rbuf[1500];
        int rlen = build_response(qbuf, (size_t)qn, rbuf, sizeof(rbuf));
        if(rlen > 0) {
            int send_fd = reply_from_wrong_source ? bad : srv;
            sendto(send_fd, rbuf, (size_t)rlen, 0, (struct sockaddr *)&cli, clilen);
        }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    close(srv);
    if(bad >= 0) close(bad);

    if(!WIFEXITED(status)) { fprintf(stderr, "child did not exit cleanly\n"); return 1; }
    return WEXITSTATUS(status);
}

int main(void)
{
    alarm(15);  /* watchdog: never hang the suite */

    uint8_t query[512];
    int qlen = dns_build_query(query, sizeof(query), TEST_ID, TEST_HOST, 1 /* A */);
    if(qlen <= 0) { fprintf(stderr, "FAIL: dns_build_query\n"); return 1; }

    int a = run_subtest(query, qlen, 1 /* reply from WRONG source */);
    int b = run_subtest(query, qlen, 0 /* reply from RIGHT source */);

    printf("A: spoof-from-wrong-source rejected ... %s (rc=%d)\n", a == 0 ? "PASS" : "FAIL", a);
    printf("B: reply-from-nameserver accepted   ... %s (rc=%d)\n", b == 0 ? "PASS" : "FAIL", b);

    if(a == 0 && b == 0) {
        printf("test_static_resolv_spoof: PASS\n");
        return 0;
    }
    printf("test_static_resolv_spoof: FAIL\n");
    return 1;
}
