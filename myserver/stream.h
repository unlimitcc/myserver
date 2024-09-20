#ifndef __CC_STREAM_H__
#define __CC_STREAM_H__

#include <memory>
#include "bytearray.h"

//保证一定操作规定字节的数据
//因为socket的recv 从 socket 的接收缓冲区中读取当前可用的数据。
//如果缓冲区中没有足够的数据，它会读取尽可能多的字节，
//直到满足调用者请求的最大长度或缓冲区中没有更多数据为止。
//通过这层封装，可以指定需要接收数据的长度
namespace cc{

class Stream {
public:
    using ptr = std::shared_ptr<Stream>;
    /**
     * 析构函数
     */
    virtual ~Stream() {}

    /**
     * 读数据
     * buffer 接收数据的内存
     * length 接收数据的内存大小
     * return
     *      >0 返回接收到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int read(void* buffer, size_t length) = 0;

    /**
     * 读数据
     * ba 接收数据的ByteArray
     * length 接收数据的内存大小
     * return
     *      >0 返回接收到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int read(ByteArray::ptr ba, size_t length) = 0;

    /**
     * 读固定长度的数据
     * buffer 接收数据的内存
     * length 接收数据的内存大小
     * return
     *      >0 返回接收到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int readFixSize(void* buffer, size_t length);

    /**
     * 读固定长度的数据
     * ba 接收数据的ByteArray
     * length 接收数据的内存大小
     * return
     *      >0 返回接收到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int readFixSize(ByteArray::ptr ba, size_t length);

    /**
     * 写数据
     * buffer 写数据的内存
     * length 写入数据的内存大小
     * return
     *      >0 返回写入到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int write(const void* buffer, size_t length) = 0;

    /**
     * 写数据
     * ba 写数据的ByteArray
     * length 写入数据的内存大小
     * return
     *      >0 返回写入到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int write(ByteArray::ptr ba, size_t length) = 0;

    /**
     * 写固定长度的数据
     * buffer 写数据的内存
     * length 写入数据的内存大小
     * return
     *      >0 返回写入到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int writeFixSize(const void* buffer, size_t length);

    /**
     * 写固定长度的数据
     * ba 写数据的ByteArray
     * length 写入数据的内存大小
     * return
     *      >0 返回写入到的数据的实际大小
     *      =0 被关闭
     *      <0 出现流错误
     */
    virtual int writeFixSize(ByteArray::ptr ba, size_t length);

    /**
     * 关闭流
     */
    virtual void close() = 0;
};

}


#endif