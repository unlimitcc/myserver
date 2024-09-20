#ifndef __CC_SOCKET_STREAM_H__
#define __CC_SOCKET_STREAM_H__

#include <memory>
#include "stream.h"
#include "socket.h"
//所有的读写都是针对socket来说的，读socket相当于写内存或者bytearray
//socket的recv是从内存接收，send是从内存写入socket
namespace cc{

// Socket流
class SocketStream : public Stream {
public:
    using ptr = std::shared_ptr<SocketStream>;

    /**
     * 构造函数
     * sock Socket类
     * owner 是否由自身完全控制
     */
    SocketStream(Socket::ptr sock, bool owner = true);

    /**
     * 析构函数
     * 如果m_owner=true,则close
     */
    ~SocketStream();

    /**
     * 读取数据，写到buffer里
     * buffer 待接收数据的内存
     * length 待接收数据的长度
     *      >0 返回实际接收到的数据长度
     *      =0 socket被远端关闭
     *      <0 socket错误
     */
    virtual int read(void* buffer, size_t length) override;

    /**
     * 读取数据
     * ba 接收数据的ByteArray
     * length 待接收数据的长度
     * return
     *      >0 返回实际接收到的数据长度
     *      =0 socket被远端关闭
     *      <0 socket错误
     */
    virtual int read(ByteArray::ptr ba, size_t length) override;

    /**
     * 写入数据，从buffer读取
     * buffer 待发送数据的内存
     * length 待发送数据的内存长度
     * return
     *      >0 返回实际接收到的数据长度
     *      =0 socket被远端关闭
     *      <0 socket错误
     */
    virtual int write(const void* buffer, size_t length) override;

    /**
     * 写入数据
     * ba 待发送数据的ByteArray
     * length 待发送数据的内存长度
     * return
     *      >0 返回实际接收到的数据长度
     *      =0 socket被远端关闭
     *      <0 socket错误
     */
    virtual int write(ByteArray::ptr ba, size_t length) override;

    // 关闭socket
    virtual void close() override;

    // 返回Socket类
    Socket::ptr getSocket() const { return m_socket;}

    //返回是否连接
    bool isConnected() const;

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();
    std::string getRemoteAddressString();
    std::string getLocalAddressString();
protected:

    // Socket类
    Socket::ptr m_socket;
    // 是否全权处理，包括善后(关闭等操作)
    bool m_owner;
};

}



#endif