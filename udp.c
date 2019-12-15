//
//  udp.c
//  dns
//
//  Created by 闫久俊 on 2019/11/30.
//  Copyright © 2019 闫久俊. All rights reserved.
//
#define __FAVOR_BSD

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>

uint16_t checksum_tcpudp(struct ip *iph, void *buff, uint16_t data_len, int len)
{
    const uint16_t *buf = buff;
    uint32_t ip_src = iph->ip_src.s_addr;
    uint32_t ip_dst = iph->ip_dst.s_addr;
    uint32_t sum = 0;
    int length = len;
    
    while (len > 1)
    {
        sum += *buf;
        buf++;
        len -= 2;
    }

    if (len == 1)
        sum += *((uint8_t *) buf);

    sum += (ip_src >> 16) & 0xFFFF;
    sum += ip_src & 0xFFFF;
    sum += (ip_dst >> 16) & 0xFFFF;
    sum += ip_dst & 0xFFFF;
    sum += htons(iph->ip_p);
    sum += data_len;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ((uint16_t) (~sum));
}

static void checksum_ip(struct ip* ip){
    ip->ip_sum = 0; // 先把校验和设置为0
    
    // 然后把整个数据看作2个字节的序列，逐个相加，如果最后还剩一个，也直接加上去。
    // 但是，ip头的长度一定是4字节的倍数，不满4字节的，会补零。即使原始头部长度是
    // 奇数，我们还是可以按照4的倍数长度来计算头部校验和。顶多最后面多加了几次0，浪费
    // 点性能。
    uint16_t* data = (uint16_t*)ip;
    int head_len = ip->ip_hl * 4; // ip长度。
    uint32_t sum = 0;
    for(int n = 0; n < head_len; n += 2){
        sum += *data++;
    }
    
    // 把高位和地位相加，确保高位为0
    sum = (sum >> 16) + (sum &0xFFFF);
    
    // 上面一次相加还是有可能溢出的（进位1），这里再加一次。
    sum += (sum >> 16);
    
    ip->ip_sum = ~sum;
}

int main(int argc,char** argv){
    // 创建原始套接字
    int fd = socket(PF_INET,SOCK_RAW,IPPROTO_UDP);
    assert(fd != -1);
    
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = inet_addr("192.168.0.11"); // 这里随便填写一个ip，udp是无连接的，源ip地址可以肆意伪造
    local.sin_port = htons(31345); // 随便给一个端口
    memset(local.sin_zero, 0, sizeof(local.sin_zero));
    
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr("192.168.0.15"); // 目标ip
    dest.sin_port = htons(5555); // 本地随便给一个端口
    memset(dest.sin_zero, 0, sizeof(dest.sin_zero));
    
    socklen_t dest_len = sizeof(dest);
    
    char buffer[512] = {0};
    memcpy(buffer + sizeof(struct ip) + sizeof(struct udphdr), "hello raw",10);
    char data_len = 10;
    
    struct ip* ip = (struct ip*)buffer;
    ip->ip_v = IPVERSION; // ip 协议版本
    ip->ip_hl = 5; // ip头部长度,这里是字长，不是字节。也就是真实头部字节长度除以4。默认ip头部是20个字节，这里的结果是5。
    ip->ip_tos = 0; // 服务类型，普通
    
    // freebsd系统里有bug，raw socket里，ip_len和ip_off必须是主机字节顺序。坑爹，我耗了一个下午，忍不住去google时候才发现。
    // 太相信系统了，千错万错都是我的错，哈哈哈。
#if defined(__FreeBSD__) || defined(__APPLE__)
    ip->ip_len = sizeof(struct ip) + sizeof(struct udphdr) + data_len;
    ip->ip_off = IP_DF;
#else
    ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + data_len); // ip数据报总长度
    ip->ip_off = htons(IP_DF);// ip分片相关的数据。有是否分片，是否最后一片，以及当前ip的片偏移。这里不分片。dont fragment！
#endif
    ip->ip_id = 0; // ip数据报id，这个用于ip分片时来确认哪些分组属于一个数据报的。

    ip->ip_ttl = 255; // TTL，ip数据报生存时间，经过一个路由器就减一，其实也就是最多经过多少个路由器。各个系统各有不同。
    ip->ip_p = IPPROTO_UDP; // ip数据报中的协议，用于系统接到数据报往哪里投递。定义在netinet/in.h中。
    ip->ip_src = local.sin_addr;
    ip->ip_dst = dest.sin_addr;
    
    // 计算ip头部校验和。这个校验每过一个路由器都要重新计算一遍。因为路由器会修改头部中的ttl字段。还好只要计算头部的，不然死定了，哈哈哈。
    // ～～～～～～～～～～～～～～
    // 特别说明一下，计算校验和是可选的。如果ip_id被设置为0，系统会自动计算校验和。这样更省事！
    // ～～～～～～～～～～～～～～
    checksum_ip(ip);
//    ip->ip_sum = 0;
    printf("ipsum %d\n",ip->ip_sum);
    
    struct udphdr* udp = (struct udphdr*)(ip + 1);
    udp->uh_sport = local.sin_port;
    udp->uh_dport = dest.sin_port;
    udp->uh_sum = 0;
    udp->uh_ulen = htons(sizeof(struct udphdr) + data_len);
    
    // 计算udp校验和，这里的校验和也是一样，如果被设置为0，对方也能接到。可能也是被系统自动计算了？
    udp->uh_sum = checksum_tcpudp(ip,udp,udp->uh_ulen,sizeof(struct udphdr) + data_len);
    printf("chsum %d\n",udp->uh_sum);
    
    int flag = 1;
    setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &flag, sizeof(flag));
    ssize_t length = sendto(fd, buffer, sizeof(struct ip) + sizeof(struct udphdr) + data_len, 0, (struct sockaddr*)&dest, dest_len);
    
    if(length == -1) perror(NULL);
    close(fd);
    return 0;
}
