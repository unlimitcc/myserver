#ifndef __CC_TCP_SERVER_H__
#define __CC_TCP_SERVER_H__

#include <memory>
#include <functional>
#include "address.h"
#include "iomanager.h"
#include "log.h"
#include "socket.h"

//实现功能:将IP地址绑定到socket，及一些错误处理
//支持同时绑定多个地址进行监听，只需要在绑定时传入地址数组即可。
//TcpServer还可以分别指定接收客户端和处理客户端的协程调度器。
namespace cc{

//当调用 shared_from_this() 时，它会返回一个指向当前对象的 std::shared_ptr，
//该指针与外部的 std::shared_ptr 共享引用计数。
//这会增加引用计数。引用计数的增加确保了对象在多个地方同时使用时不会被提前销毁
class TcpServer : public std::enable_shared_from_this<TcpServer>
                    , Noncopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    /**
     * 构造函数
     * worker 新连接的socket工作的协程调度器
     * accept_worker 服务器接收socket连接的调度器
     * io_worker 暂时未使用
     */
    TcpServer(cc::IOManager* worker = cc::IOManager::GetThis()
              ,cc::IOManager* io_woker = cc::IOManager::GetThis()
              ,cc::IOManager* accept_worker = cc::IOManager::GetThis());

    /**
     * 析构函数
     */
    virtual ~TcpServer();

    /**
     * 绑定地址
     * 返回是否绑定成功
     */
    virtual bool bind(cc::Address::ptr addr, bool ssl = false);

    /**
     * 绑定地址数组
     * addrs 需要绑定的地址数组
     * fails 绑定失败的地址
     * 是否绑定成功
     */
    virtual bool bind(const std::vector<Address::ptr>& addrs
                        ,std::vector<Address::ptr>& fails
                        ,bool ssl = false);

    /**
     * 启动服务
     * 需要bind成功后执行
     */
    virtual bool start();

    /**
     * 停止服务
     */
    virtual void stop();

    /**
     * 返回读取超时时间(毫秒)
     */
    uint64_t getRecvTimeout() const { return m_recvTimeout;}

    /**
     * 返回服务器名称
     */
    std::string getName() const { return m_name;}

    /**
     * 设置读取超时时间(毫秒)
     */
    void setRecvTimeout(uint64_t v) { m_recvTimeout = v;}

    /**
     * 设置服务器名称
     */
    virtual void setName(const std::string& v) { m_name = v;}

    /**
     * 是否停止
     */
    bool isStop() const { return m_isStop;}

    virtual std::string toString(const std::string& prefix = "");

    std::vector<Socket::ptr> getSocks() const { return m_socks;}
protected:

    /**
     * 处理新连接的Socket
     */
    virtual void handleClient(Socket::ptr client);

    /**
     * 开始接受连接
     */
    virtual void startAccept(Socket::ptr sock);
protected:
    // 监听的Socket数组
    std::vector<Socket::ptr> m_socks;
    // 新连接的Socket的调度器
    IOManager* m_worker;
    IOManager* m_ioWorker;
    // 服务器Socket接收连接的调度器
    IOManager* m_acceptWorker;
    // 接收超时时间(毫秒)
    uint64_t m_recvTimeout;
    // 服务器名称
    std::string m_name;
    // 服务器类型
    std::string m_type = "tcp";
    // 服务是否停止
    bool m_isStop;
    
    bool m_ssl = false;

    //TcpServerConf::ptr m_conf;
};


}

#endif