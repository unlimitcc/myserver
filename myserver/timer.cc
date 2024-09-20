#include "timer.h"
#include "util.h"

namespace cc{

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const{
    if(!lhs && !rhs){
        return false;
    }
    if(!lhs){
        return true;
    }
    if(!rhs){
        return false;
    }
    //m_next 表示实际的执行时间
    if(lhs->m_next < rhs->m_next){
        return true;
    }
    if(lhs->m_next > rhs->m_next){
        return false;
    }
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager)
            :m_recurring(recurring)
            ,m_ms(ms)
            ,m_cb(cb)
            ,m_manager(manager){
    m_next = cc::GetCurrentMS() + m_ms;           

}

Timer::Timer(uint64_t next)
        :m_next(next){

}

bool Timer::cancel(){
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    //有回调函数
    if(m_cb){
        m_cb = nullptr;
        //找到并删除对应的定时器
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

//重新设置执行时间
bool Timer::refresh(){
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    //没有回调函数
    if(!m_cb){
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()){
        return false;
    }
    m_manager->m_timers.erase(it);
    //重新设置定时器时间
    m_next = cc::GetCurrentMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

//重置
bool Timer::reset(uint64_t ms, bool from_now){
    //ms实际上就是说还要等多久才执行。
    //如果执行时间一样，且基准不变
    if(ms == m_ms && !from_now){
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(!m_cb){
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()){
        return false;
    }
    uint64_t start = 0;
    if(from_now){
        start = cc::GetCurrentMS();
    } else {
        //最初设置这个定时器的时间
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}


TimerManager::TimerManager(){
    m_previousTime = cc::GetCurrentMS();
}

TimerManager::~TimerManager(){

}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock){
    auto it = m_timers.insert(val).first;
    //如果插入的定时器是最快要执行的，(即在最前端的)
    //并且没有设置触发onTimerInsertedAtFront
    bool at_front = (it == m_timers.begin()) && !m_tickled;
    if(at_front){
        // 设置触发onTimerInsertedAtFront
        // 如果频繁发生插入到起始位置的情况，无需每次都进行处理
        m_tickled = true;
    }
    lock.unlock();
    if(at_front){
        onTimerInsertedAtFront();
    }
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                    bool recurring){
    //构造一个定时器
    Timer::ptr timer(new Timer(ms,cb,recurring,this));
    RWMutexType::WriteLock lock(m_mutex);
    addTimer(timer, lock);
    return timer;
}

//专门执行条件定时器任务的函数，判断条件是否为空，非空时证明有效，执行
static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb){
    //std::weak_ptr 的 lock() 成员函数用于获取一个指向其所管理对象的
    //std::shared_ptr。如果 std::weak_ptr 管理的对象已经被销毁，
    //lock() 将返回一个空的 std::shared_ptr。否则，它将返回一个指向该对象的有效 std::shared_ptr。
    std::shared_ptr<void> tmp = weak_cond.lock();
    if(tmp){
        cb();
    }
}

//创建条件定时器，也就是在创建定时器时绑定一个变量，
//在定时器触发时判断一下该变量是否仍然有效，如果变量无效，那就取消触发。
Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb,
                                            std::weak_ptr<void> weak_cond,
                                            bool recurring){
    //在定时器触发时会调用 OnTimer 函数，并在OnTimer函数中判断条件对象是否存在
    //如果存在则调用回调函数cb。
    return addTimer(ms, std::bind(&OnTimer, weak_cond, cb),recurring);
}

uint64_t TimerManager::getNextTimer(){
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false;
    if(m_timers.empty()){
        return ~0ull;
    }
    const Timer::ptr &next = *m_timers.begin();
    uint64_t now_ms = cc::GetCurrentMS();
    //定时器未正常执行，返回0立刻执行
    if(now_ms >= next->m_next){ 
        return 0;
    }else{  
        //还需的等待时间                    
        return next->m_next - now_ms;
    }
}

//返回所有超时的定时器的回调函数
void TimerManager::listExpireCb(std::vector<std::function<void()> >& cbs){
    uint64_t now_ms = cc::GetCurrentMS(); //当前时间
    std::vector<Timer::ptr> expired;      //已经超时的计时器

    {
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty()){
            return;
        }
    }
    RWMutexType::WriteLock lock(m_mutex);

    //检测超时原因：是否是由于时间被修改了
    bool rollover = detectClockRollover(now_ms);
    //如果时间未被修改，且当前定时器第一个定时器都未超时，则返回
    if(!rollover && ((*m_timers.begin())->m_next > now_ms)){
        return;
    }
  

    Timer::ptr now_timer(new Timer(now_ms));
    //lower_bound函数用于在有序集合中查找不小于指定值的第一个元素
    //upper_bound返回的是第一个大于指定值的元素

    //如果系统时间被修改了，则全部加入超时
    //否则拿到最后一个当前已超时的定时器的迭代器
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    //将当前时间的也包含进来。lb查找的是第一个大于等于的值，后面可能还有
    while(it != m_timers.end() && (*it)->m_next == now_ms){
        ++it;
    }
    //全部插入超时定时器集合
    expired.insert(expired.begin(), m_timers.begin(), it);
    //删掉原始的超时定时器
    m_timers.erase(m_timers.begin(), it);

    //reserve函数用于请求将容器的容量增加到至少足够存储n个元素。
    //它不会改变容器的大小（即元素的数量），但可以防止在添加新元素时频繁重新分配内存，
    //从而提高性能。
    cbs.reserve(expired.size());
    //超时定时器的回调函数放入待处理队列
    for(auto& timer : expired){
       
        cbs.push_back(timer->m_cb);
        //如果是循环定时器，修改执行时间并重新加入
        //否则，直接将其的回调函数取消即可
        if(timer->m_recurring){ 
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }else{
            timer->m_cb = nullptr;
        }
    }
}


//检测系统时间是否被修改
bool TimerManager::detectClockRollover(uint64_t now_ms){
    bool rollover = false;
    // 如果当前时间比上次执行时间还小, 并且小于一个小时以前的时间，系统时间被修改了
    if(now_ms < m_previousTime 
        && now_ms < (m_previousTime - 60 * 60* 1000)){//比一个小时前还小
        rollover = true;
    }
    m_previousTime = now_ms;
    return rollover;
}

bool TimerManager::hasTimer(){
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}
}