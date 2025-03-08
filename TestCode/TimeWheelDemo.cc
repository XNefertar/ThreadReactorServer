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
    // 定时器超时时间
    int _Timeout;
    // 定时器是否被取消的标志
    bool _isCanceled;
    // 定时器ID，唯一标识符
    uint64_t TimerID;
    // 定时器到期时的回调函数
    OnTimerCallback _TimerCallback;
    // 定时器释放时的回调函数
    ReleaseCallback _ReleaseCallback;

public:
    Timer(uint64_t TimerID, int Timeout)
        : TimerID(TimerID),
          _Timeout(Timeout),
          _isCanceled(false)
    {}

    ~Timer(){
        if (_ReleaseCallback)
            _ReleaseCallback();
        if (_TimerCallback && !_isCanceled)
            _TimerCallback();
    }

    void SetTimerCallback(const OnTimerCallback &callback) {
        _TimerCallback = callback; 
    }

    void SetReleaseCallback(const ReleaseCallback &callback){
        _ReleaseCallback = callback;
    }

    void Cancel(){
        _isCanceled = true;
    }

    uint64_t GetTimerID() const{
        return TimerID;
    }

    int GetTimeout() const{
        return _Timeout;
    }
};

#define WHEEL_SIZE 60

class TimeWheel
{
    using SharedPtrTimer = std::shared_ptr<Timer>;
    using WeakPtrTimer = std::weak_ptr<Timer>;

private:
    // 时间轮，每个槽是一个定时器列表
    std::vector<std::list<std::shared_ptr<Timer>>> _Wheel;
    // 定时器映射
    // 回调函数由一个 Timer 对象管理
    // TiemrWheel 如果使用 shared_ptr 会导致循环引用问题
    // 所以这里使用了 weak_ptr 指向一个 Timer 对象
    std::unordered_map<uint64_t, std::weak_ptr<Timer>> _TimerMap;
    // 定时器ID集合
    // std::unordered_set<uint64_t> _TimerIDSet;
    // 当前时间轮的索引
    int _CurIndex;
    // 上一个时间轮的索引
    int _PreIndex;
    // 时间轮大小
    int _WheelSize;
    // 最大超时时间
    int _MaxTimeout;

public:
    TimeWheel(int MaxTimeout = 3600)
        : _Wheel(WHEEL_SIZE),
          _WheelSize(WHEEL_SIZE),
          _MaxTimeout(MaxTimeout),
          _CurIndex(0),
          _PreIndex(0)
    {
    }

    ~TimeWheel(){
        for (auto &list : _Wheel){
            for (auto &timer : list){
                timer->Cancel();
            }
        }
    }

    bool HasTimer(uint64_t TimerID){
        return _TimerMap.find(TimerID) != _TimerMap.end();
    }

    uint64_t AddTimer(int TimerID, int Timeout, OnTimerCallback callback){
        if (Timeout < 0 || Timeout > _MaxTimeout){
            return -1;
        }

        SharedPtrTimer timer = std::make_shared<Timer>(TimerID, Timeout);
        // callback 为定时器到期时的回调函数
        timer->SetTimerCallback(callback);
        // timer->SetReleaseCallback(std::bind(&TimeWheel::RemoveWeakPtr, this, std::placeholders::_1));
        // 使用 lambda 表达式初始化一个函数对象
        timer->SetReleaseCallback([this, TimerID]() { this->RemoveWeakPtr(TimerID); });
        // _TimerIDSet.insert(TimerID);
        // 保存TimerID
        // 弱引用既不会增加引用计数，也不会阻止对象被释放
        // 同时可以检测对象是否已经被释放
        _TimerMap[TimerID] = WeakPtrTimer(timer);
        // _TimerIDSet.insert(TimerID);

        // std::cout << "TEST START" << std::endl;
        // std::cout << "_TimerMap.size() = " << _TimerMap.size() << std::endl;
        // std::cout << "TEST END" << std::endl;

        // 时间轮
        // 可以看作是一个环形数组
        int index = (_CurIndex + Timeout) % _WheelSize;
        // 向某个时间轮槽中插入定时器
        // list.push_back()
        _Wheel[index].push_back(timer);
        return timer->GetTimerID();
    }

    void TimerRefresh(uint64_t TimerID)
    {
        auto iter = _TimerMap.find(TimerID);
        assert(iter != _TimerMap.end());
        std::shared_ptr<Timer> timer = iter->second.lock();
        if (timer)
        {
            int timeout = timer->GetTimeout();
            int index = (_CurIndex + timeout) % _WheelSize;
            _Wheel[index].push_back(timer);
        }
        _PreIndex = _CurIndex;
    }

    void CancelTimer(uint64_t TimerID){
        if (!HasTimer(TimerID)){
            return;
        }

        auto weakTimer = _TimerMap[TimerID];
        SharedPtrTimer timer = weakTimer.lock();
        if (timer){
            timer->Cancel();
        }
    }

    void RemoveWeakPtr(uint64_t TimerID){
        auto iter = _TimerMap.find(TimerID);
        if (iter != _TimerMap.end()){
            _TimerMap.erase(iter);
        }
    }

    // void RunOntimeTask()
    // {
    //     _CurIndex = (_CurIndex + 1) % _WheelSize;
    //     auto &timers = _Wheel[_CurIndex];
    //     for (auto &timer : timers)
    //     {
    //         if (timer)
    //         {
    //             timer->Cancel();
    //             _TimerMap.erase(timer->GetTimerID());
    //             _TimerIDSet.erase(timer->GetTimerID());
    //         }
    //     }
    //     timers.clear();
    // }

    void RunOntimeTask()
    {
        // std::cout << "TEST START" << std::endl;
        // std::cout << "_CurIndex = " << _CurIndex << std::endl;
        // std::cout << "_CurIndex = " << _CurIndex << std::endl;
        
        // if(_Wheel[_CurIndex].empty()){
            // std::cout << "Wheel[_CurIndex] is empty." << std::endl;
            // return;
        // }
        
        // std::cout << "TEST END" << std::endl;
        
        auto &timers = _Wheel[_CurIndex];

        _CurIndex = (_CurIndex + 1) % _WheelSize;
        // for (auto &timer : timers)
        // {
        //     if (timer)
        //     {
        //         if (!_TimerIDSet.erase(timer->GetTimerID())) {
        //             continue;
        //         }
        //         std::cout << "Timer ID: " << timer->GetTimerID() << std::endl;
        //         _TimerMap.erase(timer->GetTimerID());

        //         std::cout << "TEST START" << std::endl;
        //         std::cout << __LINE__ << std::endl;
        //         std::cout << "Callback function is called" << std::endl;
        //         std::cout << "TEST END" << std::endl;

        //         timer->Cancel();  // 取消定时器
        //     }
        // }
        timers.clear();
    }

    int GetCurIndex(){
        return this->_CurIndex;
    }
    int GetWheelSize(){
        return this->_WheelSize;
    }
    int GetMaxTimeout(){
        return this->_MaxTimeout;
    }
    int GetPreIndex(){
        return this->_PreIndex;
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
    std::cout << "定时任务被执⾏，释放任务对象!\n";
    delete t;
}

int main()
{
    /*创建⼀个固定5s的定时任务队列， 每个添加的任务将在被添加5s后执⾏*/
    TimeWheel tq(5);
    /*new 了⼀个对象*/
    TimerTest *t = new TimerTest(10);
    /*随便设置了⼀个定时器ID*/
    int id = 3;
    /*std::bind(del, t) 构建适配了⼀个释放函数，作为定时任务执⾏函数*/
    /*这⾥其实就是添加了⼀个5秒后执⾏的定时任务，任务是销毁t指针指向的空间*/
    if(tq.AddTimer(id, 5, std::bind(del, t)) == -1){
        std::cout << "ERROR" << std::endl;
        std::cout << id << "号定时任务添加失败!" << std::endl;
    }
    /*按理说这个任务5s后就会被执⾏，析构，但是因为循环内总是在刷新任务，也就是⼆次添加任
   务，因此，它的计数总是>0，不会被释放，之后最后⼀个任务对象shared_ptr被释放才会真正析构*/
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
        std::cout << "Time has passed: " << tq.GetCurIndex() - tq.GetPreIndex() << std::endl;
        sleep(1);
        tq.RunOntimeTask(); // 每秒调⽤⼀次，模拟定时器
        if (tq.HasTimer(id) == false)
        {
            
            std::cout << "定时任务已经被执⾏完毕！\n";
            break;
        }
    }
    
    // sleep(5);

    return 0;
}