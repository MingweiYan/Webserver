#ifndef _TOOLS_H
#define _TOOLS_H


#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
# include <string.h>

class tools{

private:
    static int pipefd[2];

public:
    // epoll相关
    int setnonblocking(int fd);
    void epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET);
    void epoll_remove(int epollfd,int fd);
    void epoll_mod(int epoolfd,int fd,int event,bool one_shot,bool ET);
    // 信号相关
    void set_sigfunc(int sig,void(handler)(int), bool restrart);
    static void sig_handler(int sig);

};


#endif