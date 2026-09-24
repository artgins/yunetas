/****************************************************************************
 *          C_TEST_IP_LISTS.H
 *
 *          GClass to test the yuno's ip lists at C_TCP_S accept
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
GOBJ_DECLARE_GCLASS(C_TEST_IP_LISTS);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_ip_lists(void);

#ifdef __cplusplus
}
#endif
