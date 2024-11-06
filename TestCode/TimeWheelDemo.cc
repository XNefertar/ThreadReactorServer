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
private:
    std::vector<std::list<std::shared_ptr<Timer>>> _Wheel;
    std::unordered_map<uint64_t, std::shared_ptr<Timer>> _TimerMap;
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

    uint64_t AddTimer(int Timeout, OnTimerCallback cb, ReleaseCallback releaseCb)
    {
        if (Timeout < 0 || Timeout > _MaxTimeout)
        {
            return -1;
        }

        std::shared_ptr<Timer> timer = std::make_shared<Timer>(_TimerID++, Timeout);
        timer->SetTimerCallback(cb);
        timer->SetReleaseCallback(releaseCb);

        int index = (_CurIndex + Timeout / _Interval) % _WheelSize;
        _Wheel[index].push_back(timer);
        _TimerMap[timer->GetTimerID()] = timer;
        _TimerIDSet.insert(timer->GetTimerID());

        return timer->GetTimerID();
    }

    void CancelTimer(uint64_t TimerID)
    {
        if (_TimerIDSet.find(TimerID) == _TimerIDSet.end())
        {
            return;
        }

        std::shared_ptr<Timer> timer = _TimerMap[TimerID];
        timer->Cancel();
        _TimerMap.erase(TimerID);
        _TimerIDSet.erase(TimerID);
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
