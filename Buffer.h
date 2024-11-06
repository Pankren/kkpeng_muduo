#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲器类型定义
// buffer：1、prependable bytes区域 2、readable bytes区域 3、writable bytes区域  通过index索引
class Buffer {
public:
    static const size_t kCheapPrepend = 8; // 解决粘包问题
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend) {}
    
    size_t readableBytes() const {  // 返回readable bytes区域的大小
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const {  // 返回writable bytes区域的大小
        return buffer_.size() - writerIndex_;
    }
    size_t prependableBytes() const {  // 返回prependable bytes区域的大小
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始位置
    const char* peek() const {
        return begin() + readerIndex_;
    }

    void retrieve(size_t len) {  // 复位
        if (len < readableBytes()) {
            readerIndex_ += len;  // 应用只读取了缓冲区数据的一部分len长度数据 剩下readerIndex+len到writerIndex
        }
        else {  // len == readableBytes()
            retrieveAll();
        }
    }
    void retrieveAll() {  // 三个区域复位到最初的状态
        //readerIndex_ = writerIndex_ = kCheapPrepend;
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString() {  // 将buffer中数据转化为string类型返回
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len) {  // 
        std::string result(peek(), len);
        retrieve(len);  // result得到可读区数据之后 对buffer进行复位操作
        return result;
    }

    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }

    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char* beginWrite() {
        return begin() + writerIndex_;
    }
    const char* beginWrite() const {
        return begin() + writerIndex_;
    }

    // 从fd读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 向fd写入数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin() {
        return &*buffer_.begin(); // vector底层数组首元素的地址 就是数组的起始地址
    }
    const char* begin() const { // const方法 给const对象调用
        return &*buffer_.begin();
    }

    void makeSpace(size_t len) {  // 扩容函数
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {  // 原长不够时
            buffer_.resize(writerIndex_ + len);
        }
        else {
            size_t readable = readableBytes();
            // std::copy(iter begin, iter end, iter new_begin)
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;  // 数据可读的位置索引
    size_t writerIndex_;  // 数据可写的位置索引
};