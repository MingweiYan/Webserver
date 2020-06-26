# Webserver

# lock 文件夹

    1. throw 异常
        throw 抛出异常 std::exception() 显示std::exception() 会终止函数运行
        try 运行一个可能抛出异常的函数  catch (){}  可以定义自己的异常类 继承std::exception 然后定义what（）函数返回想输出的字符串 
        在catch里 写自己的异常类对象  然后 通过 对象的what函数输出字符串

# info 文件夹

    blockqueue类

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

        1. 条件变量的使用
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


# mysqlpoll文件夹

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


# threadpoll文件夹

    threadpoll类

        1)  拥有一个模板类型对象的list  
        2)  定义一个function对象  void  func(T*) 类型的对象  即传入对象的工作函数  只需要通过函数设置 模板对象需要执行的工作函数即可
        3)  使用了lock 和 sem 

        API：
            void thread_run_func();
            void set_work_fun(std::function<void (T*)> func);
            threadpoll(std::function<void (T*)> func,int thread_poll_size,int work_list_size);
            ~threadpoll();
            bool get(T* work);  // list是空的 失败
            bool put(T* work);   // list  已满 失败




# tools文件夹

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
            static void sig_handler(int sig);



# timer文件夹

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



# http文件夹
