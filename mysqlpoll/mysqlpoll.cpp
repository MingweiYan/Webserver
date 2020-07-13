
#include"../mysqlpoll/mysqlpoll.h"


/*
    mysql连接池实现

*/

mysqlpoll::~mysqlpoll(){
    destory();
}
// 单例模式
mysqlpoll* mysqlpoll::getInstance(){
    static mysqlpoll  instance;
    return &instance;
}
// 初始化
void mysqlpoll::init(std::string host,std::string username,std::string passwd,int port,std::string name, int connection_num){
    // 设置数据库登录信息
    dbhost = host;
    dbusername = username;
    dbpasswd = passwd;
    dbport = port;
    dbname = name;
    // 初始化数据库连接
    for(int i = 0; i < connection_num; ++i){
        MYSQL* con = NULL;
        con = mysql_init(con);
        if(con == NULL){
            LOG_ERROR("MySQL inital error")
            exit(1);
        }
        con = mysql_real_connect(con,dbhost.c_str(),dbusername.c_str(),dbpasswd.c_str(),dbname.c_str(),dbport,NULL,0);
        //LOG_INFO("%s_%s_%s_%s_%d",dbhost.c_str(),dbusername.c_str(),dbpasswd.c_str(),dbname.c_str(),dbport)
        if(con == NULL){
            LOG_ERROR("MySQL connection error")
            exit(1);
        }
        mysql_list.push_back(con);
    }
    m_sem = sem(connection_num);
}   
// 获取一个MYSQL连接
MYSQL* mysqlpoll::get_connection(){
    m_sem.wait();
    m_lock.lock();
    if(mysql_list.empty()){
        return NULL;
    }
    //从list的前面取
    MYSQL* con = mysql_list.front();
    mysql_list.pop_front();
    m_lock.unlock();
    return con;  
}
// 释放一个连接
void mysqlpoll::release_connection(MYSQL* conn){
    if(conn == NULL) return ;
    m_lock.lock();
    // 从list的后面放
    mysql_list.push_back(conn);
    m_lock.unlock();
    m_sem.post();
}
// 释放所有链接 清空list
void mysqlpoll::destory(){
    m_lock.lock();
    for(MYSQL* conn: mysql_list){
        mysql_close(conn);
    }
    mysql_list.clear();
    m_lock.unlock();
}

/*
    mysql 资源管理对象实现

*/

mysqlconnection::mysqlconnection(){
    mysql_ = mysqlpoll::getInstance()->get_connection();
}
mysqlconnection::~mysqlconnection(){
    mysqlpoll::getInstance()->release_connection(mysql_);
}
MYSQL* mysqlconnection::get_mysql(){
    return mysql_;
}