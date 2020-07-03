
#include"../webserver/webserver.h"


/*
    非成员函数
*/

// 直接发送错误信息 然后关闭连接
void show_error(int connfd,const char* info){
    send(connfd,info,sizeof(info),0);
    close(connfd);
}
// 定时器超时处理函数
void timeout_handler(timer_node* node){
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

// 信号处理函数
void sig_handler(int sig){
    int pre_erro = errno;
    send(tools::pipefd[1],(char*) &sig,sizeof(sig),0);
    errno = pre_erro;
}


/*
    构造和析构
*/

// 构造函数
webserver::webserver(){
    http_conns;
   // http_conns.reset( new http[MAX_FD] );
    char server_path[256];
    getcwd(server_path,sizeof(server_path));
    char root[6] = "/root"; 
    rootPath  += server_path;
    rootPath  += root; 
   
}
// 析构函数
webserver::~webserver(){
    
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);

    for(timer_node* timer: timer_nodes){
        if(timer != nullptr){
            delete timer;
            timer = nullptr;
        }
    }

}


/*
    初始化函数
*/


void webserver::parse_arg(std::string dbuser,std::string dbpasswd,std::string dbname_, int argc , char** argv){
    int opt;
    // 冒号表示后面有值
    // [-p port]  [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model] 

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
    server_port =  9006;
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
        // Log  异步写 0 关闭 非0打开
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
        case 'c':{
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
    m_threadpoll.reset( new threadpoll<http>(std::function<void(http*)>(http_work_func),thread_size,10000) );
    LOG_INFO("%s","initialize threadpoll function");  
}
// 初始化log
void webserver::init_log(){
    if(LogAsynWrite){
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,800);
    } else{
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,0);
    }
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
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    // LT + LT  LT + ET 
    if(trigueMode == 0 || trigueMode == 1){
        //  注册读  非oneshoot  LT  非阻塞
        tool.epoll_add(epollfd,listenfd,true,false,false);
        tool.setnonblocking(listenfd);
    } // ET + LT  ET + ET
    else{
        tool.epoll_add(epollfd,listenfd,true,false,true);
        tool.setnonblocking(listenfd);
    }

    http::set_epoll_fd(epollfd);

    LOG_INFO("%s","initialize epoll fd")

    // 创建管道
    ret = socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);

    tools::pipefd[0] = pipefd[0];
    tools::pipefd[1] = pipefd[1];

    assert(ret != -1 );
    // 写非阻塞
    tool.setnonblocking(pipefd[1]);
    tool.epoll_add(epollfd,pipefd[0],true,false,false);
    tool.setnonblocking(pipefd[0]);
    //  设置信号
    tool.set_sigfunc(SIGPIPE,SIG_IGN,false);
    tool.set_sigfunc(SIGALRM,sig_handler,false);
    tool.set_sigfunc(SIGTERM,sig_handler,false);
    
    LOG_INFO("%s","initialize pipe and signal")
} 
// 初始化定时器
void webserver::init_timer(){
    timers.reset(new list_timer(TIMESLOT));
    timers->setfunc(timeout_handler);
    timer_nodes = std::vector<timer_node*>(MAX_FD,NULL);
    alarm(timer_slot);
    LOG_INFO("%s","initialize timer")
}


/*
    定时器相关
*/


//重新调整定时器
void webserver::adjust_timernode(timer_node* timer){
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timers->adjust(timer);
    LOG_INFO("%s%d", "adjust timer",timer->sockfd);
}
//在接受连接时添加一个定时器
void webserver::add_timernode(int fd){
    // 这里有问题
    timer_node* timer( new timer_node);
    timer->sockfd = fd;
    timer->conn = &http_conns[fd];
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timers->add(timer);
    to_timernode[fd] = timer;
    timer_nodes[fd] = timer;
}
// 移除定时器节点
void webserver::remove_timernode(int fd){
    timer_node* timer = to_timernode[fd];
    timers->remove(timer);
    delete timer;
    to_timernode[fd] = nullptr;
}
// 定时器处理
void webserver::timer_handler(){
    timers->tick();
    alarm(timers->slot());
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
        }
        else{  //  LT + LT
            http_conns[connfd].init(connfd,LT);
        }
        add_timernode(connfd);
        
    }
    // ET + LT   ET + ET  listen ET
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
            //  ET + ET
            if(trigueMode ==3){
                http_conns[connfd].init(connfd,ET);
            }
            else{  //  ET + LT
                http_conns[connfd].init(connfd,LT);
            }
            add_timernode(connfd);
        }
    }
    LOG_INFO("%s","accept a new connection")
    return true;
}

// 处理信号
bool webserver::dealwith_signal(){
    char signals[1024];
    int ret = recv(pipefd[0], signals, sizeof(signals), 0);
    // 出错
    if (ret == -1){
        return false;
    }
    // 关闭
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
    // proactor
    if(actor_model == proactor){
        bool ret = http_conns[connfd].read_once();
        if(ret){
            m_threadpoll->put(&http_conns[connfd]);
         // http_conns[connfd].process();
            adjust_timernode(to_timernode[connfd]);
        }
        // 失败
        else {
            remove_timernode(connfd);
            http_conns[connfd].close_connection();
            LOG_WARN("%s","proactor http read_once failure")
        }
    }
    // reactor
    else {
        m_threadpoll->put(&http_conns[connfd]);
        adjust_timernode(to_timernode[connfd]);
    }
}
// 处理写
bool webserver::dealwith_write(int connfd){
    // proactor 
    if(actor_model == proactor){
        bool ret = http_conns[connfd].write_to_socket();
        if(ret){
           adjust_timernode(to_timernode[connfd]);
        }
        // 失败
        else {
            http_conns[connfd].close_connection();
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
            LOG_ERROR("%s %s %d", "epoll failure","errno is",errno);
            break;
        }

        LOG_INFO("%s%d%s","Epoll wait totally ",num," events trigger")

        // 判断每一个事件
        for(int i = 0; i < num; ++i){

            int sockfd = events[i].data.fd;
            //新连接
            if(sockfd == listenfd){
                LOG_INFO("%s","receive a new conncetion")
                accpet_connection();
            }
            // 错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ){
                LOG_INFO("%s","connection rd hup or error")
                remove_timernode(sockfd);
                http_conns[sockfd].close_connection();
            }
            // 信号
            else if ( (sockfd == pipefd[0]) && (events[i].events & EPOLLIN) ){
                LOG_INFO("%s","signal from pipe")
                bool ret = dealwith_signal();
                if(!ret){
                    LOG_ERROR("%s", "dealsignal failure");
                }
            }
            else if (events[i].events & EPOLLIN){
                LOG_INFO("%s","read event")
                dealwith_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT){
                 LOG_INFO("%s","write event")
                dealwith_write(sockfd);
            }
        }
        if(time_out){
            timers->dealwith_alarm();
            LOG_INFO("%s", "deal with timeout");
            time_out = false;
        }
    }
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
    LOG_INFO("%s%s","Log write is ",LogAsynWrite ? "asynronization" :"synchronization")
    LOG_INFO("%s%s","reactor is ",actor_model == proactor ? "proactor" :"reactor")
    LOG_INFO("%s%d","mysql connection number is ",sql_conn_size)
    LOG_INFO("%s%d","threadpoll connection number is ",thread_size)
    LOG_INFO("%s%s","server linger is ",sock_linger ? "opened" :"closed")
    LOG_INFO("%s","\r")
}

