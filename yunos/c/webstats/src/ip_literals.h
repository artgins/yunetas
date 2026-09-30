/****************************************************************************
 *          ip_literals.h
 *
 *          The IPv4 addresses of a mail body, written as [a.b.c.d].
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  A copy of the html in `src` with every IPv4 address of its TEXT (never
 *  inside a tag) written as [a.b.c.d], or NULL (logged) when the copy
 *  cannot be made. `src` is not owned. Why a mail cannot carry a bare
 *  address: see ip_literals.c.
 */
PUBLIC gbuffer_t *bracket_ip_literals(gbuffer_t *src);

#ifdef __cplusplus
}
#endif
