#include "tcp_server.h"
#include "config.h"
#include "log.h"
#include "noncopyable.h"

namespace cc {

static cc::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
                cc::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), 
                "tcp server read timeout");//2 mins

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

TcpServer::TcpServer(cc::IOManager* worker,
                     cc::IOManager* io_worker,
                     cc::IOManager* accept_worker)
    :m_worker(worker)
    ,m_ioWorker(io_worker)
    ,m_acceptWorker(accept_worker)
    ,m_recvTimeout(g_tcp_server_read_timeout->getValue())
    ,m_name("cc/1.0.0")
    ,m_isStop(true) {
}

//全部socket关闭
TcpServer::~TcpServer() {
    for(auto& i : m_socks) {
        i->close();
    }
    m_socks.clear();
}

// void TcpServer::setConf(const TcpServerConf& v) {
//     m_conf.reset(new TcpServerConf(v));
// }

bool TcpServer::bind(cc::Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs
                          ,std::vector<Address::ptr>& fails
                          ,bool ssl) {
    //暂时省略了ssl
    // m_ssl = ssl;
    for(auto& addr : addrs) {    
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock->bind(addr)) { //绑定失败，记录失败的地址
            CC_LOG_ERROR(g_logger) << "bind fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        if(!sock->listen()) {  //监听失败，记录失败的地址
            CC_LOG_ERROR(g_logger) << "listen fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        //绑定成功且进入监听状态
        m_socks.push_back(sock);
    }

    //如果绑定失败的地址容器不为空，bind调用函数返回false，清空所有Socket
    if(!fails.empty()) {
        m_socks.clear();
        return false;
    }

    //输出绑定成功的地址
    for(auto& i : m_socks) {
        CC_LOG_INFO(g_logger) 
            // << "type=" << m_type
            // << " name=" << m_name
            // << " ssl=" << m_ssl
            << " server bind success: " << *i;//重载了 "<<"
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while(!m_isStop) {
        Socket::ptr client = sock->accept();
        if(client) {
            //成功接收一个连接，设置对应的超时时间
            client->setRecvTimeout(m_recvTimeout);
            //将handleClient加入到工作线程队列m_worker中
            m_worker->schedule(std::bind(&TcpServer::handleClient,
                        shared_from_this(), client));
        } else {
            CC_LOG_ERROR(g_logger) << "accept errno=" << errno
                << " errstr=" << strerror(errno);
        }
    }
}

//start的功能就是把每一个监听状态的socket都绑定startaccpet加入调度
bool TcpServer::start() {
    if(!m_isStop) {
        return true;
    }
    m_isStop = false;
    for(auto& sock : m_socks) {
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept,
                            shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    // 使用shared_from_this()获取当前TcpServer对象的共享指针，并将其存储在局部变量self中。
    // 这是为了确保在异步任务执行期间TcpServer对象不会被销毁
    auto self = shared_from_this();
    m_acceptWorker->schedule([this, self]() {
        for(auto& sock : m_socks){
            sock->cancelall();
            sock->close();
        }
        m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    CC_LOG_INFO(g_logger) << "handleClient: " << *client;
}

// bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
//     for(auto& i : m_socks) {
//         auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
//         if(ssl_socket) {
//             if(!ssl_socket->loadCertificates(cert_file, key_file)) {
//                 return false;
//             }
//         }
//     }
//     return true;
// }

std::string TcpServer::toString(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "[type=" << m_type
       //<< " name=" << m_name << " ssl=" << m_ssl
       << " worker=" << (m_worker ? m_worker->getName() : "")
       << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
       << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for(auto& i : m_socks) {
        ss << pfx << pfx << *i << std::endl;
    }
    return ss.str();
}

}
