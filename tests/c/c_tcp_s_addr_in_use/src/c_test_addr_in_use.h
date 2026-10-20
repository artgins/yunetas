/****************************************************************************
 *          C_TEST_ADDR_IN_USE.H
 *
 *          GClass to test a C_TCP_S whose address is in use at its start
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
 *              Constants
 ***************************************************************/
GOBJ_DECLARE_GCLASS(C_TEST_ADDR_IN_USE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_addr_in_use(void);

#ifdef __cplusplus
}
#endif
