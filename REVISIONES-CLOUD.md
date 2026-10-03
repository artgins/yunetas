# Cloud review of main

Reviewed up to `5eae09871` (2026-10-03): the answer to the review of
`27b4b7293`. What is resolved is removed from this file.

Read-only review: the suite is run by the fixing session before each
review request, so it is not repeated here.

## Nothing pending

Every item of the previous report is resolved, and the round brings no new
defect:

- `read_process_stat()` tells "no such process" (`ENOENT` / `ESRCH`, -1) from
  "cannot be told" (any other errno, an empty or unparsable stat, -2), says
  the second once until a read works, and `process_is_gone()` takes it for
  alive: an `EMFILE` or a `hidepid` mount no longer empties the first wait.
- A pid gone before it is recorded is not kept; a yuno whose pids are all
  gone is left out of the wait and relaunched normally. One whose stat cannot
  be read at record time keeps start time 0 and is judged by existence
  (logged, and documented).
- `restart_held` keeps each act (`kill`, `disable`, `run`) per id;
  `enable-yuno` takes back the disable only. Every reader of `restart_held`
  only tests the key, so the new value shape changes nothing else.
- The header of `launch_spared_yuno()` parses again; CHANGELOG and
  `YUNO_LIFECYCLE.md` match the code.

Known gaps, not defects (already accepted): no ctest for the agent's spare
window (no ctest compiles `c_agent.c`; checked live each round), nor for the
C_UDP client, the websocket default max, or a frame of exactly `max-1`.
