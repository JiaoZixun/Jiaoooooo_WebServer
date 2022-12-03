#include "epoller.h"


epoller::epoller(int MaxEvent):epollFd_(epoll_create(512)),events_(MaxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);
}

epoller::~epoller(){
    close(epollFd_);
}

bool epoller::AddFd(int fd, uint32_t events){
    if(fd < 0)  return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}