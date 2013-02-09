#!/bin/bash
# prints media drives connected to PC (sdb*, sdc*, sdd*, ...)

NUM_LINES=$((`df -h | grep ^/dev/sd | wc | awk '{ print $1; }'`))

if [ $NUM_LINES -eq 1 ]; then
    echo 'no media drives connected'
else
    echo 'Filesystem            Size  Used Avail Use% Mounted on'
    df -h | grep ^/dev/sd | tail -$(($NUM_LINES - 1))
fi