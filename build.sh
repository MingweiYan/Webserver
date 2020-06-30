#!/bin/sh

g++ -c ./lock/lock.cpp   # lock.o

g++ -c ./tools/tools.cpp # tools.o

g++ -c ./Info/info.cpp # info.o

g++ -c  ./mysqlpoll/mysqlpoll.cpp #mysqlpoll.o

g++ -c ./http/http.cpp  #http.o

g++ -c ./timer/list_timer/list_timer.cpp #list_timer.o

g++ -c ./webserver/webserver.cpp #webserver.o

g++ -c main.cpp  #main.o

g++ -o server main.o  webserver.o list_timer.o  http.o   mysqlpoll.o  info.o  tools.o lock.o -O2 -lpthread -lmysqlclient