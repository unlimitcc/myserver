#include "config.h"
//通过Yaml来加载和修改文件配置
namespace cc{

//Config::ConfigVarMap Config::GetDatas();


ConfigVarBase::ptr Config::LookupBase(const std::string& name){
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}
//将YAML中的数据加载到output中
static void ListAllMember(const std::string& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node> >& output){
    if(prefix.find_last_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos){
        CC_LOG_ERROR(CC_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << std::endl;
        return ;
    } 
    output.push_back(std::make_pair(prefix, node));
    if(node.IsMap()){
        for(auto it = node.begin(); it != node.end(); it++){
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node& root){
    std::list<std::pair<std::string, const YAML::Node> > all_nodes;
    ListAllMember ("", root, all_nodes);
    for(auto &it : all_nodes){
        std::string key = it.first;
        if(key.empty()){
            continue;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);
        //std::cout << "key: " << key << std::endl;
        if(var){
            //如果是标量(字符串，数字(不能进一步拆分的最小数据单元))直接转换为string
            //否则，调用对应的偏特化模板进行转换
            if(it.second.IsScalar()){
                var->fromString(it.second.Scalar());
            } else {
                std::stringstream ss;
                ss << it.second;
                var->fromString(ss.str());
            }
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb){
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = GetDatas();
    for(auto it = m.begin(); it != m.end(); ++it){
        cb(it->second);
    }
}
}