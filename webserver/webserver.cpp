
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
    send(tools::pipefd[1],(char*)sig,1,0);
    errno = pre_erro;
}


/*
    构造和析构
*/

// 构造函数
webserver::webserver(){
    http_conns = new http[MAX_FD];
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
    delete timers;
    delete m_threadpoll;
    delete http_conns;
    // 释放定时器节点
    for(int i = 0; i < MAX_FD; ++i){
        if(to_timernode[i]){
            delete to_timernode[i];
        }
    }
}


/*
    初始化函数
*/


void webserver::parse_arg(std::string dbuser,std::string dbpasswd,std::string dbname, int argc , char** argv){
    int opt;
    // 冒号表示后面有值
    // [-p port]  [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model] 

    // 默认参数
    int serverport  = 9006;  
    bool logOpen = true;
    bool AsynLog = false;
    int trigmod = 1; // 0-4  LT + LT  LT + ET ET + LT ET + LT    
    bool linger = false;
    int sql_num = 8;
    int thread_num = 8;
    int actormodel = proactor;

    const char *str = "p:v:l:m:o:s:t:c:a:";
    while ( (opt = getopt(argc, argv, str)) != -1){
        
        switch (opt){
        // 
        case 'p':{
            serverport = atoi(optarg);
            break;
        }
        // 0 关闭 非0打开
        case 'l':{
            AsynLog = atoi(optarg);
            break;
        }
        //
        case 'm':{
            trigmod = atoi(optarg);
            break;
        }
        // 0 关闭 非0打开
        case 'o':{
            linger = atoi(optarg);
            break;
        }
        //
        case 's':{
            sql_num = atoi(optarg);
            break;
        }
        //
        case 't':{
            thread_num = atoi(optarg);
            break;
        }
        // 0 关闭 非0打开
        case 'c':{
            logOpen = atoi(optarg);
            break;
        }
        case 'a':{
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
    
    // 初始化
    init(dbuser,dbpasswd,dbname,logOpen,AsynLog,server_port,linger,trigmod,sql_num,thread_num,actor_model);
}


// 初始化函数
void webserver::init(std::string dbusername_,std::string dbpassword_,std::string dbname_,bool useLog,bool logAsyn,
            int servport,bool linger, int trigue_mode_,int sql_cnt,int thread_cnt,int actor_model_){
    dbusername = dbusername_;
    dbpassword = dbpassword_;
    dbport = 3306;
    dbname = dbname_;
    LogOpen = useLog;
    LogAsynWrite = logAsyn;
    sock_linger = linger;
    trigueMode = trigue_mode_;
    sql_conn_size = sql_cnt;
    thread_size = thread_cnt;
    actor_model = actor_model_; 
    m_threadpoll  = NULL;      
    server_port =  servport;
    timer_slot = TIMESLOT;
    stop_server = false;
    time_out = false;
}
// 初始化线程库
void webserver::init_threadpoll(){
    m_threadpoll = new threadpoll<http>(std::function<void(http*)>(http_work_func),thread_size,10000);
    LOG_INFO("initialize threadpoll function");  
}
// 初始化log
void webserver::init_log(){
    if(LogAsynWrite){
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,800);
    } else{
        info::getInstance()->init(LogOpen,"./ServerLog",800000,2000,0);
    }
    LOG_INFO("initialize log function");   
}
// 初始化mysql 
void webserver::init_mysqlpoll(){
    mysqlpoll::getInstance()->init("localhost",dbusername,dbpassword,dbport,dbname,sql_conn_size);
    LOG_INFO("initialize mysqlpoll function");  
}


// 初始化listen
void webserver::init_listen(){

    //初始化http静态变量
    init_http_static(rootPath);

    LOG_INFO("load user account and password from database")

    // 创建监听socket
    listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);
    // 优雅关闭  断开连接后 发送完数据
    if(sock_linger){
        struct linger tmp = {1, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else {
        struct linger tmp = {0, 1};
        setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server_port);
    
    // 重用端口
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    LOG_INFO("initialize listen socket")

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

    LOG_INFO("initialize epoll fd")

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
    alarm(timer_slot);

    LOG_INFO("initialize pipe and signal")
} 
// 初始化定时器
void webserver::init_timer(){
    timers = new list_timer(TIMESLOT);
    timers->setfunc(timeout_handler);
    LOG_INFO("initialize timer")
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
    timer_node* timer = new timer_node;
    timer->sockfd = fd;
    timer->conn = &http_conns[fd];
    time_t cur = time(NULL);
    timer->expire_time = cur + 3 * TIMESLOT;
    timers->add(timer);
    to_timernode[fd] = timer;
}
// 移除定时器节点
void webserver::remove_timernode(int fd){
    timer_node* timer = to_timernode[fd];
    timers->remove(timer);
    to_timernode[fd] = NULL;
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
            m_threadpoll->put(http_conns+connfd);
            adjust_timernode(to_timernode[connfd]);
        }
        // 失败
        else {
            http_conns[connfd].close_connection();
        }
    }
    // reactor
    else {
        m_threadpoll->put(http_conns+connfd);
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
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        LOG_INFO("%s%d%s","Epoll wait totally ",num," events trigger")

        // 判断每一个事件
        for(int i = 0; i < num; ++i){

            int sockfd = events[i].data.fd;
            //新连接
            if(sockfd == listenfd){
                LOG_INFO("receive a  new conncetion")
                accpet_connection();
            }
            // 错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ){
                LOG_INFO("connection rd hup or error")
                remove_timernode(sockfd);
                http_conns[sockfd].close_connection();
            }
            // 信号
            else if ( (sockfd == pipefd[0]) && (events[i].events & EPOLLIN) ){
                LOG_INFO("signal from pipe")
                bool ret = dealwith_signal();
                if(!ret){
                    LOG_ERROR("%s", "dealsignal failure");
                }
            }
            else if (events[i].events & EPOLLIN){
                LOG_INFO("read event")
                dealwith_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT){
                 LOG_INFO("write event")
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



