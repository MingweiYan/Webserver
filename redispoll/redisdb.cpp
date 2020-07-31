#include"redisdb.h"

// 添加cookie到set中
void redisDB::add_cookie(std::string str){
    std::string cmd = "SADD cookies %s";
    redisConnection conn;
    auto reply = (redisReply*)redisCommand(conn.get(),cmd.c_str(),str.c_str());
    if(reply == NULL){
        LOG_WARN("%s","set cookie to redis failure")
        return ;
    }
    freeReplyObject(reply);
}