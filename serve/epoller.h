#ifndef _EPOLLER_H_
#define _EPOLLER_H_
#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>


class epoller{
private:
    int epollFd_;
    std::vector<struct epoll_event> events_;

public:
    explicit epoller(int MaxEvent = 1024);
    ~epoller();
    //添加fd
    bool AddFd(int fd, uint32_t events);
    //修改fd
    bool ModFd(int fd, uint32_t events);
    //删除fd
    bool DelFd(int fd);
    //等待
    int Wait(int timeoutMs = -1);
    //获取事件fd
    int GetEventFd(size_t i) const;
    //获取事件
    uint32_t GetEvents(size_t i) const;
};


#endif