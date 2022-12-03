#ifndef _MYSAFEQUEUE_H_
#define _MYSAFEQUEUE_H_
#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include "../mylocker/mylock.h"

using namespace std;
template<class T>
class mySafaQueue{
    
public:
    mySafaQueue(int max_size = 1000){
        if(max_size < 0){
            max_size = 1000;
        }
        m_start = -1;
        m_end = -1;
        m_size = 0;
        m_max_size = max_size;
        array = new T[max_size];
    }
    ~mySafaQueue(){
        m_mutex.lock();
        if(array != NULL){
            delete []array;
        }
        m_mutex.unlock();
    }
    void clear(){
        m_mutex.lock();
        m_start = -1;
        m_end = -1;
        m_size = 0;
        m_mutex.unlock();
    }
    bool empty(){
        m_mutex.lock();
        int res = 0;
        res = m_size;
        m_mutex.unlock();
        return res == 0;
    }
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    size_t size(){
        size_t res = 0;
        m_mutex.lock();
        res = m_size;
        m_mutex.unlock();
        return res;
    }
    size_t max_size(){
        size_t res = 0;
        m_mutex.lock();
        res = m_max_size;
        m_mutex.unlock();
        return res;
    }
    bool front(T& value){
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return false;
        }
        value = array[m_start];
        m_mutex.unlock();
        return true;
    }
    bool back(T& value){
        m_mutex.lock();
        if(0 == m_size){
            m_mutex.unlock();
            return false;
        }
        value = array[m_end];
        m_mutex.unlock();
        return true;
    }
    bool push(const T& value){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        m_end = (m_end + 1)%m_max_size;
        array[m_end] = value;
        m_size++;
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }
    //弹出的同时取出队头元素 队中没有元素则阻塞
    bool pop(T& value){
        m_mutex.lock();
        while(m_size <= 0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }
        m_start = (m_start + 1) % m_max_size;
        value = array[m_start];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:
    //锁和条件变量
    locker m_mutex;
    cond m_cond;

    int m_start;
    int m_end;
    size_t m_size;
    size_t m_max_size;
    T *array;
};


#endif
