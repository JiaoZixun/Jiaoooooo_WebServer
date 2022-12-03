#ifndef _MYLOG_H_
#define _MYLOG_H_
#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <vector>
#include "mySafeQueue.h"



class myLog{
private:
    //参数
    char dir_name[128];                 //路径
    char log_name[128];                 //日志文件名
    int max_lines;                      //最大行数
    int log_buffer_size;                //缓冲区大小
    long long m_line;                   //当前行数
    int m_today;                        //当前天
    FILE *m_fp;                     //错误日志的指针
    //FILE *m_fp_info;                    //运行日志的指针
    char *m_buf;                        //缓冲区
    mySafaQueue<string> *m_log_queue;   //安全队列
    locker m_mutex;                     //锁
    int m_close_log;                    //关闭标志
    vector<pthread_t> workThreads;    //工作线程组
    bool isOpen_;                       //文件打开

private:
    //单例模式：构造函数和析构函数私有
    myLog(){
        m_line = 0;
    }
    virtual ~myLog(){
        if (m_fp != NULL)
        {
            fclose(m_fp);
        }
    }

    //异步写日志
    void *async_write_log(){
        string log_str;
        while(m_log_queue->pop(log_str)){
            m_mutex.lock();
            fputs(log_str.c_str(), m_fp);
            m_mutex.unlock();
        }
    } 

    //初始化线程池
    bool init_threadpool(int pthreadnums = 5);
    
public:
    static myLog* GetInstance(){
        static myLog log;   //创建静态变量线程安全
        return &log;
    }

    static void *callback_write(void *args){
        myLog::GetInstance()->async_write_log();
    }

    //可选择的参数有日志文件、日志缓冲区大小以及最长日志条队列
    bool init(const char *filename, int close_log, int log_buffer_size = 8192, int max_queue_size = 1000);

    void write_log(int level, const char *format, ...);

    void flush(void);

    bool isOpen(){
        return isOpen_;
    }
};
#define LOG_DEBUG(format, ...) if(myLog::GetInstance()->isOpen()){myLog::GetInstance()->write_log(0, format, ##__VA_ARGS__); myLog::GetInstance()->flush();}
#define LOG_INFO(format, ...) if(myLog::GetInstance()->isOpen()){myLog::GetInstance()->write_log(1, format, ##__VA_ARGS__); myLog::GetInstance()->flush();}
#define LOG_WARNING(format, ...) if(myLog::GetInstance()->isOpen()){myLog::GetInstance()->write_log(2, format, ##__VA_ARGS__); myLog::GetInstance()->flush();}
#define LOG_ERROR(format, ...) if(myLog::GetInstance()->isOpen()){myLog::GetInstance()->write_log(3, format, ##__VA_ARGS__); myLog::GetInstance()->flush();}

#endif