#include<pthread.h>
#include<list>
#include<functional>
#include<exception>
#include"../lock/lock.h"
#include"../threadpoll/threadpool.h"
#include"../info/info.h"

// 构造函数
template<typename T>
threadpoll<T>::threadpoll(std::function<void (T*)> func,int thread_poll_size,int work_list_size = 10000)
: thread_poll_size(thread_poll_size),max_size(work_list_size),cur_size(0),work_func(func){
    if(work_list_size<=0 || thread_poll_size<=0){
        LOG_ERROR("initial incorrect threadpoll size")
        exit(1);
    }
        
    pthread_id = new pthread_t[thread_poll_size];
    if(!pthread_id) 
        throw std::exception();
    for(int i = 0; i<thread_poll_size; ++i){
        int ret = pthread_create(pthread_id+i,NULL,pthread_init,this);
        if(ret!=0){
            delete [] pthread_id;
            LOG_ERROR("threadpoll create error")
            exit(1);
        }
        ret = pthread_detach(pthread_id[i]);
        if(ret!=0){
            delete [] pthread_id;
            LOG_ERROR("threadpoll detach error")
            exit(1);
        }
    }
}
// 析构函数
template<typename T>
threadpoll<T>::~threadpoll(){
    delete [] pthread_id;
}
//线程初始化函数 
template<typename T>
void* threadpoll<T>::thread_init_func(void* arg){
    threadpoll* curpoll = (threadpoll*)this;
    curpoll->thread_run_func();
    return curpoll;
}
// 线程运行函数
template<typename T>
void threadpoll<T>::thread_run_func(){
    while(true){
        m_sem.wait();
        T* work = NULL;
        if(get(work)){
             work_func(work);
        }
    }
}
// 设置模板类元素的工作函数
template<typename T>
void threadpoll<T>::set_work_fun(std::function<void (T*)> func){
    work_func = func;
}
// 将一项任务放入工作列表
template<typename T>
bool threadpoll<T>::put(T* work){
    m_lock.lock();
    if(cur_size>=max_size){
        m_lock.unlock();
        return false;
    }
    work_list.push_back(work);
    ++cur_size;
    m_lock.unlock;
    m_sem.post();
    return true;
}
//从工作列表取出一项任务
template<typename T>
bool threadpoll<T>::get(T* work){
    m_lock.lock();
    if(cur_size<=0){
        return false;
        m_lock.unlock();
    }
    work = work_list.front();
    work_list.pop_front();
    --cur_size;
    m_lock.unlock();
    return  true;
}

