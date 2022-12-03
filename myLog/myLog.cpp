#include "myLog.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>

#include <sys/io.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

bool myLog::init(const char *filename, int close_log, int log_buffer_size, int max_queue_size){
    isOpen_ = true;
    //初始化任务队列
    m_log_queue = new mySafaQueue<string>(max_queue_size);
    //初始化线程池
    int res = init_threadpool(5);
    if(!res){
        return false;
    }
    
    /**/
    //赋值
    this->m_close_log = close_log;
    this->log_buffer_size = log_buffer_size;
    this->m_buf = new char[log_buffer_size];
    memset(this->m_buf, '\0', log_buffer_size);
    //时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    //记录当前时间
    this->m_today = my_tm.tm_mday;
    
    //初始化日志文件夹
    const char *p = strchr(filename, '/');
    if(p == nullptr){
        filename = "./ServerLog";   //重置路径名
    }
    if(access(filename, 0) == -1){  //判断文件夹是否存在
        //不存在则创建
        int flag = mkdir(filename, S_IRWXU);  //Linux创建文件夹
        if (flag != 0) {  //创建失败
            return false;
            //throw std::exception();
        }
    }
    //将目录路径赋值给路径
    strcpy(dir_name, filename);
    //记录日志文件名称
    snprintf(log_name, 255, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
    //生成日志文件路径
    char log_full_name[255] = {0};

    strcat(log_full_name,dir_name);
    strcat(log_full_name,"/");
    strcat(log_full_name,log_name);
    
    //打开日志文件
    m_fp=fopen(log_full_name, "a");
    if(m_fp == NULL){
        return false;
    }
    return true;
}

bool myLog::init_threadpool(int pthreadnums){
    /**/
    workThreads.resize(pthreadnums);
    for(int i=0;i<pthreadnums;i++){
        //创建线程的回调函数必须是静态成员函数，因为若不是静态函数，会将this指针传入不满足void*，静态函数没有this指针
        int res = pthread_create(&workThreads[i], NULL, callback_write, NULL);
        if(res != 0)    return false;
    }
    return true;
}

void myLog::write_log(int level, const char *format, ...){
    
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    
    //生成新文件
    m_mutex.lock();
    m_line++;   //写入新的一行
    //1.判断当前日期是否等于日志记录日期
    if(m_today!=my_tm.tm_mday){
        char new_log[255] = {0};
        char log_full_name[255] = {0};
        fflush(m_fp);   //将缓冲区内容全部写入
        fclose(m_fp);   //关闭原先文件指针
        snprintf(new_log, 255, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);  //文件名
        strcat(log_full_name,dir_name);
        strcat(log_full_name,"/");
        strcat(log_full_name,new_log);
        m_today = my_tm.tm_mday;
        m_line = 0;
        /*
        if(m_today!=my_tm.tm_mday){
            snprintf(new_log, 255, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);  //文件名
            strcat(log_full_name,dir_name);
            strcat(log_full_name,"/");
            strcat(log_full_name,new_log);
            m_today = my_tm.tm_mday;
            m_line = 0;
        }
        
        else{
            snprintf(new_log, 255, "%d_%02d_%02d_%lld", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_line / max_lines);
            strcat(log_full_name,dir_name);
            strcat(log_full_name,"/");
            strcat(log_full_name,new_log);
        }
        */
        m_fp = fopen(log_full_name, "a");
    }
    m_mutex.unlock();

    //根据日志级别构建日志内容
    //日志头
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[Debug]:");
        break;
    case 1:
        strcpy(s, "[Info]:");
        break;
    case 2:
        strcpy(s, "[Warning]:");
        break;
    case 3:
        strcpy(s, "[Error]:");
        break;
    default:
        strcpy(s, "[Info]:");
        break;
    }
    //变长参数宏
    va_list valst;
    va_start(valst, format);//一组参数指向format
    string log_str;

    m_mutex.lock();
    //日志时间
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //将变长参数写入缓存区
    int m = vsnprintf(m_buf + n, log_buffer_size - 1, format, valst);

    m_buf[m + n] = '\n';    //换行符
    m_buf[m + n + 1] = '\0';//字符串结尾符号
    log_str = m_buf;
    m_mutex.unlock();

    //若队列没满放入队列
    if(!m_log_queue->full()){
        m_log_queue->push(log_str); //队列有锁
    }
    va_end(valst);
}

void myLog::flush(void){
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}