
#include"../tools/tools.h"

int tools::signal_pipefd[2] = {0,0};
int tools::close_pipefd[2] = {0,0};
std::unordered_map<std::string,std::string> tools::md5s ={};

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
// 产生文件的MD5
int tools::compute_MD5(std::string file_name){
    std:: string md5_value;
    std::ifstream file(file_name.c_str(), std::ifstream::binary);
    if (!file){
        std::cerr<<"open file failure";
        return -1;
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    char buf[1024 * 16];
    while (file.good() ) {
        file.read(buf, sizeof(buf));
        MD5_Update(&md5Context, buf, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    char hex[35];
    memset(hex, 0, sizeof(hex));
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(hex + i * 2, "%02x", result[i]);
    }
    hex[32] = '\0';
    md5_value = string(hex);
    tools::md5s.insert({file_name,md5_value});
    return 0;
}
//返回文件的MD5
std::string tools::get_MD5(std::string file_name){
    return tools::md5s[file_name];
}


