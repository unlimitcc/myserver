#ifndef __CC_HTTP_SERVER_H__
#define __CC_HTTP_SERVER_H__

#include "http.h"
#include "http_session.h"
#include "../tcp_server.h"
#include "servlet.h"

namespace cc{
namespace http{

class HttpServer : public TcpServer {
public:
    using ptr = std::shared_ptr<HttpServer>;

    /**
     * 构造函数
     * keepalive 是否长连接
     * worker 工作调度器
     * accept_worker 接收连接调度器
     */
    HttpServer(bool keepalive = false
               ,cc::IOManager* worker = cc::IOManager::GetThis()
               ,cc::IOManager* accept_worker = cc::IOManager::GetThis());

    /**
     * 获取ServletDispatch
     */
    ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}

    /**
     * 设置ServletDispatch
     */
    void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v;}

    virtual void setName(const std::string& v) override;
protected:

    virtual void handleClient(Socket::ptr client) override;
private:
    // 是否支持长连接
    bool m_isKeepalive;
    // Servlet分发器
    ServletDispatch::ptr m_dispatch;
};

}

}


#endif