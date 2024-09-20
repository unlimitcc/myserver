#include "address.h"
#include "endian.h"
#include <cstddef>
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h> 
#include "log.h"

namespace cc
{
// struct sockaddr
// {
//  uint16 sa_family;  //选择地址家族，AF_xxx | 例如是IPV4的就是选择AF_INET | AF_INET6
//  char sa_data[14];  //地址数据 包含套接字中的目标地址和端口信息   
// };
//
// IPv4
// struct sockaddr_in
// {
//  uint16 sin_family;          // 地址家族: AF_INET
//  uint16 sin_port;            // 两字节的端口号（网络字节顺序）
//  uint32 sin_addr.s_addr;     // 网络地址IP4
//  unsigned char sin_zero[8];  // 填充位,用零填充即可
// };
//
// IPv6
// struct sockaddr_in6
// {
//   uint16 sin6_family;         // 地址族
//   uint16 sin6_port;           // 端口号
//   uint32 sin6_flowinfo;       // 流信息
//   uint8  sin6_addr[16];       // IPv6 地址
//   uint32 sin6_scope_id;       // IPv6 scope-id
// };
//
// UNIX
// struct sockaddr_un
// {
//   unsigned short sun_family;  // 地址族，Unix域套字地址族为AF_UNIX
//   char sun_path[108];         // 路径字符串
// };


static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

//生成网络掩码的反
//将主机位地址转换为全1,用于获取广播地址等
//bits 掩码位数
//转换后得到一个类似于 ..00000..111的数字，1的数量为主机号位数
//取反后得到网络掩码
template<class T>
static T CreateMask(uint32_t bits){
    return (1 << (sizeof(T)*8 - bits)) - 1;
}

//统计二进制value中1的个数
template<class T>
static uint32_t CountBytes(T value){
    uint32_t result = 0;
    for(; value; ++result){
        value &= value - 1;
    }
    return result;
}

//返回一个IP地址
Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen){
    if(addr == nullptr){
        return nullptr;
    }

    Address::ptr result;
    switch(addr->sa_family){
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnKnownAddress(*addr));
            break;
    }

    return result;
}


int Address::getFamily() const{

    return getAddr()->sa_family;

}

std::string Address::toString(){
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address& rhs) const{

    //memcmp是C标准库中的一个函数，用于比较内存块中的数据。
    //它的全称是 "memory compare"，通常用于比较两个内存区域的字节内容。
    //int memcmp(const void *s1, const void *s2, size_t n);
    //s1: 指向要比较的第一个内存块的指针。
    //s2: 指向要比较的第二个内存块的指针。
    //n: 要比较的字节数。
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), minlen);
    if(result < 0){
        return true;
    }else if(result > 0){
        return false;
    }else if(getAddrLen() < rhs.getAddrLen()){
        return true;
    }
    return false;
}

//两个address怎么比
//长度相等，且每一位相等
bool Address::operator==(const Address& rhs) const{

    return getAddrLen() == rhs.getAddrLen() 
            && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

//不是一个对象则不相等
bool Address::operator!=(const Address& rhs) const{

    return !(*this == rhs);
}

//通过host地址返回对应条件的所有Address
//它通常用于网络编程中，特别是在使用函数如 getaddrinfo 时，用于处理 DNS 名字解析和套接字地址管理
bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                    int family, int type, int protocol){
    // 定义3个addrinfo结构体
    // hints : 期望获取的地址信息的特征
    // results : 结果容器
    // next : 用来遍历results

    // struct addrinfo {
    //   int              ai_flags;        // 位掩码，修改默认行为
    //   int              ai_family;       // socket()的第一个参数
    //   int              ai_socktype;     // socket()的第二个参数
    //   int              ai_protocol;     // socket()的第三个参数
    //   socklen_t        ai_addrlen;      // sizeof(ai_addr)
    //   struct sockaddr  *ai_addr;        // sockaddr指针
    //   char             *ai_canonname;   // 主机的规范名字
    //   struct addrinfo  *ai_next;        // 链表的下一个结点
    // }

    addrinfo hints, *results, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    //主机名
    std::string node;
    //端口号
    const char* service = NULL;
    
    //Eg: host = www.baidu.com:http / [2001:0db8:85a3:0000:0000:8a2e:0370:7334]:80
    //检查ipv6address,如果地址为后面一种类型则进入该分支
    if(!host.empty() && host[0] == '['){
        //void *memchr(const void *s, int c, size_t n);
        //memchr是C标准库中的一个函数，用于在一块内存区域中查找第一个匹配指定字符的字节。
        //它通常用于处理二进制数据或字符串。
        //如果找到字符 c，则返回指向该字符在内存块中的指针。
        //如果在指定的前 n 个字节中没有找到字符 c，则返回 NULL。
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if(endipv6){
            //ipv6地址"]"后面还有冒号，其地址格式则为 [IP]:端口
            if(*(endipv6 + 1) == ':'){
                service = endipv6 + 2;
            } 
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    //若node为空则说明非IPv6, 检查 ipv4地址 www.baidu.com: http
    if(node.empty()){ 
        //找第一个":"
        service = (const char*)memchr(host.c_str(), ':', host.size());
        //如果 ":" 之后没有 ":"，表明service就是端口的起始地址
        if(service){
            if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)){
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }   
    }
    // 如果host中没设置端口号，就将host赋值给node
    if(node.empty()){
        node = host;
    }


    //getaddrinfo 是一个用于网络编程的函数，定义在 POSIX 标准和 Windows 套接字 API 中。
    //它用于将主机名或地址字符串解析为 struct addrinfo 结构。
    /*
    int getaddrinfo(const char *node,
                    const char *service,
                    const struct addrinfo *hints,
                    struct addrinfo **res);
    node: 要解析的主机名（如 "www.example.com"）或 IP 地址字符串（如 "192.168.1.1"）。如果设置为 NULL，则表示本地主机
    service: 要连接的服务名称（如 "http"、"ftp"）或端口号字符串（如 "80"）。如果设置为 NULL，则表示不指定特定的服务。
    hints: 一个指向 addrinfo 结构的指针，用于指定返回的信息类型。如果为 NULL，则使用默认设置。
        此结构体允许你指定诸如地址族（IPv4 或 IPv6）、套接字类型（流式或数据报）、协议类型等。
    res: 一个指向 addrinfo 结构的指针的指针。函数将通过这个参数返回结果链表的起始地址。调用者负责释放分配的内存
        成功时返回 0，并通过 res 返回一个指向 addrinfo 结构的指针链表。
    getaddrinfo() 可能返回多个 addrinfo 结构体，形成一个链表。
        这些地址可以是不同的 IP 地址、IPv4 和 IPv6 地址，或者不同的端口号，具体取决于提供的主机名和服务名。
    失败时返回一个非零的错误码，具体错误信息可以通过 gai_strerror() 获取。
    */
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error) {
        CC_LOG_ERROR(g_logger) << "Address::Lookup getaddress" << host << ","
                               << family << ", " << type << ") error = " << error
                               << " errstr=" << strerror(errno);
        return false;
    }

    next = results;
    //取出所有返回的Address放进result
    while(next) {
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }

    //是一个在网络编程中用于释放通过 getaddrinfo 函数分配的动态内存的函数。
    freeaddrinfo(results);
    return true;
}


Address::ptr Address::LookupAny(const std::string& host, int family, int type, int protocol){
    
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)){
        //返回第一个address结果
        return result[0];
    }
    return nullptr;

}


IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,int family, int type, int protocol){

    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)){
        for(auto& i : result){
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if(v){
                return v;
            }
        }
    }
    return nullptr;
}

//std::multimap<std::string, std::pair<Address::ptr, uint32_t>> : <网卡名，（地址信息，端口号）>
bool Address::GetInterfaceAddress(std::multimap<std::string, 
                                std::pair<Address::ptr, uint32_t> >& result,
                                int family){
    struct ifaddrs *next, *results;
    //Linux 系统中的一个网络接口函数，用于获取本地系统上所有网络接口的信息。
    //getifaddrs创建一个链表，链表上的每个节点都是一个struct ifaddrs结构，result指向链表第一个元素的指针

    // struct ifaddrs {
    //     struct ifaddrs  *ifa_next;      //指向下一个接口的指针
    //     char            *ifa_name;      //接口名称
    //     unsigned int    ifa_flags;      //接口标志
    //     struct sockaddr *ifa_addr;      //主地址
    //     struct sockaddr *ifa_netmask;   //子网掩码
    //     struct sockaddr *ifa_broadaddr; //广播地址
    //     struct sockaddr *ifa_dstaddr;   //点对点接口的目标地址
    //     void            *ifa_data;      //特定于协议的数据
    // };
    if(getifaddrs(&results) != 0) {
        CC_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
            " err=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    //挨个处理拿到的地址
    try {
        for(next = results; next; next = next->ifa_next){
            Address::ptr addr;
            uint32_t prefix_len = ~0u;
            //当前接口地址簇是否符合要求
            if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            switch(next->ifa_addr->sa_family) {
                //ipv4
                case AF_INET:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        //掩码
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        //获取前缀长度即网络地址长度
                        prefix_len = CountBytes(netmask);
                    }
                    break;
                //ipv6
                case AF_INET6:
                    {
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        prefix_len = 0;
                        for(int i = 0; i < 16; ++i) {
                            prefix_len += CountBytes(netmask.s6_addr[i]);
                        }
                    }
                    break;
                default:
                    break;
            }
            //创建成功，插入result
            if(addr) {
                result.insert(std::make_pair(next->ifa_name,std::make_pair(addr, prefix_len)));
            }
        }
    } catch (...) {
        CC_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }

    freeifaddrs(results);
    return true;

}

bool Address::GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t> >& result,
                                    const std::string& iface, int family){
    //如果传入的接口名称为空或者为通配符*，表示获取所有接口的地址信息
    if(iface.empty() || iface == "*"){
        //根据family选择具体获取哪个
        if(family == AF_INET || family == AF_UNSPEC){
            result.push_back(std::make_pair(Address::ptr(new IPv4Address), 0u));
        }
        if(family == AF_INET6 || family == AF_UNSPEC){
            result.push_back(std::make_pair(Address::ptr(new IPv6Address), 0u));
        }
        return true;
    }
    
    //如果指定了网卡
    std::multimap<std::string
          ,std::pair<Address::ptr, uint32_t> > results;

    if(!GetInterfaceAddress(results, family)) {
        return false;
    }

    //equal_range 函数用于查找特定键(例如：iface)在multimap中的所有值的范围
    //返回值是一个pair，保存一对迭代器(前开后闭)
    auto its = results.equal_range(iface);
    for(; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();
}

IPAddress::ptr IPAddress::Create(const char* address, uint16_t port){
    addrinfo hints;
    addrinfo *results;
    memset(&hints, 0 ,sizeof(hints));

    //AI_NUMERICHOST标志，该参数只能是数字化的地址字符串，不能是域名
    //AF_UNSPEC则意味着函数返回的是适用于指定主机名和服务名且适合任何协议族的地址。
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;


    //addrinfo 结构体是 getaddrinfo() 函数的输出类型，
    //  用于从主机名和服务名解析得到网络地址信息。
    //  它可以处理 IPv4 和 IPv6 地址，并为套接字编程提供一个通用接口。
    //sockaddr_in 结构体专用于存储 IPv4 地址和端口号，用于在套接字编程中指定地址和端口。
    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error){
        CC_LOG_DEBUG(g_logger) << "IPAddress::Create(" << address
                               << ", " << port << ") error = " << error
                               << " errno=" << errno << " errstr=" 
                               << strerror(errno);
        return nullptr;
    }

    try{
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
            Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
        if(result){
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    }catch(...){
        freeaddrinfo(results);
        return nullptr;
    }
}


IPv4Address::ptr IPv4Address::Create(const char * address, uint16_t port){
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = byteswapOnLittleEndian(port);

    //将一个IP地址(address)的字符串表示转换为网络字节序的二进制形式，存储在m_addr中
    //返回1表示成功
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);

    if(result <= 0){
        CC_LOG_ERROR(g_logger) << "IPv4Address Create(" << address
                               << ", " << port << ") rt = " << result
                               << " errno=" << errno
                               << " errstr=" << strerror(errno); 
        return nullptr;
    }

    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address){
    m_addr = address;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port){

    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = byteswapOnLittleEndian(port);
    m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

const sockaddr* IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv4Address::getAddr(){
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

//字节方式存储的IPv4地址转换为类似"192.168.10.1"格式
//大多使用大端字节序（网络字节序）。bswap_32 可用于本地字节序和网络字节序之间的转换。
std::ostream& IPv4Address::insert(std::ostream& os) const {
    uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
    return os;
}

//获取广播地址，主机号全1
IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if(prefix_len > 32){
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    baddr.sin_addr.s_addr |= 
        byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(baddr));
}

//获取网段，主机号全0
IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if(prefix_len > 32){
        return nullptr;
    }

    sockaddr_in baddr(m_addr);
    baddr.sin_addr.s_addr &= byteswapOnLittleEndian(
        CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(baddr));
}

//获取子网掩码
IPAddress::ptr IPv4Address::subnetMaskAddress(uint32_t prefix_len) {
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const {

    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
    m_addr.sin_port = byteswapOnLittleEndian(v);
    return;
} 

IPv6Address::IPv6Address(){
    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address){
    m_addr = address;
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port){
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteswapOnLittleEndian(port);

    //将字符串类型的IP地址转为unsigned int类型的数值格式
    //例如：192.168.3.144 记为 0x c0(192)a8(168)03(3)90(144)
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);

    if(result <= 0){
        CC_LOG_ERROR(g_logger) << "IPv6Address Create(" << address
                               << ", " << port << ") rt = " << result
                               << " errno=" << errno
                               << " errstr=" << strerror(errno); 
        return nullptr;
    }

    return rt;
}


//void *memcpy(void *dest, const void *src, size_t n);
//n: 要复制的字节数
IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port){
    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteswapOnLittleEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

const sockaddr* IPv6Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv6Address::getAddr(){
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

//可读性输出ipv6地址,（正确）
std::ostream& IPv6Address::insert(std::ostream& os) const {
    os << "[";
    //m_addr.sin6_addr.s6_addr 是一个 unsigned char[16] 数组，存储了 IPv6 地址的二进制表示。
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    bool used_zeros = false;
    for(size_t i = 0; i < 8; ++i){
        if(addr[i] == 0 && !used_zeros){
            continue;
        }
        if(i && addr[i-1] == 0 && !used_zeros){
            os << ":";
            used_zeros = true;
        }
        if(i){
            os << ":";
        }
        os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
    }

    if(!used_zeros && addr[7] == 0){
        os << "::";
    }
    os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
    return os;
}

//广播地址
IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {

    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint32_t>(prefix_len % 8);

    //不够8bit的用上面的处理，其他之后的整的字节用下面处理
    for(uint32_t i=prefix_len / 8 + 1; i < 16; ++i){
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }

    return IPv6Address::ptr(new IPv6Address(baddr));
}

//存疑
IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint32_t>(prefix_len % 8);

    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0x00;
    }

    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMaskAddress(uint32_t prefix_len) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] = 
        ~CreateMask<uint32_t>(prefix_len % 8);
    
    for(uint32_t i = 0; i < prefix_len / 8; ++i){
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }

    return IPv6Address::ptr(new IPv6Address(subnet));

}


uint32_t IPv6Address::getPort() const {
    return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
    m_addr.sin6_port = byteswapOnLittleEndian(v);
    return;
} 

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) -1; 

//offsetof 是一个宏，
//用于获取结构体中某个成员相对于结构体起始位置的偏移量。
//偏移量通常以字节为单位表示
//#define offsetof(type, member) ((size_t) &(((type *)0)->member))
UnixAddress::UnixAddress(){
    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}
//struct sockaddr_un {
//   sa_family_t sun_family;   // 地址家族，必须是 AF_UNIX
//   char sun_path[108];       // 套接字路径（可以是文件系统路径或抽象命名空间中的名称）
//};
//sun_path[108] : 一个字符数组，用于存储套接字路径，
//最多为 108 个字节（长度因系统不同可能有所不同）。
//该路径可以是文件系统中的一个路径，
//表示一个绑定到文件系统的套接字，或者可以是一个抽象的名称（在 Linux 中以 \0 开头的路径）。

UnixAddress::UnixAddress(const std::string& path){
    memset(&m_addr,0,sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    //加上'\0'的长度
    m_length = path.size() + 1;

    if(!path.empty() && path[0] == '\0'){
        --m_length;
    }

    if(m_length <= sizeof(m_addr.sun_path)){
        throw std::logic_error("path too long");
    }

    //m_length 计算了 sockaddr_un 结构体的实际总长度，包括 sun_path 成员的长度。
    memcpy(m_addr.sun_path, path.c_str(), m_length);
    m_length += offsetof(sockaddr_un,sun_path);
}

void UnixAddress::setAddrLen(uint32_t v) {
    m_length = v;
}

const sockaddr* UnixAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* UnixAddress::getAddr(){
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const {
    return m_length;
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
    if(m_length > offsetof(sockaddr_un,sun_path)
        && m_addr.sun_path[0] == '\0') {
            return os << "\\0" << std::string(m_addr.sun_path + 1,
             m_length - offsetof(sockaddr_un,sun_path) -1);            
        }
    return os << m_addr.sun_path;    
}

UnKnownAddress::UnKnownAddress(int family){
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnKnownAddress::UnKnownAddress(const sockaddr& addr){
    m_addr = addr;
}

const sockaddr* UnKnownAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* UnKnownAddress::getAddr(){
    return (sockaddr*)&m_addr;
}

socklen_t UnKnownAddress::getAddrLen() const {
    return sizeof(m_addr);    
}

std::ostream& UnKnownAddress::insert(std::ostream& os) const {
    os << "[UnKnownAddress family =" << m_addr.sa_family << "]";
    return os;
}


std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.insert(os);
}


} // namespace cc
