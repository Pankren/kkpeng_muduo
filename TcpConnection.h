#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class EventLoop;
class Channel;
class Socket;

/*
* TcpServer通过Acceptor -> 有一个新用户连接时通过accept函数拿到connfd
* -> 打包TcpConnection，设置回调 -> 设置Channel -> 注册到Poller -> Poller监听到时间之后就会执行Channel的回调操作
*/
// std::enable_shared_from_this中可以得到当前对象的一个智能指针 一般使用该类用public继承
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop,
        const std::string& name,
        int sockfd,
        const InetAddress& localAddr,
        const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string& buf);
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    void connectEstablished(); // 建立连接
    void connectDestroyed(); // 连接销毁

private:
    enum StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_; // 是subloop，绝对不是mainloop，因为TcpConnection都是在subLoop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_; // 读 接受数据的缓冲区
    Buffer outputBuffer_; // 写 发送数据的缓冲区
};