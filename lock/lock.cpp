#include <pthread.h>
#include<semaphore.h>
#include<exception>
#include"lock.h"
/*
    locker 
*/
locker::locker(){
    if(pthread_mutex_init(&m_mutex,NULL)!=0){
        throw std::exception();
    }
}

locker::~locker(){
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock(){
    int ret = pthread_mutex_lock(&m_mutex);
    return ret == 0;
}

bool locker::unlock(){
    int ret = pthread_mutex_unlock(&m_mutex);
    return ret == 0;
}

pthread_mutex_t* locker::get(){
    return &m_mutex;
} 

/*
    sem
*/
sem::sem(){
    if(sem_init(&m_sem,0,0)!=0){
        throw std::exception();
    }
}
sem::sem(int i){
    if(i<0) throw std::exception();
    if(sem_init(&m_sem,0,i)!=0){
        throw std::exception();
    }
}
sem::~sem(){
    sem_destroy(&m_sem);
}
bool sem::wait(){
    int ret = sem_wait(&m_sem);
    return ret == 0;
}
bool sem::post(){
    int ret = sem_post(&m_sem);
    return ret == 0;
}

/*
    cond
*/
cond::cond(){
    if(pthread_cond_init(&my_cond,NULL)!=0){
        throw std::exception();
    }
}
cond::~cond(){
    pthread_cond_destroy(&my_cond);
}
bool cond::wait(pthread_mutex_t* lock){
   int ret = pthread_cond_wait(&my_cond,lock);
   return ret == 0;
}
void cond::signal(){
    pthread_cond_signal(&my_cond);
}
void cond::broadcast(){
    pthread_cond_broadcast(&my_cond);
}