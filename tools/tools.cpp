#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include"../tools/tools.h"

// 设置文件描述符非阻塞
void tools::setnonblocking(int fd){
    int pre = fcntl(fd,F_GETFL);
    int cur = pre | O_NONBLOCK;
    fcntl(fd,F_SETFL, cur);
}
// 在epoll中注册事件
void tools::epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET){
    epoll_event event;
    event.data.fd = fd;
    auto tmp = event.events;
    if(read){
        tmp  = EPOLLIN | EPOLLRDHUP;
    } else {
        tmp = EPOLLOUT ;
    }
    if(one_shot){
        tmp |= EPOLLONESHOT;
    }
    if(ET){
        tmp |= EPOLLET; 
    } 
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);   
}
// 信号处理函数
void tools::sig_handler(int sig, int pipefd){
    int pre_erro = errno;
    send(pipefd,(char*)sig,1,0);
    errno = pre_erro;
}