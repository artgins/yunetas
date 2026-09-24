/****************************************************************************
 *          audit_record.h
 *
 *          What the agent writes to its audit file for one command.
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
 *              Prototypes
 ***************************************************************/
/*
 *  Build the audit record of a command. `kw` is not owned and not
 *  modified. Return a new json object (yours), NULL on error (logged).
 */
PUBLIC json_t *audit_record_build(
    const char *command,
    json_t *kw,         // not owned
    const char *date
);

/*
 *  Console writes (write-tty). ycommand, ycli and gui_agent send one
 *  write-tty for each keystroke. The audit keeps only the FACT of it:
 *  who, when, which console, how many writes and bytes. Nothing of what
 *  is typed, not even a hash.
 *
 *  The writes of one user (same user and same source) into one console
 *  make a BURST, which lasts `burst_seconds` from its first write. The
 *  first write of a burst is recorded at once, alone, so that a crash of
 *  the agent cannot lose who typed:
 *      {"command":"write-tty","date":D1,"user":U,"console":C,
 *       "writes":1,"bytes":1,"source":{...}}
 *  The other writes of the burst make one record when the burst ends:
 *      {"command":"write-tty","date":D2,"user":U,"console":C,
 *       "writes":N,"bytes":B,"until":Dn,"source":{...}}
 *  (date = its first write, until = its last one). A burst ends when its
 *  interval has passed (seen at the next command of any kind), when
 *  another user or source writes into its console, at any close-console,
 *  and when the agent stops (audit_tty_close_bursts()).
 *
 *  `jn_bursts` is a dict owned by the caller (the state of the bursts).
 *  The records to write now are appended to `jn_records` (a list).
 *  Call it for EVERY command, before audit_record_build(): it returns TRUE
 *  when the command is a write-tty (its records are made here, do not call
 *  audit_record_build()), FALSE otherwise.
 */
PUBLIC BOOL audit_tty_command(
    json_t *jn_bursts,      // not owned
    const char *command,
    json_t *kw,             // not owned
    const char *date,
    unsigned burst_seconds,
    json_t *jn_records      // not owned
);

/*
 *  End every burst (the agent stops): their records are appended to
 *  `jn_records`.
 */
PUBLIC void audit_tty_close_bursts(
    json_t *jn_bursts,      // not owned
    json_t *jn_records      // not owned
);

#ifdef __cplusplus
}
#endif
