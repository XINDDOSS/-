/*
 * 程序名：tcpepoll.cpp，此程序用于演示采用epoll模型实现网络通讯的服务端。
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

// 初始化服务端的监听端口。
int initserver(int port)
{
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0)
    {
        perror("socket() failed"); 
        return -1;
    }

    int opt = 1;
    unsigned int len = sizeof(opt);
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,len);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 )
    {
        perror("bind() failed"); 
        close(sock); 
        return -1;
    }

    if (listen(sock,5) != 0 )
    {
        perror("listen() failed"); 
        close(sock); 
        return -1;
    }

    return sock;
}

int main(int argc,char*argv[])
{
    if(argc!=2){
        printf("usage: ./tcpepoll port\n");
        return -1; 
    }

    int listensock = initserver(atoi(argv[1]));
    printf("listensock=%d\n",listensock);

    if (listensock < 0) { 
        printf("initserver() failed.\n"); 
        return -1; 
    }

    int epollfd=epoll_create(1);
    epoll_event ev;
    ev.data.fd=listensock;
    ev.events=EPOLLIN;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,listensock,&ev);

    epoll_event evs[10];
    while(true)
    {
        int infds=epoll_wait(epollfd,evs,10,-1);
        if(infds<0){
           perror("epoll_wait() failed"); 
           break;
        }
        if(infds==0){
            printf("epoll timeout.\n"); 
            continue;
        }

        for(int ii=0;ii<infds;ii++)
        {
            if (evs[ii].data.fd == listensock)
            {
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock,(struct sockaddr*)&client,&len);
                if(clientsock < 0)
                {
                    perror("accept failed");
                    continue;
                }
                printf ("accept client(socket=%d) ok.\n",clientsock);

                ev.data.fd=clientsock;
                ev.events=EPOLLIN;
                epoll_ctl(epollfd,EPOLL_CTL_ADD,clientsock,&ev);
            }
            else
            {
                char buffer[1024];
                memset(buffer,0,sizeof(buffer));
                int ret = recv(evs[ii].data.fd,buffer,sizeof(buffer),0);
                if (ret <= 0)
                {
                    if(ret == 0)
                        printf("client(eventfd=%d) disconnected(graceful).\n",evs[ii].data.fd);
                    else
                        perror("recv error");
                    close(evs[ii].data.fd);
                } 
                else 
                {
                    printf("recv(eventfd=%d):%s\n",evs[ii].data.fd,buffer);
                    send(evs[ii].data.fd,buffer,strlen(buffer),0);
                }
            }
        }
    }
    close(epollfd);
    close(listensock);
    return 0;
}
