#include "CurrentThread.h"

namespace CurrentThread {
    __thread int t_cachedTid = 0;

    void cacheTid() {
        if (t_cachedTid == 0) {
            // 通过linux系统调用，获取当前线程的tid
            // syscall函数可以允许用户调用一些内核操作
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
