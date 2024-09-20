#ifndef __CC_HTTP_SERVLET_H__
#define __CC_HTTP_SERVLET_H__

#include <memory>
#include "http_session.h"
#include "http.h"
#include <map>
#include "../thread.h"
#include <unordered_map>


namespace cc{
namespace http{

// Servlet基于Java Servlet 
// 是运行在 Web 服务器或应用服务器上的程序，
// 它是作为来自 Web 浏览器或其他 HTTP 客户端的请求
// 和 HTTP 服务器上的数据库或应用程序之间的中间层。
class Servlet {
public:

    using ptr = std::shared_ptr<Servlet>;

    /*
     * 构造函数
     * name 名称
     */
    Servlet(const std::string& name)
        :m_name(name) {}

    /*
     * 析构函数
     */
    virtual ~Servlet() {}

    /*
     * 处理请求
     * request HTTP请求
     * response HTTP响应
     * session HTTP连接
     * return 是否处理成功
     */
    virtual int32_t handle(cc::http::HttpRequest::ptr request
                   , cc::http::HttpResponse::ptr response
                   , cc::http::HttpSession::ptr session) = 0;
                   
    /*
     * 返回Servlet名称
     */
    const std::string& getName() const { return m_name;}
protected:
    /// 名称
    std::string m_name;
};

/**
 * 函数式Servlet
 */
class FunctionServlet : public Servlet {
public:

    using ptr = std::shared_ptr<FunctionServlet>;
    // 函数回调类型定义
    using callback = std::function<int32_t (cc::http::HttpRequest::ptr request
                   , cc::http::HttpResponse::ptr response
                   , cc::http::HttpSession::ptr session)>;

    /**
     * 构造函数
     * cb 回调函数
     */
    FunctionServlet(callback cb);
    virtual int32_t handle(cc::http::HttpRequest::ptr request
                   , cc::http::HttpResponse::ptr response
                   , cc::http::HttpSession::ptr session) override;
private:
    // 回调函数
    callback m_cb;
};

/**
 *  Servlet分发器
 */
class ServletDispatch : public Servlet {
public:

    using ptr = std::shared_ptr<ServletDispatch>;
    // 读写锁类型定义
    using RWMutexType = RWMutex ;

    /**
     * 构造函数
     */
    ServletDispatch();
    virtual int32_t handle(cc::http::HttpRequest::ptr request
                   , cc::http::HttpResponse::ptr response
                   , cc::http::HttpSession::ptr session) override;

    /**
     * 添加servlet
     * uri
     * serlvet
     */
    void addServlet(const std::string& uri, Servlet::ptr slt);

    /**
     * 添加servlet
     * uri
     * FunctionServlet回调函数
     */
    void addServlet(const std::string& uri, FunctionServlet::callback cb);

    /**
     * 添加模糊匹配servlet
     * uri uri 模糊匹配 /cc_*
     * slt servlet
     */
    void addGlobServlet(const std::string& uri, Servlet::ptr slt);

    /**
     *  添加模糊匹配servlet
     *  uri uri 模糊匹配 /cc_*
     *  cb FunctionServlet回调函数
     */
    void addGlobServlet(const std::string& uri, FunctionServlet::callback cb);

    //void addServletCreator(const std::string& uri, IServletCreator::ptr creator);
    //void addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator);

    // template<class T>
    // void addServletCreator(const std::string& uri) {
    //     addServletCreator(uri, std::make_shared<ServletCreator<T> >());
    // }

    // template<class T>
    // void addGlobServletCreator(const std::string& uri) {
    //     addGlobServletCreator(uri, std::make_shared<ServletCreator<T> >());
    // }

    /**
     *  删除servlet
     *  uri
     */
    void delServlet(const std::string& uri);

    /**
     *  删除模糊匹配servlet
     *  uri
     */
    void delGlobServlet(const std::string& uri);

    /**
     *  返回默认servlet
     */
    Servlet::ptr getDefault() const { return m_default;}

    /**
     *  设置默认servlet
     *  v servlet
     */
    void setDefault(Servlet::ptr v) { m_default = v;}


    /**
     * 通过uri获取servlet
     * uri uri
     * 返回对应的servlet
     */
    Servlet::ptr getServlet(const std::string& uri);

    /**
     * 通过uri获取模糊匹配servlet
     * uri uri
     * 返回对应的servlet
     */
    Servlet::ptr getGlobServlet(const std::string& uri);

    /**
     * 通过uri获取servlet
     * uri uri
     * 优先精准匹配,其次模糊匹配,最后返回默认
     */
    Servlet::ptr getMatchedServlet(const std::string& uri);

    //void listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
    //void listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
private:
    // 读写互斥量
    RWMutexType m_mutex;
    // 精准匹配servlet MAP
    // uri(/cc/xxx) -> servlet
    std::unordered_map<std::string, Servlet::ptr> m_datas;
    // 模糊匹配servlet 数组
    // uri(/cc/*) -> servlet
    std::vector<std::pair<std::string, Servlet::ptr> > m_globs;
    // 默认servlet，所有路径都没匹配到时使用
    Servlet::ptr m_default;
};

/**
 *  NotFoundServlet(默认返回404)
 */
class NotFoundServlet : public Servlet {
public:
    /// 智能指针类型定义
    using ptr = std::shared_ptr<NotFoundServlet>;
    /**
     *  构造函数
     */
    NotFoundServlet(const std::string& name);
    virtual int32_t handle(cc::http::HttpRequest::ptr request
                   , cc::http::HttpResponse::ptr response
                   , cc::http::HttpSession::ptr session) override;

private:
    std::string m_name;
    std::string m_content;
};


}
}


#endif
