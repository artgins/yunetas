/****************************************************************************
 *          environment.h
 *
 *          Environment
 *
 *          These functions must be provided externally
 *          and adapted to the operating system.
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC const char *node_uuid(void);
PUBLIC const char *get_hostname(void);

#ifdef __cplusplus
}
#endif
