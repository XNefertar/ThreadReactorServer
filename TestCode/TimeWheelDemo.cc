#include <iostream>
#include <memory>
#include <list>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <unistd.h>

using OnTimerCallback = std::function<void()>;
using ReleaseCallback = std::function<void()>;

class Timer
{
private:
    int _Timeout;
    bool _isCanceled;
    uint64_t _TimerID;
    OnTimerCallback _TimerCallback;
    ReleaseCallback _ReleaseCallback;

public:
    Timer(uint64_t TimerID, int Timeout)
        :_TimerID(TimerID),
        _Timeout(Timeout),
        _isCanceled(false)
    {}

    ~Timer()
    {
        if (_ReleaseCallback)
        {
            _ReleaseCallback();
        }
        if(_TimerCallback && !_isCanceled)
        {
            _TimerCallback();
        }
    }

    void SetTimerCallback(OnTimerCallback cb)
    {
        _TimerCallback = cb;
    }

    void SetReleaseCallback(ReleaseCallback cb)
    {
        _ReleaseCallback = cb;
    }

    void Cancel()
    {
        _isCanceled = true;
    }

    uint64_t GetTimerID() const
    {
        return _TimerID;
    }

    int GetTimeout() const
    {
        return _Timeout;
    }
};

#define WHEEL_SIZE 60

class TimeWheel
{
    using SharedPtrTimer = std::shared_ptr<Timer>;
    using WeakPtrTimer = std::weak_ptr<Timer>;

private:
    std::vector<std::list<std::shared_ptr<Timer>>> _Wheel;
    std::unordered_map<uint64_t, std::weak_ptr<Timer>> _TimerMap;
    std::unordered_set<uint64_t> _TimerIDSet;
    uint64_t _TimerID;
    int _CurIndex;
    int _WheelSize;
    int _Interval;
    int _MaxTimeout;

public:
    TimeWheel(int Interval, int MaxTimeout)
        :_Wheel(WHEEL_SIZE),
        _WheelSize(WHEEL_SIZE),
        _Interval(Interval),
        _MaxTimeout(MaxTimeout),
        _CurIndex(0),
        _TimerID(0)
    {}

    ~TimeWheel()
    {
        for (auto& list : _Wheel)
        {
            for (auto& timer : list)
            {
                timer->Cancel();
            }
        }
    }

    bool HasTimer(uint64_t TimerID)
    {
        return _TimerIDSet.find(TimerID) != _TimerIDSet.end();
    }

    uint64_t AddTimer(int Timeout, OnTimerCallback cb, ReleaseCallback releaseCb)
    {
        if (Timeout < 0 || Timeout > _MaxTimeout)
        {
            return -1;
        }

        SharedPtrTimer timer = std::make_shared<Timer>(_TimerID++, Timeout);
        timer->SetTimerCallback(cb);
        timer->SetReleaseCallback(releaseCb);

        // 保存TimerID
        // 弱引用既不会增加引用计数，也不会阻止对象被释放
        // 同时可以检测对象是否已经被释放
        _TimerMap[_TimerID] = WeakPtrTimer(timer);
        int index = (_CurIndex + Timeout / _Interval) % _WheelSize;
        _Wheel[index].push_back(timer);
        return timer->GetTimerID();
    }

    void TimerRefresh(uint64_t TimerID)
    {
        auto iter = _TimerMap.find(TimerID);
        assert(iter != _TimerMap.end());
        // weak_ptr.lock() 返回一个shared_ptr对象
        // 如果对象已经被释放，返回一个空的shared_ptr
        int newTimeout = iter->second.lock()->GetTimeout();
        int newIndex = (_CurIndex + newTimeout / _Interval) % _WheelSize;
        if (newIndex == _CurIndex)
        {
            return;
        }
        _Wheel[newIndex].push_back(iter->second.lock());
    }

    void CancelTimer(uint64_t TimerID)
    {
        if (!HasTimer(TimerID))
        {
            return;
        }

        auto weakTimer = _TimerMap[TimerID];
        SharedPtrTimer timer = weakTimer.lock();
        if (timer)
        {
            timer->Cancel();
        }
    }

    void Tick()
    {
        std::list<std::shared_ptr<Timer>> timeoutList = _Wheel[_CurIndex];
        for (auto& timer : timeoutList)
        {
            timer->Cancel();
            _TimerMap.erase(timer->GetTimerID());
            _TimerIDSet.erase(timer->GetTimerID());
        }
        _Wheel[_CurIndex].clear();

        _CurIndex = (_CurIndex + 1) % _WheelSize;
    }



};
