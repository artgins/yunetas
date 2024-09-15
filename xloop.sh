#!/bin/bash

while true; do
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator3.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    sleep 1         # Optional: Wait before next iteration
done
