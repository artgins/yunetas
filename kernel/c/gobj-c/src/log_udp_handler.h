/****************************************************************************
 *          log_udp_handler.h
 *
 *          Handler to seng log by udp
 *
 *          Copyright (c) 2014,2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <stdio.h>
#include "helpers.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/
typedef enum { // All formats are terminator null!
    /*
     *  In OUTPUT_FORMAT_YUNETA the ``bf`` to write must be an json dict or json string.
     *  Yuneta format
     *      "%c%08X:%s",
     *          '0' + priority,
     *          uc->counter++,
     *          bf
     *
     */
    OUTPUT_FORMAT_YUNETA = 0,

    /*
     *  In OUTPUT_FORMAT_LUMBERJACK the ``bf`` to write must be an json dict.
     *  See lumberjack https://fedorahosted.org/lumberjack/ .
     *  Otherwise it's up to you.
     */
    OUTPUT_FORMAT_LUMBERJACK,

    /*
     *  Output to system's syslog
     */
    OUTPUT_FORMAT_SYSLOG,

    /*
     *  udpc_write() add only the terminator null
     */
    OUTPUT_FORMAT_RAW,      // WARNING with hardcore references

    OUTPUT_OLD_FORMAT_YUNETA,

    OUTPUT_LAST_ENTRY
} output_format_t;

/*****************************************************************
 *     Structures
 *****************************************************************/
typedef void * udpc_t;

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC int udpc_start_up(const char *process_name, const char *hostname, int pid);
PUBLIC void udpc_end(void); // Close all handlers

// Return NULL on error
PUBLIC udpc_t udpc_open(
    const char *url,
    const char *bindip,
    const char *if_name,
    size_t bf_size,                 // 0 = default 256K
    size_t udp_frame_size,          // 0 = default 1500
    output_format_t output_format,   // 0 = default OUTPUT_FORMAT_YUNETA
    BOOL exit_on_fail
);
PUBLIC void udpc_close(udpc_t udpc);

/*
    Esta función está pensada para enviar logs con json, es decir, strings.
    El string puede ser hasta 64K pero se envía troceado en trozos de udp_frame_size con un nulo al final.
    EL string tiene que incluir el nulo, y si no es así, se añade internamente.
    El receptor distingue los records (strings) por el nulo finalizador.
    64k puede parecer mucho, pero en los gobj-tree grandes se puede superar si piden una view.

    Ojo que el socket es síncrono.
    Eso quiere decir que escrituras masivas consumen mucho tiempo
    (retorna cuando ha concluido de escribir en la red
    y se consciente que bloqueas el yuno durante ese tiempo.
    Si solo tienes un core entonces bloqueas toda la máquina.

    Max len ``bf_size`` bytes
    Return -1 if error
 */
PUBLIC int udpc_write(udpc_t udpc, int priority, const char *bf, size_t len);
PUBLIC int udpc_fwrite(udpc_t udpc, int priority, const char *format, ...);

#ifdef __cplusplus
}
#endif
