#ifndef __CC_LOG_H__
#define __CC_LOG_H__

#include <string>
#include <iostream>
#include <cstdint>
#include <memory>
#include <list>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include "singleton.h"
#include "util.h"
#include "thread.h"

//日志生成调用顺序 LogEvent -> Logger -> LogAppender -> LogFormatter -> FormatItem::format

#define CC_LOG(logger, level) \
    if(logger->getLevel() <= level) \
        cc::LogEventWrap(cc::LogEvent::ptr(new cc::LogEvent(logger, level, \
        __FILE__, __LINE__, 0, cc::GetThreadId(), cc::GetFiberId(),time(0),cc::Thread::GetName()))).getSS()
//wrapper析构时，输出event中固定的上述日志内容，并使用get.SS()接受自定义的日志内容，并输出

#define CC_LOG_DEBUG(logger) CC_LOG(logger, cc::LogLevel::DEBUG)
#define CC_LOG_INFO(logger) CC_LOG(logger, cc::LogLevel::INFO)
#define CC_LOG_WARN(logger) CC_LOG(logger, cc::LogLevel::WARN)
#define CC_LOG_ERROR(logger) CC_LOG(logger, cc::LogLevel::ERROR)
#define CC_LOG_FATAL(logger) CC_LOG(logger, cc::LogLevel::FATAL)


#define CC_LOG_FMT(logger, level, fmt, ...)\
    if(logger->getLevel() <= level) \
        cc::LogEventWrap(cc::LogEvent::ptr(new cc::LogEvent(logger, level,\
        __FILE__, __LINE__, 0, cc::GetThreadId(), \
        cc::GetFiberId(),time(0),cc::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)
//_VA_ARGS__表示的变量按照fmt定义的格式(类似于printf的输出)输入到m_ss里,根据自定义的输出日志格式，有%m格式时
//记录message，也就是m_ss中的内容


#define CC_LOG_FMT_DEBUG(logger, fmt, ...) CC_LOG_FMT(logger, cc::LogLevel::DEBUG, fmt,  __VA_ARGS__)
#define CC_LOG_FMT_INFO(logger, fmt, ...) CC_LOG_FMT(logger, cc::LogLevel::INFO, fmt,  __VA_ARGS__)
#define CC_LOG_FMT_WARN(logger, fmt, ...) CC_LOG_FMT(logger, cc::LogLevel::WARN, fmt,  __VA_ARGS__)
#define CC_LOG_FMT_ERROR(logger, fmt, ...) CC_LOG_FMT(logger, cc::LogLevel::ERROR, fmt,  __VA_ARGS__)
#define CC_LOG_FMT_FATAL(logger, fmt, ...) CC_LOG_FMT(logger, cc::LogLevel::FATAL, fmt,  __VA_ARGS__)

#define CC_LOG_ROOT() cc::LoggerMgr::GetInstance()->getRoot()
#define CC_LOG_NAME(name) cc::LoggerMgr::GetInstance()->getLogger(name)

namespace cc {


class Logger;
class LoggerManager;

//日志级别
class LogLevel{

public:
    enum Level{
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static const char* ToString(Level Level);
    static LogLevel::Level FromString(const std::string& str);
};

//日志事件
//一个构造函数
//其他都是取值或者设置值的函数
class LogEvent{

public:

    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, uint32_t elapse,
            uint64_t threadid, uint32_t fiberid, uint64_t time, const std::string& thread_name);

    //~LogEvent();
    const char* getFileName() const { return m_file;}
    int32_t getLine() const {return m_line;}
    uint32_t getElapse() const {return m_elapse;}
    uint64_t getThread() const {return m_threadid;}
    uint32_t getfiberid() const {return m_fiberid;}
    uint64_t getTime() const {return m_time;}
    const std::string& getThreadName() const {return m_threadName;}
    const std::string getContent() const {return m_ss.str();}

    LogLevel::Level getLevel() const {return m_level;}
    std::shared_ptr<Logger> getLogger () const {return m_logger;}

    std::stringstream& getSS() {return m_ss;}

    /*
     * 格式化写入日志内容
     */
    void format(const char* fmt, ...);

    /*
     * 格式化写入日志内容
     */
    void format(const char* fmt, va_list al);
private: 

    //文件名
    const char* m_file = nullptr;
    //行号
    int32_t m_line = 0;//行号
    uint32_t m_elapse = 0;//程序启动开始到现在的时间(ms)
    uint64_t m_threadid = 0;//线程号
    uint32_t m_fiberid = 0;//协程号
    uint64_t m_time;    //时间戳
    std::stringstream m_ss;//日志内容
    
    std::shared_ptr<Logger> m_logger;
    LogLevel::Level m_level;
    std::string m_threadName;
};

//日志包装器
//析构时打印对应的日志
class LogEventWrap{

public:

    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    //拿到m_event对应的ss
    //event的SS来源于,用于在具体位置打印特定数据
    std::stringstream& getSS();
    LogEvent::ptr getEvent() const {return m_event;}
private:
    LogEvent::ptr m_event;
};

//日志格式器
//格式化日志内容
class LogFormatter{

public:
    using ptr = std::shared_ptr<LogFormatter>;
    explicit LogFormatter(const std::string &pattern);

    //将日志中各项内容按照指定的格式记录到string或者ostream中，等待logger调用打印
    //logger则根据指定的输出位置调用对应的appender进行打印
    //%t(时间) %thread_id %m    
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    
public:
    //日志各项内容
    class FormatItem{
    public:
        using ptr = std::shared_ptr<FormatItem>;
        explicit FormatItem(const std::string& fmt = ""){};
        virtual ~FormatItem() = default;

        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };
    
    void init();

    bool isError() const {return m_error;}
    const std::string getPattern() const {return m_pattern;}
private:
    //日志格式例如"%s%T%d"之类
    std::string m_pattern;
    //存放具体item的项
    std::vector<FormatItem::ptr> m_item;
    bool m_error = false; 
};

//日志输出位置

class LogAppender{
friend class Logger;
public:
    using ptr = std::shared_ptr<LogAppender>;
    using MutexType = Spinlock;
    virtual ~LogAppender() = default;

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;//输出日志

    virtual std::string toYamlString() = 0;

    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getformatter();

    void setLevel(LogLevel::Level val) {m_level = val;}
    LogLevel::Level getLevel() const {return m_level;}
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    bool m_hasFormatter = false;
    MutexType m_mutex;
    LogFormatter::ptr m_formatter;
};

//日志输出器
//成员包含日志名称m_name、日志级别m_level、日志输出地集合m_appender、格式器m_formatter。
//构造函数进行一个简单初始化日志名、日志级别以及默认日志格式
//log函数会遍历m_appender里面的日志输出地，调用对应的Appender的log函数进行输出。
//addAppender函数会在当前日志item没有格式器时赋予一个默认值，否则直接加入m_appender集合
//delAppender函数删除一个指定的日志输出地

class Logger : public std::enable_shared_from_this<Logger>{
public:
    friend class LoggerManager;
    using ptr = std::shared_ptr<Logger>;
    using MutexType = Spinlock;

    explicit Logger(const std::string &name = "root");
    
    void log(LogLevel::Level level, LogEvent::ptr event);

    //输出对应级别的日志
    void debug(LogLevel::Level level, LogEvent::ptr event);
    void info(LogLevel::Level level, LogEvent::ptr event);
    void warn(LogLevel::Level level, LogEvent::ptr event);
    void error(LogLevel::Level level, LogEvent::ptr event);
    void fatal(LogLevel::Level level, LogEvent::ptr event);

    //添加输出位置
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);

    //获取level
    LogLevel::Level getLevel() const {return m_level;}
    void setLevel(LogLevel::Level val) {m_level = val;}

    const std::string& getName() const {return m_name;}
    void clearAppenders();

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter(); 

    std::string toYamlString();
private:

    std::string m_name;     //日志名称
    LogLevel::Level m_level;//日志级别
    std::list<LogAppender::ptr> m_appenders;//日志输出位置集合
    MutexType m_mutex;
    LogFormatter::ptr m_formatter; //日志格式
    Logger::ptr m_root;
};

//输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
friend class Logger;
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;
    void log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};


//输出到文件的Appender
class FileLogAppender : public LogAppender {

public:
    using ptr = std::shared_ptr<FileLogAppender>;

    //对应的日志文件名
    explicit FileLogAppender(const std::string& filename);
    void log(Logger::ptr logger,LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;

    //文件重新打开
    bool reopen();
private:

    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_lastTime = 0;

};


class LoggerManager{
public:
    using MutexType = Spinlock;
    LoggerManager();
    Logger::ptr getLogger(const std::string& name) ;
    void init();
    Logger::ptr getRoot () const {return m_root;}
    std::string toYamlString();
private:
    MutexType m_mutex;
    //<日志器名称, 日志器>
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

//v是静态的，它只会在程序的整个生命周期中存在一个实例。
//每次调用 GetInstance() 返回的都是同一个地址，即同一个对象 v。
typedef cc::Singleton<LoggerManager> LoggerMgr;

}

#endif