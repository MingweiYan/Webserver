#include"redispoll.h"


/*
    redis 线程池
*/

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
    m_sem.post();
    m_lock.unlock();
    
}

// 释放所有连接的资源
void redisPoll::destory(){
    for(auto conn: redis_list){
        redisFree(conn);
    }
    redis_list.clear();
}




/*
    redisclient类
*/

// 添加cookie到set中
void redisClient::add_cookie(std::string user,std::string cookie){
    std::string cmd = "HSET cookiesTable ";
    cmd = cmd + user + " " + cookie ;
    redisConnection conn;
    // 添加到哈希表  user -> cookie
    auto reply = (redisReply*)redisCommand(conn.get(),cmd.c_str());
    if(reply == NULL){
        LOG_ERROR("%s","set cookie to redis failure")
        return ;
    }
    if(reply->type != REDIS_REPLY_INTEGER){
        LOG_WARN("%s","set cookie command error")
        return ;
    }
    freeReplyObject(reply);

    // 添加到set
    std::string cmd2 = "SADD cookies ";
    cmd2 += cookie;
    reply = (redisReply*)redisCommand(conn.get(),cmd2.c_str());
    if(reply == NULL){
        LOG_ERROR("%s","set cookie to redis failure")
        return ;
    }
    if(reply->type != REDIS_REPLY_INTEGER){
        LOG_WARN("%s","set cookie command error")
        return ;
    }
}
// 返回用户的cookie
bool redisClient::get_cookie(std::string user,std::string& cookie){
    std::string cmd = "HGET cookiesTable ";
    cmd = cmd + user;
    redisConnection conn;
    auto reply = (redisReply*)redisCommand(conn.get(),cmd.c_str());
    if(reply == NULL){
        LOG_ERROR("%s","get cookie from redis failure")
        return false;
    }
    if(reply->type == REDIS_REPLY_NIL){
        return false;
    } 
    else if(reply->type == REDIS_REPLY_STRING){
        cookie = reply->str;
        return true;
    } else {
        LOG_WARN("%s","wrong redis reply type")
        return false;
    }
}
// 判断cookie是否存在  存在将用户名保存到string
bool redisClient:: verify_cookie(std::string cookie){
    std::string cmd = "SISMEMBER cookies ";
    cmd = cmd + cookie;
    redisConnection conn;
    auto reply = (redisReply*)redisCommand(conn.get(),cmd.c_str());
    if(reply == NULL){
        LOG_ERROR("%s","get cookie to redis failure")
        return false;
    }
    if(reply->type != REDIS_REPLY_INTEGER){
        return false;
    } 
    if(reply->integer !=1){
        return false;
    }
    return true;
} 
