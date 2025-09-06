#!/bin/bash

/yuneta/bin/yshutdown

if [ -f "/usr/sbin/update-rc.d" ]; then
    /usr/sbin/update-rc.d -f yuneta_agent remove
fi

if [ -f "/sbin/chkconfig" ]; then
    /sbin/chkconfig --del yuneta_agent
fi

rm /etc/init.d/yuneta_agent
