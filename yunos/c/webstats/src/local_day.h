/****************************************************************************
 *          local_day.h
 *
 *          Local calendar days before a moment, counted on the calendar.
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

/*
 *  Write in `bf` ("YYYY-MM-DD") the local day `days` calendar days before
 *  the local day of `t` (1 = yesterday_of()). 0, or -1 (logged).
 */
PUBLIC int day_before_of(hgobj gobj, time_t t, int days, char *bf, size_t bfsize);

/*
 *  The same local time of day `days` calendar days before `t`, or -1
 *  (logged): where an age of `days` days begins.
 */
PUBLIC time_t moment_days_before(hgobj gobj, time_t t, int days);

#ifdef __cplusplus
}
#endif
