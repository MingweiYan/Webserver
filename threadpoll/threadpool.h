#ifndef _THREAD_POLL_H
#define _THREAD_POLL_H

#include<pthread.h>
#include<list>
#include<functional>
#include"../lock/lock.h"


/*
    threadpoll 类
*/

template<typename T>
class threadpoll{

private:
    // 线程池中的任务
    int max_size;
    int cur_size;
    std::list<T*> work_list;
    // 线程同步
    locker m_lock;
    sem  m_sem;
    // 线程相关
    std::function<void (T*)> work_func;
    pthread_t* pthread_id;
    int thread_poll_size;
    static void* thread_init_func(void * arg);
public:
    void thread_run_func();
    threadpoll(std::function<void (T*)> func,int thread_poll_size,int work_list_size);
    ~threadpoll();
    bool get(T* work);
    bool put(T* work);

};

#endif