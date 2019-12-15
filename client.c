/**
  * udp攻击测试服务器端，接到数据包直接返回给客户端一个数据包
  *
  */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define PORT 5555

int main(int argc,char** argv){
    struct sockaddr_in local_address;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(PORT);
    memset(local_address.sin_zero, 0, sizeof(local_address.sin_zero));

    int fd = socket(PF_INET,SOCK_DGRAM,0);
    if(-1 == fd ){
        return 1;
    }

    while(1){

        char buffer[1024];
        struct sockaddr_in address;
        inet_aton("192.168.0.15",&address.sin_addr);
        address.sin_family = AF_INET;
        address.sin_port = htons(PORT);
        memset(address.sin_zero,0,sizeof(address.sin_zero));

        socklen_t addlen = sizeof(address);
        ssize_t len = sendto(fd,"hello world",11,0,(struct sockaddr*)&address,addlen);
        printf("发送数据 %d\n",len);

        len = recvfrom(fd,buffer,1024,0,(struct sockaddr*)&address,&addlen);
        printf("接到数据len=%d,address=%s,port=%d \n",len,inet_ntoa(address.sin_addr),ntohs(address.sin_port));

        break;
    }

    return 0;
}
