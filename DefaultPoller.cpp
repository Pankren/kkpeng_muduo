#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>  // getenv()

// 不将该函数写在Poller.cpp中的原因在于：
// 构造poll或者epoll实例时必然需要包含poll和epoll实现的头文件
// 而Poller作为父类，父类中包含子类的头文件是不合规范的
// 单独写一个源文件避免了父类和子类的耦合
Poller* Poller::newDefualPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr; // 生成poll实例
    }else{
        return new EPollPoller(loop); // 生成epoll实例
    }
}