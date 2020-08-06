
#include"../list_timer/list_timer.h"


/*
    构造和析构
*/

// 构造函数
list_timer::list_timer(){timer(); last_time = 0;}
list_timer::list_timer(int slot) :timer(slot),last_time(0){}
list_timer::~list_timer(){}


/*
    定时器节点操作
*/

// 向定时器列表中插入一个定时器节点
void list_timer::add(timer_node* timer){
    if(timer == NULL){
        LOG_WARN("add a NULL timer node to the list_timer");
        return ;
    }
    // 找到第一个大于等于
    auto iter = std::upper_bound(timer_list.begin(),timer_list.end(),timer,
        [](timer_node* left,timer_node* right){return left->expire_time < right->expire_time;});
    // 
    auto pos = timer_list.insert(iter,timer);
    toIter[timer->sockfd] = pos;
} 
// 定时器列表删除一个定时器节点
void list_timer::remove(timer_node* timer){
    auto iter = toIter.find(timer->sockfd);
    if(iter == toIter.end()){
        LOG_WARN("delete a non-exist timer node in the list_timer");
        return;
    } 
    timer_list.erase(iter->second);
    toIter.erase(timer->sockfd);
}
// 调整定时器中的一个节点
void list_timer::adjust(timer_node* timer){
    remove(timer);
    add(timer);
}
// 定时处理函数
void list_timer::tick(){
    time_t now = time(NULL);
    for(auto iter = timer_list.begin(); iter != timer_list.end();){
        timer_node * cur = *iter;
        ++ iter ;
        if( cur->expire_time < now && cur->conn ){
            LOG_INFO("%d%s",cur->sockfd," is timing out")
            execute(cur);
        }
    }
}
// 返回定时时间
int list_timer::compute_slot(){
    time_t now = time(NULL);
    int interval = timer_list.front()->expire_time - now;
    if(interval <= 0) interval = slot();
    interval = interval > slot() ? interval: slot();
    return interval;
}
