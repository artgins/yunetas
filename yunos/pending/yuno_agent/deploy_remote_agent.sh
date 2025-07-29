#!/bin/sh

#  Exit immediately if a command exits with a non-zero status.
set -e

ssh yuneta@$1 "/yuneta/bin/yshutdown"
scp -p \
    /yuneta/bin/ybatch \
    /yuneta/bin/ylist \
    /yuneta/bin/ystats \
    /yuneta/bin/ycommand \
    /yuneta/bin/ytestconfig \
    /yuneta/bin/yuneta \
    /yuneta/bin/yshutdown \
    /yuneta/bin/tr2list \
    /yuneta/bin/tr2keys \
    /yuneta/bin/trq_list \
    /yuneta/bin/msg2db_list \
    /yuneta/bin/time2date \
    /yuneta/bin/time2range \
    /yuneta/bin/trmsg_list \
    /yuneta/bin/tranger_search \
    /yuneta/bin/treedb_list \
    /yuneta/bin/json_diff \
    /yuneta/bin/ytests\
    /yuneta/bin/tranger_delete \
    /yuneta/bin/list_queue_msgs \
    yuneta@$1:/yuneta/bin

scp -p \
    /yuneta/agent/yuneta_agent22 \
    /yuneta/agent/yuneta_agent \
    yuneta@$1:/yuneta/agent

ssh yuneta@$1 "/yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --start"
