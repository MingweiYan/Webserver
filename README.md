# 使用部分


 - 命令行  
    ./server  [-p port]  [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-i close_log] [-a actor_model] 
    ./server -s 20 -t 20 -c 0

    默认 ./server -p 1122 -l 0 -m 1 -o 0 -s 8 -t 8 -i 1 -a 1
    
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
    * -i    关闭日志 0关闭 非0打开
    * -a    并发处理模式  0 是reactor  1是 proactor 

- webbench
    ./webbench -2 -c 10000 -t 5 http://127.0.0.1:1122/

# 测试
 
- 参数测试  

当前软件运行在一个主频3.6G无睿频 2核4线程8G内存 Ubuntu18.04 + mysql5.7的虚拟机

|         | 默认状态|日志异步| LT+LT | ET+LT  | ET+ET| linger | 关闭LOG | reactor | -s -t 6| -s -t 4| -s -t 10| -s -t 12|  
|:-------:|:------:|:----: |:-----: |:-----:|:-----:|:----:  | :----: |:-------:|:------:|:------: |:------:|:------: |
|pages/min|372768  |235956 |411576  |445980 |449232 | 388404 | 735000 | 273648  | 415116 | 437052  | 445308  |75048   |
|bytes/sec|4811665 |3042805|5309680 |5756545|5798240| 5015025|9479800 | 3530612 | 5357420| 5640450 | 5748485 |969060  |
|requests |31063   |19659  |34295   |37165  |37436  | 32363  |61244   |22793    | 34593  | 36421   | 37107   |6254    |
|failed   | 1      |4      |3       |     0 |0      |   4    |6       |11       |       0|       0 |     2   |   0    |



- 最优参数  

    服务端  $ ./server -m 3 -i 0 -s 10 -t 10    

    测试端  $ webbench -c 10000 -t 5 -2 http://127.0.0.1:1122/  

    Webbench - Simple Web Benchmark 1.5
    Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

    Request:
    GET / HTTP/1.1
    User-Agent: WebBench 1.5
    Host: 127.0.0.1
    Connection: close


    Runing info: 10000 clients, running 5 sec.

    Speed=1020060 pages/min, 13172365 bytes/sec.
    Requests: 85005 susceed, 0 failed.
   

- 极限测试 

    服务端 $ ./server -m 3 -i 0 -s 10 -t 10
    测试端 $ webbench -c 13000 -t 5 -2 http://127.0.0.1:1122/

	Webbench - Simple Web Benchmark 1.5
	Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

	Request:
	GET / HTTP/1.1
	User-Agent: WebBench 1.5
	Host: 127.0.0.1
	Connection: close


	Runing info: 13000 clients, running 5 sec.

	Speed=1014804 pages/min, 13099515 bytes/sec.
	Requests: 84567 susceed, 0 failed.



# 依赖关系

              webserver
              /       \
         list_timer    \
             \          \
            http         \
            /   \         \
           /   mysqlpoll threadpoll  
          /        \     /
         /           info
        /             /
       /         blockqueue
      /             /
    tool          lock

    
    编译顺序 g++ 中  A依赖B  A应该在B左边 webserver list_timer http mysqlpoll threadpoll info blockqueue tool lock 



# 具体实现

- lock 文件夹

    + locker 类封装互斥锁
        * bool lock();
        * bool unlock();
        * pthread_mutex_t* get();

    + sem 类封装信号量
        * sem(n);
        * bool wait();
        * bool post();

    + cond 类封装条件变量
        * void signal();
        * void broadcast();
        * bool wait(pthread_mutex_t * locker); 


- tools 文件夹

    + tools类 实现epoll以及信号辅助函数

        * static tools * getInstance();
        * int setnonblocking(int fd);
        * void epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET);
        * void epoll_remove(int epollfd,int fd);
        * void epoll_mod(int epoolfd,int fd,int event,bool one_shot,bool ET);
        * void set_sigfunc(int sig,void(handler)(int), bool restrart);

- info 文件夹

   + blockqueue类 使用数组来存储模板对象 两个位置指针分别指向头尾元素 循环计算下标 实现的是资源同步问题（锁） 同时利用条件变量唤醒

        * blockqueue(int size );
        * bool push_back(const T&); 
            - 当循环数组已满时 返回false
        * bool pop_front(T&);
            - 当条件变量出错时 返回false  
        * void clear();
        * bool isFull();
        * bool empty();


    + info类 单例模式 写一行记录到写缓存然后写入文件
    
        * static info* getInstance();
        * bool init(bool LogOpen,std::string file_name,int max_line_perfile,int write_buf_size,int queue_size);
            - 打开文件出错时返回false
        * void flush();
        * void write_line(int level, const char* format, ...);
        * bool isLogOpen();
        * void directOutput(std::string);


- mysqlpoll 文件夹

    + mysqlpoll类  单例模式 mysql连接池 获取/释放mysql连接

        * static mysqlpoll* getInstance();
        * MYSQL* get_connection();
        * void release_connection(MYSQL*);
        * void init(std::string host,std::string dbusername,std::string dbpasswd,int port,std::string dbname, int connection_num);
        * void destory();

    + mysqlconnection 类  资源管理类 用来管理mysql连接
        
        * mysqlconnection();
        * ~mysqlconnection();
        * MYSQL* get_mysql();


- threadpoll文件夹

    + threadpoll类 模板类 线程池

        * void thread_run_func();
        * void set_work_fun(std::function<void (T*)> func);
        * static void* thread_init_func(void* arg)
        * threadpoll(std::function<void (T*)> func,int thread_poll_size,int work_list_size);
        * ~threadpoll();
        * bool get(T* work);  
            - list空的 失败
        * bool put(T* work);   
            - list已满 失败



- http文件夹

    + http类  解析http请求并进行处理

        * void init(int sockfd,int TriggerMode);
        * void init();
        * friend static void init_http_static(char*);
        * static void set_epoll_fd(int fd);
        * static void set_actor_model(int model);
        * bool read_once();
            - 从socket读取到缓存区，返回false的原因有  缓存区已满，对面关闭了连接 或者出错
        * void process();
        * REQUEST_STATE process_read();
        * LINE_STATE parse_line();
            - 从解析指针开始逐字符解析 直到解析一行或者解析完缓存区
        * char* get_line();
            -  BADREQUEST 可能是处理请求行或者首部行 
        * REQUEST_STATE parse_requestline(char* line);
            - 解析请求行   解析任意一个环节出错返回 BAD——REQUEST 解析完毕则返回 NO_REQUEST
        * REQUEST_STATE parse_header(char* line);
            - 解析首部行  如果GET 解析完毕就返回 GET_REQUEST 否则就是NO_REQUEST
        * REQUEST_STATE parse_content(char*);
        * REQUEST_STATE do_request();
            - // 根据报文请求写文件  只有当写内容出错 或者 传入的请求不对时返回false
        * bool process_write(REQUEST_STATE);
        * bool add_line(const char* format, ...);
        * bool add_statusline(int status,const char*);
        * bool add_header(int);
        * bool add_keepalive();
        * bool add_content_len(int size);
        * bool add_blank_line();
        * bool add_content(const char*);
        * bool write_to_socket();
            - 写一行内容到写缓存，  返回false 可能是 写缓存已满，或者不满但放不下要写的内容
        * void inform_close();
        * void close_connection();
        * bool isProactor();
        * void unmmap();
  
        

- timer文件夹

    + timer_node  定时器节点
         - expire_time 过期时间
         - sockfd 对应的连接套接字描述符
         - http*  指向当前连接的http对象
         - void clear()

    + timer类
        * timer(int slot);
        * virtual ~timer();
        * void set_slot(int)
        * virtual void add(timer_node*) ;
        * virtual void remove(timer_node *) ;
        * virtual void adjust(timer_node*) ;
        * virtual void tick() ;
        * void dealwith_alarm();
        * virtual int slot()
        * void setfunc(std::function<void(timer_node*)> f)
        * void execute(timer_node* timer)

    + list_timer类

        * list_timer();
        * list_timer(int slot);
        * ~list_timer();
        * void add(timer_node* timer);
        * void remove(timer_node* timer);
        * void adjust(timer_node* timer);
        * void tick();



- webserver文件夹

    + webserver类

        * void parse_arg(int argc, char* argv[]);
        * void init_log();
        * void init_threadpoll();
        * void init_mysqlpoll();
        * void init_listen();
        * void init_timer();
        * void printSetting();
        
        * void adjust_timernode(timer_node*);
        * void add_timernode(int fd);
        * void remove_timernode(int);
        * void timeout_handler(timer_node*);

        * void events_loop();
        * bool accpet_connection();
        * bool dealwith_signal();
        * bool dealwith_close();
        * bool dealwith_read(int);
        * bool dealwith_write(int);

        * void http_work_func(http* conn);
        * void close_connection(int fd);

    + 非成员函数
        * void sig_handler(int sig);
        * void show_error(int connfd,const char* info);
        * void init_static_member();
        * void nonmember_timeout_handler (timer_node* cur);
        * void nonmember_http_work_func(http* conn);
        
