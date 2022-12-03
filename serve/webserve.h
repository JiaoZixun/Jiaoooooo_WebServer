#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../myLog/myLog.h"
#include "../threadpool/mythreadpool.h"
#include "../http/http_cond.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int threadNum, bool openLog, int logLevel, int logQueSize
    );

    ~WebServer();
    void Start();

private:
    bool InitSocket_(); 
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);
  
    void DealListen_();
    void DealWrite_(http_cond* client);
    void DealRead_(http_cond* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(http_cond* client);
    void CloseConn_(http_cond* client);

    void OnRead_(http_cond* client);
    void OnWrite_(http_cond* client);
    void OnProcess(http_cond* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    //char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
   
    std::unique_ptr<myThPool> threadpool_;
    std::unique_ptr<epoller> epoller_;
    std::unordered_map<int, http_cond> users_;
};


#endif //WEBSERVER_H