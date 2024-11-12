#ifndef __TIMER_WHHEL_HPP__
#define __TIMER_WHHEL_HPP__

#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <list>
#include <unistd.h>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask
{
private:
    uint64_t _id;
    int _timeout;
    bool _isCanceled;
    TaskFunc _taskFunc;
    ReleaseFunc _releaseFunc;
public:
    TimerTask(uint64_t id, int timeout)
        : _id(id),
          _timeout(timeout),
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
};


class TimerWheel
{
    using WeakPtr = std::shared_ptr<TimerTask>;
    using WeakPtrMap = std::unordered_map<uint64_t, WeakPtr>;
    using WeakPtrSet = std::unordered_set<uint64_t>;
    using SharedPtr = std::shared_ptr<TimerTask>;
    using TaskList = std::list<SharedPtr>;
    using Wheel = std::vector<TaskList>;

private:

public:
    TimerWheel()

};



#endif // __TIMER_WHHEL_HPP__