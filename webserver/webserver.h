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
#include <getopt.h>
#include<memory>

#include"../info/info.h"
#include"../threadpoll/threadpool.h"
#include"../mysqlpoll/mysqlpoll.h"
#include"../timer/http_timer.h"
#include"../timer/list_timer/list_timer.h"
#include"../tools/tools.h"
#include"../http/http.h"


const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位


/*struct client_data{
    http* conn;
    int fd;
    timer_node timer_;
};*/

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
    std::unique_ptr<timer>  timers;
    int pipefd[2];
    time_t timer_slot;

    //timer_node* to_timernode[MAX_FD];
   // std::vector<timer_node*> timer_nodes;
    std::unique_ptr<timer_node[]> timer_nodes;
    //epoll 相关
    int epollfd;
    int listenfd;
    epoll_event events[MAX_EVENT_NUMBER];
    //服务器
    int server_port; 
    bool sock_linger;
    int trigueMode;
    //std::unordered_map<int,http> http_conns;
    std::unordered_map<http*,int> toSockfd;
    std::unique_ptr<http[]> http_conns;
    bool stop_server;
    bool time_out;
    int actor_model;
    std::string rootPath;
    //线程池
    std::unique_ptr< threadpoll<http> >  m_threadpoll;
    int thread_size;
    // 日志
    bool LogOpen;
    bool LogAsynWrite;
public:
    // 构造
    webserver();
    ~webserver();
    void parse_arg(std::string dbuser,std::string dbpasswd,std::string dbname, int argc, char* argv[]);
    // 初始化
    void init_log();
    void init_threadpoll();
    void init_mysqlpoll();
    void init_timer();
    void init_listen();
    // 定时器相关
    void add_timernode(int fd);
    void adjust_timernode(int);
    void remove_timernode(int);
    void timer_handler();
    void timeout_handler(timer_node*);
    // epollfd相关
    bool accpet_connection();
    bool dealwith_signal();
    bool dealwith_read(int);
    bool dealwith_write(int);
    // 线程池相关
    void http_work_func(http* conn);
    // 主循环
    void events_loop();
    // 输出设置的信息
    void printSetting();

};

/*
    非成员函数

*/
void sig_handler(int sig);
void show_error(int connfd,const char* info);
void init_static_member();
void nonmember_timeout_handler (timer_node* cur);

#endif