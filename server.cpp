/*
 * 程序名：server.cpp，此程序用于演示封装socket通讯的服务端
*/
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;
class ctcpserver{
    private:
    int m_clientfd;
    int m_listenfd;
string m_clientip;
unsigned short m_port;
    public:
    ctcpserver():m_listenfd(-1),m_clientfd(-1){}
    //初始化服务器
    bool initserver(const unsigned short in_port){
        //创建socket
        if((m_listenfd=socket(AF_INET,SOCK_STREAM,0))==-1)return false;
        m_port=in_port;
        //绑定。
        struct sockaddr_in servaddr;
        memset(&servaddr,0,sizeof(servaddr));
        servaddr.sin_family=AF_INET;
        servaddr.sin_port=htons(m_port);
        servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        if(bind(m_listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr))==-1){
            close(m_listenfd);
            m_listenfd=-1;
            return false;
        }
        //把socket设置为监听状态
        if(listen(m_listenfd,5)==-1){
            close(m_listenfd);
            m_listenfd=-1;
            return false;
        }
        return true;
    }
    //受理连接
    bool accept(){
     struct sockaddr_in caddr;
     socklen_t addrlen=sizeof(caddr);
     if((m_clientfd=::accept(m_listenfd,(struct sockaddr*)&caddr,&addrlen))==-1){
        return false;
     }
     m_clientip=inet_ntoa(caddr.sin_addr);
     return true;
    }
    //获得客户端ip
    const string &clientip()const{
        return m_clientip;
    }
    //发送报文
    bool send(const string &buffer){
        if(m_clientfd==-1)return false;
        if((::send(m_clientfd,buffer.data(),buffer.size(),0))<=0)return false;
        return true;
    }
    //接收报文
    bool recv(string &buffer,const size_t maxlen){
         buffer.clear();
         buffer.resize(maxlen);
         int readn=::recv(m_clientfd,&buffer[0],buffer.size(),0);
         if(readn<=0){
            buffer.clear();
            return false;
         }
         buffer.resize(readn);
         return true;
    }
    //关闭客户端。
    bool closelisten(){
        if(m_listenfd==-1)return false;
        ::close(m_listenfd);
        m_listenfd=-1;
        return true;
    }
    //关闭客户端连的socket.
    bool closeclient(){
        if(m_clientfd==-1)return false;
        ::close(m_clientfd);
        m_clientfd=-1;
        return true;
    }
    ~ctcpserver(){
        closelisten();
        closeclient();
    }
};
int main(int argc,char*argv[]){
 if(argc!=2){
     cout << "Using:./server 通讯端口\nExample:./server 5005\n\n";   // 端口大于1024，不与其它的重复。
    cout << "注意：运行服务端程序的Linux系统的防火墙必须要开通5005端口。\n";
    cout << "      如果是云服务器，还要开通云平台的访问策略。\n\n";
    return -1;
 }
 ctcpserver tcpserver;
 if(tcpserver.initserver(atoi(argv[1]))==false){
    perror("initserver");
    return -1;
 }
 if(tcpserver.accept()==false){
    perror("accept()");
    return -1;
 }
 cout<<"客户端已经连接("<<tcpserver.clientip()<<").\n";
 string buffer;
 while(true){
    if(tcpserver.recv(buffer,1024)==false){
        perror("recv()");
        break;
    }
    cout<<"接收"<<buffer<<endl;
    buffer="ok";
    if(tcpserver.send(buffer)==false){
        perror("send()");
        break;
    }
    cout<<"发送"<<buffer<<endl;
 }
 cout<<"完成服务"<<endl;
}