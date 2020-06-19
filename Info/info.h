#ifndef _INFO_H
#define _INFO_H

#include<time.h>
#include<string>
#include"../lock/lock.h"
#include"../info/blockqueue.h"



class info{

private:
    // 日志文件相关
    char log_full_name[255];
    char log_name[255];
    char dir_name[255];
    int file_number;
    tm* last_time;
    int max_line;
    int cur_line;
    FILE* fp;
    // 线程同步
    locker m_lock;
    // 写缓存
    char* write_buf;
    int write_buf_size;
    // 异步写相关
    bool isAsyn;
    blockqueue<std::string>* messages;
    static void* pthread_init_write(void*);
    void asyn_write(); 
    bool LogOpen;
public:
    static info* getInstance();
    info();
    ~info();
    bool init(bool LogOpen,std::string file_name,int max_line_perfile,int write_buf_size,int queue_size);
    void flush();
    void write_line(int level, const char* format, ...);
    bool isLogOpen();
};


#define LOG_DEBUG(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(0,format,##__VA_ARGS__); }
#define LOG_INFO(format, ...) if(info::getInstance()->isLogOpen()) {info::getInstance()->write_line(1,format,##__VA_ARGS__); }
#define LOG_WARN(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(2,format,##__VA_ARGS__); }
#define LOG_ERROR(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(3,format,##__VA_ARGS__); }


#endif