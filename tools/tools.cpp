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
    if(read){
        event.events  = EPOLLIN | EPOLLRDHUP;
    } else {
        event.events = EPOLLOUT ;
    }
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    if(ET){
        event.events |= EPOLLET; 
    } 
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);   
}
// 在epoll中移除fd
void tools::epoll_remove(int epollfd,int fd){
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}
// 在epoll中修改fd
void tools::epoll_mod(int epoolfd,int fd,int ev,bool one_shot,bool ET){
    epoll_event event;
    event.data.fd = fd;
    event.events |= ev;
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    if(ET){
        event.events |= EPOLLET; 
    } 
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event); 
}
// 信号处理函数
void tools::sig_handler(int sig, int pipefd){
    int pre_erro = errno;
    send(pipefd,(char*)sig,1,0);
    errno = pre_erro;
}