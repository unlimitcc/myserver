/*配置系统*/
#ifndef __CC_CONFIG_H__
#define __CC_CONFIG_H__

#include <memory>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <string>
#include "log.h"
#include <yaml-cpp/yaml.h>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <functional>
#include "thread.h"
#include "log.h"

// 一般配置应该具备以下内容：
// 参数名称：唯一字符串，不能与其它配置项冲突
// 参数类型：基本类型int,double等，或者自定义类型
// 缺省/默认值：如果用户没有给指定配置项赋值，程序需要赋予默认值
// 类似观察者模式
// 配置更新通知：一旦配置发生变化，需要通知所有使用了这项配置的代码
// 校验方法：确保用户不会给配置项设置一个非法值

//一个配置模块应具备的基本功能：
//1.支持定义/声明配置项，也就是在提供配置名称、
//  类型以及可选的默认值的情况下生成一个可用的配置项。
//2.支持更新配置项的值。
//3.支持从预置的途径中加载配置项，一般是配置文件，也可以是命令行参数
//  这里不仅应该支持基本数据类型的加载，也应该支持复杂数据类型的加载，
//  比如直接从配置文件中加载一个map类型的配置项，或是直接从一个预定格式的配置文件中加载一个自定义结构体。
//4.支持给配置项注册配置变更通知。配置模块应该提供方法让程序知道某项配置被修改了，以便于进行一些操作。
//  这个功能一般是通过注册回调函数来实现的，配置使用方预先给配置项注册一个配置变更回调函数
//  ，配置项发生变化时，触发对应的回调函数以通知调用方。
//  由于一项配置可能在多个地方引用，所以配置变更回调函数应该是一个数组的形式。
//5.支持给配置项设置校验方法。配置项在定义时也可以指定一个校验方法，以保证该项配置不会被设置成一个非法的值，
//  比如对于文件路径类的配置，可以通过校验方法来确保该路径一定存在。

namespace cc {

class ConfigVarBase{

public:

    using ptr = std::shared_ptr<ConfigVarBase>;
    ConfigVarBase(const std::string& name, const std::string& description = "") 
        : m_name(name)
        , m_description(description) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin() ,::tolower); //转换为小写
    }

    virtual ~ConfigVarBase () {};

    const std::string& getName() { return m_name;}
    const std::string& getDescription() {return m_description;}

    //转换为字符串形式
    virtual std::string toString() = 0;
    //字符串转换为值
    virtual bool fromString(const std::string& val) = 0;    
    virtual std::string getTypeName() const = 0;
protected:
    //参数名
    std::string m_name;
    //参数描述
    std::string m_description; 

};

//类型转换模板
//F 原类型， T 目的类型
template<class F, class T>
class LexicalCast {
public:
    T operator()(const F& v){
        //更加通用且支持多种类型转换
        //会在转换失败时抛出 boost::bad_lexical_cast 异常。
        //这意味着在转换过程中会进行严格的类型检查，避免不正确的转换。
        return boost::lexical_cast<T>(v);
    }

};

//vector与string类型的偏特化版本，以下部分均类似
template<class T>
class LexicalCast<std::string, std::vector<T> >{

public:
    std::vector<T> operator()(const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            //1.当以无参数形式调用 ss.str() 时，它会返回
            //当前 stringstream 对象内部存储的字符串内容。
            //2.ss.str(const std::string& str) 形式可以用来
            //设置 stringstream 内部的字符串内容，相当于重置流。
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::vector<T>, std::string>{

public:
    std::string operator()(const std::vector<T>& v){
        YAML::Node node;
        for(auto &it : v){
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(it)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<class T>
class LexicalCast<std::string, std::list<T> >{

public:
    std::list<T> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::list<T> ls;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            ls.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return ls;
    }
};

template<class T>
class LexicalCast<std::list<T>, std::string>{

public:
    std::string operator() (const std::list<T>& v){
        YAML::Node node;
        for(auto &it : v){
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(it)));
        }
        typename std::list<T> ls;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//set
template<class T>
class LexicalCast<std::string, std::set<T> >{

public:
    std::set<T> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::set<T> s;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            s.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return s;
    }
};

template<class T>
class LexicalCast<std::set<T>, std::string>{

public:
    std::string operator() (const std::set<T>& v){
        YAML::Node node;
        for(auto &it : v){
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(it)));
        }
        typename std::set<T> s;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//unordered_set
template<class T>
class LexicalCast<std::string, std::unordered_set<T> >{

public:
    std::unordered_set<T> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::unordered_set<T> us;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            us.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return us;
    }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string>{

public:
    std::string operator() (const std::unordered_set<T>& v){
        YAML::Node node;
        for(auto &it : v){
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(it)));
        }
        typename std::unordered_set<T> us;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//map
template<class T>
class LexicalCast<std::string, std::map<std::string, T> >{

public:
    std::map<std::string, T> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::map<std::string, T> mp;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            mp.insert(std::make_pair(it->first.Scalar() ,LexicalCast<std::string, T>()(ss.str())));
        }
        return mp;
    }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string>{

public:
    std::string operator()(const std::map<std::string, T>& v){
        YAML::Node node;
        for(auto &it : v){
            node[it.first] = YAML::Load(LexicalCast<T, std::string>()(it.second));
        }
        typename std::map<std::string, T> mp;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//unordered_map
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T> >{

public:
    std::unordered_map<std::string, T> operator() (const std::string &v){
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> ump;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            ump.insert(std::make_pair(it->first.Scalar() ,LexicalCast<std::string, T>()(ss.str())));
        }
        return ump;
    }
};


//unordered_map
template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string>{

public:
    std::string operator()(const std::unordered_map<std::string, T>& v){
        YAML::Node node;
        for(auto &it : v){
            node[it.first] = YAML::Load(LexicalCast<T, std::string>()(it.second));
        }
        typename std::unordered_map<std::string, T> ump;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//FromStr 是从string中转换成自定义的格式
//Tostr 则是相反
template<class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase{
public:
    using RWMutexType = RWMutex;
    using ptr = std::shared_ptr<ConfigVar>;

    //回调函数: 配置发生变更时通知，使得用户感知并针对具体变化做出响应; cb : callback
    using on_change_cb = std::function<void (const T& old_value, const T& new_value)>;

    //配置关注三个内容: 配置变量名称，配置描述，配置值
    ConfigVar(const std::string& name 
            , const T& default_val
            , const std::string& description = "") 
            : ConfigVarBase(name, description)
            , m_val(default_val){

    }

    std::string toString() override{
        try{
            //return boost::lexical_cast<std::string>(m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        }catch(std::exception& e){
            //类型转换string失败
            CC_LOG_ERROR(CC_LOG_ROOT()) << "ConfigVar::toString exception " << e.what() << " convert: "
                << typeid(m_val).name() << " to string";
        }
        return "";
    }

    bool fromString(const std::string& val) override {
        try{
            //m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));
        }catch(std::exception& e){
            CC_LOG_ERROR(CC_LOG_ROOT()) << "ConfigVar::toString exception " << e.what() 
            << " convert: string to " << typeid(m_val).name();
        }
        return false;
    }

    const T getValue() {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    void setValue(const T& v) {
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(m_val == v){//未发生变化
                return;
            }
            //调用与该值相关的回调函数触发相应
            for(auto &i : m_cbs){
                i.second(m_val, v);
            }
        }
        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }

    std::string getTypeName() const override {return typeid(T).name();}

    //listener：回调函数
    //回调函数存储格式<id，函数>
    uint64_t addListener(on_change_cb cb){
        static uint64_t s_fun_id = 0;
        RWMutexType::WriteLock lock(m_mutex);
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    //删除回调函数，根据key(s_fun_id)删除
    void delListener(uint64_t key){
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }

    void getListener(uint64_t key){
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    void clearListener(){
        RWMutexType::WriteLock lock(m_mutex);
        m_cbs.clear();
    }
private:

    //参数值
    T m_val;
    //变更回调记录函数组，key唯一标识一个回调，用于后续更新，清理等
    std::map<uint64_t, on_change_cb> m_cbs;
    RWMutexType m_mutex;
};


//管理类
//  负责托管全部的ConfigVar对象，单例模式。提供Lookup方法，用于根据配置名称查询配置项。
//  如果调用Lookup查询时同时提供了默认值和配置项的描述信息，那么在未找到对应的配置时，
//  会自动创建一个对应的配置项，这样就保证了配置模块定义即可用的特性。
//  除此外，Config类还提供了LoadFromYaml和LoadFromConfDir两个方法，
//  用于从YAML对象或从命令行-c选项指定的配置文件路径中加载配置。
//  Config的全部成员变量和方法都是static类型，保证了全局只有一个实例。
class Config{

public:
    //key为配置名称，value为配置名称对应的配置参数
    using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;
    using RWMutexType = RWMutex;

    //根据名称，内容，描述符 查找对应的配置
    template<class T> //typename 告诉编译器 ConfigVar<T>::ptr 是返回类型
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = ""){
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetDatas().find(name);
        //纠错机制，如果一个key对应多个value时会报错
        if(it != GetDatas().end()){
            //把 std::shared_ptr 指针从基类类型安全地转换为派生类类型
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
            if(tmp) {
                CC_LOG_INFO(CC_LOG_ROOT()) << "Lookup name = " << name << " exists";
            } else {
                //转换失败，可能是现有的类型和预期类型T不一致所导致
                CC_LOG_ERROR(CC_LOG_ROOT()) << "Lookup name = " << name << " exists but type is not"
                    << typeid(T).name() << " real_type = " << it->second->getTypeName()
                    << " " << it->second->toString();
                return nullptr;
            }
        }
        
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos){
            CC_LOG_ERROR(CC_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
            return nullptr;
        }
        //如果没有找到对应的配置内容，创建参数配置并用default_value赋值
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetDatas()[name] = v;
        return v;
    }

    //根据配置参数名称查找 
    template<class T> 
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        if(it == GetDatas().end()){
            return nullptr;
        }        
        //多态类型安全转换：当你有一个指向基类的智能指针，
        //并且需要将其转换为指向派生类的智能指针时使用。
        //避免内存泄漏：智能指针自动管理内存，
        //使用 std::dynamic_pointer_cast 可以避免手动类型转换可能带来的内存管理错误。
        return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
    }

    static void LoadFromYaml(const YAML::Node& root);

    //查找配置参数,返回配置参数的基类
    static ConfigVarBase::ptr LookupBase(const std::string& name);

    //遍历配置模块里面所有配置项
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
private:
    
    //经典单例模式
    //返回所有配置项
    static ConfigVarMap& GetDatas(){
        static ConfigVarMap s_datas;
        return s_datas;
    }

    //专用于配置项的RWMutex
    static RWMutexType& GetMutex(){
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}


#endif