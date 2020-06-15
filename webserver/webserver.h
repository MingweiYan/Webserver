#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include<string>

#include"../Info/info.h"
#include"../threadpoll/threadpool.h"
#include"../mysqlpoll/mysqlpoll.h"
#include"../timer/timer.h"
#include"../timer/list_timer/list_timer.h"

class webserver
{
private:
    std::string dbusername;
    std::string dbpassword;
    std::string dbname;
    int dbport;
    int serverport; 
public:
    webserver();
    ~webserver();
    void init_log();
    void init_threadpoll();
    void init_mysqlpoll();
    void init_timer();
    void init_tools();
};

void init_webserver();


#endif