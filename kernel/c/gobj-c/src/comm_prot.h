/****************************************************************************
 *          COMM_PROT.H
 *
 *          Communication protocols register
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include "gobj.h"

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
   Register a gclass with a communication protocol
**rst**/
PUBLIC int comm_prot_register(gclass_name_t gclass_name, const char *schema);

/**rst**
   Get the gclass name implementing the schema
**rst**/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema);

/**rst**
   Free comm_prot register
**rst**/
PUBLIC void comm_prot_free(void);

#ifdef __cplusplus
}
#endif
