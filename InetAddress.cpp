#include "InetAddress.h"
#include <strings.h>  // bzero()
#include <string.h>  // strlen()

InetAddress::InetAddress(uint16_t port, std::string ip){  // 默认值在声明和定义中只能出现一次
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);  // host to net short
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  // string转char*
}

std::string InetAddress::toIp() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);  // 点分十进制转网络字节序 比inet_ntoa()更加适用 可以指定ipv4或ipv6
    return buf;
}

std::string InetAddress::toIpPort() const {  // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);  // 网络字节序转点分十进制
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);  // net to host short
    sprintf(buf+end, ":%u", port);  // sprintf()函数可以将内容保存到一个buf中 snprintf()可以指定该buf大小
    return buf;
}

uint16_t InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

// #include <iostream>
// int main(){
//     InetAddress addr(8888);
//     std::cout<<addr.toIpPort()<<std::endl;
//     return 0;
// }