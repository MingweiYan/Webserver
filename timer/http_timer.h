#ifndef _HTTP_TIMER_H
#define _HTTP_TIMER_H

#include<functional>
#include<unistd.h>
#include<time.h>
#include"../http/http.h"

/*
    定时器节点类
*/

class timer_node{
    
public:
    time_t expire_time;
    http* conn;
    int sockfd; 
    void clear(){
        conn = NULL;
        sockfd = 0;
    }
};
/*
    自定义定时器虚基类
*/
class timer{
private:
    // 定时器间隔
    int timer_slot;
    // 超时处理函数
    std::function<void(timer_node*)> func;

public:
    timer(){
        timer_slot = 5;
    };
    timer(int slot):timer_slot(slot){};
    virtual ~timer(){};
    void set_slot(int i) {
        timer_slot = i;
    }
    virtual void add(timer_node*) {};
    virtual void remove(timer_node *) {};
    virtual void adjust(timer_node*){};
    virtual void tick() {};
    void dealwith_alarm(){
         tick(); 
         alarm(slot());
    }
    virtual int slot(){
        return timer_slot;
    }
    // 设置超时处理函数
    void setfunc(std::function<void(timer_node*)> f){
        func = f;
    }
    void execute(timer_node* timer){
    //    LOG_INFO("%s%d", "deal with timeout connection who is ",timer->sockfd)
        func(timer);
    }
};

#endif