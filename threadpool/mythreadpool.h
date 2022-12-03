#ifndef _MYTHREADPOOL_H_
#define _MYTHREADPOOL_H_

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>


class myThPool{
private:
    using TaskType = std::function<void()>;
    struct pool_para{
        std::queue<TaskType> Tasks;		//任务队列
        std::vector<std::thread> WorkThreads;		//工作线程组，独占指针指向每一个线程
        std::mutex taskmutex;		//互斥锁
        std::condition_variable cond;		//条件变量
        std::atomic<bool> isClose;
    };
    std::shared_ptr<pool_para> pool_;   //智能指针指向线程池参数方便管理
    
public:
    myThPool(int threadnums = 5):pool_(std::make_shared<pool_para>()){
        for(int i=0;i<threadnums;i++){
            pool_->WorkThreads.emplace_back(
                //工作组存入线程的lambel表达式
                //捕获pool参数在表达式中才能使用
                std::thread([pool = pool_]{
                    std::unique_lock<std::mutex>locker(pool->taskmutex);  //加锁
                    while(true){
                        if(!pool->Tasks.empty()){
                            TaskType task = std::move(pool->Tasks.front());
                            pool->Tasks.pop();
                            locker.unlock();
                            task();
                            locker.lock();
                        }
                        else if(pool->isClose){
                            break;
                        }
                        else{
                            pool->cond.wait(locker);
                        }
                    }
                })
            );
        }
    }
    ~myThPool(){
        {
            std::unique_lock<std::mutex>lock(pool_->taskmutex);        //抢到独占锁
            pool_->isClose = true;
        }
        pool_->cond.notify_all();
        for (auto &workthread : pool_->WorkThreads) {
		    workthread.join();//由于线程都开始竞争了，因此必定会执行完，join可等待线程执行完
        }
	}
    //使用完美转发实现添加任务
    template<class F>
    void PushTask(F &&f){
        {
            std::lock_guard<std::mutex>locker(pool_->taskmutex);
            pool_->Tasks.emplace(std::forward<F>(f));
        }
        pool_->cond.notify_one();
    }
    
};
# endif