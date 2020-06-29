CXX = g++
FLAGS = -O2

#server: main.cpp ./lock/lock.cpp ./Info/blockqueue.cpp ./Info/info.cpp ./mysqlpoll/mysqlpoll.cpp ./threadpoll/threadpoll.cpp ./timer/list_timer/list_timer.cpp  ./tools/tools.cpp ./http/http.cpp  ./webserver/webserver.cpp
server:	main.cpp ./webserver/webserver.cpp ./http/http.cpp ./timer/list_timer/list_timer.cpp ./threadpoll/threadpoll.cpp ./mysqlpoll/mysqlpoll.cpp ./Info/info.cpp ./tools/tools.cpp ./Info/blockqueue.cpp ./lock/lock.cpp 
	$(CXX) -o server  $^ $(FLAGS) -lpthread -lmysqlclient
clean: 
	rm -r server