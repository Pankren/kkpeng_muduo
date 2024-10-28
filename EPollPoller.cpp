#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

const int kNew = -1; // 表示channel还没有添加到poller
const int kAdded = 1; // 表示channel已经添加到poller
const int kDeleted = 2; // 表示channel已经从poller中删除

// epoll操作1、构造函数类似于epoll_create/epoll_create1
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop) // 一个线程代表一个事件循环
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC)) // 
    , events_(kInitEventListSize){
        if(epollfd_ < 0){
            LOG_FATAL("epoll_create error:%d \n", errno);
        }
    }

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

// epoll操作3、poll函数类似于epoll_wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    // 实际上应该使用LOG_DEBUG更合理 因为每次调用LOG_INFO影响效率
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size()); // channels_size() is long unsigned int类型

    int numEvents = ::epoll_wait(epollfd_
        , &*events_.begin()  // events_是一个vector 第一个位置迭代器解引用得到第一个事件 然后取地址
        , static_cast<int>(events_.size())  // vec.size()返回size_t类型 而epoll_wait该参数需要int类型
        , timeoutMs);  // epoll_wait调用的超时事件

    int saveErrno = errno;  // errno是全局变量 而每个线程都可能会写该errno所以实现记录该值在每个线程中单独访问
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) { // epoll_wait超时返回0
        LOG_DEBUG("%s timeout! \n", __FUNCTION__); // __FUNCTION__代表当前函数名 常用于日志
    }
    else {
        if (saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }

    return now;
}

/*
channel的update和remove是通过EventLoop中的updateChannel和removeChannel来实现的
而EventLoop中的这两个函数又是调用Poller中的updateChannel和removeChannel来实现的
实际上，Channel和Poller不互属，EventLoop中包含Poller和Channel可以作为中间的过度来使用
*/
// epoll操作2、updateChannel和removeChannel类似于epoll_ctl
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();  // channel的index成员初始化是-1，index用于标识状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), channel->index());

    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else {  // channel已经在poller上注册过了
        int fd = channel->fd();
        if (channel->isNoneEvent()) {  // channel对任何事件都不感兴趣
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__ ,fd);

    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}


void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for (int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}