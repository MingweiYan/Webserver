
#include"../webserver/webserver.h"


/*
    非成员函数
*/

// 直接发送错误信息 然后关闭连接
void show_error(int connfd,const char* info){
    send(connfd,info,strlen(info),0);
    close(connfd);
}
// 信号处理函数
void sig_handler(int sig){
    int pre_erro = errno;
    send(tools::signal_pipefd[1],(char*) &sig,sizeof(sig),0);
    errno = pre_erro;
}
// 非成员函数版超时处理函数
void nonmember_timeout_handler (timer_node* cur){
    cur->conn->close_connection();
    cur->clear();
    LOG_INFO("non-membertimeout func is called")
}
//
// 非成员函数版的线程池的工作函数  
void nonmember_http_work_func(http* conn){
    // reactor
    if(!conn->isProactor()){
        // 读取
        int ret = conn->read_once();
        if(ret){
            //  读取成功 进行处理
            LOG_INFO("%s%d","reactor read from socket sucess the sockfd is ",conn->fd())
            conn->process();
        } 
        else { // 读取失败 直接关闭
            LOG_ERROR("%s","reactor read from socket error ")
          //  conn->close_connection(); 
            conn->inform_close();
        }
        ret = conn->write_to_socket();
        if(!ret){
            LOG_ERROR("%s","reactor write to socket error ")
           // conn->close_connection(); 
            conn->inform_close();
        }
        LOG_INFO("%s%d","write to socket sucess and adjust tiemrnode the sockfd is ",conn->fd())
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
    std::unique_ptr<http[]> p (new http[MAX_FD]);
    http_conns = std::move(p);
    char server_path[256];
    char* tmp;
    tmp = getcwd(server_path,sizeof(server_path));
    char root[6] = "/root"; 
    rootPath  += server_path;
    rootPath  += root; 
   
}
// 析构函数
webserver::~webserver(){
    
    close(epollfd);
    close(listenfd);
    close(signal_pipefd[1]);
    close(signal_pipefd[0]);
    close(close_pipefd[1]);
    close(close_pipefd[0]);
   /* for(timer_node* timer: timer_nodes){
        if(timer != nullptr){
            delete timer;
            timer = nullptr;
        }
    }
    */

}


/*
    初始化函数
*/


void webserver::parse_arg(std::string dbuser,std::string dbpasswd,std::string dbname_, int argc , char** argv){
    int opt;
    // 冒号表示后面有值
    // [-p port]  [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-i close_log] [-a actor_model] 

    // 默认参数
    dbusername = dbuser;
    dbpassword = dbpasswd;
    dbport = 3306;
    dbname = dbname_;
    LogOpen = true;
    LogAsynWrite = false;
    sock_linger = false;
    trigueMode = 1; 
    sql_conn_size = 8;
    thread_size = 8;
    actor_model = proactor;      
    server_port =  1122;
    timer_slot = TIMESLOT;
    stop_server = false;
    time_out = false;


    const char *str = "p:v:l:m:o:s:t:c:a:";
    while ( (opt = getopt(argc, argv, str)) != -1){
        
        switch (opt){
        //  服务器端口
        case 'p':{
            server_port = atoi(optarg);
            break;
        }
        // Log  异步写 0关闭 非0打开
        case 'l':{
            LogAsynWrite = atoi(optarg);
            break;
        }
        // 触发模式  0-4  LT + LT  LT + ET ET + LT ET + LT  
        case 'm':{
            trigueMode = atoi(optarg);
            break;
        }
        // linger 0 关闭 非0打开
        case 'o':{
            sock_linger = atoi(optarg);
            break;
        }
        //  mysql 连接数目
        case 's':{
            sql_conn_size = atoi(optarg);
            break;
        }
        // 线程池大小
        case 't':{
            thread_size = atoi(optarg);
            break;
        }
        // Log 打开 0 关闭 非0打开
        case 'i':{
            LogOpen = atoi(optarg);
            break;
        }
        // 并发时间处理模式
        case 'a':{
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }

}


// 初始化线程库
void webserver::init_threadpoll(){
   // std::function<void(http*)> func = std::bind(&webserver::http_work_func,this,std::placeholders::_1);
    std::function<void(http*)> func = nonmember_http_work_func;
    std::unique_ptr< threadpoll<http> > p (new threadpoll<http>(func,thread_size,10000) );
    m_threadpoll = std::move(p);
    LOG_INFO("%s","initialize threadpoll function");  
}
// 初始化定时器
void webserver::init_timer(){
    std::unique_ptr<timer> tmp (new list_timer(TIMESLOT));
    timers = std::move(tmp);
   // timers->setfunc(std::bind(&webserver::timeout_handler,this,std::placeholders::_1));
    timers->setfunc(nonmember_timeout_handler);
    std::unique_ptr<timer_node[]> p ( new timer_node[MAX_FD] );
    timer_nodes = std::move(p);
    alarm(timer_slot);

    // 创建管道
    int ret = socketpair(PF_UNIX,SOCK_STREAM,0,close_pipefd);

    tools::close_pipefd[0] = close_pipefd[0];
    tools::close_pipefd[1] = close_pipefd[1];

    assert(ret != -1 );
    // 写非阻塞
    tools::getInstance()->setnonblocking(close_pipefd[1]);
    tools::getInstance()->epoll_add(epollfd,close_pipefd[0],true,false,false);
    tools::getInstance()->setnonblocking(close_pipefd[0]);

    LOG_INFO("%s","initialize timer")
}
// 初始化log
void webserver::init_log(){
    if(LogAsynWrite){
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,800);
    } else{
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,0);
    }
    LOG_STR(" /***********************************************************************************************/")
    LOG_STR("                                                                                                 ")
    LOG_INFO("%s","initialize log function");   
}
// 初始化mysql 
void webserver::init_mysqlpoll(){
    mysqlpoll::getInstance()->init("localhost",dbusername,dbpassword,dbport,dbname,sql_conn_size);
    LOG_INFO("%s","initialize mysqlpoll function");  
}
// 初始化listen
void webserver::init_listen(){

    //初始化http静态变量
    init_http_static(const_cast<char*>( rootPath.c_str()));
    LOG_INFO("%s","load user account and password from database")

    // 创建监听socket
    listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server_port);

    // 优雅关闭  断开连接后 发送完数据
    if(sock_linger){
        struct linger tmp = {1, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else {
        struct linger tmp = {0, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    // 重用端口
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    LOG_INFO("%s","initialize listen socket")

    // epoll
    epollfd = epoll_create(MAX_FD);
    assert(epollfd != -1);
    // LT + LT  LT + ET 
    if(trigueMode == 0 || trigueMode == 1){
        //  注册读  非oneshoot  LT  非阻塞
        tools::getInstance()->epoll_add(epollfd,listenfd,true,false,false);
        tools::getInstance()->setnonblocking(listenfd);
    } // ET + LT  ET + ET
    else{
        tools::getInstance()->epoll_add(epollfd,listenfd,true,false,true);
        tools::getInstance()->setnonblocking(listenfd);
    }

    http::set_epoll_fd(epollfd);
    http::set_actor_model(actor_model);

    LOG_INFO("%s","initialize epoll fd")

    // 创建管道
    ret = socketpair(PF_UNIX,SOCK_STREAM,0,signal_pipefd);

    tools::signal_pipefd[0] = signal_pipefd[0];
    tools::signal_pipefd[1] = signal_pipefd[1];

    assert(ret != -1 );
    // 写非阻塞
    tools::getInstance()->setnonblocking(signal_pipefd[1]);
    tools::getInstance()->epoll_add(epollfd,signal_pipefd[0],true,false,false);
    tools::getInstance()->setnonblocking(signal_pipefd[0]);
    //  设置信号
    tools::getInstance()->set_sigfunc(SIGPIPE,SIG_IGN,true);
    tools::getInstance()->set_sigfunc(SIGALRM,sig_handler,false);
    tools::getInstance()->set_sigfunc(SIGTERM,sig_handler,false);
    
    LOG_INFO("%s","initialize pipe and signal")
} 
// 输出配置的服务器信息
void webserver::printSetting(){
    LOG_INFO("%s %s %s %s","mysql username: ",dbusername.c_str()," mysql password: ",dbpassword.c_str())
    LOG_INFO("%s %s %s %d","mysql databasename :",dbname.c_str()," mysql port: ",dbport)
    LOG_INFO("%s%d","server port is ",server_port)
    switch (trigueMode)
            {
            case 0:
                LOG_INFO("%s","Trigger mode is listen LT + http_conn LT")
                break;
            case 1:
                LOG_INFO("%s","Trigger mode is listen LT + http_conn ET")
                break;
            case 2:
                LOG_INFO("%s","Trigger mode is listen ET + http_conn LT")
                break;
            case 3:
                LOG_INFO("%s","Trigger mode is listen ET + http_conn ET")
                break;
            default:
                LOG_INFO("%s","Opps Unkonwn Trigger mode ")
                break;
            }
    LOG_INFO("%s%s","Log write : ",LogAsynWrite ? "asynronization" :"synchronization")
    LOG_INFO("%s%s","actor model : ",actor_model == proactor ? "proactor" :"reactor")
    LOG_INFO("%s%d","mysql connection number : ",sql_conn_size)
    LOG_INFO("%s%d","threadpoll connection number : ",thread_size)
    LOG_INFO("%s%s","server linger : ",sock_linger ? "opened" :"closed")
    LOG_STR(" /***********************************************************************************************/")
    LOG_STR("                                                                                                 ")
}



/*
    定时器相关
*/


//重新调整定时器
void webserver::adjust_timernode(int fd){
    timer_node* timer = &timer_nodes[fd];
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timers->adjust(timer);
 //   LOG_INFO("%s%d", "adjust timernmode whose sockfd is ",fd);
}
//在接受连接时添加一个定时器
void webserver::add_timernode(int fd){
    // 这里有问题
    timer_node * timer = &timer_nodes[fd];
    timer->sockfd = fd;
    timer->conn = &http_conns[fd];
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timers->add(timer); 
//    LOG_INFO("%s%d", "add timernmode whose sockfd is ",fd);
}
// 移除定时器节点
void webserver::remove_timernode(int fd){
    timer_node* timer = &timer_nodes[fd];
    timers->remove(timer);
    timer_nodes[fd].clear();
    LOG_INFO("%s%d","remove timer node whose sockfd is ",fd)
}
// 定时器处理
void webserver::timer_handler(){
    timers->tick();
    alarm(timers->slot());
}
// 定时器超时处理函数
void webserver::timeout_handler(timer_node* node){
    int fd = node->sockfd;
    LOG_INFO("%d%s",fd,"  is time out close connection and remove timer node")
    close_connection(fd);
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
        if(connfd < 0){
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
            http_conns[connfd].init(connfd,ET);
            toSockfd[ &http_conns[connfd] ] = connfd;
        }
        else{  //  LT + LT
            http_conns[connfd].init(connfd,LT);
            toSockfd[ &http_conns[connfd] ] = connfd;
        }
        add_timernode(connfd);
        LOG_INFO("%s%d","LT listen fd establish a new connection, sockfd is ",connfd)
    }
    // ET + LT   ET + ET  listen ET
    else{
        while(true){
            int connfd = accept(listenfd,(sockaddr*)&client_addr,&client_addr_len);
            if (connfd < 0){
                LOG_ERROR("%s%d", "accept a new connection error the errno is ", errno);
                break;
            }
            if (http::cur_user_cnt >= MAX_FD){
                show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            //  ET + ET
            if(trigueMode ==3){
                http_conns[connfd].init(connfd,ET);
                toSockfd[ &http_conns[connfd] ] = connfd;
            }
            else{  //  ET + LT
                http_conns[connfd].init(connfd,LT);
                toSockfd[ &http_conns[connfd] ] = connfd;
            }
            add_timernode(connfd);
            LOG_INFO("%s%d","ET listen fd establish a new connection, sockfd is ",connfd)
        }
    }
  //  LOG_INFO("%s","accept a new connection")
    return true;
}

// 处理信号
bool webserver::dealwith_signal(){
    char signals[1024];
    int ret = recv(signal_pipefd[0], signals, sizeof(signals), 0);
    // 出错
    if (ret == -1){
        LOG_ERROR("%s%d","recv signal from pipe failed the errno is ",errno)
        return false;
    }
    // 关闭
    else if (ret == 0){
        LOG_ERROR("%s","recv signal from pipe failed pipe is closed ")
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
// 处理关闭连接
bool webserver::dealwith_close(){
    int fds[1024] ;
    for(int i = 0; i < 1024; ++i){
        fds[i] = 0;
    }
    int ret = recv(close_pipefd[0], fds, sizeof(fds), 0);
    // 出错
    if (ret == -1){
        LOG_ERROR("%s%d","recv close fd from pipe failed the errno is ",errno)
        return false;
    }
    // 关闭
    else if (ret == 0){
        LOG_ERROR("%s","recv close fd from pipe failed pipe is closed ")
        return false;
    }
    for(int i = 0; i < 1024  && fds[i]>=0; ++i){
        close_connection(fds[i]);
        fds[i] = 0;
    }
    
}
// 处理读
bool webserver::dealwith_read(int connfd){
    // proactor
    if(actor_model == proactor){
        bool ret = http_conns[connfd].read_once();
        if(ret){
            m_threadpoll->put(&http_conns[connfd]);
         // http_conns[connfd].process();
            adjust_timernode(connfd);
            LOG_INFO("%s%d","proactor read from socket sucess the sockfd is ",connfd)
        }
        // 失败
        else {
            close_connection(connfd);
            LOG_WARN("%s%d","proactor http read from socket failure the sockfd is ",connfd)
        }
    }
    // reactor
    else {
        m_threadpoll->put(&http_conns[connfd]);
        adjust_timernode(connfd);
    }
}
// 处理写
bool webserver::dealwith_write(int connfd){
    // proactor 
    if(actor_model == proactor){
        bool ret = http_conns[connfd].write_to_socket();
        if(ret){
            LOG_INFO("%s%d","write to socket sucess and adjust tiemrnode the sockfd is ",connfd)
            adjust_timernode(connfd);
        }
        // 失败  或者读写完毕
        else {
            close_connection(connfd);
        }
    }
    // reactor
    else {
        // 这里不考虑  线程池分读写状态
        adjust_timernode(connfd);
    }
}


// 线程池的工作函数  
void webserver::http_work_func(http* conn){
    // reactor
    if(!conn->isProactor()){
        // 读取
        int ret = conn->read_once();
        if(ret){
            //  读取成功 进行处理
            LOG_INFO("%s%d","reactor read from socket sucess the sockfd is ",conn->fd())
            conn->process();
        } 
        else { // 读取失败 直接关闭
            LOG_ERROR("%s","proactor read from socket failure ")
            close_connection(toSockfd[conn]);
        }
        ret = conn->write_to_socket();
        if(!ret){
            LOG_ERROR("%s","proactor write to socket failure ")
            close_connection(toSockfd[conn]);    
        }
        LOG_INFO("%s%d","write to socket sucess and adjust tiemrnode the sockfd is ",conn->fd())
    } 
    // proactor
    else{
        conn->process();
    }
}
// 关闭一个连接
void webserver::close_connection(int fd){
   // m_lock.lock();
    http* conn = &http_conns[fd];
    conn->close_connection();
    remove_timernode(fd);
    toSockfd[conn] = 0;
  //  m_lock.unlock();
}





/*
    主循环
*/
// 循环读取时间
void webserver::events_loop(){
    while(!stop_server){

        int num = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if (num < 0 && errno != EINTR){
            LOG_ERROR("%s %s %d", "epoll failure","errno is",errno);
            break;
        }
    //    LOG_INFO("%s%d%s","Epoll wait totally ",num," events trigger")

        // 判断每一个事件
        for(int i = 0; i < num; ++i){

            int sockfd = events[i].data.fd;
            //新连接
            if(sockfd == listenfd){
                LOG_INFO("%s","epoll receive a new conncetion")
                accpet_connection();
            }
            // 错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ){
              /*  if(events[i].events & EPOLLRDHUP){
                    LOG_WARN("%s","epoll RDHUP")
                } else if (events[i].events & EPOLLHUP){
                    LOG_WARN("%s","epoll HUP")
                } else if (events[i].events & EPOLLERR){
                    LOG_WARN("%s","epoll ERR")
                }
                */
                LOG_WARN("%s","connection is closed by clients")
                close_connection(sockfd);
            }
            // 信号
            else if ( (sockfd == close_pipefd[0]) && (events[i].events & EPOLLIN) ){
             //   LOG_INFO("%s","signal from pipe")
                bool ret = dealwith_close();
                if(!ret){
                    LOG_ERROR("%s", "dealclose failure");
                }
            }
            // 关闭连接
            else if ( (sockfd == signal_pipefd[0]) && (events[i].events & EPOLLIN) ){
             //   LOG_INFO("%s","signal from pipe")
                bool ret = dealwith_signal();
                if(!ret){
                    LOG_ERROR("%s", "dealsignal failure");
                }
            }
            // 可读
            else if (events[i].events & EPOLLIN){
             //   LOG_INFO("%s","read event")
                dealwith_read(sockfd);
            }
            // 可写
            else if (events[i].events & EPOLLOUT){
              //   LOG_INFO("%s","write event")
                dealwith_write(sockfd);
            }
        }
        if(time_out){
            timers->dealwith_alarm();
        //    LOG_INFO("%s", "deal with timeout");
            time_out = false;
        }
    }
}



