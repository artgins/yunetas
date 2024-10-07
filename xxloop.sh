#!/bin/bash

while true; do
    ./tests/c/timeranger2/test_topic_pkey_integer.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator2.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator3.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator4.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator5.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    ./tests/c/timeranger2/test_topic_pkey_integer_iterator6.c.bin
    if [ $? -ne 0 ]; then
        echo "Program failed, exiting loop."
        break       # Exit the loop if the program fails
    fi
    echo "fin"
done
