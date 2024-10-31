#include "EventLoop.h"
#include "Logger.h"
//#include "CurrentThread.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <memory> //
#include <fcntl.h> // 

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用超时时间
const int kPollTimeMs = 10000;

/*
eventfd(unsigned int initval, int flags);
用于在进程和线程之间实现事件通知机制 返回的fd可视为一个计数器
一个进程（或线程）通知另一个进程（或线程）某个事件发生时通过操作该计数器实现 initval为初始化的计数器值
EFD_CLOEXEC：标识在调用exec系列函数之后，fd会自动关闭
EFD_NONBLOCK：将fd设置为非阻塞状态
*/
int createEventfd() {
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
    wakeupChannel_->disableAll(); // 复位（设置为对所有事件都不感兴趣）
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_) {
        activeChannels_.clear();

        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);  // poll类似于epoll_wait
        for(Channel *channel : activeChannels_) {
            // poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        doPendingFunctors();        
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环调用quit的情况：1、loop在自己的线程中调用quit 2、在非loop的线程中调用loop的quit
void EventLoop::quit() {
    quit_ = true;

    // 如果在其他线程中调用quit
    if(!isInLoopThread()){
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb) {
    if(isInLoopThread()){ // 在当前线程
        cb();
    }
    else { // 不在当前线程 需要唤醒loop所在的线程执行cb
        queueInLoop(cb);
    }
}

// 将cb放入队列，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_); // unique_lock比lock_guard灵活 因为可以设置条件变量
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的、需要执行上面回调操作的loop的线程
    // 或上callingPendingFunctors_的意思：当loop正在执行回调操作但是loop有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup();
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1; // 任意读 表示有读事件发生
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// 唤醒loop所在的线程 向wakeupfd写入一个数据，wakeupChannel就会发生可读事件 当前线程就会被唤醒
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop调用Poller中的方法
void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel) {
    return poller_->hasChannel(channel);
}

// 执行回调
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;  // 额外创建该局部变量并swap是为了保证线程安全
    callingPendingFunctors_ = true; // 表示正在执行回调操作
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for(const Functor &functor : functors){
        functor();
    }
    callingPendingFunctors_ = false; // 所有回调函数执行完成
}