#!/bin/zsh

# clean bulid revelant
for file in main server  main.o  webserver.o list_timer.o  http.o  threadpoll.o mysqlpoll.o  info.o blockqueue.o tools.o lock.o
do
    if [ -f "$file" ]; then
        rm  $file
    fi
done

# clean log file 
rm 2020*