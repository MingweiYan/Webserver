CXX = g++
FLAGS = -O2 

#server: main.cpp ./lock/lock.cpp ./Info/blockqueue.cpp ./Info/info.cpp ./mysqlpoll/mysqlpoll.cpp ./threadpoll/threadpoll.cpp ./timer/list_timer/list_timer.cpp  ./tools/tools.cpp ./http/http.cpp  ./webserver/webserver.cpp
server:	main.cpp ./webserver/webserver.cpp ./timer/list_timer/list_timer.cpp ./http/http.cpp  ./mysqlpoll/mysqlpoll.cpp ./info/info.cpp  ./tools/tools.cpp ./lock/lock.cpp 
	$(CXX) -o server -g $^ $(FLAGS) -lpthread -lmysqlclient
clean: 
	rm -r server