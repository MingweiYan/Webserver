#include<list>
#include<string>
#include<mysql/mysql.h>
#include"../lock/lock.h"
#include"../mysqlpoll/mysqlpoll.h"
#include"../info/info.h"

/*
    mysql连接池实现

*/

// 单例模式
mysqlpoll* mysqlpoll::getInstance(){
    static mysqlpoll  instance;
    return &instance;
}
// 初始化
void mysqlpoll::init(std::string host,std::string dbusername,std::string dbpasswd,int port,std::string dbname, int connection_num){
    host = host;
    dbusername = dbusername;
    dbpasswd = dbpasswd;
    port = port;
    dbname = dbname;
    max_size = connection_num;
    for(int i = 0; i<connection_num; ++i){
        MYSQL* con = NULL;
        con = mysql_init(con);
        if(con == NULL){
            LOG_ERROR("MySQL inital error")
            exit(1);
        }
        con = mysql_real_connect(con,host.c_str(),dbname.c_str(),dbpasswd.c_str(),dbname.c_str(),port,NULL,0);
        if(con == NULL){
            LOG_ERROR("MySQL connection error")
            exit(1);
        }
        mysql_list.push_back(con);
    }
    m_sem = sem(connection_num);
}   
// 释放所有链接
void mysqlpoll::destory(){
    m_lock.lock();
    for(MYSQL* conn: mysql_list){
        mysql_close(conn);
    }
    mysql_list.clear();
    m_lock.unlock();
}
// 获取一个MYSQL连接
MYSQL* mysqlpoll::get_connection(){
    m_sem.wait();
    m_lock.lock();

    MYSQL* con = NULL;
    con = mysql_list.front();
    mysql_list.pop_front();
    m_lock.unlock();
    return con;  
}
// 释放一个连接
void mysqlpoll::release_connection(MYSQL* conn){
    if(conn == NULL) return ;
    m_lock.lock();
    mysql_list.push_back(conn);
    m_lock.unlock();
    m_sem.post();
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