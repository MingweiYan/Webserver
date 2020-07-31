#ifndef _REDISPOLL_H
#define _REDISPOLL_H


#include<bits/stdc++.h>
//#include <hiredis/hiredis.h>

#include"../lock/lock.h"
#include"../info/info.h"

class redisPoll{
private:
    redisPoll() = default;
    std::list<redisContext *> redis_list;
    locker  m_lock;
    sem m_sem;
public:
    static redisPoll* getInstance();
    void init(int);
    redisContext* get_connection();
    void release_connection(); 
    void destory();
};


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



#endif