

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
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
        tmp = EPOLLOUT | EPOLLWRHUP;
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
void tools::sig_handler(int sig, int rdfd,int wtfd){
    int pre_erro = errno;
    send(pipefd,(char*)sig,1,0);
    errno = pre_erro;
}