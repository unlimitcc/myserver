#include "log.h"
#include "config.h"

namespace cc {

const char* LogLevel::ToString(LogLevel::Level Level){
    switch(Level){
    #define XX(name) \
        case LogLevel::name: \
            return #name; \
            break; 

        XX(DEBUG);
        XX(INFO);
        XX(WARN);
        XX(ERROR);
        XX(FATAL);

    #undef XX

    default:
        return "UNKNOWN";
        break;
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string& str){
#define XX(level, s) \
    if(str == #s){ \
        return LogLevel::level; \
    }

    XX(DEBUG, debug);
    XX(INFO, info);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOWN;

#undef XX
}

LogEventWrap::LogEventWrap(LogEvent::ptr e)
    :m_event(e) {
}

LogEventWrap::~LogEventWrap() {

    m_event->getLogger()->log(m_event->getLevel(), m_event);
    
}

std::stringstream& LogEventWrap::getSS(){
    
    return m_event->getSS();
}
    
void LogAppender::setFormatter(LogFormatter::ptr val){
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    if(m_formatter){
        m_hasFormatter = true;  
    }else{
        m_hasFormatter = false;  
    }
}

LogFormatter::ptr LogAppender::getformatter(){
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

//format（格式化写入日志内容）
class MessageFormatItem : public LogFormatter::FormatItem{
public:
    MessageFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        //getcontent return m_ss;
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem{

public:
    LevelFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << LogLevel::ToString(level);
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem{

public:
    ElapseFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getElapse();
    }
};

class NameFormatItem : public LogFormatter::FormatItem{

public:
    NameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getLogger()->getName();
    }
};

//获取线程id
class ThreadIdFormatItem : public LogFormatter::FormatItem{

public:
    ThreadIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getThread();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem{

public:
    FiberIdFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getfiberid();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem{

public:
    ThreadNameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getThreadName();
    }
};

class FileNameFormatItem : public LogFormatter::FormatItem{

public:
    FileNameFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getFileName();
    }

};

class LineFormatItem : public LogFormatter::FormatItem{

public:
    LineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << event->getLine();
    }

};

class NewLineFormatItem : public LogFormatter::FormatItem{

public:
    NewLineFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << std::endl;
    }

};

class DateTimeFormatItem : public LogFormatter::FormatItem{

public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        : m_format(format){
        if(m_format.empty()){
            m_format = "%Y-%m-%d %H:%M:%S";
        }

    }
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        struct tm tm;
        time_t time = event->getTime();
        //用于将一个给定的时间值（通常是 time_t 类型）转换为当地时间的表示形式
        localtime_r(&time, &tm);
        char buf[64];
        //将时间格式化为字符串，根据指定的格式输出
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;

};

//tab 制表位输出
class TabFormatItem : public LogFormatter::FormatItem{

public:
    TabFormatItem(const std::string& str = ""){}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << "\t";
    }
private:
    std::string m_string;
};

class StringFormatItem : public LogFormatter::FormatItem{

public:
    StringFormatItem(const std::string& str)
        :FormatItem(str), m_string(str){
    }
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override{
        os << m_string;
    }
private:
    std::string m_string;
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, int32_t line, uint32_t elapse,
        uint64_t threadid, uint32_t fiberid, uint64_t time, const std::string& thread_name)
        : m_file(file)
        , m_line(line)
        , m_elapse(elapse)
        , m_threadid(threadid)
        , m_fiberid(fiberid)
        , m_time(time)
        , m_logger(logger)
        , m_level(level) 
        , m_threadName(thread_name){ 
}

//va_list是一个类型，用于访问可变数量的参数
//va_start(al, fmt); 使其指向可变参数列表的第一个参数。
//fmt即为格式，与printf相同例如("%s","%d"等)
void LogEvent::format(const char* fmt, ...) {
    
    va_list al;
    //使用va_start(al, fmt)宏初始化al, 并将其指向参数列表中的第一个参数
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

//vasprintf 函数将 al 按照 fmt格式 写入动态分配的缓冲区buf中，
//并返回生成的字符串的长度len(不包括终止的null字符)。
void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    //len表示返回的字符串长度
    int len = vasprintf(&buf, fmt, al);
    if(len != -1) {
        m_ss << std::string(buf, len);//创建一个包含从 buf 开始，长度为 len 的字符串对象。
        free(buf);
    }
}

Logger::Logger(const std::string& name)
    : m_name(name)
    , m_level(LogLevel::DEBUG) {
    m_formatter.reset(new LogFormatter("%d%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));//默认日志格式

}

void Logger::setFormatter(LogFormatter::ptr val){

    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    //每一个输出位置的格式如果没有，都改为val
    for(auto& i : m_appenders){
        MutexType::Lock ll(i->m_mutex);
        if(!i->m_hasFormatter){
            i->m_formatter = m_formatter;
        }
    }
}

void Logger::setFormatter(const std::string& val){
    cc::LogFormatter::ptr new_val(new cc::LogFormatter(val));
    if(new_val->isError()){
        std::cout << "Logger setFormatter name = " << m_name
                << "value = " << val << " is invalid foramtter"
                << std::endl;
        return;
    }
    setFormatter(new_val);
}

std::string Logger::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if(m_level != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(m_level);
    }
    
    if(m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    
    for(auto& i : m_appenders){
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::ptr Logger::getFormatter(){
    MutexType::Lock lock(m_mutex);
    return m_formatter;
} 

//向指定appender添加格式  
void Logger::addAppender(LogAppender::ptr appender){
    MutexType::Lock lock(m_mutex);
    
    if(!appender->getformatter()){
        MutexType::Lock ll(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender){
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it!=m_appenders.end(); ++it){
        if(*it == appender) {
            m_appenders.erase(it); 
            break;
        }
    }
}

void Logger::clearAppenders(){
    m_appenders.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event){
    if(level >= m_level){
        auto self = shared_from_this();
        MutexType::Lock lock(m_mutex);
        //如果当前日志器有指定的输出位置，按指定位置的格式输出
        if(!m_appenders.empty()){
            for(auto &it : m_appenders){
                it->log(self, level, event);
            }
        //如果没有指定的输出位置，按root格式输出
        } else if(m_root){
            m_root->log(level, event);
        }
    }
}

//输出对应级别的日志
void Logger::debug(LogLevel::Level level, LogEvent::ptr event){
    log(LogLevel::DEBUG, event);
}
void Logger::info(LogLevel::Level level, LogEvent::ptr event){
    log(LogLevel::INFO, event);
}
void Logger::warn(LogLevel::Level level, LogEvent::ptr event){
    log(LogLevel::WARN, event);
}
void Logger::error(LogLevel::Level level, LogEvent::ptr event){
    log(LogLevel::ERROR, event);
}
void Logger::fatal(LogLevel::Level level, LogEvent::ptr event){
    log(LogLevel::FATAL, event);
}

FileLogAppender::FileLogAppender(const std::string &filename)
    : m_filename(filename) {
    reopen();
}

//
void FileLogAppender::log(std::shared_ptr<Logger> logger,LogLevel::Level level, LogEvent::ptr event){
    
    if(level >= m_level){
        uint64_t now = time(0);
        if(now != m_lastTime){ //防止误删除日志文件后，系统无感知，因此不断重复打开文件，防止日志记录丢失
            reopen();
            m_lastTime = now;
        }
        MutexType::Lock lock(m_mutex);
        m_filestream << m_formatter->format(logger, level, event);//根据具体定义格式输出
    }
}

bool FileLogAppender::reopen(){
    MutexType::Lock lock(m_mutex);
    if(m_filestream){
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream;
}

std::string FileLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if(m_level != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,LogLevel::Level level, LogEvent::ptr event){
    if(level >= m_level){
        MutexType::Lock lock(m_mutex);
        //按照格式输出到cout里
        m_formatter->format(std::cout, logger, level, event);
    }
}

std::string StdoutLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if(m_level != LogLevel::UNKNOWN){
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern)
    :m_pattern(pattern) {
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,LogLevel::Level level, LogEvent::ptr event){
    std::stringstream ss;
    for(auto &i : m_item){
        i->format(ss, logger, level, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    for(auto &i : m_item) {
        i->format(ofs, logger, level, event);
    }
    return ofs;
}

//初始化格式器
//使用LOG4J的日志格式 
//待解析的格式只有 %xxx 或者 %xxx{xxx} 或者 %%(此时一个'%'表示转义)
void LogFormatter::init(){
    
    //日志项内容，日志项格式，日志解析方式
    //vec<2> = 0：直接作为字符串解析
    //vec<2> = 1：作为特殊类型解析，传递至宏XX(#str, C)解析
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;//暂时保存解析出的pattern的内容
    for(size_t i=0; i<m_pattern.size(); ++i){
        if(m_pattern[i] != '%'){
            //append 在字符串的末尾添加1个字符m_pattern[i]
            //去掉了所有格式中的%
            nstr.append(1,m_pattern[i]);
            continue;
        }

        //处理可能出现两个%%的情况
        if(i + 1 < m_pattern.size()){
            //第二个%作为转义字符，因为有两个%
            if(m_pattern[i + 1] == '%') {
                nstr.append(1, '%');
                continue;
            }
        }
        size_t n = i + 1;
        //记录有无 "{XXX}" 类型pattern
        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;
        //解析XXX{XXX}这类格式
        //"%d%T%N%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
        while (n < m_pattern.size()){
            //不是字母，也不是 "{" 或 "}"(即普通的%s%d类型的pattern)
            if(!fmt_status && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}') break;
            if(fmt_status == 0){
                if(m_pattern[n] == '{'){
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1;
                    fmt_begin = n;
                    ++n;
                    continue; 
                }
            }else if(fmt_status == 1){
                if(m_pattern[n] == '}'){
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 2;
                    break;
                }
            }
            ++n;
            if(n == m_pattern.size()) {
                if(str.empty()) {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if(fmt_status == 0){//没有大括号的格式
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            str = m_pattern.substr(i + 1, n - i - 1);
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if(fmt_status == 1){ //格式错误，只出现了"{"
            std::cout << "pattern parse error: " << m_pattern << "-" << m_pattern.substr(i) << std::endl; 
            m_error = true;
            vec.push_back(std::make_tuple("<<pattern_error>>",fmt,0));
        } else if(fmt_status == 2){
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, "", 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 0));
            i = n - 1;
        } 
    }

    if(!nstr.empty()){
        vec.push_back(std::make_tuple(nstr, "", 0));
    }

    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_item = {
#define XX(str, C) \
        {#str, [](const std::string& fmt){return FormatItem::ptr(new C(fmt));}}

        XX(m, MessageFormatItem),   //消息
        XX(p, LevelFormatItem),     //日志级别
        XX(r, ElapseFormatItem),    //累计毫秒数
        XX(c, NameFormatItem),      //debug日志级别
        XX(t, ThreadIdFormatItem),  //线程id
        XX(n, NewLineFormatItem),   //换行
        XX(d, DateTimeFormatItem),  //时间
        XX(f, FileNameFormatItem),  //文件名
        XX(F, FiberIdFormatItem),   //协程id
        XX(l, LineFormatItem),      //行号
        XX(T, TabFormatItem),       //tab
        XX(N, ThreadNameFormatItem),//线程名

#undef XX
    };
    
    //先判断三元组第3个参数是否等于0，如果是说明这一条元组是一个类型字符代号(例如：%d就是时间)，
    //存入到m_items中，然后调用相应的format方法。
    //如果不是0，表示相应字符代号后面的内容，则去创建的map里面遍历，找到了就push到m_items，否则push一条错误信息。
    //日志项内容，日志项格式，日志解析方式
    for(auto &i : vec){
        if(std::get<2>(i) == 0) {
            m_item.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
        } else {
            auto it = s_format_item.find(std::get<0>(i));
            if(it == s_format_item.end()) {
                m_item.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            } else {
                m_item.push_back(it->second(std::get<1>(i)));
            }
        }
        // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
    }
}

LoggerManager::LoggerManager(){
    m_root.reset(new Logger);
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));
    m_loggers[m_root->m_name] = m_root;
    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name){
    
    MutexType::Lock lock(m_mutex);
    //日志器存储：<日志器名称, 日志器指针>
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()){ //存在
        return it->second;
    }else{ //新建一个日志器
        Logger::ptr logger(new Logger(name));
        logger->m_root = m_root;
        m_loggers[name] = logger;
        return logger;
    }

}


struct LogAppenderDefine{

    int32_t type = 0; // 1: File, 2: Stdout
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file; 

    bool operator==(const LogAppenderDefine& oth) const {
        return type == oth.type
            && level == oth.level
            && formatter == oth.formatter
            && file == oth.file;
    }
};

//处理使用文件修改日志配置的情况
struct LogDefine{

    std::string name;
    LogLevel:: Level level = LogLevel::UNKNOWN;;
    std::string formatter;

    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine& oth) const {
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == oth.appenders;
    }

    bool operator<(const LogDefine& oth) const {
        return name < oth.name;
    }
};

template<>
class LexicalCast<std::string, std::set<LogDefine> >{

public:
    std::set<LogDefine> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        std::set<LogDefine> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            auto n = node[i];
            if(!n["name"].IsDefined()){ //YAML yml中是否存在name项
                std::cout << "log config error: name is null, " << n
                          << std::endl;
                continue;
            }
            
            LogDefine ld;
            ld.name = n["name"].as<std::string>();
            ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
            if(n["formatter"].IsDefined()){
                ld.formatter = n["formatter"].as<std::string>();
            }

            if(n["appenders"].IsDefined()){
                for(size_t j=0; j<n["appenders"].size(); ++j){
                    auto a = n["appenders"][j];
                    if(!a["type"].IsDefined()){
                        std::cout << "log config error: appender type is null, " << n
                                  << std::endl;
                        continue;
                    }
                    std::string type = a["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if(type == "FileLogAppender"){
                        lad.type = 1;
                        if(!a["file"].IsDefined()){
                            std::cout << "log config error: fileappender file is null, " << n
                                    << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if(a["formatter"].IsDefined()){
                            lad.formatter = a["formatter"].as<std::string>();
                        }
                    }else if(type == "StdoutLogAppender"){
                        lad.type = 2;
                    }else{
                        std::cout << "log config error: appender type is invalid, " << n
                                  << std::endl;
                        continue;
                    }
                    ld.appenders.push_back(lad);
                }
            }
            vec.insert(ld);
        }
        return vec;
    }
};

template<>
class LexicalCast<std::set<LogDefine>, std::string>{

public:
    std::string operator() (const std::set<LogDefine>& v){
        YAML::Node node;
        for(auto& i : v){
            YAML::Node n;
            n["name"] = i.name;
            if(i.level != LogLevel::UNKNOWN){
                n["level"] = LogLevel::ToString(i.level);
            }
            if(!i.formatter.empty()){
                n["fromatter"] = i.formatter;
            }

            for(auto& a : i.appenders){
                YAML::Node na;
                if(a.type == 1){
                    na["type"] = "FileLogAppender";
                    na["file"] = a.file;
                }else if(a.type == 2){
                    na["type"] = "StdoutLogAppender";
                }
                if(a.level != LogLevel::UNKNOWN){
                    na["level"] = LogLevel::ToString(a.level);
                }
                
                if(a.formatter.empty()){
                    na["formatter"] = a.formatter;
                }

                n["appenders"].push_back(na);
            }

            node.push_back(n);
        }
        
        //std::set<LogDefine> vec;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
}; 

cc::ConfigVar<std::set<LogDefine> >::ptr g_log_defines = 
    cc::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter{
    LogIniter(){
        g_log_defines->addListener([](const std::set<LogDefine>& old_value,
            const std::set<LogDefine>& new_value){
                CC_LOG_INFO(CC_LOG_ROOT()) << "on_logger_conf_changed";
                //配置：新增+修改
                for(auto &i : new_value){
                    cc::Logger::ptr logger;
                    auto it = old_value.find(i);
                    if(it == old_value.end()){
                        //新增logger
                        logger = CC_LOG_NAME(i.name);
                    }else{
                        if(!(i == *it)){
                            //修改logger 
                            logger = CC_LOG_NAME(i.name);
                        }    
                    }
                    logger->setLevel(i.level);
                    if(!i.formatter.empty()){
                        logger->setFormatter(i.formatter);
                    }

                    logger->clearAppenders();
                    for(auto& a : i.appenders){
                        cc::LogAppender::ptr ap;
                        if(a.type == 1){
                            ap.reset(new FileLogAppender(a.file));
                        } else if(a.type == 2){
                            ap.reset(new StdoutLogAppender());
                        }
                        ap->setLevel(a.level);
                        if(!a.formatter.empty()){
                            LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                            if(!fmt->isError()){
                                ap->setFormatter(fmt);
                            } else {
                                std::cout << "log.name = " << i.name << "appender type = " 
                                          << a.type << " formatter=" << a.formatter
                                          << " is invalid" << std::endl;
                            }
                        }
                        logger->addAppender(ap);
                    }
                }

                for(auto& i : old_value){
                    auto it = new_value.find(i);
                    if(it == new_value.end()){
                        //删除logger,实际上没有删除，该操作后会使用root输出
                        auto logger = CC_LOG_NAME(i.name);
                        logger->setLevel((LogLevel::Level)100);
                        logger->clearAppenders();
                    }
                }
                //修改
                //删除
            });
    }
};

static LogIniter __log_init;

std::string LoggerManager::toYamlString(){
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    for(auto & i : m_loggers){
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void LoggerManager::init(){

}

}