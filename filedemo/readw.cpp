// readw.cpp，本程序演示了Linux底层文件的操作-读取文件。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;
int main(){
    int fd;
    fd = open("data.txt", O_RDONLY);
    if(fd == -1){
        perror("open(data.txt)");
        return -1;
    }
    printf("文件描述符fd=%d\n", fd);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    if(read(fd, buffer, sizeof(buffer)-1) == -1){
        perror("read");
        return -1;
    }
    printf("%s", buffer);
    close(fd);
    return 0;
}