#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>  // iovec
#include <unistd.h>

ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char extrabuf[65536] = {0};

    /*
    struct iovec{
    void *iov_base;	// Pointer to data.
    size_t iov_len;	// Length of data.
    };
    iovec 结构体主要用于在一次系统调用中处理多个不连续的缓冲区
    实现数据的聚集（gather）和分散（scatter）操作
    */
    struct iovec vec[2];

    const size_t writable = writableBytes(); // buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
    // readv()可以将文件描述符中元素读取到多个缓冲区
    // iovcnt表示iov数组中元素的个数，即要读取数据到的缓冲区的数量。
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;  // 取两个缓冲区 如果源buffer足够大取一个
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *saveErrno = errno;
    }
    else if (n <= writable) {
        writerIndex_ += n;
    }
    else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }
    return n;
}