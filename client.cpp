/* 
 * 程序名：client.cpp，此程序用于演示文件传输的客户端。
*/
#include <iostream>
#include <fstream>
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
   if(readn<=-1){
    buffer.clear();
    return false;
   }
   buffer.resize(readn);
   return true;
  }
  //向服务端发送二进制数据报文
  bool send(void*buffer,const size_t size){
    if(m_clientfd==-1)return false;
    if((::send(m_clientfd,buffer,size,0))<=0)return false;
    return true;
  }
//断开连接
bool close(){
  if(m_clientfd==-1)return false;
  ::close(m_clientfd);
  m_clientfd=-1;
  return true;
}
bool sendfile(const string &filename,const size_t filesize){
  ifstream fin(filename,ios::binary);
  if(fin.is_open()==false){
    cout<<"打开文件"<<filename<<"失败"<<endl;
    return false;
  }
  int onread=0;
  int totalbytes=0;
  char buffer[4096];
  while(true){
    memset(buffer,0,sizeof(buffer));
    if(filesize-totalbytes>4096)onread=4096;
    else onread=filesize-totalbytes;
    fin.read(buffer,onread);
    if(send(buffer,onread)==false)return false;
    totalbytes+=onread;
    if(totalbytes==filesize)break;
  }
  return true;
}
 ~ctcpclient(){close();}
};
int main(int argc,char *argv[])
{
  if (argc!=5)
  {
    cout << "Using:./client 服务端的IP 服务端的端口 文件名 文件大小\n";
    cout << "Example:./client 192.168.101.138 5005 aaa.txt 2424\n\n";
    return -1;
  }
  ctcpclient tcpclient;
  if (tcpclient.connect(argv[1],atoi(argv[2]))==false)  // 向服务端发起连接请求。
  {
    perror("connect()"); return -1;
  }
  //发送文件流程
  struct st_fileinfo{
    char filename[256];
    int filesize;
  }fileinfo;
  memset(&fileinfo,0,sizeof(fileinfo));
  strcpy(fileinfo.filename,argv[3]);
  fileinfo.filesize=atoi(argv[4]);
  if(tcpclient.send(&fileinfo,sizeof(fileinfo))==false){
    perror("send");
    return -1;
  }
  cout<<"发送文件信息的结构体"<<fileinfo.filename<<" 大小:"<<fileinfo.filesize<<endl;
  string buffer;
  if(tcpclient.recv(buffer,2)==false){
    perror("recv()");
    return -1;
  }
  if(buffer!="ok"){
    cout<<"服务器没有回复ok"<<endl;
    return -1;
  }
  cout<<"发送开始"<<endl;
  if(tcpclient.sendfile(fileinfo.filename,fileinfo.filesize)==false){
    perror("sendfile()");
    return -1;
  }
   if(tcpclient.recv(buffer,2)==false){
    perror("recv()");
    return -1;
  }
  if(buffer!="ok"){
    cout<<"发送文件失败"<<endl;
    return -1;
  }
  cout<<"发送文件成功"<<endl;
}