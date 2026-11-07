/****************************************************************************
 *          C_TEST_CLI_OPERATOR.H
 *
 *          GClass to test how C_IEVENT_CLI takes the operator of a request
 *          of its server (verify_operator, service_principal)
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
GOBJ_DECLARE_GCLASS(C_TEST_CLI_OPERATOR);

/*
 *  A wrong result found by the driver
 */
extern int test_cli_operator_failed;
extern char *test_tokens[7];          // the tokens the tester made: main.c looks for them in the log

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_cli_operator(void);

#ifdef __cplusplus
}
#endif
