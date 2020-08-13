#ifndef _REDISPOLL_H
#define _REDISPOLL_H


#include<bits/stdc++.h>
#include <hiredis/hiredis.h>
#include"../lock/lock.h"
#include"../info/info.h"

/*
    redis连接池
*/
class redisPoll{
private:
    redisPoll() = default;
    std::list<redisContext *> redis_list;
    locker  m_lock;
    sem m_sem;
public:
    redisPoll(redisPoll &) = delete;
    redisPoll& operator = (redisPoll&) = delete;
    
    static redisPoll* getInstance();
    void init(int);
    redisContext* get_connection();
    void release_connection(redisContext *); 
    void destory();
};


/*
    redis连接类
*/

class redisConnection{

private:
    redisContext * conn;
public:
    redisConnection() : conn(redisPoll::getInstance()->get_connection()){}
    ~redisConnection(){
        redisPoll::getInstance()->release_connection(conn);
        conn = NULL;   
    }
    redisContext * get(){ return conn;}
};


/*
    redis客户端类
*/

class redisClient{

public:
    void add_cookie(std::string,std::string);
    bool get_cookie(std::string,std::string&);
    bool verify_cookie(std::string);
};


#endif