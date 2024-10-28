#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <string>
#include <functional>
/*
基于muduo网络库开发服务器程序
1、组合TcpServer对象
2、创建EventLoop事件循环对象的指针
3、明确TcpServer构造函数需要哪些参数，输出ChatServer的构造函数
4、在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写事件的回调函数
5、设置合适的服务器线程数量，muduo库会自己分配I/O线程和worker线程
*/
class ChatServer
{
public:
    ChatServer(muduo::net::EventLoop *loop, // 事件循环
        const muduo::net::InetAddress &listenAddr, // IP+Port
        const std::string &nameArg) //服务器名字
        : _server(loop, listenAddr, nameArg), 
        _loop(loop){
        // 给服务器注册用户连接的创建和断开回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, std::placeholders::_1));

        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置服务器端的线程数量 设置1个io线程 3个worker线程
        _server.setThreadNum(4);
    }

    // 开启事件循环
    void start(){
        _server.start();
    }

private:
    // 专门处理永和的创建连接和断开  相当于epoll listenfd accept
    void onConnection(const muduo::net::TcpConnectionPtr &conn){
        if(conn->connected()){
            std::cout<<conn->peerAddress().toIpPort()<<" -> "<<conn->localAddress().toIpPort()<<" state: online"<<std::endl;
        }
        else{
            std::cout<<conn->peerAddress().toIpPort()<<" -> "<<conn->localAddress().toIpPort()<<" state: offline"<<std::endl;
            conn->shutdown(); // close(fd)
        }
    }

    // 专门处理用户的读写事件
    void onMessage(const muduo::net::TcpConnectionPtr &conn, // 连接
        muduo::net::Buffer *buffer, // 缓冲区
        muduo::Timestamp time){ // 接收到数据的时间信息
        std::string buf = buffer->retrieveAllAsString();
        std::cout<<"recvive data: "<<buf<<"time: "<<time.toString()<<std::endl;
        conn->send(buf);
    }
    muduo::net::TcpServer _server;
    muduo::net::EventLoop *_loop;
};

int main(){
    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr("127.0.0.1", 8888);
    ChatServer server(&loop, addr, "ChatServer");

    server.start(); // listenfd epoll_ctl
    loop.loop();

    return 0;
}