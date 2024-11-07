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
        : _TimerID(TimerID),
          _Timeout(Timeout),
          _isCanceled(false)
    {
    }

    ~Timer()
    {
        if (_ReleaseCallback)
        {
            _ReleaseCallback();
        }
        if (_TimerCallback && !_isCanceled)
        {
            _TimerCallback();
        }
    }

    void SetTimerCallback(const OnTimerCallback &cb)
    {
        _TimerCallback = cb;
    }

    void SetReleaseCallback(const ReleaseCallback &cb)
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
    TimeWheel(int Interval = 3, int MaxTimeout = 3600)
        : _Wheel(WHEEL_SIZE),
          _WheelSize(WHEEL_SIZE),
          _Interval(Interval),
          _MaxTimeout(MaxTimeout),
          _CurIndex(0),
          _TimerID(0)
    {
    }

    ~TimeWheel()
    {
        for (auto &list : _Wheel)
        {
            for (auto &timer : list)
            {
                timer->Cancel();
            }
        }
    }

    bool HasTimer(uint64_t TimerID)
    {
        return _TimerIDSet.find(TimerID) != _TimerIDSet.end();
    }

    uint64_t AddTimer(int TimerID, int Timeout, OnTimerCallback cb)
    {
        if (Timeout < 0 || Timeout > _MaxTimeout)
        {
            return -1;
        }

        SharedPtrTimer timer = std::make_shared<Timer>(_TimerID++, Timeout);
        timer->SetTimerCallback(cb);
        // timer->SetReleaseCallback(std::bind(&TimeWheel::RemoveWeakPtr, this, std::placeholders::_1));
        timer->SetReleaseCallback([this, TimerID]() { this->RemoveWeakPtr(TimerID); });

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

    void RemoveWeakPtr(uint64_t TimerID)
    {
        auto iter = _TimerMap.find(TimerID);
        if (iter != _TimerMap.end())
        {
            _TimerMap.erase(iter);
        }
    }

    void RunOntimeTask()
    {
        _CurIndex = (_CurIndex + 1) % _WheelSize;
        _Wheel[_CurIndex].clear();
    }

    // void Tick()
    // {
    //     // std::list<std::shared_ptr<Timer>> timeoutList = _Wheel[_CurIndex];
    //     // for (auto& timer : timeoutList)
    //     // {
    //     //     timer->Cancel();
    //     //     _TimerMap.erase(timer->GetTimerID());
    //     //     _TimerIDSet.erase(timer->GetTimerID());
    //     // }
    //     _CurIndex = (_CurIndex + 1) % _WheelSize;
    //     _Wheel[_CurIndex].clear();

    // }
};

class TimerTest
{
private:
    int _data;

public:
    TimerTest(int data) : _data(data) { std::cout << "test 构造!\n"; }
    ~TimerTest() { std::cout << "test 析构!\n"; }
};

void del(TimerTest *t)
{
    delete t;
}

int main()
{
    /*创建⼀个固定5s的定时任务队列， 每个添加的任务将在被添加5s后执⾏*/
    TimeWheel tq;
    /*new 了⼀个对象*/
    TimerTest *t = new TimerTest(10);
    /*随便设置了⼀个定时器ID*/
    int id = 3;
    /*std::bind(del, t) 构建适配了⼀个释放函数，作为定时任务执⾏函数*/
    /*这⾥其实就是添加了⼀个5秒后执⾏的定时任务，任务是销毁t指针指向的空间*/
    tq.AddTimer(0, 5, std::bind(del, t));
    /*按理说这个任务5s后就会被执⾏，析构，但是因为循环内总是在刷新任务，也就是⼆次添加任
   务，
    因此，它的计数总是>0，不会被释放，之后最后⼀个任务对象shared_ptr被释放才会真正析构*/
    for (int i = 0; i < 5; i++)
    {
        sleep(1);
        tq.TimerRefresh(id);
        std::cout << "刷新了⼀下3号定时任务!\n";
        tq.RunOntimeTask(); // 每秒调⽤⼀次，模拟定时器
    }
    std::cout << "刷新定时任务停⽌, 5s后释放任务将被执⾏\n";
    while (1)
    {
        std::cout << "--------------------\n";
        sleep(1);
        tq.RunOntimeTask(); // 每秒调⽤⼀次，模拟定时器
        if (tq.HasTimer(id) == false)
        {
            std::cout << "定时任务已经被执⾏完毕！\n";
            break;
        }
    }
    return 0;
}
