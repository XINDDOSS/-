/*
 * 程序名：demo7.cpp，此程序用于演示封装socket通讯的客户端
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
class ctcpclient //TCP通讯
{
  private:
   int m_clientfd;   //-1表示断开，》=0表示有效
   string m_ip; //ip
   unsigned short m_port;// 端口
  public:
   ctcpclient():m_clientfd(-1){}

   //向服务端发起请求
  bool connect(const string&in_ip,const unsigned short in_port){
    m_ip=in_ip;m_port=in_port;
    //步骤1：创建客户端socket。
    if((m_clientfd=socket(AF_INET,SOCK_STREAM,0))==-1)return false;
    //步骤2:向服务器发起请求。
    struct sockaddr_in servaddr; //存放协议，Ip，端口。
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(m_port);
    struct hostent*h;//大端序
    if((h=gethostbyname(m_ip.c_str()))==0){
      ::close(m_clientfd);
      m_clientfd=-1;
      return false;
    }
    memcpy(&servaddr.sin_addr,h->h_addr,h->h_length);//指定服务器ip
  if(::connect(m_clientfd,(struct sockaddr*)&servaddr,sizeof(servaddr))==-1){
    ::close(m_clientfd);
    m_clientfd=-1;
    return false;
  }
  return true;
}
  //向服务器发送报文。
  bool send(const string &buffer){
    if(m_clientfd==-1)return false;
    if((::send(m_clientfd,buffer.data(),buffer.size(),0))<=0)return false;
    return true;
  }
  //接受服务器报文
  //buffer放内容，maxlen放报文最大长度
  bool recv(string &buffer,const size_t maxlen){
    buffer.clear();
   buffer.resize(maxlen);
   int readn=::recv(m_clientfd,&buffer[0],buffer.size(),0);
   if(readn==-1){
    buffer.clear();
    return false;
   }
   buffer.resize(readn);
   return true;
  }
//断开连接
bool close(){
  if(m_clientfd==-1)return false;
  ::close(m_clientfd);
  m_clientfd=-1;
  return true;
}

 ~ctcpclient(){close();}
};
int main(int argc,char*argv[]){
  if(argc!=3){
    cout<<"Using:./client 服务端的IP 服务端的端口\nExample:./client 127.0.0.1 5005\n\n";
    return -1;
  }
  ctcpclient tcpclient;
  if(tcpclient.connect(argv[1],atoi(argv[2]))==false)//发起连接请求
  {
    perror("connect()");
    return -1;
  }
   string buffer;
   for(int ii=0;ii<10;ii++){
    buffer="这是第"+to_string(ii+1)+"个超级女生，编号"+to_string(ii+1)+"。";
    if(tcpclient.send(buffer)==false){
      perror("send");
      break;
    }
    cout<<"发送"<<buffer<<endl;
    //接受
    if(tcpclient.recv(buffer,1024)==false){
      perror("recv");
      break;
    }
    cout<<"接收"<<buffer<<endl;
    sleep(1);
   }
}