#ifndef __CC_NONCOPYABLE_H__
#define __CC_NONCOPYABLE_H__

namespace cc{

class Noncopyable{

public:
    Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable operator=(const Noncopyable&) = delete;
};

}

#endif