#include"redispoll.h"

// 单例模式
redisPoll* redisPoll::getInstance(){
    static redisPoll instance;
    return &instance;
}
// 初始化连接
void redisPoll::init(int poll_size){
    const char* ip_addr = "127.0.0.1";
    int port =  6379;
    for(int i = 0; i < poll_size; ++i){
        redisContext* conn =  redisConnect(ip_addr,port);
        // 连接出了问题
        if(conn->err){
            LOG_ERROR("Redis connection error the error info is %s",conn->errstr)
            exit(1); 
        }
        redis_list.push_back(conn);
    }
    m_sem = sem(poll_size);
}
// 获取一个连接
redisContext* redisPoll::get_connection(){
    m_sem.wait();
    m_lock.lock();
    redisContext* conn = redis_list.front();
    redis_list.pop_front();
    m_lock.unlock();
    return conn;
}
// 释放一个连接
void redisPoll::release_connection(redisContext* conn){
    m_lock.lock();
    redis_list.push_back(conn);
    m_lock.unlock();
    m_sem.post();
}

// 释放所有连接的资源
void redisPoll::destory(){
    for(auto conn: redis_list){
        redisFree(conn);
    }
    redis_list.clear();
}
