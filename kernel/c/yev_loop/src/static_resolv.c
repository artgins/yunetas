/****************************************************************************
 *          static_resolv.c
 *
 *          NSS-free replacement for getaddrinfo(3) and freeaddrinfo(3)
 *          used in CONFIG_FULLY_STATIC builds to avoid glibc NSS/dlopen.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta_config.h>  /* must precede CONFIG_FULLY_STATIC guard */
#ifdef CONFIG_FULLY_STATIC

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/random.h>     /* getrandom() — unpredictable DNS transaction id */

#include <time.h>           /* clock_gettime(CLOCK_MONOTONIC) — cache expiry */
#include <syslog.h>         /* the only log this file can reach — see resolv_warn() */
#include "static_resolv.h"

/*
 *  Why syslog(3) directly and not print_error()/gobj_log_*():
 *
 *  - This file is the libc-only replacement for glibc's resolver, and its
 *    unit test #includes it and links nothing but libc, so it cannot reach
 *    into gobj-c (see tests/c/yev_loop/static_resolv/CMakeLists.txt).
 *  - print_error() malloc()s its message before handing it to syslog(), so it
 *    is precisely the wrong tool for reporting an allocation failure. This
 *    formats into a stack buffer and allocates nothing, so it still works
 *    when there is no memory left to report with.
 *
 *  Same destination as print_error(PEF_SYSLOG), fewer ways to fail.
 */
static void resolv_warn(int priority, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void resolv_warn(int priority, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    syslog(priority, "YUNETAS static_resolv: %s", msg);
}

/***************************************************************
 *              Internal helpers
 ***************************************************************/

/*
 * Free a linked list produced by yuneta_getaddrinfo.
 */
void yuneta_freeaddrinfo(struct addrinfo *res)
{
    while(res) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res->ai_canonname);
        free(res);
        res = next;
    }
}

/*
 * Allocate and populate one struct addrinfo node.
 */
static struct addrinfo *make_addrinfo_node(
    int family, int socktype, int protocol,
    const struct sockaddr *addr, socklen_t addrlen,
    const char *canonname)
{
    struct addrinfo *ai = calloc(1, sizeof(*ai));
    if(!ai) {
        resolv_warn(LOG_ERR, "out of memory building an addrinfo node");
        return NULL;
    }

    ai->ai_family   = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen  = addrlen;
    ai->ai_addr     = malloc(addrlen);
    if(!ai->ai_addr) {
        resolv_warn(LOG_ERR, "out of memory building an addrinfo address");
        free(ai);
        return NULL;
    }
    memcpy(ai->ai_addr, addr, addrlen);

    if(canonname) {
        ai->ai_canonname = strdup(canonname);
        if(!ai->ai_canonname) {
            resolv_warn(LOG_ERR, "out of memory building an addrinfo canonname");
            free(ai->ai_addr);
            free(ai);
            return NULL;
        }
    }
    return ai;
}

/*
 * Render resolved addresses into the result list.
 *
 * `tailp` is the address of the caller's tail pointer, so successive calls
 * keep appending.  Shared by the cache-hit and the fresh-answer paths, so a
 * cached lookup is indistinguishable from a fresh one.
 * Returns -1 if a node could not be allocated; the caller owns the cleanup.
 */
static int append_addrs(
    struct addrinfo ***tailp,
    const struct in_addr *a4, int n4,
    const struct in6_addr *a6, int n6,
    uint16_t port, int ai_socktype, int ai_protocol)
{
    for(int j = 0; j < n4; j++) {
        struct sockaddr_in sa = {0};
        sa.sin_family = AF_INET;
        sa.sin_addr   = a4[j];
        sa.sin_port   = htons(port);
        struct addrinfo *ai = make_addrinfo_node(
            AF_INET, ai_socktype, ai_protocol,
            (struct sockaddr *)&sa, sizeof(sa), NULL);
        if(!ai) {
            return -1;
        }
        **tailp = ai;
        *tailp = &ai->ai_next;
    }
    for(int j = 0; j < n6; j++) {
        struct sockaddr_in6 sa = {0};
        sa.sin6_family = AF_INET6;
        sa.sin6_addr   = a6[j];
        sa.sin6_port   = htons(port);
        struct addrinfo *ai = make_addrinfo_node(
            AF_INET6, ai_socktype, ai_protocol,
            (struct sockaddr *)&sa, sizeof(sa), NULL);
        if(!ai) {
            return -1;
        }
        **tailp = ai;
        *tailp = &ai->ai_next;
    }
    return 0;
}

/*
 * Read nameserver lines from /etc/resolv.conf.
 */
#define YUNETA_MAX_NS      3
#define YUNETA_NS_ADDRLEN  46   /* INET6_ADDRSTRLEN */

/* Per-query wait. Paid twice (A + AAAA) for every nameserver that does not
 * answer, and paid inside the event loop, so it is the whole cost of a dead
 * entry in /etc/resolv.conf. */
#define YUNETA_DNS_QUERY_TIMEOUT   3000    /* milliseconds */

/* DNS server port. Overridable at compile time so tests can drive dns_query()
 * against an unprivileged localhost nameserver; production is always 53. */
#ifndef YUNETA_DNS_PORT
#define YUNETA_DNS_PORT    53
#endif

static void parse_resolv_conf(
    char nameservers[][YUNETA_NS_ADDRLEN], int *count, int maxns)
{
    *count = 0;
    FILE *f = fopen("/etc/resolv.conf", "r");
    if(!f) return;

    char line[256];
    while(fgets(line, sizeof(line), f) && *count < maxns) {
        if(strncmp(line, "nameserver", 10) != 0) continue;
        char *p = line + 10;
        while(*p == ' ' || *p == '\t') p++;
        char *end = p;
        while(*end && *end != '\n' && *end != '\r' && *end != ' ') end++;
        *end = '\0';
        size_t len = (size_t)(end - p);
        if(len > 0 && len < YUNETA_NS_ADDRLEN) {
            memcpy(nameservers[*count], p, len + 1);
            (*count)++;
        }
    }
    fclose(f);
}

/*
 * Encode a dotted hostname into DNS wire format.
 * Returns 0 on success, -1 on overflow.
 */
static int dns_encode_name(
    uint8_t *buf, size_t bufsz, size_t *off, const char *name)
{
    const char *p = name;
    while(*p) {
        const char *dot = strchr(p, '.');
        size_t lablen = dot ? (size_t)(dot - p) : strlen(p);
        if(*off + 1 + lablen >= bufsz) return -1;
        buf[(*off)++] = (uint8_t)lablen;
        memcpy(buf + *off, p, lablen);
        *off += lablen;
        p += lablen;
        if(*p == '.') p++;
    }
    if(*off >= bufsz) return -1;
    buf[(*off)++] = 0;  /* root label */
    return 0;
}

/*
 * Build a minimal DNS query packet (RFC 1035).
 * Returns packet length or -1.
 */
static int dns_build_query(
    uint8_t *buf, size_t bufsz, uint16_t id,
    const char *name, uint16_t qtype)
{
    if(bufsz < 12) return -1;
    buf[0]  = (uint8_t)(id >> 8); buf[1]  = (uint8_t)(id & 0xff);
    buf[2]  = 0x01; buf[3]  = 0x00;    /* QR=0 Opcode=0 RD=1 */
    buf[4]  = 0x00; buf[5]  = 0x01;    /* QDCOUNT=1 */
    buf[6]  = 0x00; buf[7]  = 0x00;    /* ANCOUNT=0 */
    buf[8]  = 0x00; buf[9]  = 0x00;    /* NSCOUNT=0 */
    buf[10] = 0x00; buf[11] = 0x00;    /* ARCOUNT=0 */
    size_t off = 12;
    if(dns_encode_name(buf, bufsz, &off, name) < 0) return -1;
    if(off + 4 > bufsz) return -1;
    buf[off++] = (uint8_t)(qtype >> 8);
    buf[off++] = (uint8_t)(qtype & 0xff);
    buf[off++] = 0x00; buf[off++] = 0x01;  /* QCLASS=IN */
    return (int)off;
}

/*
 * Decode a DNS name from msg at offset off, following compression pointers.
 * Writes NUL-terminated result to out[outsz].  Sets *next_off past the name.
 * Returns 0 on success, -1 on error.
 */
static int dns_decode_name(
    const uint8_t *msg, size_t msglen, size_t off,
    char *out, size_t outsz, size_t *next_off)
{
    size_t out_pos = 0;
    int jumped = 0;
    int ptr_count = 0;
    size_t saved_off = 0;

    while(off < msglen) {
        uint8_t len = msg[off];
        if((len & 0xC0) == 0xC0) {
            if(off + 1 >= msglen) return -1;
            size_t ptr = ((size_t)(len & 0x3F) << 8) | msg[off + 1];
            if(ptr >= msglen) return -1;    // F-002: pointer must stay inside the message
            if(!jumped) { saved_off = off + 2; jumped = 1; }
            if(++ptr_count > 10) return -1;
            off = ptr;
            continue;
        }
        off++;
        if(len == 0) break;
        if(off + len > msglen) return -1;   // F-001: label bytes must lie within the message
        if(out_pos + len + 2 > outsz) return -1;
        if(out_pos > 0) out[out_pos++] = '.';
        memcpy(out + out_pos, msg + off, len);
        out_pos += len;
        off += len;
    }
    out[out_pos] = '\0';
    if(next_off) *next_off = jumped ? saved_off : off;
    return 0;
}

/*
 * Send a DNS query via UDP and receive the response.
 * Returns response length or -1 on error/timeout.
 */
static int dns_query(
    const char *nameserver_ip,
    const uint8_t *query, size_t qlen,
    uint8_t *resp, size_t resp_bufsz,
    int timeout_ms)
{
    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };

    /* Try IPv4 nameserver */
    struct sockaddr_in srv4 = {0};
    srv4.sin_family = AF_INET;
    srv4.sin_port   = htons(YUNETA_DNS_PORT);

    if(inet_pton(AF_INET, nameserver_ip, &srv4.sin_addr) == 1) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock < 0) return -1;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        /* connect() the UDP socket so the kernel drops datagrams from any
         * source other than the chosen nameserver — closes the "any source"
         * delivery used for off-path DNS response forgery. (F-001 spoofing) */
        if(connect(sock, (struct sockaddr *)&srv4, sizeof(srv4)) < 0) {
            close(sock); return -1;
        }
        if(send(sock, query, qlen, 0) < 0) {
            close(sock); return -1;
        }
        int n;
        do {
            n = (int)recv(sock, resp, resp_bufsz, 0);
        } while(n < 0 && errno == EINTR);   /* restart if interrupted by a signal */
        close(sock);
        return n;
    }

    /* Try IPv6 nameserver */
    struct sockaddr_in6 srv6 = {0};
    srv6.sin6_family = AF_INET6;
    srv6.sin6_port   = htons(YUNETA_DNS_PORT);

    if(inet_pton(AF_INET6, nameserver_ip, &srv6.sin6_addr) == 1) {
        int sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if(sock < 0) return -1;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        /* connect() the UDP socket so the kernel drops datagrams from any
         * source other than the chosen nameserver — closes the "any source"
         * delivery used for off-path DNS response forgery. (F-001 spoofing) */
        if(connect(sock, (struct sockaddr *)&srv6, sizeof(srv6)) < 0) {
            close(sock); return -1;
        }
        if(send(sock, query, qlen, 0) < 0) {
            close(sock); return -1;
        }
        int n;
        do {
            n = (int)recv(sock, resp, resp_bufsz, 0);
        } while(n < 0 && errno == EINTR);   /* restart if interrupted by a signal */
        close(sock);
        return n;
    }

    return -1;
}

/*
 * Parse answer records from a DNS response.
 * Validates the transaction ID against expected_id.
 * Collects A records into addrs4[] and AAAA records into addrs6[].
 * Returns number of records found, -1 on parse error or ID mismatch.
 */
/*
 *  `min_ttl` (optional) collects the smallest TTL seen across the answer
 *  records that were actually used, which is what the cache may hold the
 *  result for.  Left untouched when no usable record is found, so the
 *  caller's initial value decides what "no answer" means.
 */
static int dns_parse_response(
    const uint8_t *msg, size_t msglen,
    uint16_t expected_id,
    struct in_addr *addrs4, int *naddrs4,
    struct in6_addr *addrs6, int *naddrs6,
    int maxaddrs,
    uint32_t *min_ttl)
{
    if(msglen < 12) return -1;
    /* Validate transaction ID to reject spoofed/stray UDP responses */
    uint16_t resp_id = ((uint16_t)msg[0] << 8) | msg[1];
    if(resp_id != expected_id) return -1;
    if(!(msg[2] & 0x80)) return -1;        /* QR must be 1 */
    if((msg[3] & 0x0F) != 0) return -1;    /* RCODE must be 0 */

    int qdcount = (msg[4] << 8) | msg[5];
    int ancount = (msg[6] << 8) | msg[7];
    size_t off  = 12;

    /* Skip question section */
    for(int i = 0; i < qdcount; i++) {
        while(off < msglen) {
            uint8_t len = msg[off];
            if((len & 0xC0) == 0xC0) { off += 2; break; }
            off++;
            if(len == 0) break;
            off += len;
        }
        off += 4;   /* QTYPE + QCLASS */
    }

    int found = 0;
    if(off > msglen) return found;  // F-003: a malformed question section must not run past the buffer

    for(int i = 0; i < ancount; i++) {
        if(off >= msglen) break;
        /* Skip name */
        size_t next_off;
        char rname[256];
        if(dns_decode_name(msg, msglen, off, rname, sizeof(rname), &next_off) < 0) break;
        off = next_off;
        if(off + 10 > msglen) break;

        uint16_t rtype  = ((uint16_t)msg[off] << 8) | msg[off+1]; off += 2;
        off += 2;   /* class */
        uint32_t rttl   = ((uint32_t)msg[off]   << 24) |
                          ((uint32_t)msg[off+1] << 16) |
                          ((uint32_t)msg[off+2] <<  8) |
                          ((uint32_t)msg[off+3]);       off += 4;
        uint16_t rdlen  = ((uint16_t)msg[off] << 8) | msg[off+1]; off += 2;

        if(rtype == 1 && rdlen == 4 &&
           addrs4 && *naddrs4 < maxaddrs && off + 4 <= msglen) {
            memcpy(&addrs4[*naddrs4], msg + off, 4);
            (*naddrs4)++;
            found++;
            if(min_ttl && rttl < *min_ttl) {
                *min_ttl = rttl;
            }
        } else if(rtype == 28 && rdlen == 16 &&
                  addrs6 && *naddrs6 < maxaddrs && off + 16 <= msglen) {
            memcpy(&addrs6[*naddrs6], msg + off, 16);
            (*naddrs6)++;
            found++;
            if(min_ttl && rttl < *min_ttl) {
                *min_ttl = rttl;
            }
        }
        off += rdlen;
    }
    return found;
}

/***************************************************************
 *              Resolution cache
 *
 *  Without it every connect re-queries DNS, and a yuno that builds N
 *  channels to the same host pays the full round-trip N times.  That is
 *  invisible while DNS answers in a millisecond and fatal when it does
 *  not: a dead first `nameserver` in /etc/resolv.conf costs the A and
 *  AAAA timeouts below (~6 s) on *every* connect, and yuneta_getaddrinfo
 *  runs synchronously inside the event loop, so it blocks the whole
 *  process.  One yuno with 25 channels spent ~3 minutes there and never
 *  reached its agent.
 *
 *  Only step 5 (DNS) is cached.  Numeric literals and /etc/hosts are
 *  already cheap, and caching /etc/hosts would break the expectation
 *  that editing it takes effect at once.
 *
 *  Entries are held for the answer's own TTL, clamped: the floor stops a
 *  zero/near-zero TTL from restoring the stampede, the ceiling keeps a
 *  long-TTL record from outliving a real failover for the life of the
 *  process.  Fixed-size table, no allocation: a resolver cache must not
 *  be able to grow without bound, and this file's nodes come from libc
 *  malloc rather than gbmem_*, so keeping the cache allocation-free
 *  keeps it out of that question entirely.
 *
 *  Failures are deliberately NOT cached.  A negative entry would spare
 *  the timeout while an IdP is down, but it also holds the outage open
 *  after DNS recovers, and the observed failure mode was a *slow*
 *  success, not a failure.
 ***************************************************************/
#define YUNETA_DNS_CACHE_ENTRIES    32
#define YUNETA_DNS_CACHE_ADDRS      8
#define YUNETA_DNS_CACHE_MIN_TTL    5       /* seconds */
#define YUNETA_DNS_CACHE_MAX_TTL    300     /* seconds */

/*
 *  Deadlines are kept on CLOCK_MONOTONIC, so an NTP step — which lands
 *  exactly where it hurts most, at boot — cannot freeze or flush the cache.
 *
 *  These mirror start_sectimer()/test_sectimer() from gobj-c's helpers.h,
 *  which is what the rest of the tree uses for timeouts.  They are repeated
 *  here on purpose: this file is the libc-only replacement for glibc's
 *  resolver and its unit test #includes it and links nothing but libc (see
 *  tests/c/yev_loop/static_resolv/CMakeLists.txt).  Pulling in gobj-c for two
 *  arithmetic helpers would invert the layering and break that test.
 */
static time_t cache_deadline(time_t seconds)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + seconds;
}

static int cache_expired(time_t deadline)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec >= deadline;
}

static uint64_t monotonic_msec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

/*
 *  A nameserver that never answers is the expensive failure here, and it is
 *  silent by nature: resolution still succeeds via the next one in
 *  /etc/resolv.conf, just seconds later, every single time. Nothing in the
 *  yuno's own log can show that — this file is below all of it — so say it
 *  where it can be said, once in a while per nameserver.
 *
 *  Rate limited because this runs inside the event loop on every connect:
 *  a broken nameserver must leave a trail, not a flood.
 */
#define YUNETA_DNS_WARN_SLOTS       YUNETA_MAX_NS
#define YUNETA_DNS_WARN_PERIOD      300     /* seconds between repeats */

typedef struct {
    char    ns[YUNETA_NS_ADDRLEN];
    time_t  next_warn;      /* 0 = never warned */
} dns_warn_slot_t;

static dns_warn_slot_t dns_warn_slots[YUNETA_DNS_WARN_SLOTS];

/*
 *  TRUE when this nameserver is due a warning (and arms the next one).
 */
static int dns_warn_due(const char *ns)
{
    dns_warn_slot_t *free_slot = NULL;
    for(int i = 0; i < YUNETA_DNS_WARN_SLOTS; i++) {
        dns_warn_slot_t *w = &dns_warn_slots[i];
        if(!w->next_warn) {
            if(!free_slot) {
                free_slot = w;
            }
            continue;
        }
        if(strcmp(w->ns, ns) == 0) {
            if(!cache_expired(w->next_warn)) {
                return 0;
            }
            w->next_warn = cache_deadline(YUNETA_DNS_WARN_PERIOD);
            return 1;
        }
    }
    if(!free_slot) {
        free_slot = &dns_warn_slots[0];
    }
    /* Precision-cap the copy: ns is a const char* GCC cannot prove is
     * NUL-terminated within its 46-byte array, so %s alone worst-cases a
     * read past the field. The source is always an address string < 46. */
    snprintf(free_slot->ns, sizeof(free_slot->ns), "%.*s",
        (int)(sizeof(free_slot->ns) - 1), ns);
    free_slot->next_warn = cache_deadline(YUNETA_DNS_WARN_PERIOD);
    return 1;
}

typedef struct {
    char            host[NI_MAXHOST];
    struct in_addr  a4[YUNETA_DNS_CACHE_ADDRS];
    int             n4;
    struct in6_addr a6[YUNETA_DNS_CACHE_ADDRS];
    int             n6;
    time_t          expire;     /* start_sectimer() deadline; 0 = free slot */
} dns_cache_entry_t;

static dns_cache_entry_t dns_cache[YUNETA_DNS_CACHE_ENTRIES];

/*
 *  Return the live entry for `host`, or NULL.  Expired slots are freed on
 *  the way so a long-lived process recycles them without a sweep.
 */
static dns_cache_entry_t *dns_cache_get(const char *host)
{
    for(int i = 0; i < YUNETA_DNS_CACHE_ENTRIES; i++) {
        dns_cache_entry_t *e = &dns_cache[i];
        if(!e->expire) {
            continue;
        }
        if(cache_expired(e->expire)) {
            e->expire = 0;
            continue;
        }
        if(strcasecmp(e->host, host) == 0) {
            return e;
        }
    }
    return NULL;
}

/*
 *  Store (or refresh) the answer for `host`.  Picks a free slot, else the
 *  one closest to expiring — with a fixed table something has to go, and
 *  the soonest-dead entry is the cheapest to lose.
 */
static void dns_cache_put(
    const char *host,
    const struct in_addr *a4, int n4,
    const struct in6_addr *a6, int n6,
    uint32_t ttl)
{
    if(n4 <= 0 && n6 <= 0) {
        return;
    }
    if(ttl < YUNETA_DNS_CACHE_MIN_TTL) {
        ttl = YUNETA_DNS_CACHE_MIN_TTL;
    }
    if(ttl > YUNETA_DNS_CACHE_MAX_TTL) {
        ttl = YUNETA_DNS_CACHE_MAX_TTL;
    }

    dns_cache_entry_t *victim = NULL;
    for(int i = 0; i < YUNETA_DNS_CACHE_ENTRIES; i++) {
        dns_cache_entry_t *e = &dns_cache[i];
        if(!e->expire || cache_expired(e->expire)) {
            victim = e;
            break;
        }
        if(strcasecmp(e->host, host) == 0) {
            victim = e;
            break;
        }
        if(!victim || e->expire < victim->expire) {
            victim = e;
        }
    }
    if(!victim) {
        return;
    }

    memset(victim, 0, sizeof(*victim));
    snprintf(victim->host, sizeof(victim->host), "%s", host);

    victim->n4 = (n4 > YUNETA_DNS_CACHE_ADDRS)? YUNETA_DNS_CACHE_ADDRS : (n4 > 0? n4 : 0);
    for(int i = 0; i < victim->n4; i++) {
        victim->a4[i] = a4[i];
    }
    victim->n6 = (n6 > YUNETA_DNS_CACHE_ADDRS)? YUNETA_DNS_CACHE_ADDRS : (n6 > 0? n6 : 0);
    for(int i = 0; i < victim->n6; i++) {
        victim->a6[i] = a6[i];
    }

    victim->expire = cache_deadline((time_t)ttl);
}

/*
 * DNS transaction ID. Use the kernel CSPRNG: a predictable/monotonic ID makes
 * off-path response forgery (cache/answer poisoning) far easier, so each query
 * gets an unpredictable 16-bit id. The source port is left to the OS ephemeral
 * assignment, which modern Linux randomizes (RFC 6056). (F-004)
 */
static uint16_t dns_random_id(void)
{
    uint16_t id;
    if(getrandom(&id, sizeof(id), 0) != (ssize_t)sizeof(id)) {
        // Fallback if getrandom is unavailable: mix a stack address with a
        // per-call counter — weaker than the CSPRNG, but not a fixed sequence.
        static uint16_t fallback_counter;
        id = (uint16_t)(((uintptr_t)&id >> 4) ^ ++fallback_counter);
    }
    return id;
}

/***************************************************************
 *              Public API
 ***************************************************************/

/*
 * Static replacement for getaddrinfo(3).
 *
 * Resolution order:
 *   1. NULL / empty node  → INADDR_ANY or loopback
 *   2. Numeric IPv4       → direct
 *   3. Numeric IPv6       → direct
 *   4. /etc/hosts lookup
 *   5. DNS query (UDP, /etc/resolv.conf nameservers)
 */
int yuneta_getaddrinfo(
    const char *node, const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res)
{
    if(!res) return EAI_FAMILY;
    *res = NULL;

    int ai_family   = hints ? hints->ai_family   : AF_UNSPEC;
    int ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    int ai_protocol = hints ? hints->ai_protocol : IPPROTO_TCP;
    int ai_flags    = hints ? hints->ai_flags    : 0;

    if(ai_socktype == 0) ai_socktype = SOCK_STREAM;
    if(ai_protocol == 0) ai_protocol = IPPROTO_TCP;

    uint16_t port = service ? (uint16_t)atoi(service) : 0;

    struct addrinfo *head = NULL;
    struct addrinfo **tail = &head;

    /* ------ Step 1: NULL/empty node ------ */
    if(!node || node[0] == '\0') {
        if(ai_family == AF_INET || ai_family == AF_UNSPEC) {
            struct sockaddr_in sa = {0};
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = (ai_flags & AI_PASSIVE)
                                 ? INADDR_ANY : htonl(INADDR_LOOPBACK);
            sa.sin_port = htons(port);
            struct addrinfo *ai = make_addrinfo_node(
                AF_INET, ai_socktype, ai_protocol,
                (struct sockaddr *)&sa, sizeof(sa), NULL);
            if(!ai) return EAI_MEMORY;
            *tail = ai; tail = &ai->ai_next;
        }
        if(ai_family == AF_INET6 || ai_family == AF_UNSPEC) {
            struct sockaddr_in6 sa = {0};
            sa.sin6_family = AF_INET6;
            sa.sin6_addr   = (ai_flags & AI_PASSIVE) ? in6addr_any : in6addr_loopback;
            sa.sin6_port   = htons(port);
            struct addrinfo *ai = make_addrinfo_node(
                AF_INET6, ai_socktype, ai_protocol,
                (struct sockaddr *)&sa, sizeof(sa), NULL);
            if(!ai) { yuneta_freeaddrinfo(head); return EAI_MEMORY; }
            *tail = ai; tail = &ai->ai_next;
        }
        if(head) { *res = head; return 0; }
        return EAI_NONAME;
    }

    /* ------ Step 2: numeric IPv4 ------ */
    if(ai_family == AF_INET || ai_family == AF_UNSPEC) {
        struct in_addr a4;
        if(inet_pton(AF_INET, node, &a4) == 1) {
            struct sockaddr_in sa = {0};
            sa.sin_family = AF_INET;
            sa.sin_addr   = a4;
            sa.sin_port   = htons(port);
            struct addrinfo *ai = make_addrinfo_node(
                AF_INET, ai_socktype, ai_protocol,
                (struct sockaddr *)&sa, sizeof(sa), NULL);
            if(!ai) return EAI_MEMORY;
            *res = ai;
            return 0;
        }
    }

    /* ------ Step 3: numeric IPv6 ------ */
    if(ai_family == AF_INET6 || ai_family == AF_UNSPEC) {
        struct in6_addr a6;
        if(inet_pton(AF_INET6, node, &a6) == 1) {
            struct sockaddr_in6 sa = {0};
            sa.sin6_family = AF_INET6;
            sa.sin6_addr   = a6;
            sa.sin6_port   = htons(port);
            struct addrinfo *ai = make_addrinfo_node(
                AF_INET6, ai_socktype, ai_protocol,
                (struct sockaddr *)&sa, sizeof(sa), NULL);
            if(!ai) return EAI_MEMORY;
            *res = ai;
            return 0;
        }
    }

    /* ------ Step 4: /etc/hosts lookup ------ */
    {
        FILE *f = fopen("/etc/hosts", "r");
        if(f) {
            char line[512];
            while(fgets(line, sizeof(line), f)) {
                char *hash = strchr(line, '#');
                if(hash) *hash = '\0';

                char *saveptr = NULL;
                char *ip = strtok_r(line, " \t\r\n", &saveptr);
                if(!ip) continue;

                char *tok;
                while((tok = strtok_r(NULL, " \t\r\n", &saveptr)) != NULL) {
                    if(strcasecmp(tok, node) != 0) continue;

                    if(ai_family == AF_INET || ai_family == AF_UNSPEC) {
                        struct in_addr a4;
                        if(inet_pton(AF_INET, ip, &a4) == 1) {
                            struct sockaddr_in sa = {0};
                            sa.sin_family = AF_INET;
                            sa.sin_addr   = a4;
                            sa.sin_port   = htons(port);
                            struct addrinfo *ai = make_addrinfo_node(
                                AF_INET, ai_socktype, ai_protocol,
                                (struct sockaddr *)&sa, sizeof(sa), NULL);
                            if(!ai) { fclose(f); yuneta_freeaddrinfo(head); return EAI_MEMORY; }
                            *tail = ai; tail = &ai->ai_next;
                        }
                    }
                    if(ai_family == AF_INET6 || ai_family == AF_UNSPEC) {
                        struct in6_addr a6;
                        if(inet_pton(AF_INET6, ip, &a6) == 1) {
                            struct sockaddr_in6 sa = {0};
                            sa.sin6_family = AF_INET6;
                            sa.sin6_addr   = a6;
                            sa.sin6_port   = htons(port);
                            struct addrinfo *ai = make_addrinfo_node(
                                AF_INET6, ai_socktype, ai_protocol,
                                (struct sockaddr *)&sa, sizeof(sa), NULL);
                            if(!ai) { fclose(f); yuneta_freeaddrinfo(head); return EAI_MEMORY; }
                            *tail = ai; tail = &ai->ai_next;
                        }
                    }
                    break;  /* matched first hostname token; move to next line */
                }
            }
            fclose(f);
        }
    }
    if(head) { *res = head; return 0; }

    /* ------ Step 5a: cache ------ */
    {
        dns_cache_entry_t *e = dns_cache_get(node);
        if(e) {
            int want4 = (ai_family == AF_INET  || ai_family == AF_UNSPEC)? e->n4 : 0;
            int want6 = (ai_family == AF_INET6 || ai_family == AF_UNSPEC)? e->n6 : 0;
            if(want4 > 0 || want6 > 0) {
                if(append_addrs(&tail, e->a4, want4, e->a6, want6,
                        port, ai_socktype, ai_protocol) < 0) {
                    yuneta_freeaddrinfo(head);
                    return EAI_MEMORY;
                }
                *res = head;
                return 0;
            }
            /*
             *  The entry holds the other family only: fall through and query,
             *  rather than answering EAI_NONAME from a partial cache hit.
             */
        }
    }

    /* ------ Step 5b: DNS query ------ */
    {
        char nameservers[YUNETA_MAX_NS][YUNETA_NS_ADDRLEN];
        int ns_count = 0;
        parse_resolv_conf(nameservers, &ns_count, YUNETA_MAX_NS);

        if(ns_count == 0) {
            resolv_warn(LOG_ERR,
                "no usable 'nameserver' line in /etc/resolv.conf, cannot resolve '%s'",
                node);
        }

        struct in_addr  addrs4[16];
        struct in6_addr addrs6[16];
        int naddrs4 = 0, naddrs6 = 0;
        uint32_t ttl = YUNETA_DNS_CACHE_MAX_TTL;
        uint64_t t0 = monotonic_msec();

        for(int nsi = 0; nsi < ns_count && !naddrs4 && !naddrs6; nsi++) {
            uint8_t qbuf[512];
            uint8_t rbuf[4096];

            /* A record query */
            if(ai_family == AF_INET || ai_family == AF_UNSPEC) {
                uint16_t qid4 = dns_random_id();
                int qlen = dns_build_query(
                    qbuf, sizeof(qbuf), qid4, node, 1 /* A */);
                if(qlen > 0) {
                    int rlen = dns_query(nameservers[nsi],
                                         qbuf, (size_t)qlen,
                                         rbuf, sizeof(rbuf), YUNETA_DNS_QUERY_TIMEOUT);
                    if(rlen > 0) {
                        int naddrs6_dummy = 0;
                        dns_parse_response(rbuf, (size_t)rlen, qid4,
                                           addrs4, &naddrs4,
                                           NULL, &naddrs6_dummy, 16, &ttl);
                    }
                }
            }

            /* AAAA record query */
            if(ai_family == AF_INET6 || ai_family == AF_UNSPEC) {
                uint16_t qid6 = dns_random_id();
                int qlen = dns_build_query(
                    qbuf, sizeof(qbuf), qid6, node, 28 /* AAAA */);
                if(qlen > 0) {
                    int rlen = dns_query(nameservers[nsi],
                                         qbuf, (size_t)qlen,
                                         rbuf, sizeof(rbuf), YUNETA_DNS_QUERY_TIMEOUT);
                    if(rlen > 0) {
                        int naddrs4_dummy = 0;
                        dns_parse_response(rbuf, (size_t)rlen, qid6,
                                           NULL, &naddrs4_dummy,
                                           addrs6, &naddrs6, 16, &ttl);
                    }
                }
            }

            /*
             *  This nameserver gave nothing. Harmless when it is the last
             *  one and the name simply does not exist; expensive when it is
             *  the first of several, because then every resolution in the
             *  process pays its timeouts before reaching one that answers.
             */
            if(!naddrs4 && !naddrs6 && nsi + 1 < ns_count && dns_warn_due(nameservers[nsi])) {
                resolv_warn(LOG_WARNING,
                    "nameserver %s did not answer for '%s' after %d ms, "
                    "falling back to %s. Every lookup pays this: check the "
                    "'nameserver' order in /etc/resolv.conf",
                    nameservers[nsi], node, YUNETA_DNS_QUERY_TIMEOUT * 2, nameservers[nsi + 1]);
            }
        }

        uint64_t elapsed = monotonic_msec() - t0;
        if(elapsed >= 1000) {
            /*
             *  yuneta_getaddrinfo() runs synchronously inside the event loop,
             *  so this is not just a slow lookup: the whole process was
             *  stopped for that long. Worth saying out loud with the number.
             */
            resolv_warn(LOG_WARNING,
                "resolving '%s' took %llu ms and blocked the event loop "
                "(%d nameserver(s) in /etc/resolv.conf)",
                node, (unsigned long long)elapsed, ns_count);
        }

        if(naddrs4 > 0 || naddrs6 > 0) {
            dns_cache_put(node, addrs4, naddrs4, addrs6, naddrs6, ttl);
            if(append_addrs(&tail, addrs4, naddrs4, addrs6, naddrs6,
                    port, ai_socktype, ai_protocol) < 0) {
                yuneta_freeaddrinfo(head);
                return EAI_MEMORY;
            }
        }
    }

    if(head) { *res = head; return 0; }
    return EAI_NONAME;
}

#endif /* CONFIG_FULLY_STATIC */
