#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

/*
epoll使用：
epoll_create
epoll_ctl  add/mod/del
epoll_wait
*/

class Channel;

class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop *loop);  // epoll_create
    ~EPollPoller() override;  // close(epoll_fd)

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;  // epoll_wait
    void updateChannel(Channel *channel) override;  // epoll_ctl add/mod
    void removeChannel(Channel *channel) override;  // epoll_ctl del
private:
    static const int kInitEventListSize = 16;  // 设置channelList vec长度

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    // 更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};