#!/bin/zsh

# clean bulid revelant
for file in "main"
rm  server main.o  webserver.o list_timer.o  http.o  threadpoll.o mysqlpoll.o  info.o blockqueue.o tools.o lock.o
# clean log file 
rm 2020*