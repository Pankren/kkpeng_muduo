#pragma once

// 派生类继承之后可以正常地构造和析构，但是不能进行拷贝构造和赋值操作

class noncopyable{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};