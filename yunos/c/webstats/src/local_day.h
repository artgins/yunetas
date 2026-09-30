/****************************************************************************
 *          local_day.h
 *
 *          The local calendar day before a moment.
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
 *  Write in `bf` ("YYYY-MM-DD") the local day before the local day of
 *  `now`: the day a report run at `now` is about. 0, or -1 (logged).
 */
PUBLIC int yesterday_of(hgobj gobj, time_t now, char *bf, size_t bfsize);

#ifdef __cplusplus
}
#endif
