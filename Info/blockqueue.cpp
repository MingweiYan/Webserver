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
}
//析构函数
template<typename T>
blockqueue<T>::~blockqueue(){
    delete [] res;
}
// 在队列尾放置元素
template<typename T>
bool blockqueue<T>::push_back(const T& item){
    m_lock.lock();
    if(cur_size>=max_size){
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
    while(cur_size<=0){
        bool ret = m_cond.wait(&m_lock);
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


