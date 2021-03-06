#ifndef _MYSQLPOLL_H
#define _MYSQLPOLL_H

#include<list>
#include<string>
#include<mysql/mysql.h>
#include<iostream>

#include"../lock/lock.h"
#include"../info/info.h"

/*
    mysqlpoll  mysql池
*/
class mysqlpoll{

private:
    // 连接池池
    std::list<MYSQL*> mysql_list;
    // 线程同步
    locker m_lock;
    sem m_sem;
    // 数据库参数
    std::string dbhost;
    int dbport;
    std::string dbusername;
    std::string dbpasswd;
    std::string dbname; 
    // 构造函数
    mysqlpoll(){};
public:
    mysqlpoll(mysqlpoll&) = delete;
    mysqlpoll & operator = (mysqlpoll&) = delete;
    
    ~mysqlpoll();
    static mysqlpoll* getInstance();
    MYSQL* get_connection();
    void release_connection(MYSQL*);
    void init(std::string host,std::string dbusername,std::string dbpasswd,int port,std::string dbname, int connection_num);
    void destory();
};
    
/*
    mysqlconnection  mysql资源管理类
*/
class mysqlconnection{

private:
    MYSQL* mysql_;
public:
    mysqlconnection();
    ~mysqlconnection();
    MYSQL* get_mysql();
};






#endif