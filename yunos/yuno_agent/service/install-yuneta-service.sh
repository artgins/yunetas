#!/bin/bash

cp /yuneta/agent/service/yuneta_agent /etc/init.d

if [ -f "/usr/sbin/update-rc.d" ]; then
    /usr/sbin/update-rc.d yuneta_agent defaults
fi

if [ -f "/sbin/chkconfig" ]; then
    /sbin/chkconfig --add yuneta_agent
fi

