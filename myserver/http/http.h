#ifndef __CC_HTTP_HTTP_H__
#define __CC_HTTP_HTTP_H__

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <boost/lexical_cast.hpp>
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace cc{

namespace http{

/* Request Methods */
#define HTTP_METHOD_MAP(XX)         \
  XX(0,  DELETE,      DELETE)       \
  XX(1,  GET,         GET)          \
  XX(2,  HEAD,        HEAD)         \
  XX(3,  POST,        POST)         \
  XX(4,  PUT,         PUT)          \
  /* pathological */                \
  XX(5,  CONNECT,     CONNECT)      \
  XX(6,  OPTIONS,     OPTIONS)      \
  XX(7,  TRACE,       TRACE)        \
  /* WebDAV */                      \
  XX(8,  COPY,        COPY)         \
  XX(9,  LOCK,        LOCK)         \
  XX(10, MKCOL,       MKCOL)        \
  XX(11, MOVE,        MOVE)         \
  XX(12, PROPFIND,    PROPFIND)     \
  XX(13, PROPPATCH,   PROPPATCH)    \
  XX(14, SEARCH,      SEARCH)       \
  XX(15, UNLOCK,      UNLOCK)       \
  XX(16, BIND,        BIND)         \
  XX(17, REBIND,      REBIND)       \
  XX(18, UNBIND,      UNBIND)       \
  XX(19, ACL,         ACL)          \
  /* subversion */                  \
  XX(20, REPORT,      REPORT)       \
  XX(21, MKACTIVITY,  MKACTIVITY)   \
  XX(22, CHECKOUT,    CHECKOUT)     \
  XX(23, MERGE,       MERGE)        \
  /* upnp */                        \
  XX(24, MSEARCH,     M-SEARCH)     \
  XX(25, NOTIFY,      NOTIFY)       \
  XX(26, SUBSCRIBE,   SUBSCRIBE)    \
  XX(27, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  XX(28, PATCH,       PATCH)        \
  XX(29, PURGE,       PURGE)        \
  /* CalDAV */                      \
  XX(30, MKCALENDAR,  MKCALENDAR)   \
  /* RFC-2068, section 19.6.1.2 */  \
  XX(31, LINK,        LINK)         \
  XX(32, UNLINK,      UNLINK)       \
  /* icecast */                     \
  XX(33, SOURCE,      SOURCE)       \

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                 \
  XX(100, CONTINUE,                        Continue)                        \
  XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)             \
  XX(102, PROCESSING,                      Processing)                      \
  XX(200, OK,                              OK)                              \
  XX(201, CREATED,                         Created)                         \
  XX(202, ACCEPTED,                        Accepted)                        \
  XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)   \
  XX(204, NO_CONTENT,                      No Content)                      \
  XX(205, RESET_CONTENT,                   Reset Content)                   \
  XX(206, PARTIAL_CONTENT,                 Partial Content)                 \
  XX(207, MULTI_STATUS,                    Multi-Status)                    \
  XX(208, ALREADY_REPORTED,                Already Reported)                \
  XX(226, IM_USED,                         IM Used)                         \
  XX(300, MULTIPLE_CHOICES,                Multiple Choices)                \
  XX(301, MOVED_PERMANENTLY,               Moved Permanently)               \
  XX(302, FOUND,                           Found)                           \
  XX(303, SEE_OTHER,                       See Other)                       \
  XX(304, NOT_MODIFIED,                    Not Modified)                    \
  XX(305, USE_PROXY,                       Use Proxy)                       \
  XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)              \
  XX(308, PERMANENT_REDIRECT,              Permanent Redirect)              \
  XX(400, BAD_REQUEST,                     Bad Request)                     \
  XX(401, UNAUTHORIZED,                    Unauthorized)                    \
  XX(402, PAYMENT_REQUIRED,                Payment Required)                \
  XX(403, FORBIDDEN,                       Forbidden)                       \
  XX(404, NOT_FOUND,                       Not Found)                       \
  XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)              \
  XX(406, NOT_ACCEPTABLE,                  Not Acceptable)                  \
  XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)   \
  XX(408, REQUEST_TIMEOUT,                 Request Timeout)                 \
  XX(409, CONFLICT,                        Conflict)                        \
  XX(410, GONE,                            Gone)                            \
  XX(411, LENGTH_REQUIRED,                 Length Required)                 \
  XX(412, PRECONDITION_FAILED,             Precondition Failed)             \
  XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)               \
  XX(414, URI_TOO_LONG,                    URI Too Long)                    \
  XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)          \
  XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)           \
  XX(417, EXPECTATION_FAILED,              Expectation Failed)              \
  XX(421, MISDIRECTED_REQUEST,             Misdirected Request)             \
  XX(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)            \
  XX(423, LOCKED,                          Locked)                          \
  XX(424, FAILED_DEPENDENCY,               Failed Dependency)               \
  XX(426, UPGRADE_REQUIRED,                Upgrade Required)                \
  XX(428, PRECONDITION_REQUIRED,           Precondition Required)           \
  XX(429, TOO_MANY_REQUESTS,               Too Many Requests)               \
  XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large) \
  XX(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)   \
  XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)           \
  XX(501, NOT_IMPLEMENTED,                 Not Implemented)                 \
  XX(502, BAD_GATEWAY,                     Bad Gateway)                     \
  XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)             \
  XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                 \
  XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)      \
  XX(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)         \
  XX(507, INSUFFICIENT_STORAGE,            Insufficient Storage)            \
  XX(508, LOOP_DETECTED,                   Loop Detected)                   \
  XX(510, NOT_EXTENDED,                    Not Extended)                    \
  XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required) \

/**
 * HTTP方法枚举
 * enum class 的枚举值不能隐式转换为其他类型（如整数）
 * enum class 定义的枚举值被限定在其作用域内
 * 方法格式: 名称 = 请求方法代码
 */
enum class HttpMethod {
#define XX(num, name, string) name = num,
    HTTP_METHOD_MAP(XX)
#undef XX
    INVALID_METHOD 
};

/**
 * HTTP状态枚举
 */
enum class HttpStatus {
#define XX(code, name, desc) name = code,
    HTTP_STATUS_MAP(XX)
#undef XX
};


// 将字符串方法名转成HTTP方法
// m: HTTP方法
// HTTP方法枚举
HttpMethod StringToHttpMethod(const std::string& m);

HttpMethod CharsToHttpMethod(const char* m);


// 将HTTP方法枚举转换成字符串
// m: HTTP方法枚举
// 字符串
const char* HttpMethodToString(const HttpMethod& m);

const char* HttpStatusToString(const HttpStatus& s);

struct CaseInsensitiveLess {
    /**
     * 忽略大小写比较字符串
     */
    bool operator()(const std::string& lhs, const std::string& rhs) const;
};


/**
 * 获取Map中的key值,并转成对应类型T,返回是否成功
 * m Map数据结构
 * key 关键字
 * val 保存转换后的值
 * def 默认值
 * return
 *      true 转换成功, val 为对应的值
 *      false 不存在或者转换失败 val = def
 */
template<class MapType, class T>
bool checkGetAs(const MapType& m, const std::string& key, T& val, const T& def = T()) {
    auto it = m.find(key);
    if(it == m.end()) {
        val = def;
        return false;
    }
    try {
        val = boost::lexical_cast<T>(it->second);
        return true;
    } catch (...) {
        val = def;
    }
    return false;
}

/**
 * 获取Map中的key值,并转成对应类型
 * m Map数据结构
 * key 关键字
 * def 默认值
 * 如果存在且转换成功返回对应的值,否则返回默认值
 */
template<class MapType, class T>
T getAs(const MapType& m, const std::string& key, const T& def = T()) {
    auto it = m.find(key);
    if(it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<T>(it->second);
    } catch (...) {
    }
    return def;
}

class HttpResponse;

class HttpRequest{

public:
    using ptr = std::shared_ptr<HttpRequest>;
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess> ;

    //默认版本1.1
    HttpRequest(uint8_t version = 0x11, bool close = true);

    HttpMethod getMethod() const {return m_method;}
    uint8_t    getVersion() const {return m_version;}
    HttpStatus getStatus() const {return m_status;}

    const std::string& getPath() const {return m_path;}

    /**
     * 返回响应消息体
     * 消息体
     */
    const std::string& getBody() const { return m_body;}

    /**
     * 返回响应头部MAP
     * MAP
     */
    const MapType& getHeader() const { return m_headers;}
    const MapType& getParam() const { return m_params;}
    const MapType& getCookie() const { return m_cookies;}

    //是否自动关闭
    bool isClose() {return m_close;}

    void setClose(bool v) {m_close = v;}

    // 设置HTTP响应状态
    void setStatus(HttpStatus v) { m_status = v;}
    void setVersion(uint8_t v) { m_version = v;}
    // 设置HTTP请求的路径
    void setPath(const std::string& v) {m_path = v;}
    // 设置HTTP请求的查询参数
    void setQuery(const std::string& v) {m_query = v;}
    // 设置HTTP请求的Fragment
    void setFragment(const std::string& v) {m_fragment = v;}

    // 设置响应消息体,其他类似
    void setBody(const std::string& v) { m_body = v;}
    void setHeaders(const MapType& v) {m_headers = v;}
    void setParams(const MapType& v) {m_params = v;}
    // 设置HTTP请求的Cookie，cookies是一个MAP
    void setCookies(const MapType& v) {m_cookies = v;}
    void setMethod(HttpMethod v) { m_method = v;}

    /**
     * 获取响应头部参数
     * key 关键字
     * def 默认值
     * 如果存在返回对应值,否则返回def
     */
    std::string getHeader(const std::string& key, const std::string& def = "") const;
    std::string getParam(const std::string& key, const std::string& def = "");
    std::string getCookie(const std::string& key, const std::string& def = "");

    /**
     * 设置响应头部参数
     * key 关键字
     * val 值
     */
    void setHeader(const std::string& key, const std::string& val);
    void setParam(const std::string& key, const std::string& val);
    void setCookie(const std::string& key, const std::string& val);

    /**
     * 删除响应头部参数
     * key 关键字
     */
    void delHeader(const std::string& key);
    void delParam(const std::string& key);
    void delCookie(const std::string& key);

    /**
     * 判断HTTP请求的头部参数是否存在
     * key 关键字
     * val 如果存在,val非空则赋值
     * 是否存在
     */
    bool hasHeader(const std::string& key, std::string* val = nullptr);
    bool hasParam(const std::string& key, std::string* val = nullptr);
    bool hasCookie(const std::string& key, std::string* val = nullptr);

    /*
     * 检查并获取HTTP请求的头部参数
     * @tparam T 转换类型
     * key 关键字
     * val 返回值
     * def 默认值
     * 如果存在且转换成功返回true,否则失败val=def
     */
    template<class T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
     * 获取HTTP请求的头部参数
     * @tparam T 转换类型
     * key 关键字
     * def 默认值
     * 如果存在且转换成功返回对应的值,否则返回def
     */
    template<class T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    /**
     * 检查并获取HTTP请求的请求参数
     * @tparam T 转换类型
     * key 关键字
     * val 返回值
     * def 默认值
     * 如果存在且转换成功返回true,否则失败val=def
     */
    template<class T>
    bool checkGetParamAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
     * 获取HTTP请求的请求参数
     * @tparam T 转换类型
     * key 关键字
     * def 默认值
     * 如果存在且转换成功返回对应的值,否则返回def
     */
    template<class T>
    T getParamAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    template<class T>
    bool checkGetCookieAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    template<class T>
    T getCookieAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }


    std::string toString() const;

    //序列化输出到流中
    std::ostream& dump(std::ostream& os) const;
private:

    /**
     * 获取Map中的key值,并转成对应类型,返回是否成功
     * m Map数据结构
     * key 关键字
     * val 保存转换后的值
     * def 默认值
     * return
     *      true 转换成功, val 为对应的值
     *      false 不存在或者转换失败 val = def
     */
    template<class MapType, class T>
    bool checkGetAs(const MapType& m, const std::string& key, T& val, const T& def = T()) {
        auto it = m.find(key);
        if(it == m.end()) {
            val = def;
            return false;
        }
        try {
            val = boost::lexical_cast<T>(it->second);
            return true;
        } catch (...) {
            val = def;
        }
        return false;
    }

    /**
     * 获取Map中的key值,并转成对应类型
     * m Map数据结构
     * key 关键字
     * def 默认值
     * 如果存在且转换成功返回对应的值,否则返回默认值
     */
    template<class MapType, class T>
    T getAs(const MapType& m, const std::string& key, const T& def = T()) {
        auto it = m.find(key);
        if(it == m.end()) {
            return def;
        }
        try {
            return boost::lexical_cast<T>(it->second);
        } catch (...) {
        }
        return def;
    }

private:
    HttpMethod m_method;
    HttpStatus m_status;
    //0x11 -> HTTP 1.1
    uint8_t m_version; 
    //长连接(false)还是非长连接(true) 
    bool m_close;      

    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    std::string m_body;

    MapType m_headers;
    //HTTP请求的参数MAP
    MapType m_params;
    MapType m_cookies;

};


class HttpResponse{

public:
    // HTTP响应结构智能指针
    using ptr = std::shared_ptr<HttpResponse>;
    // MapType
    using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;
    
    HttpResponse(uint8_t version = 0x11, bool close = true);

    // 返回响应状态
    HttpStatus getStatus() const { return m_status;}

    uint8_t getVersion() const { return m_version;}

    /**
     * 返回响应消息体
     * 消息体
     */
    const std::string& getBody() const { return m_body;}

    /**
     * 返回响应原因
     */
    const std::string& getReason() const { return m_reason;}

    /**
     * 返回响应头部MAP
     * MAP
     */
    const MapType& getHeaders() const { return m_headers;}

    /**
     * 设置响应状态
     * v 响应状态
     */
    void setStatus(HttpStatus v) { m_status = v;}

    void setVersion(uint8_t v) { m_version = v;}

    /**
     * 设置响应消息体
     * v 消息体
     */
    void setBody(const std::string& v) { m_body = v;}

    /**
     * 设置响应原因
     * v 原因
     */
    void setReason(const std::string& v) { m_reason = v;}

    /**
     * 设置响应头部MAP
     * v MAP
     */
    void setHeaders(const MapType& v) { m_headers = v;}

    bool isClose() {return m_close;}
    void setClose(bool v) {m_close = v;}

    std::string getHeader(const std::string& key, const std::string& def = "") const;
    // 设置响应头部参数
    void setHeader(const std::string& key, const std::string& val);
    void delHeader(const std::string& key);


    //def 默认值，有值返回值，无值返回def
    template<class T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
     * 获取HTTP请求的头部参数
     * @tparam T 转换类型
     * key 关键字
     * def 默认值
     * 如果存在且转换成功返回对应的值,否则返回def
     */
    template<class T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    std::string toString() const;

    std::ostream& dump(std::ostream& os) const;
private:

    HttpStatus m_status;
    uint8_t m_version;
    bool m_close;
    std::string m_body;
    std::string m_reason;
    MapType m_headers;

};

/**
 * 流式输出HttpRequest
 * os 输出流
 * req HTTP请求
 * 输出流
 */
std::ostream& operator<<(std::ostream& os, const HttpRequest& req);

/**
 * 流式输出HttpResponse
 * os 输出流
 * rsp HTTP响应
 * 输出流
 */
std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp);


}
}


#endif