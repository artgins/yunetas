/***************************************************************************
 *          replace_string.h
 *
 *          A copy of `str` with every occurrence of `old` replaced by
 *          `snew`, left to right and without overlaps.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***************************************************************************/
#pragma once

#include "gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  Return a new string, to free with gbmem_free(), or NULL (logged) when
 *  an argument is missing, `old` is empty, or there is no memory.
 *
 *      char *s = replace_string("a-b-c", "-", "+");   // "a+b+c"
 */
PUBLIC char *replace_string(const char *str, const char *old, const char *snew);

#ifdef __cplusplus
}
#endif
