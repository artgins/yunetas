/****************************************************************************
 *          C_CURL.H
 *          GClass of CURL uv-mixin.
 *
 *          Mixin libcurl-uv-gobj
 *          Based on https://gist.github.com/clemensg/5248927
 *
 *          Copyright (c) 2014 by Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_CURL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_SEND_CURL);
GOBJ_DECLARE_EVENT(EV_CURL_RESPONSE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_curl(void);

#ifdef __cplusplus
}
#endif
