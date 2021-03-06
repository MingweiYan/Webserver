
#include"../info/info.h"


// 单例模式
info* info::getInstance(){
    static info instacne;
    return &instacne;
}
// 构造函数
info::info()
:cur_file_line(1),isAsyn(false),cur_file_num(1){
}
// 析构函数
info::~info(){
    if(fp != NULL){
        fclose(fp);
    }
}
// 初始化
bool info::init(bool LogOpen_,std::string file_name,int maxline_perfile,int buf_size,int queue_size){
    LogOpen = LogOpen_;
    if(!LogOpen) return true;
    max_file_line = maxline_perfile;
    write_buf_size = buf_size;
    // 异步写
    if(queue_size > 0){
        isAsyn = true;
        // 创建阻塞队列
        std::unique_ptr< blockqueue<std::string> > tmp (new blockqueue<std::string>(queue_size) ) ;
        lines_to_write = std::move(tmp); 
        // 异步写线程 并分离线程
        pthread_t tid;
        int ret = pthread_create(&tid,NULL,pthread_init_write,NULL);
        if(ret != 0){
            std::cerr<<" create info asyn pthread failure";
            throw std::exception();
        }
        ret = pthread_detach(tid);
        if(ret != 0){
            std::cerr<<" detach info asyn pthread failure";
            throw std::exception();
        }
    }
    // 创建缓存区
    std::unique_ptr<char[]> tmp(new char[write_buf_size]);
    write_buf = std::move(tmp);
    memset(write_buf.get(),'\0',write_buf_size);
    //
    // time 返回自1970 起的秒数  是无符号数
    // localtime 把time_t 解释为tm 包含时分秒年月日的结构体 
    time_t cur = time(NULL);
    struct tm * now = localtime(&cur);
    // 构造log文件名 并且打开  文件名为  文件夹/year_month_daylog名字
    // flie_name 中 / 最后一次出现的位置 返回是字符串数组包含/  处理存在上次文件夹的情况
    const char *p = strrchr(file_name.c_str(), '/');
    if (p == NULL) {
        // 如果没有文件夹
        snprintf(log_full_name, 1000, "%d_%02d_%02d_%s", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, file_name.c_str());
    }
    else{
        // +1 去除 / 得到文件名
        strcpy(log_name, p + 1); 
        // 得到文件夹名
        strncpy(dir_name, file_name.c_str(), p - file_name.c_str() + 1);  
        // 构建完整的文件名
        snprintf(log_full_name, 1000, "%s%d_%02d_%02d_%s", dir_name, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, log_name);
    }
    last_time_day = now->tm_mday;
    // 打开文件 
    fp = fopen(log_full_name,"a");
    if(fp == NULL){
        std::cerr << " open log file failure";
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
    // 只要blockqueue取出元素  条件变量不出错就一直写
    while(lines_to_write->pop_front(line)){
        m_lock.lock();
        fputs(line.c_str(),fp);
        m_lock.unlock();
    }
    // 异步写失败了  关闭异步写
    std::cerr<<"cond error casuing log asyn write failure";
    isAsyn = false;
}
// 同步写函数   
void info::write_line(int level, const char* format, ...){
    // 获取当前时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t cur = time(NULL);
    tm* cur_time = localtime(&cur);

    m_lock.lock();
    // 上次写文件的时间不是今天
    if(cur_time->tm_mday != last_time_day){
        // 异步写在写新的一行前 先写完所有存在的数据
        while(isAsyn && !lines_to_write->empty()) ;
        fflush(fp);
        fclose(fp);
        // 创建新文件
        snprintf(log_full_name, 1000, "%s%d_%02d_%02d_%s", dir_name, cur_time->tm_year + 1900, cur_time->tm_mon + 1, cur_time->tm_mday, log_name);
        fp = fopen(log_full_name,"a");
        if(fp == NULL){
            std::cerr<<"open log file failure";
            throw std::exception();
        }
    }
    // 超过一个文件的行数  _1  _2 如此命名
    if(cur_file_line % max_file_line == 0){
        fflush(fp);
        fclose(fp);
        snprintf(log_full_name, 1000, "%s%d_%02d_%02d_%s_%d", dir_name, cur_time->tm_year + 1900, cur_time->tm_mon + 1, cur_time->tm_mday, log_name,cur_file_num);
        ++cur_file_num;
        fp = fopen(log_full_name,"a");
        if(fp == NULL){
            std::cerr<<"open log file failure";
            throw std::exception();
        }
    }
    m_lock.unlock();

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
    //写入的具体时间内容格式
    m_lock.lock();
    int n = snprintf(write_buf.get(), 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     cur_time->tm_year + 1900, cur_time->tm_mon + 1, cur_time->tm_mday,
                     cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec, now.tv_usec, log_type);
    
    int m = vsnprintf(write_buf.get() + n, write_buf_size-n, format, valst);
    va_end(valst);

    write_buf[n + m] = '\n';
    write_buf[n + m + 1] = '\0';
    std::string line = write_buf.get();

    if(isAsyn && !lines_to_write->isFull()){
        lines_to_write->push_back(line);
    } else { 
        fputs(line.c_str(),fp);
    }
    ++cur_file_line;
    m_lock.unlock();
}
// 不加日志直接写
void info::directOutput(std::string str){
    m_lock.lock();
    int n = snprintf(write_buf.get(),write_buf_size-2,"%s",str.c_str());
    write_buf[n] = '\n';
    write_buf[n+1] = '\0';
    std::string line = write_buf.get();

    if(isAsyn && !lines_to_write->isFull()){
        lines_to_write->push_back(line);
    } else { 
        fputs(line.c_str(),fp);
    }
    ++cur_file_line;
    m_lock.unlock();

}
// 清空缓存区 直接输出。
void info::flush(){
    m_lock.lock();
    fflush(fp);
    m_lock.unlock();
}
// 判断是否使用log
bool info::isLogOpen(){
    return LogOpen;
}
// 完成关闭日志的工作
void info::close_log(){
    while(LogOpen && isAsyn && !lines_to_write->empty()){
        ;
        // donothing
    }
    flush();
}



