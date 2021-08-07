#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<iostream>
#include<poll.h>
#include<errno.h>
#include<vector>
#include<fcntl.h>
#include<sys/epoll.h>
#include<unistd.h>

int main()
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
    {
        std::cout << "create listen socket error" << std::endl;
        return -1;
    }

    // 端口号和ip地址重用
    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(char*)&on, sizeof(on));
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT,(char*)&on, sizeof(on));

    // 设置监听为非阻塞
    int oldSocketFlag = fcntl(listenfd, F_GETFL, 0);
    int newSocketFlag = oldSocketFlag | o_NONBLOCK;
    if(fcntl(listenfd, F_SETFL, newSocketFlag) == -1)
    {
        close(listenfd);
        std::cout<<"set listenfd to nonblock error"<<std::endl;
        return -1;
    }

    sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port =htons(3000);

    if(bind(listenfd,(sockaddr*)&bindaddr, sizeof(sockaddr_in)) == -1)
    {
        std::cout<<"bind listen socker error" << std::endl;
        close(listenfd);
        return -1;
    }


    if(listen(listenfd, 5) == -1)
    {
        std::cout<< "listen error"<< std::endl;
        close(listenfd);
        return -1;
    }

    // 创建epoll
    int epollfd = epoll_create(1);
    if(epollfd == -1)
    {
        std::cout<<"create epollfd error"<<std::endl;
        close(listenfd);
        return -1;
    }

    epoll_event listen_fd_event;
    listen_fd_event.data.fd = listenfd;
    listen_fd_event.events = EPOLLIN;

    // 设置为边缘模式 默认为水平模式
    //listen_fd_event.events |= EPOLLET;

    if(epoll_ctl(epollfd, EPOLL_CYL_ADD, listenfd, &listen_fd_event) == -1)
    {
        std::cout<<"epoll_ctl error"<<std::endl;
        close(listenfd);
        return -1;
    }

    int n;
    while (true)
    {
        epoll_event epoll_evevts[1024];
        n = epoll_wait(epollfd, epoll_evevts, 1024, 1000);
        if(n < 0)
        {
            // 被信号中断
            if(errno == ETNTR)
            {
                continue;
            }

            std::cout<< "epoll wait error"<< std::endl;
            break;
        }
        else if(n == 0)
        {
            // 超时
            continue;
        }

        // 有消息到来
        for(size_t i = 0; i < n; ++i)
        {
            // 可读事件
            if(epoll_evevts[i].events & EPOLLIN)
            {
                // 有可接受的客户端连接
                if(epoll_evevts[i].data.fd == listenfd)
                {
                    sockaddr_in clientaddr;
                    socklen_t clientaddrlen = sizeof(clientaddr);
                    int clientfd = accept(listenfd, (sockaddr*)&clientaddr, &clientaddrlen);
                    if(clientfd != -1)
                    {
                        // 设置非阻塞
                        int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
                        int newSockFlag = oldSocketFlag | O_NONBLOCK;
                        if(fcntl(clientfd, F_SETFL, newSocketFlag) != -1)
                        {
                            std::cout<< "new client accepted, clientfd: "<<clientfd<<std::endl;
                        }
                        else
                        {
                            std::cout<<"add client fd to epollfd error"<<std::endl;
                            close(clientfd);
                        }
                    }
                }
                // 普通可读
                else
                {
                    char ch;
                    int m = recv(epoll_evevts[i].data.fd, &ch, 1, 0)
                    if(m == 0)
                    {
                        // 对端关闭了连接
                        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_evevts[i].data.fd, NULL) != -1)
                        {
                            std::cout<< "client disconnect clientfd: "<< epoll_evevts[i].data.fd<< std::endl;
                        }

                        close(epoll_evevts[i].data.fd);
                    }
                    else if(m < 0)
                    {
                        // 出错
                        if(errno != EWOULDBLOCK && errno != EINTR)
                        {
                            if(epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_evevts[i].data.fd, NULL) != -1)
                            {
                                std::cout<< "client disconnect clientfd: "<< epoll_evevts[i].data.fd<< std::endl;
                            }

                            close(epoll_evevts[i].data.fd);
                        }
                    }
                    else
                    {
                        // 正常收到数据
                        std::cout<<"recf from clientfd: "<< epoll_evevts[i].data.fd<< ", "<<ch<<std::endl;
                    }
                }

            }
            else
            {
                std::cout <<"error event"<< std::endl;
            }
        }
    }
    
    close(listenfd);
    return 0;
}