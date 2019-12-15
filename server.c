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
#include <stdlib.h>

int main(int argc,char** argv){
    if(argc <= 1){
        printf("参数错误，./server 端口号,比如./server 5555\n");
        return 0;
    }

    int port = atoi(argv[1]);

    struct sockaddr_in local_address;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(port);
    memset(local_address.sin_zero, 0, sizeof(local_address.sin_zero));

    int fdopt = 1;
    int fd = socket(PF_INET,SOCK_DGRAM,0);
    if(-1 == fd || setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &fdopt, sizeof(int)) || bind(fd, (struct sockaddr*)&local_address, sizeof(local_address))){
        printf("绑定端口错误\n");
        return 1;
    }

    while(1){
        char buffer[1024];
        struct sockaddr_in address;
        socklen_t addlen;
        ssize_t len = recvfrom(fd,buffer,1024,0,(struct sockaddr*)&address,&addlen);
        printf("接到数据len=%d,address=%s,port=%d \n",len,inet_ntoa(address.sin_addr),ntohs(address.sin_port));


        len = sendto(fd,buffer,len,0,(struct sockaddr*)&address,addlen);
        printf("发回数据 %d\n",len);
    }

    return 0;
}
