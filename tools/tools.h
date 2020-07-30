#ifndef _TOOLS_H
#define _TOOLS_H


#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include<assert.h>
#include <fstream>
#include <openssl/md5.h>
#include<unordered_map>
#include<iostream>

class tools{

private:
    static std::unordered_map<std::string,std::string> file_md5s;
    tools(){};
    
public:
    // 单例模式
    static tools * getInstance();
    // epoll相关
    int setnonblocking(int fd);
    void epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET);
    void epoll_remove(int epollfd,int fd);
    void epoll_mod(int epoolfd,int fd,int event,bool one_shot,bool ET);
    // 信号相关
    void set_sigfunc(int sig,void(handler)(int), bool restrart);
    //  传递信号到主线程
    static int signal_pipefd[2];
    static int close_pipefd[2];
    // 产生文件MD5
    int compute_fileMD5(std::string);
    std::string get_fileMD5(std::string);
    bool verify_MD5(const std::string&,const std::string&);
    // 产生字符串
    std::string get_stringMD5(std::string);
};




#endif