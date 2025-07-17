#!/bin/bash
DIRECTORY="/yuneta/store/queues/dba_postgres/"

if [ ! -d "$DIRECTORY" ];
then
    echo "No existe el directorio "$DIRECTORY""
    exit
fi

cd $DIRECTORY

for d in */
do
    QUEUE="${d%/}"

    cd "$QUEUE"

    for d2 in */
    do
        QUEUE2="${d2%/}"
        echo $PWD " ===>" list_queue_msgs -d "." -p "$QUEUE2" ;
        list_queue_msgs -d "." -p "$QUEUE2" $1;

    done

    cd ..

done

