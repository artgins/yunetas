/****************************************************************************
 *          C_CURL.H
 *          GClass of CURL uv-mixin.
 *
 *          Mixin libcurl-uv-gobj
 *          Based on https://gist.github.com/clemensg/5248927
 *
 *          Copyright (c) 2014 by Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif


/*********************************************************************
 *      GClass
 *********************************************************************/
PUBLIC GCLASS *gclass_curl(void);

#define GCLASS_CURL_NAME  "Curl"
#define GCLASS_CURL gclass_curl()


#ifdef __cplusplus
}
#endif
