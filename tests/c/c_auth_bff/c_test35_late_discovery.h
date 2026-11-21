/****************************************************************************
 *          C_TEST35_LATE_DISCOVERY.H
 *
 *          Driver GClass for test35_late_discovery: the late answer of a
 *          discovery that timed out reaches no other request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST35_LATE_DISCOVERY);

PUBLIC int register_c_test35_late_discovery(void);

#ifdef __cplusplus
}
#endif
