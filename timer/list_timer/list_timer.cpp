
#include"../list_timer/list_timer.h"


/*
    构造和析构
*/

// 构造函数
list_timer::list_timer(){timer();}
list_timer::list_timer(int slot) :timer(slot){}
list_timer::~list_timer(){}


/*
    定时器节点操作
*/

// 向定时器列表中插入一个定时器节点
void list_timer::add(timer_node* timer){
    if(timer == NULL){
        LOG_WARN("add a NULL timer node");
        return ;
    }
    m_lock.lock();
    // 找到第一个大于等于
    auto iter = std::upper_bound(timer_list.begin(),timer_list.end(),timer,
        [](timer_node* left,timer_node* right){return left->expire_time < right->expire_time;});
    auto pos = timer_list.insert(iter,timer);
    toIter[timer->sockfd] = pos;
    m_lock.unlock();
} 
// 定时器列表删除一个定时器节点
void list_timer::remove(timer_node* timer){
    m_lock.lock();
    auto iter = toIter.find(timer->sockfd);
    if(iter == toIter.end()){
        LOG_WARN("delete a non-exist timer node");
        return;
    } 
    timer_list.erase(iter->second);
    toIter.erase(timer->sockfd);
    m_lock.unlock();
}
// 调整定时器中的一个节点
void list_timer::adjust(timer_node* timer){
    remove(timer);
    add(timer);
}
// 定时处理函数
void list_timer::tick(){
    m_lock.lock();
    time_t now = time(NULL);
    for(auto iter = timer_list.begin(); iter!= timer_list.end(); ++iter){
        if( (*iter)->expire_time < now){
            execute(*iter);
        }
    }
    m_lock.unlock();
}