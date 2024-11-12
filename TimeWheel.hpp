#ifndef __TIMER_WHHEL_HPP__
#define __TIMER_WHHEL_HPP__

#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <list>
#include <unistd.h>
#include <sys/timerfd.h>
#include <cstdio>
#include "Channel.hpp"

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask
{
private:
    uint64_t _id;
    uint32_t _timeout;
    bool _isCanceled;
    TaskFunc _taskFunc;
    ReleaseFunc _releaseFunc;
public:
    TimerTask(uint64_t id, int timeout, const TaskFunc &taskFunc)
        : _id(id),
          _timeout(timeout),
          _taskFunc(taskFunc),
          _isCanceled(false)
    {}

    ~TimerTask()
    {
        if (_taskFunc && !_isCanceled) { _taskFunc(); }
        if (_releaseFunc) { _releaseFunc(); }
    }

    void SetTaskFunc(const TaskFunc &taskFunc) { _taskFunc = taskFunc; }
    void SetReleaseFunc(const ReleaseFunc &releaseFunc) { _releaseFunc = releaseFunc; }
    void Cancel() { _isCanceled = true; }
    uint64_t GetID() const { return _id; }
    uint32_t GetTimeout() const { return _timeout; }
};


class TimerWheel
{
    using WeakTask   = std::weak_ptr<TimerTask>;
    using TimerMap   = std::unordered_map<uint64_t, WeakTask>;
    using TimerSet   = std::unordered_set<uint64_t>;
    using SharedPtr  = std::shared_ptr<TimerTask>;
    using TaskList   = std::list<SharedPtr>;
    using Wheel      = std::vector<TaskList>;

private:
    int _Capacity;
    int _Tick;
    Wheel _Wheel;
    TimerMap _TimerMap;

    EventLoop *_Loop;
    // 定时器描述符
    int _Timerfd; 
    std::unique_ptr<Channel> _TimerChannel;

private:
    static int CreateTimerfd();
    int ReadTimerfd();
    void AddTimer(const SharedPtr &timer);
    void RemoveTimer(uint64_t id);
    void RunOntimeTask();
    void OnTimerTask();
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefreshInLoop(uint64_t id);
    void TimerCancelInLoop(uint64_t id);

public:
    TimerWheel(EventLoop *Loop)
        : _Capacity(60),
          _Tick(0),
          _Wheel(_Capacity),
          _Loop(Loop),
          _Timerfd(CreateTimerfd()),
          _TimerChannel(new Channel(Loop, _Timerfd))
    {
        _TimerChannel->SetReadCallback(std::bind(&TimerWheel::OnTimerTask, this));
        _TimerChannel->EnableRead();
    }

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);

    bool HasTimer(uint64_t id){
        return _TimerMap.find(id) != _TimerMap.end();
    }
};

int TimerWheel::CreateTimerfd()
{
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0){
        perror("timerfd_create error");
        exit(1);
    }

    struct itimerspec new_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    timerfd_settime(timer_fd, 0, &new_value, nullptr);
    return timer_fd;
}

int TimerWheel::ReadTimerfd()
{
    uint64_t times;
    ssize_t n = read(_Timerfd, &times, sizeof(times));
    if (n != sizeof(times)){
        perror("read timerfd error");
        exit(1);
    }
    return times;
}

void TimerWheel::AddTimer(const SharedPtr &timer){
    int timeout = timer->GetTimeout();
    int index = (_Tick + timeout) % _Capacity;
    _Wheel[index].push_back(timer);
    _TimerMap[timer->GetID()] = timer;
}

void TimerWheel::RemoveTimer(uint64_t id){
    auto iter = _TimerMap.find(id);
    if (iter != _TimerMap.end()){
        _TimerMap.erase(iter);
    }
}

void TimerWheel::RunOntimeTask(){
    _Tick = (_Tick + 1) % _Capacity;
    _Wheel[_Tick].clear();
}

void TimerWheel::OnTimerTask(){
    int times = ReadTimerfd();
    for(int i = 0; i < times; i++){
        RunOntimeTask();
    }
}

void TimerWheel::TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb){
    SharedPtr pt(new TimerTask(id, delay, cb));
    pt->SetReleaseFunc(std::bind(&TimerWheel::RemoveTimer, this, id));

    int newPos = (_Tick + delay) % _Capacity;
    _Wheel[newPos].push_back(pt);
    _TimerMap[id] = WeakTask(pt);
}

void TimerWheel::TimerRefreshInLoop(uint64_t id){
    auto iter = _TimerMap.find(id);
    if(iter == _TimerMap.end()) return;
    SharedPtr pt = iter->second.lock();
    int delay = pt->GetTimeout();
    int new_pos = (delay + _Tick) % _Capacity;
    _Wheel[new_pos].push_back(pt); 
}

void TimerWheel::TimerCancelInLoop(uint64_t id){
    auto it = _TimerMap.find(id);
    if(it == _TimerMap.end()){
        return;
    }
    SharedPtr pt = it->second.lock();
    if (pt){
        pt->Cancel();
    }
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb){
    auto iter = _TimerMap.find(id);
    if(iter == _TimerMap.end()) return;
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}

void TimerWheel::TimerRefresh(uint64_t id){
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}

void TimerWheel::TimerCancel(uint64_t id){
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

#endif // __TIMER_WHHEL_HPP__