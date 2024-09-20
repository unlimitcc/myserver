#ifndef __CC_ADDRESS_H__
#define __CC_ADDRESS_H__

#include <arpa/inet.h>
#include <memory>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <vector>
#include <map>

//Address模块就是把C标准版库中与socket相关的地址API采用C++面向对象的思想封装起来。
namespace cc{

class IPAddress;

class Address{

public:

    using ptr = std::shared_ptr<Address>;

    //通过sockaddr指针创建Address
    /* 基础Socket地址
    struct sockaddr{
        //选择地址家族，AF_xxx | 例如是IPV4的就是选择AF_INET | AF_INET6
        uint16 sa_family;
        //地址数据 包含套接字中的目标地址和端口信息           
        char sa_data[14];             
    };
    */
    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);
    /**
     * 通过host地址返回对应条件的所有Address
     * result 保存满足条件的Address
     * host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
     * family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * type socketl类型SOCK_STREAM、SOCK_DGRAM 等
     * protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
     * 返回是否转换成功
     */
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                        int family = AF_INET, int type = 0, int protocol = 0);
    /*
     * 通过host地址返回对应条件的任意Address
     * host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
     * family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * type socket类型SOCK_STREAM、SOCK_DGRAM 等
     * protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
     * 返回满足条件的任意Address,失败返回nullptr
    */
    static Address::ptr LookupAny(const std::string& host,
                        int family = AF_INET, int type = 0, int protocol = 0);

    //与LookupAny类似，但是返回IPAddress
    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host, 
                        int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * 返回本机所有网卡的<网卡名, 地址, 子网掩码位数>
     * result 保存本机所有地址
     * family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * 是否获取成功
     */
    static bool GetInterfaceAddress(std::multimap<std::string, 
                                    std::pair<Address::ptr, uint32_t> >& result,
                                    int family = AF_INET);

    /**
     * 获取指定网卡的地址和子网掩码位数
     * result 保存指定网卡所有地址
     * iface 网卡名称
     * family 协议族(AF_INT, AF_INT6, AF_UNIX)
     * return 是否获取成功
     */
    static bool GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t> >& result,
                                    const std::string& iface, int family = AF_INET);
    virtual ~Address() {}
    //IPv4 | IPv6
    int getFamily() const;

    // 获得sockaddr指针
    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr* getAddr() = 0;
    // 获得sockaddr长度
    virtual socklen_t getAddrLen() const = 0;

    //可读性输出地址
    virtual std::ostream& insert(std::ostream& os) const = 0;
    std::string toString();

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

};

class IPAddress : public Address{
public:

    using ptr = std::shared_ptr<IPAddress>;
    /**
     * 通过域名,IP,服务器名创建IPAddress
     * address 域名,IP,服务器名等.举例: www.sylar.top
     * port 端口号
     * 调用成功返回IPAddress,失败返回nullptr
     */
    static IPAddress::ptr Create(const char* address, uint16_t port = 0);

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMaskAddress(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint16_t v) = 0; 
};


class IPv4Address : public IPAddress{

public:

    using ptr = std::shared_ptr<IPv4Address>;
    
    //文本类型的地址转换为IPv4Address格式
    /*网络Socket地址
    struct sockaddr_in
    {
        uint16 sin_family;          // 地址家族: AF_INET
        uint16 sin_port;            // 两字节的端口号（网络字节顺序）
        uint32 sin_addr.s_addr;     // 网络地址IP4
        unsigned char sin_zero[8];  // 填充字节(通常置0，不使用)
    };
    */
    static IPv4Address::ptr Create(const char * address, uint16_t port = 0);
    
    IPv4Address(const sockaddr_in& address);
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    /*
     * 获取该地址的广播地址
     * prefix_len 子网掩码位数
     * 调用成功返回IPAddress,失败返回nullptr
     */
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMaskAddress(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override; 

private:
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress{

public:

    using ptr = std::shared_ptr<IPv6Address>;
    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

    IPv6Address();
    IPv6Address(const sockaddr_in6& address);
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMaskAddress(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;

private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address{

public:

    using ptr = std::shared_ptr<UnixAddress>;
    UnixAddress();
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    void setAddrLen(uint32_t v);
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

private:
    /*
    sockaddr_un用于实现本地进程间通信，
    通过文件系统中的路径来标识和访问套接字，
    适合于同一台主机上的进程间快速通信。
    struct sockaddr_un {
        sa_family_t    sun_family;   // 地址族(AF_UNIX)
        char           sun_path[108]; // 套接字文件路径
    };
    */
    struct sockaddr_un m_addr;
    //长度
    socklen_t m_length;
};

class UnKnownAddress : public Address{

public:
    using ptr = std::shared_ptr<UnKnownAddress>;
    UnKnownAddress(int family);
    UnKnownAddress(const sockaddr& addr);
    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

private:
    sockaddr m_addr;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);


}

#endif