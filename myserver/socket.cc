#include "socket.h"
#include "fd_manager.h"
#include "iomanager.h"
#include "log.h"
#include <limits.h>
#include <netinet/tcp.h>
#include "macro.h"
#include "hook.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>

namespace cc{

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

Socket::ptr Socket::CreateTCP(cc::Address::ptr address){
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(cc::Address::ptr address){
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    //sock->newSock();
    //sock->m_isConnected = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket(){
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket(){
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6(){
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6(){
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket(){
    Socket::ptr sock(new Socket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket(){
    Socket::ptr sock(new Socket(UNIX, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    :m_sock(-1)
    ,m_family(family)
    ,m_type(type)
    ,m_protocol(protocol)
    ,m_isConnected(false) {
}

Socket::~Socket() {
    close();
}

//套接字选项SO_RCVTIMEO：用来设置socket接收数据的超时时间
//套接字选项SO_SNDTIMEO：用来设置socket发送数据的超时时间
int64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx) {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t v) {
    struct timeval tv{ int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx) {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

//getsockopt 是用于获取套接字（socket）选项值的函数，
//它允许你查询套接字的当前配置，包括超时设置、缓冲区大小等
//int getsockopt(int sockfd, int level, int option, void *result, socklen_t *len);
//  sockfd: 套接字文件描述符，通过 socket() 或其他创建的套接字句柄。
//  level: 指定选项的协议层次。通常是 SOL_SOCKET（套接字层），也可能是其他协议（例如 IPPROTO_TCP）。
//  option: 想要获取的套接字选项名称，例如 SO_SNDTIMEO、SO_RCVBUF 等。
//  result: 指向保存选项值的缓冲区的指针。
//  len: 输入输出参数，输入时表示 result 缓冲区的大小，输出时表示实际返回的数据大小。
//成功时返回 0。
//失败时返回 -1，并设置 errno 来指示错误类型。

//获取socket指定选项的值，具体内容存在result中
bool Socket::getOption(int level, int option, void* result, socklen_t* len) {
    int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
    if(rt) {
        CC_LOG_DEBUG(g_logger) << "getOption sock=" << m_sock
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

//用于设置套接字的选项。它允许你修改套接字的行为，如超时时间、缓冲区大小、选项标志等。
//int setsockopt(int sockfd, int level, int option, const void *result, socklen_t optlen);
//参数定义与getsocket相同
bool Socket::setOption(int level, int option, const void* result, socklen_t len){
    if(setsockopt(m_sock, level, option, result, (socklen_t)len)){
        CC_LOG_DEBUG(g_logger) << "setOption sock=" << m_sock
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

// 创建新的socket对象，用于与客户端通信
Socket::ptr Socket::accept(){
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
    //int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    //sockfd：监听套接字的文件描述符，它是通过 socket() 创建并绑定和监听的。
    //addr：指向 sockaddr 结构的指针，用于存储客户端的地址信息。
    //  可以传 NULL，表示不关心客户端的地址。
    //addrlen：指向一个 socklen_t 变量的指针，用于指定 addr 结构的大小并返回实际使用的大小。
    //  可以传 NULL，表示不需要获取地址长度。
    //该函数从监听套接字（listen socket）中接受一个挂起的连接请求
    //  并返回一个新的已连接套接字（connected socket）用于与客户端通信。
    //  被调用时，若没有客户端连接请求，则阻塞等待
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if(newsock == -1){
        CC_LOG_ERROR(g_logger) << "accept (" << m_sock << ") errno=" 
                               << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    
    if(sock->init(newsock)){
        return sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr addr){
    //无效的话先创建一个新的socket
    if(CC_UNLIKELY(!isValid())){
        newSock();
        if(CC_UNLIKELY(!isValid())){
            return false;
        }
    }
    //协议簇不一样
    if(CC_UNLIKELY(addr->getFamily() != m_family)) {
        CC_LOG_ERROR(g_logger) << "bind sock.family("
            << m_family << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }
    //为socket绑定一个地址
    if(::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
        CC_LOG_ERROR(g_logger) << "bind error errrno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }

    getLocalAddress();

    return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms){
    if(CC_UNLIKELY(!isValid())){
        newSock();
        if(CC_UNLIKELY(!isValid())){
            return false;
        }
    }

    if(CC_UNLIKELY(addr->getFamily() != m_family)) {
        CC_LOG_ERROR(g_logger) << "bind sock.family("
            << m_family << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }

    //未设置超时时间
    if(timeout_ms == (uint64_t)-1) {
        //连接失败
        if(::connect(m_sock, addr->getAddr(), addr->getAddrLen())){
            CC_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                << ") error errno=" << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    //设置了超时时间，连接超时
    } else {
        if(::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
            CC_LOG_ERROR(g_logger) << "sock=" << m_sock << " connect(" << addr->toString()
                << ") timeout=" << timeout_ms << " error errno="
                << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    
    //连接成功
    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog){
    if(!isValid()) {
        CC_LOG_ERROR(g_logger) << "listen error sock=-1";
        return false;
    }

    //backlog 
    //指定在套接字的监听队列中允许的最大未完成连接数。
    //这是一个整数值，表示允许在服务器接受连接之前，
    //操作系统内核中未完成的连接请求的最大数量。
    //超出这个数量的连接请求将被拒绝或丢弃。
    //listen 函数本身不会阻塞。它的作用是将套接字设置为被动监听状态，
    //准备接受连接请求。具体来说，listen 告诉操作系统该套接字将用于接受传入的连接请求，
    //并设置连接队列的长度，但并不会等到有连接到来或阻塞程序执行。
    if(::listen(m_sock, backlog)) {
        CC_LOG_ERROR(g_logger) << "listen error errno=" << errno
                               << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}
    
bool Socket::close(){
    if(!m_isConnected && m_sock == -1) {
        return true;
    }
    m_isConnected = false;
    if(m_sock != -1) {
        ::close(m_sock);
        m_sock = -1;
    }
    return false;
}

int Socket::send(const void* buffer, size_t length, int flags){
    if(isConnected()){
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

//发送多个内存块的数据
//sendmsg: 使用 iovec 数组来发送多个非连续的内存块，使得数据发送更加灵活和高效
// struct msghdr {
//     void         *msg_name;       // 指向套接字地址的指针（可选）
//     socklen_t     msg_namelen;    // 套接字地址的长度
//     struct iovec *msg_iov;        // 指向 iovec 数组的指针，用于存放数据
//     int           msg_iovlen;     // iovec 数组的元素个数
//     void         *msg_control;    // 指向辅助数据（控制消息）的指针
//     socklen_t     msg_controllen; // 辅助数据的大小
//     int           msg_flags;      // 消息传输的标志
// };
int Socket::send(const iovec* buffer, size_t length, int flags){
    if(isConnected()){
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

//函数用于在套接字上发送数据，并允许指定目的地的地址。
//它常用于无连接的套接字，如 UDP 套接字，但也可以用于已连接的套接字。
int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags){
    if(isConnected()){
        return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}


int Socket::sendTo(const iovec* buffer, size_t length, const Address::ptr to, int flags){
    //sendmsg 是一个高级的套接字发送函数，提供了发送多部分数据和控制信息的能力。
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}


int Socket::recv(void* buffer, size_t length, int flags){
    if(isConnected()) {
        //sock 已连接的socket, buffer:接收数据的存储缓冲区, len指定的接收的数据量
        //flag 这是接收操作的标志位, 用来控制接收的行为
        return ::recv(m_sock, buffer, length, flags);
    }
    return -1;
}

//ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
//sockfd: 指定从哪个套接字接收数据。
//msghdr: 描述接收的数据、控制消息、发送者地址
int Socket::recv(iovec* buffer, size_t length, int flags){
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags){
    if(isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sock, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffer, size_t length, Address::ptr from, int flags){
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffer;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress(){
    if(m_remoteAddress){
        return m_remoteAddress;
    }

    Address::ptr result;
    switch(m_family) {
    case AF_INET:
        result.reset(new IPv4Address());
        break;
    case AF_INET6:
        result.reset(new IPv6Address());
        break;
    case AF_UNIX:
        result.reset(new UnixAddress());
        break;
    default:
        result.reset(new UnKnownAddress(m_family));
        break;
    }

    socklen_t addrlen = result->getAddrLen();
    // getpeername函数用于在网络编程中获取已连接套接字的对端地址。
    // 它常用于通过套接字文件描述符获取远程端的连接信息。
    // m_sock是通过accept返回的已经建立连接的socket
    if(getpeername(m_sock, result->getAddr(), &addrlen)){
        CC_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
                               << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnKnownAddress(m_family));
    }

    if(m_family == AF_UNIX){
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }

    m_remoteAddress = result;
    return m_remoteAddress;
}

Address::ptr Socket::getLocalAddress(){
    if(m_localAddress) {
        return m_localAddress;
    }

    Address::ptr result;
    switch(m_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnKnownAddress(m_family));
            break;
    }
    socklen_t addrlen = result->getAddrLen();
    //函数用于获取与套接字关联的本地地址信息。它通常用于在客户端或服务器程序中检查本地套接字的地址和端口。
    if(getsockname(m_sock, result->getAddr(), &addrlen)) {
        CC_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock
            << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnKnownAddress(m_family));
    }
    if(m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    m_localAddress = result;
    return m_localAddress;
}

bool Socket::isValid() const{
    return m_sock != -1;
}


int Socket::getError(){
    int error = 0;
    socklen_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        error = errno;
    }
    return error;
}

std::ostream& Socket::dump(std::ostream& os) const{
    os << "[Socket sock=" << m_sock
       << " is_connected=" << m_isConnected
       << " family=" << m_family
       << " type=" << m_type
       << " protocol=" << m_protocol;
    if(m_localAddress) {
        os << " local_address=" << m_localAddress->toString();
    }
    if(m_remoteAddress) {
        os << " remote_address=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}


bool Socket::cancelRead(){
    return IOManager::GetThis()->cancelEvent(m_sock, cc::IOManager::READ);
}

bool Socket::cancelWrite(){
    return IOManager::GetThis()->cancelEvent(m_sock, cc::IOManager::WRITE);
}

bool Socket::cancelAccept(){
    return IOManager::GetThis()->cancelEvent(m_sock, cc::IOManager::READ);
}

bool Socket::cancelall(){
    return IOManager::GetThis()->cancelAll(m_sock);
}

bool Socket::init(int sock){
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
    if(ctx && ctx->isSocket() && !ctx->isClose()){
        m_sock = sock;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

//初始化socket
void Socket::initSock(){
    int val = 1;
    //setOption(SOL_SOCKET, SO_REUSEADDR, val); 
    //  其中 val = 1 用于设置套接字选项 SO_REUSEADDR，
    //  它允许重新绑定一个已使用的地址和端口。这个选项对于服务器端套接字尤其重要，
    //  主要作用是在服务器重启或程序异常退出时，立即重新使用之前绑定的地址和端口，
    //  而不需要等待 TCP 协议的 TIME_WAIT 状态结束。
    setOption(SOL_SOCKET,SO_REUSEADDR,val);
    if(m_type == SOCK_STREAM){
        //val = 1 表示 启用 TCP_NODELAY 选项可以禁用Nagle算法，从而降低网络通信中的延迟，提高实时性。
        //Nagle算法: 发送数据大于MSS且当前窗口大于MSS才发送或者收到之前数据的ACK
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

//新建socket并且初始化
void Socket::newSock(){
    m_sock = socket(m_family, m_type, m_protocol);
    if(CC_LIKELY(m_sock != -1)){
        initSock();
    }else{
        CC_LOG_ERROR(g_logger) << "socket(" << m_family
            << ", " << m_type << ", " << m_protocol << ") errno="
            << errno << " errstr=" << strerror(errno);
    }
}

std::ostream& operator<<(std::ostream& os, const Socket& sock){
    return sock.dump(os);
}

}