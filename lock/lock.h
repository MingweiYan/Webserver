#ifndef _LOCK_H
#define _LOCK_H

#include <pthread.h>
#include<semaphore.h>

/*
    locker 互斥锁类
*/
class locker{
public:
    locker();
    ~locker();
    bool lock();
    bool unlock();
private:
    pthread_mutex_t m_mutex;
};
/*
    sem  信号量类
*/

class sem
{
private:
    sem_t m_sem;
public:
    bool wait();
    bool post();
    sem();
    sem(int i);
    ~sem();
};

/*
    cond 条件变量类
*/

class cond
{
private:
    pthread_cond_t my_cond;
public:
    void signal();
    void broadcast();
    bool wait(pthread_mutex_t * locker); 
    cond(/* args */);
    ~cond();
};




#endif