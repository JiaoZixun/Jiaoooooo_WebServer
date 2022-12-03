#include "http_cond.h"
using namespace std;

std::atomic<int> http_cond::userCount;

http_cond::http_cond():http_(std::make_shared<http_para>()){
    http_->addr_ = {0};
    http_->fd_ = -1;
    http_->isClose = true;
    http_->read_.wriable=BUFF_SIZE;
    http_->write_.wriable=BUFF_SIZE;
}

http_cond::~http_cond(){
    Close();
}

bool http_cond::init(int fd, const sockaddr_in& addr){
    userCount++;    //用户数++
    http_->fd_=fd;
    http_->addr_=addr;
    http_->isClose=false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", http_->fd_, GetIP(), GetPort(), (int)userCount);
}

void http_cond::Close(){
    if(http_->isClose == false){
        http_->isClose=true;
        userCount--;
        close(http_->fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", http_->fd_, GetIP(), GetPort(), (int)userCount);
    }
}

const char* http_cond::GetIP() const{
    return inet_ntoa(http_->addr_.sin_addr);
}

int http_cond::GetPort() const{
    return http_->addr_.sin_port;
}

int http_cond::Getfd() const{
    return http_->fd_;
}

struct sockaddr_in http_cond::GetAddr() const{
    return http_->addr_;
}

//一次全读ET 不考虑超出长度
bool http_cond::read(int *saveErrno){
    ssize_t len = -1;
    //缓冲区不够直接返回false
    if(http_->read_.wriable <= 0){
        return false;
    }
    int bytes_read = 0;
    while(true){
        // recv(fd, 缓存区开始写位置, 缓存区剩余大小)
        bytes_read = recv(http_->fd_, http_->read_.data_ + http_->read_.writepos, http_->read_.wriable, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                *saveErrno = errno;
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        http_->read_.writepos += bytes_read;
        http_->read_.wriable -= bytes_read;
    }
    return true;
}

bool http_cond::write(int *saveErrno){
    int len = 0;
    while (1)
    {
        len = writev(http_->fd_, http_->iov_, http_->iovCnt_);

        if (len < 0)
        {
            *saveErrno = errno;
            break;
        }
        if(http_->iov_[0].iov_len + http_->iov_[1].iov_len == 0)  break;

        else if(static_cast<size_t>(len) > http_->iov_[0].iov_len) {
            http_->iov_[1].iov_base = (uint8_t*) http_->iov_[1].iov_base + (len - http_->iov_[0].iov_len);
            http_->iov_[1].iov_len -= (len - http_->iov_[0].iov_len);
            if(http_->iov_[0].iov_len) {
                //清空写缓存
                bzero(&http_->write_.data_[0], BUFF_SIZE);
                http_->write_.writepos = 0;
                http_->write_.readpos = 0;
                http_->write_.wriable = BUFF_SIZE;
                http_->write_.readable = 0;
                http_->iov_[0].iov_len = 0;
            }
        }
        else {
            http_->iov_[0].iov_base = (uint8_t*)http_->iov_[0].iov_base + len; 
            http_->iov_[0].iov_len -= len; 
            http_->write_.readable += len;
            http_->write_.wriable -= len;
        }
        if(http_->iov_[0].iov_len + http_->iov_[1].iov_len > 10240)   break;
    }
}

bool http_cond::process(){
    //解析
    MakeResponse(&http_->write_);
    //iov链接写缓存区
    http_->iov_[0].iov_base = http_->write_.data_ + http_->write_.readpos;
    http_->iov_[0].iov_len = http_->write_.readable;
    http_->iov_[1].iov_base = 0;
    http_->iov_[1].iov_len = 0;
    http_->iovCnt_ = 2;
    
}

void http_cond::MakeResponse(struct buffer* buff){
    //写入写缓存区
    int len = 11;
    strcpy(buff->data_, "hello world");
    buff->readable += len;
    buff->writepos += len;
    buff->wriable -= len;

}