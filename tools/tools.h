#ifndef _TOOLS_H
#define _TOOLS_H



class tools{

public:
    // epoll相关
    void setnonblocking(int fd);
    void epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET);
    void epoll_remove(int epollfd,int fd);
    // 信号相关
    void sig_handler(int sig,int infd);
    void set_sigfunc(int sig,void(handler)(int), bool restrart);
};


#endif