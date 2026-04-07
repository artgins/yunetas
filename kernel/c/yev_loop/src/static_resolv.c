/****************************************************************************
 *          static_resolv.c
 *
 *          NSS-free replacement for getaddrinfo(3) and freeaddrinfo(3)
 *          used in CONFIG_FULLY_STATIC builds to avoid glibc NSS/dlopen.
 *
 *          Copyright (c) 2024-2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yuneta_config.h>  /* must precede CONFIG_FULLY_STATIC guard */
#ifdef CONFIG_FULLY_STATIC

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "static_resolv.h"

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
    if(!ai) return NULL;

    ai->ai_family   = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen  = addrlen;
    ai->ai_addr     = malloc(addrlen);
    if(!ai->ai_addr) { free(ai); return NULL; }
    memcpy(ai->ai_addr, addr, addrlen);

    if(canonname) {
        ai->ai_canonname = strdup(canonname);
        if(!ai->ai_canonname) { free(ai->ai_addr); free(ai); return NULL; }
    }
    return ai;
}

/*
 * Read nameserver lines from /etc/resolv.conf.
 */
#define YUNETA_MAX_NS      3
#define YUNETA_NS_ADDRLEN  46   /* INET6_ADDRSTRLEN */

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
            if(!jumped) { saved_off = off + 2; jumped = 1; }
            if(++ptr_count > 10) return -1;
            off = ptr;
            continue;
        }
        off++;
        if(len == 0) break;
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
    srv4.sin_port   = htons(53);

    if(inet_pton(AF_INET, nameserver_ip, &srv4.sin_addr) == 1) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock < 0) return -1;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if(sendto(sock, query, qlen, 0,
                  (struct sockaddr *)&srv4, sizeof(srv4)) < 0) {
            close(sock); return -1;
        }
        int n = (int)recv(sock, resp, resp_bufsz, 0);
        close(sock);
        return n;
    }

    /* Try IPv6 nameserver */
    struct sockaddr_in6 srv6 = {0};
    srv6.sin6_family = AF_INET6;
    srv6.sin6_port   = htons(53);

    if(inet_pton(AF_INET6, nameserver_ip, &srv6.sin6_addr) == 1) {
        int sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if(sock < 0) return -1;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if(sendto(sock, query, qlen, 0,
                  (struct sockaddr *)&srv6, sizeof(srv6)) < 0) {
            close(sock); return -1;
        }
        int n = (int)recv(sock, resp, resp_bufsz, 0);
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
static int dns_parse_response(
    const uint8_t *msg, size_t msglen,
    uint16_t expected_id,
    struct in_addr *addrs4, int *naddrs4,
    struct in6_addr *addrs6, int *naddrs6,
    int maxaddrs)
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
        off += 4;   /* TTL */
        uint16_t rdlen  = ((uint16_t)msg[off] << 8) | msg[off+1]; off += 2;

        if(rtype == 1 && rdlen == 4 &&
           addrs4 && *naddrs4 < maxaddrs && off + 4 <= msglen) {
            memcpy(&addrs4[*naddrs4], msg + off, 4);
            (*naddrs4)++;
            found++;
        } else if(rtype == 28 && rdlen == 16 &&
                  addrs6 && *naddrs6 < maxaddrs && off + 16 <= msglen) {
            memcpy(&addrs6[*naddrs6], msg + off, 16);
            (*naddrs6)++;
            found++;
        }
        off += rdlen;
    }
    return found;
}

/* Monotonically-increasing DNS transaction ID */
static uint16_t yuneta_dns_id = 1;

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

    /* ------ Step 5: DNS query ------ */
    {
        char nameservers[YUNETA_MAX_NS][YUNETA_NS_ADDRLEN];
        int ns_count = 0;
        parse_resolv_conf(nameservers, &ns_count, YUNETA_MAX_NS);

        for(int nsi = 0; nsi < ns_count && !head; nsi++) {
            uint8_t qbuf[512];
            uint8_t rbuf[4096];

            /* A record query */
            if(ai_family == AF_INET || ai_family == AF_UNSPEC) {
                uint16_t qid4 = yuneta_dns_id++;
                int qlen = dns_build_query(
                    qbuf, sizeof(qbuf), qid4, node, 1 /* A */);
                if(qlen > 0) {
                    int rlen = dns_query(nameservers[nsi],
                                         qbuf, (size_t)qlen,
                                         rbuf, sizeof(rbuf), 3000);
                    if(rlen > 0) {
                        struct in_addr addrs4[16];
                        int naddrs4 = 0, naddrs6_dummy = 0;
                        dns_parse_response(rbuf, (size_t)rlen, qid4,
                                           addrs4, &naddrs4,
                                           NULL, &naddrs6_dummy, 16);
                        for(int j = 0; j < naddrs4; j++) {
                            struct sockaddr_in sa = {0};
                            sa.sin_family = AF_INET;
                            sa.sin_addr   = addrs4[j];
                            sa.sin_port   = htons(port);
                            struct addrinfo *ai = make_addrinfo_node(
                                AF_INET, ai_socktype, ai_protocol,
                                (struct sockaddr *)&sa, sizeof(sa), NULL);
                            if(!ai) { yuneta_freeaddrinfo(head); return EAI_MEMORY; }
                            *tail = ai; tail = &ai->ai_next;
                        }
                    }
                }
            }

            /* AAAA record query */
            if(ai_family == AF_INET6 || ai_family == AF_UNSPEC) {
                uint16_t qid6 = yuneta_dns_id++;
                int qlen = dns_build_query(
                    qbuf, sizeof(qbuf), qid6, node, 28 /* AAAA */);
                if(qlen > 0) {
                    int rlen = dns_query(nameservers[nsi],
                                         qbuf, (size_t)qlen,
                                         rbuf, sizeof(rbuf), 3000);
                    if(rlen > 0) {
                        struct in6_addr addrs6[16];
                        int naddrs4_dummy = 0, naddrs6 = 0;
                        dns_parse_response(rbuf, (size_t)rlen, qid6,
                                           NULL, &naddrs4_dummy,
                                           addrs6, &naddrs6, 16);
                        for(int j = 0; j < naddrs6; j++) {
                            struct sockaddr_in6 sa = {0};
                            sa.sin6_family = AF_INET6;
                            sa.sin6_addr   = addrs6[j];
                            sa.sin6_port   = htons(port);
                            struct addrinfo *ai = make_addrinfo_node(
                                AF_INET6, ai_socktype, ai_protocol,
                                (struct sockaddr *)&sa, sizeof(sa), NULL);
                            if(!ai) { yuneta_freeaddrinfo(head); return EAI_MEMORY; }
                            *tail = ai; tail = &ai->ai_next;
                        }
                    }
                }
            }
        }
    }

    if(head) { *res = head; return 0; }
    return EAI_NONAME;
}

#endif /* CONFIG_FULLY_STATIC */
