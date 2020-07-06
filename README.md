# 使用部分


 - 命令行  
    ./server  [-p port]  [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model] 

    * -p    设置服务器端口号   
    * -l    异步写开关 0关闭 非0打开
    * -m    套接字的触发模式
        + 0   listenfd  LT   +    httpfd  LT     
        + 1   listenfd  LT   +    httpfd  ET   
        + 0   listenfd  ET   +    httpfd  ET   
        + 0   listenfd  ET   +    httpfd  ET   
    * -o    优雅关闭 0 关闭 非0打开
    * -s    数据库连接数目
    * -t    线程池连接数目
    * -c    关闭日志 0关闭 非0打开
    * -a    并发处理模式  0 是reactor  1是 proactor 

- webbench
    ./webbench -2 -c 10 -t 30 http://127.0.0.1:9006/

# 依赖关系

    g++ 中  A依赖B  A应该在B左边

    blockqueue 依赖  lock
    info  依赖  blockqueue  lock
    mysqlpoll  依赖  info  和 lock
    threadpoll 依赖 info 和lock
    list_timer 依赖于lock 和 http
    http 依赖 info  mysql 和 tool
    webserver 依赖 所有

    tool  和 lock 不依赖

    编译顺序  webserver list_timer http mysqlpoll threadpoll info blockqueue tool lock 



# 具体实现

- lock 文件夹

    1. throw 异常
        throw 抛出异常 std::exception() 显示std::exception() 会终止函数运行
        try 运行一个可能抛出异常的函数  catch (){}  可以定义自己的异常类 继承std::exception 然后定义what（）函数返回想输出的字符串 
        在catch里 写自己的异常类对象  然后 通过 对象的what函数输出字符串

- info 文件夹

   + blockqueue类

        实现的是资源同步问题（锁） 同时利用条件变量唤醒
        1) 模板类  使用数组来存储模板对象 两个位置指针分别指向头尾元素 循环计算下标
        2) 利用 lock 和 cond 来维持线程同步

        API:
            blockqueue(int size );
            // 当循环数组已满时 返回false
            bool push_back(const T&); 
            // 当条件变量出错时 返回false  
            bool pop_front(T&);
            void clear();
            bool isFull();
            bool empty();

        * 条件变量的使用
            while(cur_size <= 0){
                bool ret = m_cond.wait(&m_lock);
                // 条件变量出错
                if(!ret){
                    return false;
                }
            }
            pop 时
                1)   cur_size = 0  阻塞在wait  push之后broadcast就会继续取出一个元素
                2）  cur_size > 0  不会阻塞 会取出一个元素

    info类
        
        1) 单例模式
        2)  也用到了lock  维护写缓存
        
        
        API :
            static info* getInstance();
            // 打开文件出错时返回false
            bool init(bool LogOpen,std::string file_name,int max_line_perfile,int write_buf_size,int queue_size);
            void flush();
            // 写一行到写缓存
            void write_line(int level, const char* format, ...);
            bool isLogOpen();


## mysqlpoll文件夹

    mysqlpoll类

        1)  包含了MYSQL*的 list  
        2)  使用了 lock 和 sem 

        API:
            static mysqlpoll* getInstance();
            MYSQL* get_connection();
            void release_connection(MYSQL*);
            void init(std::string host,std::string dbusername,std::string dbpasswd,int port,std::string dbname, int connection_num);
            void destory();

    mysqlconnection 类

        实现 RAII 来管理mysql连接
        
        API：
            mysqlconnection();
            ~mysqlconnection();
            MYSQL* get_mysql();


## threadpoll文件夹

    threadpoll类

        1)  拥有一个模板类型对象的list  
        2)  定义一个function对象  void  func(T*) 类型的对象  即传入对象的工作函数  只需要通过函数设置 模板对象需要执行的工作函数即可
        3)  使用了lock 和 sem 

        API：
            void thread_run_func();
            void set_work_fun(std::function<void (T*)> func);
            static void* thread_init_func(void* arg)
            threadpoll(std::function<void (T*)> func,int thread_poll_size,int work_list_size);
            ~threadpoll();
            bool get(T* work);  // list是空的 失败
            bool put(T* work);   // list  已满 失败




## tools文件夹

    tools类

        静态变量pipefd  静态方法 信号处理函数

        API：
            // 返回之前的选项
            int setnonblocking(int fd);
            void epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET);
            void epoll_remove(int epollfd,int fd);
            void epoll_mod(int epoolfd,int fd,int event,bool one_shot,bool ET);
            // 信号相关
            void set_sigfunc(int sig,void(handler)(int), bool restrart);
        



## timer文件夹

    timer_node 包含expire_time  sockfd  和 http* 对象

    timer基类

        接口：
           
            virtual ~timer();
            virtual void add(timer_node*) const = 0;
            virtual void remove(timer_node *) const = 0;
            virtual void adjust(timer_node*) const = 0;
            virtual void tick() const = 0;
            
        实现：

            timer(int slot = 15):timer_slot(slot){};
            void set_slot(int i) {timer_slot = i;}
            void dealwith_alarm(){ tick(); alarm(timer_slot);}
            int slot()

    list_timer类

        实现：
            list存储元素  哈希表定义 fd -> iterator 的映射



## http文件夹

    http类



        API：

            void init(int sockfd,int TriggerMode);
            void init();
            friend static void init_http_static(char*);
            
            void process();
            void close_connection();
            bool isProactor();
            void unmmap();
            // 从socket读取到缓存区，返回false的原因有  缓存区已满，对面关闭了连接 或者出错
            bool read_once();
            // BADREQUEST 可能是处理请求行或者首部行 
            REQUEST_STATE process_read();
            char* get_line();
            // 从解析指针开始逐字符解析 直到解析一行或者解析完缓存区
            LINE_STATE parse_line();
            // 解析请求行   解析任意一个环节出错返回 BAD——REQUEST 解析完毕则返回 NO_REQUEST
            REQUEST_STATE parse_requestline(char* line);
            // 解析首部行  如果GET 解析完毕就返回 GET_REQUEST 否则就是NO_REQUEST
            REQUEST_STATE parse_header(char* line);
            REQUEST_STATE parse_content(char*);
            // 处理得到的请求
            REQUEST_STATE do_request();
            // 根据报文请求写文件  只有当写内容出错 或者 传入的请求不对时返回false
            bool process_write(REQUEST_STATE);
            // 返回false 的情况有  写出错  或者写完之后  返回true的情况有  不需要发送 或者发送完 重新初始化
            bool write_to_socket();
            // 写一行内容到写缓存，  返回false 可能是 写缓存已满，或者不满但放不下要写的内容
            bool add_line(const char* format, ...);
            bool add_statusline(int status,const char*);
            bool add_header(int);
            bool add_keepalive();
            bool add_content_type();
            bool add_content_len(int size);
            bool add_blank_line();
            bool add_content(const char*);
            static void set_epoll_fd(int fd);





## webserver文件夹

    webserver类

        API :
            // 初始化
            void parse_arg(int argc, char* argv[]);
            void init_log();
            void init_threadpoll();
            void init_mysqlpoll();
            void init_listen();
            void init_timer();
            // 定时器相关
            void adjust_timernode(timer_node*);
            void add_timernode(int fd);
            void remove_timernode(int);
            void timer_handler();
            // epollfd相关
            bool accpet_connection();
            bool dealwith_signal();
            bool dealwith_read(int);
            bool dealwith_write(int);
            // 主循环
            void events_loop();