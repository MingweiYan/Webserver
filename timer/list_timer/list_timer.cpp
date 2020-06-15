#include<functional>
#include<unordered_map>
#include<algorithm>
#include<numeric>
#include"../../timer/timer.h"
#include"../list_timer/list_timer.h"

// 构造函数
list_timer::list_timer(){}
list_timer::list_timer(int slot)
:timer(slot){}
// 析构函数
list_timer::~list_timer(){}
// 向定时器列表中插入一个定时器
void list_timer::add(timer_node* timer){
    m_lock.lock();
    auto iter = std::lower_bound(timer_list.begin(),timer_list.end(),timer->expire_time,
        [](timer_node* left,timer_node* right){return left->expire_time < right->expire_time;});
    auto pos = timer_list.insert(iter,timer);
    tab[timer->sockfd] = pos;
    m_lock.unlock();
} 
// 定时器列表删除一个定时器 
void list_timer::remove(timer_node* timer){
    m_lock.lock();
    auto iter = tab.find(timer->sockfd);
    if(iter==tab.end()) return;
    timer_list.erase(iter->second);
    tab.erase(timer->sockfd);
    m_lock.unlock();
}
// 调整定时器中的一个
void list_timer::adjust(timer_node* timer){
    m_lock.lock();
    auto iter = tab.find(timer->sockfd);
    if(iter!=tab.end()){
        timer_list.erase(iter->second);
        tab.erase(timer->sockfd);
    }
    m_lock.unlock();
    add(timer);
}
// 定时处理函数
void list_timer::tick(){
    m_lock.lock();
    time_t now = time(NULL);
    for(auto iter = timer_list.begin(); iter!= timer_list.end(); ++iter){
        if((*iter)->expire_time < now){
            execute(*iter);
        }
    }
    m_lock.unlock();
}