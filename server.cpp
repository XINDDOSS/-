/*
 * 程序名：clinet.cpp，此程序用于演示文件传输的服务端。
*/
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<string>
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
    //接收二进制客户端报文
    bool recv(void*buffer,const size_t size){
      if(::recv(m_clientfd,buffer,size,0)<=0)return false;
      return true;
    }
    //接收报文内容
    bool recvfile(const string &filename,const size_t filesize){
      ofstream fout;
      fout.open(filename,ios::binary);
      if(fout.is_open()==false){
        cout<<"打开文件"<<filename<<"失败"<<endl;
        return false;
      }
      int totalbytes=0;
      int onread=0;
      char buffer[4096];
      while(true){
        if(filesize-totalbytes>4096)onread=4096;
        else onread=filesize-totalbytes;
        if(recv(buffer,onread)==false)return false;
        fout.write(buffer,onread);
        totalbytes+=onread;
        if(filesize==totalbytes)break;
      }
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
ctcpserver tcpserver;
void fathexit(int sig){
  signal(SIGINT,SIG_IGN);
  signal(SIGTERM,SIG_IGN);
  cout<<"父进程退出，sig="<<sig<<endl;
  kill(0,SIGTERM);
  tcpserver.closelisten();
exit(0);
}
void chldexit(int sig){
  signal(SIGINT,SIG_IGN);
  signal(SIGTERM,SIG_IGN);
  cout<<"子进程"<<getpid()<<"退出,sig="<<sig<<endl;
  tcpserver.closeclient();
exit(0);
}
int num=0;
int main(int argc,char*argv[]){
  if(argc!=3){
     cout << "Using:./server 通讯端口 文件存放的目录\n";
    cout << "Example:./server 5005 /tmp\n\n";
    cout << "注意：运行服务端程序的Linux系统的防火墙必须要开通5005端口。\n";
    cout << "      如果是云服务器，还要开通云平台的访问策略。\n\n";
    return -1;
  }
  for(int ii=1;ii<=64;ii++){
    signal(ii,SIG_IGN);
  }
 signal(SIGINT,fathexit);
 signal(SIGTERM,fathexit);
  if(tcpserver.initserver(atoi(argv[1]))==false){
    perror("initserver()");
    return -1;
  }
  while(true){
    num++;
    if(tcpserver.accept()==false){
      perror("accept()");
      return -1;
    }
    int pid=fork();
    if(pid==-1){
      perror("fork()");
      return -1;
    }
    if(pid>0){
      tcpserver.closeclient();
      continue;
    }
   signal(SIGTERM,chldexit);
    signal(SIGINT,SIG_IGN);
    cout<<"客户端已经连接"<<tcpserver.clientip()<<endl;
    struct st_fileinfo{
      char filename[256];
      int filesize;
    }fileinfo;
    memset(&fileinfo,0,sizeof(fileinfo));
    if(tcpserver.recv(&fileinfo,sizeof(fileinfo))==false){
      perror("recv");
      return -1;
    }
    cout<<"文件信息结构体"<<fileinfo.filename<<fileinfo.filesize<<endl;
    if(tcpserver.send("ok")==false){
      perror("send()");
      return -1;
    }
    if(tcpserver.recvfile(string(argv[2])+"/"+"client"+to_string(num),fileinfo.filesize)==false){
      cout<<"接受文件失败"<<endl;
      return -1;
    }
    cout<<"接收文件成功"<<endl;
    tcpserver.send("ok");
    return 0;
  }
}