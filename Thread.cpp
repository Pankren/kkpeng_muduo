#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h> // 信号量 用于多线程同步、控制对共享资源的访问、线程之间的通信和协调

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name) {
        setDefaultName();
    }

Thread::~Thread() {
    if(started_ && !joined_){
        thread_->detach();
    }
}

void Thread::start() {
    started_ = true;
    sem_t sem; // 信号量
    /*
    int sem_init(sem_t *sem, int pshared, unsigned int value);
    pshared指定进程(true)还是线程(false)间使用该sem, value表示信号量初始值
    */
    sem_init(&sem, false, 0);

    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem); // 对信号量sem加一 之后执行这步（线程获取到了id），sem_wait才不会阻塞

        func_();
    }));

    sem_wait(&sem); // 信号量的值大于0信号量减一后程序继续执行 信号量等于0调用sem_wait的线程会被阻塞
    /*
    使用该信号量的目的：保证start后创建的子线程都有线程id
    */
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;  // 最开始numCreated_ = 0
    if(name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}