#ifndef _LIST_TIMER_H
#define _LIST_TIMER_H

#include<functional>
#include<list>
#include<unordered_map>
#include<algorithm>
#include<numeric>

#include"../../timer/http_timer.h"
#include"../../lock/lock.h"

class list_timer : public timer{

private:
    std::list<timer_node*> timer_list;
    std::unordered_map<int,decltype(timer_list.begin())> toIter;
    time_t last_time;

public:
    list_timer();
    list_timer(int slot);
    ~list_timer();

    void add(timer_node* timer);
    void remove(timer_node* timer);
    void adjust(timer_node* timer);
    void tick();
    void dealwith_alarm();
};


#endif