#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include"../lock/lock.h"

template<typename T>
class blockqueue{

private:
    // 数组相关
    int cur_size;
    int max_size;
    T* res;
    int front;
    int back;
    // 线程同步
    locker m_lock;
    cond m_cond;
public:
    blockqueue(int size );
    ~blockqueue();
    bool push_back(const T&);
    bool pop_front(T&);
    void clear();
    bool isFull();
    bool empty();
};

#endif