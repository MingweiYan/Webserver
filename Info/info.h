#ifndef _INFO_H
#define _INFO_H

#include<time.h>
#include<string>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include<memory.h>

#include"../lock/lock.h"
#include"../info/blockqueue.h"



class info{

private:
    // 日志文件相关
    char log_full_name[1024];
    char log_name[128];
    char dir_name[128];
    int cur_file_line;
    int max_file_line;
    int last_time_day;
    // 文件名中计数后缀
    int cur_file_num;
    // 线程同步
    locker m_lock;
    // 写相关
    std::unique_ptr<char[]> write_buf;
    int write_buf_size;
    FILE* fp;
    // 异步写相关
    bool isAsyn;
    std::unique_ptr< blockqueue<std::string> > lines_to_write;
    static void* pthread_init_write(void*);
    void asyn_write(); 
    // 是否使用log
    bool LogOpen;
    // 构造函数
    info();

public:
    static info* getInstance();
    
    ~info();
    bool init(bool LogOpen,std::string file_name,int max_line_perfile,int write_buf_size,int queue_size);
    void flush();
    void write_line(int level, const char* format, ...);
    bool isLogOpen();
};


#define LOG_DEBUG(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(0,format,##__VA_ARGS__); info::getInstance()->flush();}
#define LOG_INFO(format, ...) if(info::getInstance()->isLogOpen()) {info::getInstance()->write_line(1,format,##__VA_ARGS__); info::getInstance()->flush();}
#define LOG_WARN(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(2,format,##__VA_ARGS__); info::getInstance()->flush();}
#define LOG_ERROR(format, ...) if(info::getInstance()->isLogOpen()) { info::getInstance()->write_line(3,format,##__VA_ARGS__); info::getInstance()->flush();}


#endif