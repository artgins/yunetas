#!/bin/bash

while true; do
    ctest --output-on-failure
    if [ $? -ne 0 ]; then
        echo "ctest failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
done
