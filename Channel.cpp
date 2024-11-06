#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;  // 代表常规可读情况和紧急可读情况
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , tied_(false){

    }

Channel::~Channel() {}

// 当一个TcpConnection新连接创建的时候 TcpConnection -> Channel
void Channel::tie(const std::shared_ptr<void> &obj){
    tie_ = obj;
    tied_ = true;
}

// 当改变channel所表示的fd的events事件后，update负责在poller中修改fd相应的epoll_ctl
// EventLoop类中包含ChannelList以及Poller，所以修改poller需要通过EventLoop
void Channel::update() {
    // 通过Channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在Channel所属的EvnetLoop中，将当前的channel删除
void Channel::remove() {
    loop_->removeChannel(this);
}

// fd得到poller通知后，处理事件
void Channel::handleEvent(Timestamp receiveTime) {
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock(); // weak_ptr -> shared_ptr
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if((revents_ & EPOLLHUP) && (revents_ & EPOLLIN)) {
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_ & EPOLLERR) {
        if(errorCallback_){
            errorCallback_();
        }
    }
    if(revents_ & (EPOLLIN | EPOLLPRI)) {
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_ & EPOLLOUT) {
        if(writeCallback_){
            writeCallback_();
        }
    }
}