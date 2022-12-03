#include "webserve.h"


WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, 
                    int threadNum, bool openLog, int logLevel, int logQueSize
        ):port_(port),timeoutMS_(timeoutMS),isClose_(false),
        threadpool_(new myThPool(threadNum)),epoller_(new epoller())
{
    http_cond::userCount = 0;

    //InitEventMode_(trigMode);
    //新建监听fd
    if(!InitSocket_()) { isClose_ = true;}

    if(openLog) {
        myLog::GetInstance()->init("./log", 0);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
        }
    }
}

WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
}

void WebServer::Start(){
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                //监听事件
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                //关闭客户端连接
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                //读事件
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                //写事件
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARNING("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(http_cond* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->Getfd());
    epoller_->DelFd(client->Getfd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].Getfd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(http_cond::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARNING("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(http_cond* client) {
    assert(client);
    //ExtentTime_(client);
    threadpool_->PushTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(http_cond* client) {
    assert(client);
    //ExtentTime_(client);
    threadpool_->PushTask(std::bind(&WebServer::OnWrite_, this, client));
}

/*
void WebServer::ExtentTime_(http_cond* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->Getfd(), timeoutMS_); }
}
*/

void WebServer::OnRead_(http_cond* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(http_cond* client) {
    if(client->process()) {
        epoller_->ModFd(client->Getfd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->Getfd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(http_cond* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        OnProcess(client);
        return;
        /*
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
        */
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->Getfd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


