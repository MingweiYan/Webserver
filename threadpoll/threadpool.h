#ifndef _THREAD_POLL_H
#define _THREAD_POLL_H

#include<pthread.h>
#include<list>
#include<functional>
#include<exception>
#include"../lock/lock.h"
#include"../info/info.h"


/*
    threadpoll 类
*/

template<typename T>
class threadpoll{
private:
    // 线程池中的任务
    int max_work_size;
    int cur_work_size;
    std::list<T*> work_list;
    // 线程同步
    locker m_lock;
    sem  m_sem;
    // 线程相关
    std::function<void (T*)> work_func;
    std::unique_ptr<pthread_t[]> pthread_id;
    int thread_size;
public:
     // 构造函数
    threadpoll(std::function<void (T*)> func,int poll_size,int work_list_size){
        thread_size = poll_size;
        max_work_size = work_list_size;
        cur_work_size = 0;
        work_func = func;

        if(work_list_size<=0 || thread_size<=0){
            LOG_ERROR("input incorrect threadpoll size when init threadpoll")
            exit(1);
        }
        // 创建线程ID数组
        std::unique_ptr<pthread_t[]> tmp ( new pthread_t[thread_size] );
        pthread_id = std::move(tmp);
        if(!pthread_id.get()){
            LOG_ERROR("allocate pthread matrix error ")
            throw std::exception();
        } 
        // 创建线程并设置分离态  
        for(int i = 0; i<thread_size; ++i){
            int ret = pthread_create(&pthread_id[i],NULL,thread_init_func,this);
            if(ret!=0){
                LOG_ERROR("threadpoll create thread error")
                exit(1);
            }
            ret = pthread_detach(pthread_id[i]);
            if(ret!=0){
                LOG_ERROR("threadpoll detach  thread error")
                exit(1);
            }
        }
    }
    //线程初始化函数 
    static void* thread_init_func(void* arg){
        threadpoll* curpoll = (threadpoll*)arg;
        curpoll->thread_run_func();
        return curpoll;
    }
    // 线程运行函数
    void thread_run_func(){
        while(true){
            m_sem.wait();
            T* work = NULL;
            if(get(work)){
                work_func(work);
            }
        }
    }
    // 设置模板类元素的工作函数
    void set_work_fun(std::function<void (T*)> func){
        work_func = func;
    }
    // 将一项任务放入工作列表
    bool put(T* work){
        m_lock.lock();
        if(cur_work_size >= max_work_size){
            LOG_ERROR("%s","can not put a work, threadpoll worklist is full ")
            m_lock.unlock();
            return false;
        }
      //  LOG_INFO("%s", work == NULL? "put NULL in the threadpoll":"not put NULL in the threadpoll")
        work_list.push_back(work);
        ++cur_work_size;
        m_lock.unlock();
        m_sem.post();
        return true;
    }
    //从工作列表取出一项任务
    bool get(T* &  work){
        m_lock.lock();
        if(cur_work_size <= 0){
            LOG_ERROR("%s","can not get a work, threadpoll worklist is empty ")
            return false;
            m_lock.unlock();
        }
        work = work_list.front();
        work_list.pop_front();
     //   LOG_INFO("%s", work == NULL? "get NULL in the threadpoll":"not get NULL in the threadpoll")
        --cur_work_size;
        m_lock.unlock();
        return  true;
    }

};

#endif