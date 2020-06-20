#ifndef _TIMER_H
#define _TIMER_H

#include<functional>
#include<time.h>
/*
    定时器节点类
*/
struct timer_node{
    timer_node* prev;
    timer_node* next;
    time_t expire_time;
    int sockfd; 
};
/*
    自定义定时器虚基类
*/
class timer{
private:
    // 定时器间隔
    int timer_slot;
    // 留出
    std::function<void(timer_node*)> func;

public:
    timer(int slot = 15):timer_slot(15){};
    virtual ~timer();
    void set_slot(int i) {timer_slot = i;}
    virtual void add(timer_node*) const = 0;
    virtual void remove(timer_node *) const = 0;
    virtual void adjust(timer_node*) const = 0;
    virtual void tick() const = 0;
    void dealwith_alarm(){ tick(); alarm(timer_slot);}
    int slot(){
        return timer_slot;
    }
    void setfunc(std::function<void(timer_node*)> f){
        func = f;
    }
    void execute(timer_node* timer){
        func(timer);
    }
};

#endif