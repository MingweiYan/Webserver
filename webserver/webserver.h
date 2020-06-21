#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include<string>
#include<vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<assert.h>
#include <sys/socket.h>
#include <sys/types.h>

#include"../Info/info.h"
#include"../threadpoll/threadpool.h"
#include"../mysqlpoll/mysqlpoll.h"
#include"../timer/http_timer.h"
#include"../timer/list_timer/list_timer.h"
#include"../tools/tools.h"
#include"../http/http.h"


const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位



class webserver
{
private:
    // 数据库相关
    std::string dbusername;
    std::string dbpassword;
    std::string dbname;
    int dbport;
    int sql_conn_size;
    // 定时器相关
    timer*  timer_;
    int pipefd[2];
    time_t timer_slot;
    std::unordered_map<int,timer_node*> to_timernode;
    //epoll 相关
    int epollfd;
    int listenfd;
    epoll_event events[MAX_EVENT_NUMBER];
    //服务器
    int server_port; 
    bool  sock_linger;
    int trigueMode;
    http* connections;
    bool stop_server;
    bool time_out;
    //线程池
    threadpoll<http> * threadpoll_;
    int thread_size;
    // 其他
    tools tool;
    int actor_model;
    bool LogOpen;
    bool  LogAsynWrite;
    char * rootPath;
public:
    // 初始化
    webserver();
    ~webserver();
    void init(std::string dbusername,std::string dbpassword,std::string dbname,bool useLog,bool logAsyn,int servport,bool linger, int trigue_mode,int sql_cnt,int thread_cnt,int actor_model);
    void init_log();
    void init_threadpoll();
    void init_mysqlpoll();
    void init_listen();
    // 定时器相关
    void adjust_timernode(timer_node*);
    void add_timernode(int fd);
    void remove_timernode(int);
    // epollfd相关
    bool accpet_connection();
    bool dealwith_signal();
    bool dealwith_read(int);
    bool dealwith_write(int);
    // 主循环
    void events_loop();
};

void sig_handler(int sig, int pipefd);

#endif