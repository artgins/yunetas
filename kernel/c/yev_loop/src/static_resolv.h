/****************************************************************************
 *          static_resolv.h
 *
 *          NSS-free replacement for getaddrinfo(3) and freeaddrinfo(3)
 *          used in CONFIG_FULLY_STATIC builds to avoid glibc NSS/dlopen.
 *
 *          Resolution order:
 *            1. NULL / empty node  → INADDR_ANY or loopback
 *            2. Numeric IPv4       → direct (inet_pton)
 *            3. Numeric IPv6       → direct (inet_pton)
 *            4. /etc/hosts lookup
 *            5. DNS query (UDP, nameservers from /etc/resolv.conf)
 *
 *          Copyright (c) 2024-2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef CONFIG_FULLY_STATIC

#include <netdb.h>

int  yuneta_getaddrinfo(
        const char *node, const char *service,
        const struct addrinfo *hints, struct addrinfo **res);

void yuneta_freeaddrinfo(struct addrinfo *res);

/* Redirect all call sites in yev_loop.c to the static implementations */
#define getaddrinfo(n, s, h, r)  yuneta_getaddrinfo((n), (s), (h), (r))
#define freeaddrinfo(r)          yuneta_freeaddrinfo(r)

#endif /* CONFIG_FULLY_STATIC */
