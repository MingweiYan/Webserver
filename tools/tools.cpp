
#include"../tools/tools.h"

int tools::signal_pipefd[2] = {0,0};
int tools::close_pipefd[2] = {0,0};

// 单例模式
tools* tools::getInstance(){
        static tools instance;
        return &instance;
}
// 设置文件描述符非阻塞并返回之前的描述符选项
int tools::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
// 在epoll中注册事件 可选 读/写  oneshot ET/LT
void tools::epoll_add(int epollfd, int fd,bool read,bool one_shot,bool ET){
    epoll_event event;
    event.data.fd = fd;
    // 读 写
    if(read){
        event.events  = EPOLLIN | EPOLLRDHUP;
    } else {
        event.events = EPOLLOUT | EPOLLRDHUP ;
    }
    // oneshot
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    // ET LT
    if(ET){
        event.events |= EPOLLET; 
    } 
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);   
}
// 在epoll中移除fd
void tools::epoll_remove(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}
// 在epoll中修改fd的事件  可以重新设置 oneshot 和 ET/LT
void tools::epoll_mod(int epollfd,int fd,int ev,bool one_shot,bool ET){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLRDHUP;
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    if(ET){
        event.events |= EPOLLET; 
    } 
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event); 
}
// 设置信号处理函数
void tools::set_sigfunc(int sig,void(handler)(int), bool restrart){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if(restrart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    int ret = sigaction(sig,&sa,NULL);
    assert(ret!=-1);
}

