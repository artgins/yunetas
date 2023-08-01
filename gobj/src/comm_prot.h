/****************************************************************************
 *          COMM_PROT.H
 *
 *          Communication protocols register
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

/**rst**
   Register a gclass with a communication protocol
**rst**/
PUBLIC int comm_prot_register(const char *schema, gclass_name_t gclass_name);

/**rst**
   Get the schema of an url
**rst**/
PUBLIC const char *comm_prot_get_schema(const char *url);

/**rst**
   Get the gclass name implementing the schema
**rst**/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema);

#ifdef __cplusplus
}
#endif
