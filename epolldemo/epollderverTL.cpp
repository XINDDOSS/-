/*
 * 程序名：tcpepoll2.cpp，边缘触发ET epoll 回声服务端
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

// 把socket设置成非阻塞
int setnonblocking(int fd)
{
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 初始化服务端监听端口
int initserver(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket() failed");
        return -1;
    }
    int opt = 1;
    unsigned int len = sizeof(opt);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, len);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 )
    {
        perror("bind() failed");
        close(sock);
        return -1;
    }
    if (listen(sock, 5) != 0 )
    {
        perror("listen() failed");
        close(sock);
        return -1;
    }
    return sock;
}

int main(int argc,char *argv[])
{
    if (argc != 2)
    {
        printf("usage: ./tcpepoll2 port\n");
        return -1;
    }

    int listensock = initserver(atoi(argv[1]));
    printf("listensock=%d\n", listensock);
    if (listensock < 0)
    {
        printf("initserver() failed.\n");
        return -1;
    }
    setnonblocking(listensock);

    int epollfd = epoll_create(1);
    epoll_event ev;
    ev.data.fd = listensock;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listensock, &ev);

    epoll_event evs[10];
    while(true)
    {
        int infds = epoll_wait(epollfd, evs, 10, -1);
        if(infds < 0)
        {
            perror("epoll_wait failed");
            break;
        }
        if(infds == 0)
        {
            printf("epoll timeout.\n");
            continue;
        }

        for(int ii = 0; ii < infds; ii++)
        {
            if(evs[ii].data.fd == listensock)
            {
                // ET监听fd：循环accept直到EAGAIN
                while(true)
                {
                    struct sockaddr_in client;
                    socklen_t len = sizeof(client);
                    int clientsock = accept(listensock,(struct sockaddr*)&client,&len);
                    if (clientsock < 0)
                    {
                        if(errno == EAGAIN)
                            break;
                        perror("accept error");
                        break;
                    }
                    printf ("accept client(socket=%d) ok.\n", clientsock);
                    setnonblocking(clientsock);
                    ev.data.fd = clientsock;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsock, &ev);
                }
            }
            else
            {
                // ET读事件：while循环recv，读到EAGAIN为止
                char buffer[1024];
                memset(buffer,0,sizeof(buffer));
                int readn;
                char *ptr = buffer;
                while(true)
                {
                    readn = recv(evs[ii].data.fd, ptr, 5, 0);
                    if(readn <= 0)
                    {
                        if(readn < 0 && errno == EAGAIN)
                        {
                            // 数据读完了，回发
                            send(evs[ii].data.fd, buffer, strlen(buffer), 0);
                            printf("recv(eventfd=%d):%s\n", evs[ii].data.fd, buffer);
                        }
                        else
                        {
                            // readn==0 对方关闭 / 其他错误
                            printf("client(eventfd=%d) disconnected.\n", evs[ii].data.fd);
                            close(evs[ii].data.fd);
                        }
                        break;
                    }
                    ptr = ptr + readn;
                }
            }
        }
    }
    close(epollfd);
    close(listensock);
    return 0;
}
