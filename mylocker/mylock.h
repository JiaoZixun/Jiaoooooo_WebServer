#ifndef _MYLOCK_H_
#define _MYLOCK_H_
#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem{
private:
    sem_t m_sem;
public:
    //默认构造函数，初始化结果大于0失败
    sem(){
        if(sem_init(&m_sem, 0, 0)!=0){
            throw std::exception();
        }
    }
    //指定信号量初始值
    sem(int num){
        if(sem_init(&m_sem, 0, num)!=0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    //sem_wait阻塞线程直到信号量大于0
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }
    //sem_post 原子操作对信号量+1
    bool post(){
        return sem_post(&m_sem) == 0;
    }
};

class locker{
private:
    pthread_mutex_t m_mutex;

public:
    //默认构造函数，成功返回0
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL)!=0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    //加锁 成功返回0
    bool lock(){
        pthread_mutex_lock(&m_mutex) == 0;
    }
    //解锁
    bool unlock(){
        pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t* get(){
        return &m_mutex;
    }
};

class cond{
private:
    pthread_cond_t m_cond;

public:
    //构造函数 成功返回0
    cond(){
        if(pthread_cond_init(&m_cond, NULL)!=0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    //等待信号量通过 得到通知后自动加锁
    bool wait(pthread_mutex_t *m_mutex){
        int res = 0;
        res = pthread_cond_wait(&m_cond, m_mutex);
        return res == 0;
    }
    //设定等待超时时间
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        int res = 0;
        res = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return res == 0;
    }
    //发送信号量进行通知 每次只唤醒一个等待线程
    bool sigle(){
        pthread_cond_signal(&m_cond) == 0;
    }
    //唤醒全部等待线程
    bool broadcast(){
        pthread_cond_broadcast(&m_cond) == 0;
    }
};



#endif