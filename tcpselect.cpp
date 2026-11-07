/* 
/*
 程序名：tcpselect.cpp，此程序用于演示采用select模型实现网络通讯的服务端。
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
//
int initserver(int port){
  int sock=socket(AF_INET,SOCK_STREAM,0);
if(sock<0){
    perror("socket()failed");
    return -1;
}
int opt=1;
unsigned int len =sizeof(opt);
setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,len);
struct sockaddr_in servaddr;
servaddr.sin_family=AF_INET;
servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
servaddr.sin_port=htons(port);
if(bind(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
    perror("bind()failed");
    close(sock);
    return -1;
}
if(listen(sock,5)!=0){
    perror("listen()failed");
    close(sock);
    return -1;
}
return sock;
}
int main(int argc,char*argv[]){
    if(argc!=2){
        printf("usage: ./tcpselect port\n");
         return -1; 
        }
    int listensock=initserver(atoi(argv[1]));
    printf("listensock=%d\n",listensock);
    if(listensock<0){
         printf("initserver() failed.\n"); 
         return -1; 
    }
    fd_set readfds;//需要监视的socket合集，大小为16字节；
    FD_ZERO(&readfds);//初始化，把每一位变成0
    FD_SET(listensock,&readfds);//监听的socket加入readfds
    int maxfd=listensock;
    while(true){
        //用于表示超时时间的结构体
        struct timeval timeout;
       timeout.tv_sec=5; //秒
       timeout.tv_usec=0; //微秒
       fd_set tmpfds=readfds;//select()函数中，会修改bitmap，所以，要把readfds复制一份给tmpfds，再把tmpfds传给select()。
       //调用select()等待事件发生
       int infds=select(maxfd+1,&tmpfds,NULL,NULL,&timeout);
       if(infds<0){
        perror("select()failed");
        break;
       }   
       if(infds==0){
        printf("select()timeout\n");
        break;
       }
       for(int eventfd=0;eventfd<=maxfd;eventfd++){
        if(FD_ISSET(eventfd,&tmpfds)==0){
            continue;
        }
        //说明有新顾客连接
        if(eventfd==listensock){
            struct sockaddr_in client;
            socklen_t len=sizeof(client);
            int clientsock=accept(listensock,(struct sockaddr*)&client,&len);
            if(clientsock<0){
                perror("accept()failed");
                continue;
            }
            printf("accept client(socket=%d)\n",clientsock);
            FD_SET(clientsock,&readfds);
            if(maxfd<clientsock)maxfd=clientsock;
        }else{
            //如果已经连接
            char buffer[1024];
            memset(buffer,0,sizeof(buffer));
            if(recv(eventfd,buffer,sizeof(buffer),0)<=0){
                printf("client(eventfd=%d)disconnect\n",eventfd);
                close(eventfd);
                FD_CLR(eventfd,&readfds);
                if(eventfd==maxfd){
                    for(int ii=maxfd;ii>0;ii--){
                        if(FD_ISSET(ii,&readfds)){
                            maxfd=ii;
                            break;
                        }
                    }

                }
            }
            else{
                printf("recv(eventfd=%d):%s\n",eventfd,buffer);
                send(eventfd,buffer,strlen(buffer),0);
            }
        }
       }
    }
    return 0;
}