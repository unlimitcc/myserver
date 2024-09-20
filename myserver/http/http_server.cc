#include "http_server.h"
#include "../log.h"


namespace cc {
namespace http {

static cc::Logger::ptr g_logger = CC_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive
               ,cc::IOManager* worker
               ,cc::IOManager* accept_worker)
    :TcpServer(worker, accept_worker)
    ,m_isKeepalive(keepalive) {

    m_dispatch.reset(new ServletDispatch);
    //m_type = "http";
    //m_dispatch->addServlet("/_/status", Servlet::ptr(new StatusServlet));
    //m_dispatch->addServlet("/_/config", Servlet::ptr(new ConfigServlet));
}

void HttpServer::setName(const std::string& v) {
    TcpServer::setName(v);
    // m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
    CC_LOG_DEBUG(g_logger) << "handleClient " << *client;
    HttpSession::ptr session(new HttpSession(client));
    do {
        auto req = session->recvRequest();
        if(!req) {
            CC_LOG_DEBUG(g_logger) << "recv http request fail, errno="
                << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }

        HttpResponse::ptr rsp(new HttpResponse(req->getVersion(),
                                req->isClose() || !m_isKeepalive));
        
        rsp->setBody("hello myserver");
        
        CC_LOG_INFO(g_logger) << "request : " << std::endl
                              << *req;

        CC_LOG_INFO(g_logger) << "reponse : " << std::endl
                              << *rsp;
        //rsp->setHeader("Server", getName());
        //使用servlet处理HTTP请求
        m_dispatch->handle(req, rsp, session);
        session->sendResponse(rsp);

        // if(!m_isKeepalive || req->isClose()) {
        //     break;
        // }
    } while(m_isKeepalive);//while(true);
    session->close();
}

}
}
