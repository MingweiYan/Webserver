#include<time.h>
#include <sys/time.h>
#include<string>
#include <unistd.h>
#include <stdarg.h>
#include"../lock/lock.h"
#include"../info/blockqueue.h"
#include"../info/info.h"


// 单例模式
info* info::getInstance(){
    static info single;
    return &single;
}
// 构造函数
info::info()
:cur_line(0),file_number(1){
}
// 析构函数
info::~info(){
    if(fp!=NULL){
        fclose(fp);
    }
    if(messages!=NULL){
        delete messages;
    }
    if(write_buf!=NULL){
        delete write_buf;
    }
}
// 初始化
bool info::init(std::string file_name,int max_line_perfile,int buf_size,int queue_size){
    max_line = max_line_perfile;
    write_buf_size = buf_size;
    if(queue_size>0){
        isAsyn = true;
        messages = new blockqueue<std::string>(queue_size); 
        pthread_t id;
        int ret = pthread_create(&id,NULL,pthread_init_write,NULL);
        if(ret!=0){
            throw std::exception();
        }
    }
    write_buf = new char[write_buf_size];
    memset(write_buf,'\0',write_buf_size);
    // time 返回自1970 起的秒数  是无符号数
    // localtime 把time_t 解释为tm 包含时分秒年月日的结构体 
    time_t cur = time(NULL);
    struct tm * now = localtime(&cur);

    // 构造log文件名 并且打开  文件名为  文件夹/year_month_daylog名字
    // flie_name 中 / 最后一次出现的位置 返回是字符串数组包含/
    const char *p = strrchr(file_name.c_str(), '/');
    if (p == NULL) {
        // 如果未找到
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, file_name);
    }
    else{
        // +1 去除 / 得到文件名
        strcpy(log_name, p + 1); 
        // 得到文件夹名
        strncpy(dir_name, file_name.c_str(), p - file_name.c_str() + 1);  
        // 构建完整的文件名
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, log_name);
    }
    last_time = now;
    // 打开文件 
    fp = fopen(log_full_name,"a");
    if(fp==NULL){
        return false;
    }
    return true;
}
// 异步写线程初始化
void* info::pthread_init_write(void*arg){
    info::getInstance()->asyn_write();
}
// 异步写函数
void info::asyn_write(){
    std::string line;
    while(messages->pop_front(line)){
        m_lock.lock();
        fputs(line.c_str(),fp);
        m_lock.unlock();
    }
}
// 同步写函数   
void info::write_line(int level, const char* format, ...){
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);

    time_t cur = time(NULL);
    tm* cur_time = localtime(&cur);

    m_lock.lock();
    if(cur_time->tm_mday!=last_time->tm_mday){
        fflush(fp);
        fclose(fp);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, cur_time->tm_year + 1900, cur_time->tm_mon + 1, cur_time->tm_mday, log_name);
        fp = fopen(log_full_name,"a");
        if(fp!=NULL){
            throw std::exception();
        }
    }
    // 超过一个文件的行数  _1  _2 如此命名
    if(cur_line%max_line==0){
        fflush(fp);
        fclose(fp);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s_%d", dir_name, cur_time->tm_year + 1900, cur_time->tm_mon + 1, cur_time->tm_mday, log_name,file_number);
        ++file_number;
        fp = fopen(log_full_name,"a");
        if(fp!=NULL){
            throw std::exception();
        }
    }
    char log_type[9] = {'\0'};
    // 前缀log分类
    switch (level)
    {
    case 0:
        strcpy(log_type,"[debg]:");
        break; 
    case 1:
        strcpy(log_type,"[info]:");
        break; 
    case 2:
        strcpy(log_type,"[warn]:");
        break; 
    case 3:
        strcpy(log_type,"[erro]:");
        break; 
    default:
        strcpy(log_type,"[info]:");
        break;
    }
    // 可变参数 按格式输出
    va_list valst;
    va_start(valst, format);

    std::string line;

    //写入的具体时间内容格式
    int n = snprintf(write_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     last_time->tm_year + 1900, last_time->tm_mon + 1, last_time->tm_mday,
                     last_time->tm_hour, last_time->tm_min, last_time->tm_sec, now.tv_usec, log_type);
    
    int m = vsnprintf(write_buf + n, write_buf_size-n, format, valst);
    va_end(valst);
    write_buf[n + m] = '\n';
    write_buf[n + m + 1] = '\0';
    line = write_buf;
    m_lock.unlock();
    if(isAsyn){
        messages->push_back(line);
    } else {
        m_lock.lock();
        fputs(line.c_str(),fp);
        m_lock.unlock();
    }
    ++cur_line;
    flush();
}

void info::flush(){
    m_lock.lock();
    fflush(fp);
    m_lock.unlock();
}




