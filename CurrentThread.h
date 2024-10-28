#pragma once

#include <unistd.h>
#include <sys/syscall.h> // syscall()

// 为了获取线程tid
namespace CurrentThread {
    /*
    extern: 声明变量或者函数在其他源文件中定义 编译器不分配内存只是在当前文件中声明存在
    __thread: 编译器扩展关键字(表示线程局部存储变量) 指定一个变量是线程局部存在的 表示每个线程都有自己独立的副本
        可以避免数据竞争和同步问题
    */
    extern __thread int t_cacheTid;

    void cacheTid();

    inline int tid() {
        /*
        __builtin_expect((expr), (expected_value)): 编译器提供内建函数 info gcc可以查看
        提供分支预测优化的作用：表示t_catchedTid == 0 的情况为0（即概率较低） 可以优化逻辑
        */
        if (__builtin_expect(t_cacheTid == 0, 0)) {
            cacheTid();
        }
        return t_cacheTid;
    }
}