#include<exception>
#include"../lock/lock.h"
#include"../info/blockqueue.h"

// 构造函数
template<typename T>
blockqueue<T>::blockqueue(int size )
: max_size(size),cur_size(0),front(-1),back(-1){
    if(size<0){
        throw std::exception();
    }
    res = new T[size];
    if(!res){
        throw std::exception();
    }
    cur_size = 0;
    max_size = size;
    front = -1;
    back = -1;
}
//析构函数
template<typename T>
blockqueue<T>::~blockqueue(){
    m_lock.lock();
    if(res){
        delete [] res;
    }
    m_lock.unlock();
}
// 在队列尾放置元素
template<typename T>
bool blockqueue<T>::push_back(const T& item){
    m_lock.lock();
    if(cur_size>=max_size){
        m_cond.broadcast();
        m_lock.unlock();
        return false;
    }
    ++back;
    back = back % max_size;
    res[back] = item;
    ++cur_size;
    m_lock.unlock();
    m_cond.broadcast();
    return true;
}
// 在队列首取走元素
template<typename T>
bool blockqueue<T>::pop_front(T& item){
    m_lock.lock();
    while(cur_size <= 0){
        bool ret = m_cond.wait(&m_lock);
        // 条件变量出错
        if(!ret){
            return false;
        }
    }
    ++front;
    front = front%max_size;
    item = res[front] ;
    --cur_size;
    m_lock.unlock();
    return true;
}
// 清空阻塞队列
template<typename T>
void  blockqueue<T>::clear(){
    m_lock.lock();
    cur_size = 0 ;
    front = -1;
    back = -1;
    m_lock.unlock();
}
// 判断是否已满
template<typename T>
bool blockqueue<T>::isFull(){
    m_lock.lock();
    bool flag =  ( cur_size >= max_size);
    m_lock.unlock();
    return flag;
}
