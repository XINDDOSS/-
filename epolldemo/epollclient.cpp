// 网络通讯的客户端程序。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>

//把socket设置成【非阻塞】
int setnonblocking(int fd)
{
    int flag;
    if((flag=fcntl(fd,F_GETFL,0))==-1)
    {
        flag=0;
    }
    return fcntl(fd,F_SETFL,flag | O_NONBLOCK);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage:./client1 ip port\n"); 
        return -1;
    }

    int sockfd;
    struct sockaddr_in servaddr;
    char buf[1024];

    if ((sockfd=socket(AF_INET,SOCK_STREAM,0))<0) 
    { 
        printf("socket() failed.\n"); 
        return -1; 
    }
    setnonblocking(sockfd);

    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(atoi(argv[2]));
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);

    if (connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr)) != 0)
    {
        if (errno != EINPROGRESS)
        {
            printf("connect(%s:%s) failed.\n",argv[1],argv[2]); 
            close(sockfd);  
            return -1;
        }
    }

    struct pollfd fds;
    fds.fd = sockfd;
    fds.events = POLLOUT;
    poll(&fds, 1, -1);

    if (fds.revents & POLLOUT)
    {
        printf("connect ok.\n");
    }
    else
    {
        printf("connect failed.\n");
        close(sockfd);
        return -1;
    }

    for (int ii=0; ii<200000; ii++)
    {
        memset(buf,0,sizeof(buf));
        printf("please input:"); 
        scanf("%s",buf);

        if (send(sockfd,buf,strlen(buf),0) <=0)
        { 
            printf("write() failed.\n");  
            close(sockfd);  
            return -1;
        }
        
        memset(buf,0,sizeof(buf));
        fds.events = POLLIN;
        int ret = poll(&fds, 1, -1);
        if(ret <=0 || !(fds.revents & POLLIN))
        {
            printf("poll wait read failed.\n");
            close(sockfd);
            return -1;
        }
        int n = recv(sockfd,buf,sizeof(buf)-1,0);
        if (n <=0) 
        { 
            printf("read() failed.\n");
            close(sockfd);
            return -1;
        }
        buf[n] = '\0';
        printf("recv:%s\n",buf);
    }

    close(sockfd);
    return 0;
}
