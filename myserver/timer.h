#ifndef __CC_TIMER_H__
#define __CC_TIMER_H__

#include <memory>
#include <set>
#include <vector>
#include <functional>
#include "thread.h"

namespace cc{

class TimerManager;

//定时器，一个管理时间信息的机制
//通过继承 std::enable_shared_from_this<Timer>
//Timer 类可以使用 shared_from_this 成员函数生成指向自身的 std::shared_ptr。
//定时器的设计采用时间堆的方式，将所有定时器按照最小堆的方式排列，
//能够简单的获得当前超时时间最小的定时器，计算出超时需要等待的时间，然后等待超时。
//超时时间到后，获取当前的绝对时间，并且把时间堆中已经超时的所有定时器都收到一个容器中，执行他们的回调函数。
class Timer : public std::enable_shared_from_this<Timer>{
    friend class TimerManager;
public:
    using ptr = std::shared_ptr<Timer>;
    //取消定时器
    bool cancel();
    //重新设置执行时间          
    bool refresh();                        

    //重置定时器时间
    //ms 定时器执行间隔时间(单位: 毫秒)
    //from_now 是否从当前时间开始计算
    bool reset(uint64_t ms, bool from_now); //修改间隔时间
private:
    //ms: 时间(还有多久执行该定时器的任务)
    //cb: 回调函数
    //recurring: 是否循环
    Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager);
    //时间戳
    Timer(uint64_t next);
    
private:
    bool m_recurring = false;       //是否循环定时器
    uint64_t m_ms = 0;              //执行周期(理解为该定时器还有多久执行)
    uint64_t m_next = 0;            //具体执行时间
    std::function<void()> m_cb;     //定时器需要执行的任务
    TimerManager* m_manager = nullptr;
private:
    //定时器比较的仿函数
    struct Comparator{
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
};

class TimerManager{
    friend class Timer;
public:
    using RWMutexType = RWMutex;

    TimerManager();
    virtual ~TimerManager();

    /**
     * 添加定时器
     * ms 定时器执行间隔时间
     * cb 定时器回调函数
     * weak_cond 条件
     * recurring 是否循环定时器
     */
    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb,
                        bool recurring = false);
    
    //weak_cond表示
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                                 std::weak_ptr<void> weak_cond,
                                 bool recurring = false);
    
    //获取当前定时器中时间待执行时间最近的
    uint64_t getNextTimer();        

    //获取需要执行的定时器的回调函数列表
    void listExpireCb(std::vector<std::function<void()> >& cbs);

    bool hasTimer();
protected:

    //当有新的定时器其执行时间为最小的时,执行该函数
    virtual void onTimerInsertedAtFront() = 0;
    //将定时器添加到管理器中
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);
    
private:
    //检测服务器时间是否修改
    bool detectClockRollover(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    //管理的定时器列表
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    //是否触发onTimerInsertedAtFront
    bool m_tickled = false;
    //上次执行时间
    uint64_t m_previousTime = 0;
};

}

#endif