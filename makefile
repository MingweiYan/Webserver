CXX = g++
FLAGS = -O2

server : main.cpp ./http/http.cpp ./info/blockqueue.cpp ./info/info.cpp lock/lock.cpp mysqlpoll/mysqlpoll.cpp threadpoll/threadpoll.cpp timer/list_timer/list_timer.cpp tools/tools.cpp webserver/webserver.cpp
	$(CXX) -o server  $^ $(FLAGS) -lpthread -lmysqlclient
clean : 
	rm -r server