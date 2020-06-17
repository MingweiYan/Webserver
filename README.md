# Webserver

# threadpoll类
    1)  拥有一个模板类型的list  支持put放置模板类型参数 和 get从模板类list中取出一个对象
    2)  定义一个function对象  void  func(T*) 类型的对象  即传入对象的工作函数  只需要通过函数设置 模板对象需要执行的工作函数即可
    3)  使用了lock 和 sem

    需要利用构造函数来初始化


# mysqlpoll 
    mysqlpoll类
        1)  包含了MYSQL*的 list  getConnection 和 releaseConnection 获取和释放一个MYSQL连接
        2)  使用了 lock 和 sem

    单例模式  需要调用init初始化   mysql连接数应该和线程数一致

    mysqlconnection 实现 RAII 来管理mysql连接
        1)  构造函数来获取一个mysql连接  通过 getmysql来得到底层MYSQL* 指针
        


# info
    blockqueue类
        1) 模板类  使用循环数组来存储模板对象 
        2) 利用 lock 和 cond 来维持线程同步

    info类
        1) 单例模式
        2)  也用到了lock  维护写缓存

    需要 init初始化


# timer
    