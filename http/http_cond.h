#ifndef _HTTP_COND_H_
# define _HTTP_COND_H_

#include "../mylocker/mylock.h"
#include "../myLog/myLog.h"

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h> 
#include <thread>
#include <atomic>
#include <unistd.h>
#include <string.h>

class http_cond{
public:
    static const int BUFF_SIZE = 2048;
    static std::atomic<int> userCount;
private:
    //缓冲区结构体：
    //缓冲区数组，已经使用的字节数量
    struct buffer{
        char data_[BUFF_SIZE];
        int wriable;    //剩余可写
        int readable;   //剩余可读
        int writepos;   //可写位置
        int readpos;    //可读位置
    };
    struct http_para{
        int status;//0读 1写
        bool isClose;
        int fd_;
        struct sockaddr_in addr_;
        int iovCnt_;
        struct iovec iov_[2];       //写准备区
        struct buffer read_;
        struct buffer write_;

    };
    std::shared_ptr<http_para> http_;
    
public:
    http_cond();
    ~http_cond();

    bool init(int fd, const sockaddr_in& addr);    //初始化
    void Close();

    bool read(int *saveErrno);    //读
    bool write(int *saveErrno);   //写
    bool process(); //解析
    
    const char* GetIP() const;  //获取ip
    int GetPort() const;    //获取端口号
    int Getfd() const;
    struct sockaddr_in GetAddr() const;

    //生成返回
    void MakeResponse(struct buffer* buff);

    int ToWriteBytes() { 
        return http_->iov_[0].iov_len + http_->iov_[1].iov_len; 
    }
};
#endif