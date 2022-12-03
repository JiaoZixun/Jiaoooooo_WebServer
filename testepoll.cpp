#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<time.h>
#include<iostream>
#include<string>
#include<fstream>


const int MAXEVENTS = 100;

//把socket设置为非阻塞的方式
int setnonblocking(int post);

//初始化服务端的监听端口
int initserver(int port);

//返回首页
void httpResponse_index(int clientsock);

int main(int argc, char *argv[]){
    if(argc!=2){
        std::cout<<"usage:./tcpepoll port"<<std::endl;
        return -1;
    }
    //初始化服务端的监听socket
    int listensock = initserver(atoi(argv[1]));
    
    //设置socket失败
    if(listensock < 0){
        std::cout<<"initserver() failed."<<std::endl;
        return -1;
    }
    std::cout<<"listensock="<<listensock<<std::endl;

    //创建一个epoll描述符
    int epollfd = epoll_create(1);
    

    //添加监听描述符事件
    struct epoll_event ev;
    ev.data.fd = listensock;
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listensock, &ev);

    while(1){
        //存放有事件发生的结构数组
        struct epoll_event events[MAXEVENTS];

        //等待监听的socket有事件发生
        int infds = epoll_wait(epollfd, events, MAXEVENTS, -1);

        //返回失败
        if(infds < 0){
            std::cout<<"epoll_wait() failed."<<std::endl;
            perror("epoll_wait()");
            break;
        }

        //超时
        if(infds == 0){
            std::cout<<"epoll_wait() timeout."<<std::endl;
            continue;
        }

        //遍历有事件发生的结构数组
        for(int i=0;i<infds;i++){
            //接收客户端请求
            char request[1024];
            memset(request, '\0', sizeof(request));
            recv(events[i].data.fd, request, 1024, 0);
            if(strlen(request)){
                printf("%s\n",request);
                printf("successeful!\n");
            }
            
            //如果发生的是listensock，表示有新客户端连接
            if((events->data.fd == listensock) && (events[i].events&EPOLLIN)){
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock, (struct sockaddr*)&client, &len);
                //连接失败
                if(clientsock < 0){
                    std::cout<<"accept() failed."<<std::endl;
                    continue;
                }

                //添加新客户端到epoll中
                memset(&ev, 0, sizeof(struct epoll_event));
                ev.data.fd = clientsock;
                ev.events = EPOLLIN;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsock, &ev);
                std::cout<<time(0)<<": client(socket = "<<clientsock<<
                    ") connected ok."<<std::endl;

                httpResponse_index(clientsock);
                std::cout<<"infds:"<<infds<<std::endl;
                continue;
            }
            //客户端有数据传来或客户端断开连接
            else if(events[i].events & EPOLLIN){
                //申请缓冲区
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));

                //读取客户端数据
                ssize_t isize = read(events[i].data.fd, buffer, sizeof(buffer));

                //如果是客户端断开连接，读到的长度小于等于0
                if(isize <= 0){
                    std::cout<<time(0)<<": client(eventfd = "
                    <<events[i].data.fd<<") disconnected."
                    <<std::endl;

                    //将断开连接的客户端从epoll中删除
                    memset(&ev, 0, sizeof(struct epoll_event));
                    //将该描述符重新置为可读事件
                    ev.events = EPOLLIN;
                    ev.data.fd = events[i].data.fd;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                    close(events[i].data.fd);
                    continue;
                }
                //接收到的数据
                std::cout<<"接收到的数据： ";
                printf("%s\n",buffer);
                //发送数据
                //write(events[i].data.fd, buffer, strlen(buffer));
            }
        }
    }
    close(epollfd);
    return 0;
}

//初始化服务端监听端口
int initserver(int port){
    int sock=socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){
        printf("socket()failed.\n");return -1;
    }

    int opt = 1;
    unsigned int len=sizeof(opt);
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,len);
    setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&opt,len);

    struct sockaddr_in servaddr;
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(port);

    if(bind(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
        printf("bind() failed.\n");close(sock);return -1;
    }

    if(listen(sock,5)!=0){
        printf("listen() failed.\n");close(sock);return -1;
    }
    return sock;
}

//把socket设置为非阻塞的方式
int setnonblocking(int sockfd){
    if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFD,0)|O_NONBLOCK)==1){
        return -1;
    }
    return 0;
}

//返回给浏览器首页html
void httpResponse_index(int clientsock){
    using std::string;
    using std::fstream;
    string rootfile="./root";
    string indexfile = rootfile + "/log.html";
    fstream fs(indexfile);
    if(!fs.is_open()){
        std::cout<<"open file failed."<<std::endl;
        return ;
    }
    //按行读取首页的html
    char temp[1024];
    memset(temp, 0, sizeof(temp));
    //HTTP响应
    char respose[] = "HTTP/1.1 200 ok\r\nconnection: close\r\n\r\n";
    write(clientsock, respose, strlen(respose));
    while (fs.getline(temp, 1024)){
        //std::cout << temp << std::endl;
        //发送给clientsock
        write(clientsock, temp, strlen(temp));
    }
    return ;
}