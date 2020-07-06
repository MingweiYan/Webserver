#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include<exception>
#include<memory>
#include<iostream>

#include"../lock/lock.h"

template<typename T>
class blockqueue{

private:
    // 数组相关
    int cur_size;
    int max_size;
    std::unique_ptr<T[]>res;
    int front;
    int back;
    // 线程同步
    locker m_lock;
    cond m_cond;
public:
    // 构造函数
    blockqueue(int size){
        max_size = size;
        cur_size = 0;
        front = -1;
        back = -1;

        if(size < 0){
            std::cout<<"use negative initialize blockqueue"<<std::endl;
            throw std::exception();
        }
        std::unique_ptr<T[]> p( new T[size] );
        res = std::move(p);
        if(!p.get()){
            std::cout<<" create blockqueue matix failure "<<std::endl;
            throw std::exception();
        }
    }
    // 析构函数
    /*
    ~blockqueue(){
        m_lock.lock();
        if(res){
            delete [] res;
        }
        m_lock.unlock();
    };
    */
    //
    // 在队列尾放置元素
    bool push_back(const T& item){
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
    bool pop_front(T& item){
        m_lock.lock();
        while(cur_size <= 0){
            bool ret = m_cond.wait(m_lock.get());
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
    void clear(){
        m_lock.lock();
        cur_size = 0 ;
        front = -1;
        back = -1;
        m_lock.unlock();
    }
    // 判断是否已满
    bool isFull(){
        m_lock.lock();
        bool flag =  ( cur_size >= max_size);
        m_lock.unlock();
        return flag;
    }
    // 判断是否为空
    bool empty(){
        m_lock.lock();
        bool flag =  ( cur_size == 0);
        m_lock.unlock();
        return flag;
    }
};

#endif