#!/bin/bash
COMMAND="opencpn"

while true; do
    if ! pgrep -f "^$COMMAND$" > /dev/null; then
        echo "Program not running. Restarting..."
        "$COMMAND" & 
    fi
    sleep 10
done

