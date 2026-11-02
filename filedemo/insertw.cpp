// demo3.cpp，本程序演示了Linux底层文件的操作-创建文件并写入数据。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
int main()
{
int fd;
fd=open("data.txt",O_CREAT|O_RDWR|O_TRUNC,0666);
if(fd==-1){
perror("open(data.txt)");
return -1;
}
printf("文件描述符fd=%d\n",fd);
char buffer[1024];
strcpy(buffer,"我是一个大帅哥。\n");
if(write(fd,buffer,strlen(buffer))==-1)
{
perror("write()");
return -1;
}
printf("写入成功\n");
close(fd);
}
