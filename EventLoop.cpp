#include "EventLoop.h"
#include "Logger.h"
#include "CurrentThread.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用超时时间
const int kPollTimeMs = 10000;

int createEventfd() {
    /*
    eventfd(unsigned int initval, int flags);
    用于在进程和线程之间实现事件通知机制 返回的fd可视为一个计数器
    一个进程（或线程）通知另一个进程（或线程）某个事件发生时通过操作该计数器实现 initval为初始化的计数器值
    EFD_CLOEXEC：标识在调用exec系列函数之后，fd会自动关闭
    EFD_NONBLOCK：将fd设置为非阻塞状态
    */
    int evtfd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error:%d", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefualPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_)){
        LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
        if (t_loopInThisThread) {
            LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
        }
        else{
            t_loopInThisThread = this;
        }

        // 设置wakeupfd的事件类型以及发生事件后的回调操作
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
        wakeupChannel_->enableReading();
    }

    EventLoop::~EventLoop() {
        wakeupChannel_->disableAll();
        wakeupChannel_->remove();
        ::close(wakeupFd_);
        t_loopInThisThread = nullptr;
    }