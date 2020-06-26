
#include"../webserver/webserver.h"

/*
    非成员函数
*/

// 直接发送错误信息 
void show_error(int connfd,const char* info){
    send(connfd,info,sizeof(info),0);
    close(connfd);
}
// 定时器定时处理函数
void timer_handler(timer_node* node){
    node->conn->close_connection();
}

//  !!!  这里需不需要对conn分状态？？？
// 线程池的工作函数  
void http_work_func(http* conn){
    // reactor
    if(!conn->isProactor()){
        int ret = conn->read_once();
        if(ret){
            conn->process();
        } 
        // 读取失败 直接关闭
        else {
            conn->close_connection();
        }
        ret = conn->write_to_socket();
        if(!ret){
            conn->close_connection();
        }
    } 
    // proactor
    else{
        conn->process();
    }
}

/*
    构造和析构
*/
// 构造函数
webserver::webserver(){
    connections = new http[MAX_FD];
    char server_path[256];
    getcwd(server_path,sizeof(server_path));
    char root[6] = "/root"; 
    rootPath = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(rootPath,server_path);
    strcat(rootPath,root);
}
// 析构函数
webserver::~webserver(){
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    free(rootPath);
    delete timer_;
    delete threadpoll_;
    delete connections;
}


/*
    初始化函数
*/


// 初始化函数
void webserver::init(std::string dbusername,std::string dbpassword,std::string dbname,bool useLog,bool logAsyn,
            int servport,bool linger, int trigue_mode,int sql_cnt,int thread_cnt,int actor_model){
    dbusername = dbusername;
    dbpassword = dbpassword;
    dbport = 3306;
    dbname = dbname;
    LogOpen = useLog;
    LogAsynWrite = logAsyn;
    sock_linger = linger;
    trigue_mode = trigue_mode;
    sql_conn_size = sql_cnt;
    thread_size = thread_cnt;
    actor_model = actor_model; 
    threadpoll_  = NULL;      
    server_port =   servport;
    timer_slot = 5;
    stop_server = false;
    time_out = false;
}
// 初始化线程库
void webserver::init_threadpoll(){
    threadpoll_ = new threadpoll<http>(http_work_func,thread_size);
}
// 初始化log
void webserver::init_log(){
    if(LogAsynWrite){
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,800);
    } else{
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,0);
    }   
}
// 初始化mysql 
void webserver::init_mysqlpoll(){
    mysqlpoll::getInstance()->init("localhost",dbusername,dbpassword,dbport,dbname,sql_conn_size);
}
// 初始化listen
void webserver::init_listen(){
    listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);
    // 优雅关闭
    if(sock_linger){
        struct linger tmp = {1, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else {
        struct linger tmp = {0, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    // listen
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server_port);
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    // LT + LT  LT + ET 
    if(trigueMode == 0 || trigueMode == 1){
        tool.epoll_add(epollfd,listenfd,false,false,false);
    }
    http::set_epoll_fd(epollfd);

    ret = socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret != -1 );
    // 写非阻塞
    tool.setnonblocking(pipefd[1]);
    tool.epoll_add(epollfd,pipefd[0],true,false,false);
    tool.set_sigfunc(SIGPIPE,SIG_IGN,false);
    tool.set_sigfunc(SIGALRM,sig_handler,false);
    tool.set_sigfunc(SIGTERM,sig_handler,false);
    alarm(timer_slot);
} 


/*
    定时器相关
*/


//重新调整定时器
void webserver::adjust_timernode(timer_node* timer){
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timer_->adjust(timer);
    LOG_INFO("%s%d", "adjust timer",timer->sockfd);
}
//在接受连接时添加一个定时器
void webserver::add_timernode(int fd){
    // 这里有问题
    timer_node* timer ;
    timer->sockfd = fd;
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timer_->add(timer);
    to_timernode[fd] = timer;
}
// 移除定时器节点
void webserver::remove_timernode(int fd){
    timer_node* timer = to_timernode[fd];
    timer_->remove(timer);
    to_timernode.erase(fd);
}
// 定时器处理
void webserver::timer_handler(){
    timer_->tick();
    alarm(timer_->slot());
}


/*
    处理epoll事件
*/

// 建立连接
bool webserver::accpet_connection(){
    struct sockaddr_in  client_addr;
    socklen_t  client_addr_len = sizeof(client_addr);
    // LT + LT  LT + ET  listen LT
    if(trigueMode == 0 || trigueMode == 1){
        int connfd = accept(listenfd,(sockaddr*)&client_addr,&client_addr_len);
        if(connfd<0){
            LOG_ERROR("%s:errno is:%d", "accept connection error", errno);
            return false;
        }
        if (http::cur_user_cnt >= MAX_FD){
            show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        // LT + ET    
        if(trigueMode ==1){
            connections[connfd].init(connfd,ET);
        }
        else{  //  LT + LT
            connections[connfd].init(connfd,LT);
        }
        add_timernode(connfd);
    }
    // ET + LT   ET + ET 
    else{
        while(true){
            int connfd = accept(listenfd,(sockaddr*)&client_addr,&client_addr_len);
            if (connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http::cur_user_cnt >= MAX_FD){
                show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            add_timernode(connfd);
            //  ET + ET
            if(trigueMode ==3){
                connections[connfd].init(connfd,ET);
            }
            else{  //  ET + LT
                connections[connfd].init(connfd,LT);
            }
        }
    }
    
}

// 处理信号
bool webserver::dealwith_signal(){
    char signals[1024];
    int ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1){
        return false;
    }
    else if (ret == 0){
        return false;
    }
    else{
        for (int i = 0; i < ret; ++i){
            switch (signals[i]){
            case SIGALRM:
                time_out = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            }
        }
    }
    return true;
}
// 处理读
bool webserver::dealwith_read(int connfd){
    if(actor_model == proactor){
        bool ret = connections[connfd].read_once();
        if(ret){
            threadpoll_->put(connections+connfd);
            adjust_timernode(to_timernode[connfd]);
        }
        // 失败
        else {
            tool.epoll_remove(epollfd,connfd);
            close(connfd);
        }
    }
    // reactor
    else {
        threadpoll_->put(connections+connfd);
        adjust_timernode(to_timernode[connfd]);
    }
}
// 处理写
bool webserver::dealwith_write(int connfd){
    if(actor_model == proactor){
        bool ret = connections[connfd].write_to_socket();
        if(ret){
            adjust_timernode(to_timernode[connfd]);
        }
        // 失败
        else {
            tool.epoll_remove(epollfd,connfd);
            close(connfd);
        }
    }
    // reactor
    else {
        // 这里不考虑  线程池分读写状态
        adjust_timernode(to_timernode[connfd]);
    }
}
/*
    主循环
*/
// 循环读取时间
void webserver::events_loop(){
    while(!stop_server){
        int num = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if (num < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for(int i = 0; i < num; ++i){
            int sockfd = events[i].data.fd;
            //新连接
            if(sockfd == listenfd){
                accpet_connection();
            }
            // 错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ){
                tool.epoll_remove(epollfd,sockfd);
                close(sockfd);
            }
            // 信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)){
                bool ret = dealwith_signal();
                if(!ret){
                    LOG_ERROR("%s", "dealsignal failure");
                }
            }
            else if (events[i].events & EPOLLIN){
                dealwith_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT){
                dealwith_write(sockfd);
            }
        }
        if(time_out){
            timer_->dealwith_alarm();
            time_out = false;
        }
    }
}



