#ifndef __CC_SOCKET_H__
#define __CC_SOCKET_H__

#include "address.h"
#include <memory>
#include "noncopyable.h"

namespace cc{

//套接字类关注的属性：
//  文件描述符
//  地址类型（AF_INET, AF_INET6等）
//  套接字类型（SOCK_STREAM, SOCK_DGRAM等）
//  协议类型（这项其实可以忽略）
//  是否连接（针对TCP套接字，如果是UDP套接字，则默认已连接）
//  本地地址和对端的地址


class Socket : public std::enable_shared_from_this<Socket>, Noncopyable{

public:

    using ptr = std::shared_ptr<Socket>;
    using weak_ptr = std::weak_ptr<Socket>;

    enum Type{
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    enum Family{
        IPv4 = AF_INET,
        IPv6 = AF_INET6,
        UNIX = AF_UNIX,
    };

    static Socket::ptr CreateTCP(cc::Address::ptr address);
    static Socket::ptr CreateUDP(cc::Address::ptr address);

    static Socket::ptr CreateTCPSocket();
    static Socket::ptr CreateUDPSocket();

    static Socket::ptr CreateTCPSocket6();
    static Socket::ptr CreateUDPSocket6();

    static Socket::ptr CreateUnixTCPSocket();
    static Socket::ptr CreateUnixUDPSocket();
    /*
     * Socket构造函数
     * family 协议簇
     * type 类型
     * protocol 协议(默认为0即可)
     */
    Socket(int family, int type, int protocol = 0);

    /**
     * 析构函数
     */
    virtual ~Socket();

    /**
     * 获取发送超时时间(毫秒)
     */
    int64_t getSendTimeout();

    /**
     * 设置发送超时时间(毫秒)
     */
    void setSendTimeout(int64_t v);

    /**
     * 获取接受超时时间(毫秒)
     */
    int64_t getRecvTimeout();

    /**
     * 设置接受超时时间(毫秒)
     */
    void setRecvTimeout(int64_t v);

    /**
     * 获取sockopt
     */
    bool getOption(int level, int option, void* result, socklen_t* len);

    /**
     * 获取sockopt模板
     */
    template<class T>
    bool getOption(int level, int option, T& result) {
        socklen_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    /**
     * 设置sockopt
     */
    bool setOption(int level, int option, const void* result, socklen_t len);

    /**
     * 设置sockopt模板
     */
    template<class T>
    bool setOption(int level, int option, const T& value) {
        return setOption(level, option, &value, sizeof(T));
    }

    Socket::ptr accept();
    
    bool bind(const Address::ptr addr);
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    bool listen(int backlog = SOMAXCONN);
    bool close();
    //TCP   
    int send(const void* buffer, size_t length, int flags = 0);
    int send(const iovec* buffer, size_t length, int flags = 0);
    //UDP
    int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
    int sendTo(const iovec* buffer, size_t length, const Address::ptr to, int flags = 0);
    //TCP
    int recv(void* buffer, size_t length, int flags = 0);
    int recv(iovec* buffer, size_t length, int flags = 0);
    //UDP
    int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
    int recvFrom(iovec* buffer, size_t length, Address::ptr from, int flags = 0);

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();

    int getFamily() const {return m_family;}
    int getType() const {return m_type;}
    int getProtocol() const {return m_protocol;}

    bool isConnected() const {return m_isConnected;}
    //m_sock是否有效
    bool isValid() const;
    //返回Socket错误
    int getError();

    //输出socket信息到流中
    virtual std::ostream& dump(std::ostream& os) const;

    virtual std::string toString() const;

    int getSocket() const {return m_sock;}

    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelall();
private:

    //初始化socket
    bool init(int sock);
    //初始化sock
    void initSock();
    //创建socket
    void newSock();
private:

    //socket文件描述符
    int m_sock;
    //协议簇
    int m_family;
    //类型(主要为:TCP UDP这种类型)
    int m_type;
    //协议
    int m_protocol;
    //是否已连接
    int m_isConnected;
    
    Address::ptr m_localAddress;
    Address::ptr m_remoteAddress;

};

std::ostream& operator<<(std::ostream& os, const Socket& sock);
}


#endif