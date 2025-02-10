#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage: ./wait.sh [IP to wit for]"
    exit
fi

online=0

while ((online == 0 ))
do
        if ping -c 1 $1 &> /dev/null
        then
                echo "$1 is online"
                online=1
        fi
done
