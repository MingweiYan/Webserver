#include"../webserver/webserver.h"


// 信号处理函数
void sig_handler(int sig, int pipefd){
    int pre_erro = errno;
    send(pipefd,(char*)sig,1,0);
    errno = pre_erro;
}

// 线程池的工作函数  
void http_work_func(http* conn){
    // reactor
    if(!conn->isProactor()){
        int ret = conn->read_once();
        if(ret){
            conn->process();
        }
        ret = conn->write_to_socket();
    }
    // proactor
    else{
        conn->process();
    }
}
// 构造函数
webserver::webserver(){
    //connections = new http[MAX_FD];
    connections.resize(MAX_FD);
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
    close(m_pipefd[0]);
    free(rootPath);
    delete(timer_);
}
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
    serverport =   servport;
    timer_slot = 5;
}
// 初始化线程库
void webserver::init_threadpoll(){
    threadpoll_ = new threadpoll<http*>(http_work_func,thread_size);
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
    address.sin_famlily = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(server_port);
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);
    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    // LT + LT  LT + ET 
    if(trigueMode == 0 || trigueMode == 1){
        tool.epoll_add(epollfd,listenfd,flase,false);
    }
    http::set_epoll_fd(epollfd);

    ret = socket_pair(PF_UNIX,SOCK_STEREAM,0,pipefd);
    assert(ret != -1 );
    // 写非阻塞
    tool.setnonblocking(pipefd[1]);
    tool.epoll_add(epollfd,pipefd[0],true,false,false);
    tool.set_sigfunc(SIGPIPE,SIG_IGN,false);
    tool.set_sigfunc(SIGALRM,sig_handler,false);
    tool.set_sigfunc(SIGTERM,sig_handler,false);
    alarm(timer_slot);
}   
//重新调整定时器
void webserver::adjust_timernode(timer_node* timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    timer_->adjust(timer);
    LOG_INFO("%s%d", "adjust timer",timer->sockfd);
}
//在接受连接时添加一个定时器
void webserver::add_timernode(int fd){
    timer_node timer;
    timer->sockfd = fd;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    timer_->add(timer);
}




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
        if (http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        
    }
    // ET + LT   ET + ET 
    else{

    }
    
}




