#!/bin/bash

# Command to find and delete files owned by the current user in /dev/shm
find /dev/shm -user "$(whoami)" -delete

# Command to kill all processes named 'proj2'
killall proj2

# Command to display message queues, semaphore sets, and shared memory segments owned by the current user
ipcs -tm | grep "$(whoami)"

# Command to find files owned by the current user in /dev/shm
find /dev/shm -user "$(whoami)"

# Command to display detailed information about processes
ps ux

# chmod +x clean.sh