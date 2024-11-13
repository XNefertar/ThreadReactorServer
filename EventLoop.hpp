#ifndef __EVENTLOOP_HPP__
#define __EVENTLOOP_HPP__

#include <mutex>
#include <thread>
#include <vector>
#include <cassert>
#include <functional>
#include <sys/eventfd.h>
#include "Log.hpp"
#include "Poller.hpp"
#include "Channel.hpp"
#include "TimeWheel.hpp"

class EventLoop
{
    using Functor = std::function<void()>;
private:
    int _EventFd;
    std::thread::id _ThreadID; // 线程ID
    TimerWheel _TimerWheel; // 定时器模块
    std::unique_ptr<Channel> _EventChannel;
    Poller _Poller; // 文件描述符监控
    std::vector<Functor> _Tasks; // 任务池
    std::mutex _Mutex;

public:
    void RunAllTask(){
        // 为什么不直接在加锁状态下直接执行_tasks内的任务，而使用swap函数？
        // 1. 会长时间持有锁，导致程序整体的执行效率下降；
        // 2. swap函数高效且不增加内存开销，本质上是常数时间的操作；
        // 3. 将任务队列的执行和管理解耦，保证二者之间的独立性
        std::vector<Functor> _TempFunctor;
        {
            std::lock_guard lk(_Mutex);
            _Tasks.swap(_TempFunctor);
        }
        // 在临时函数容器中执行任务队列
        for(auto &func : _TempFunctor){
            func();
        }
        return;
    }

    static int CreateEventFd(){
        int EventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if(EventFd < 0){
            ERR_LOG("CREATE EVENTFD ERROR");
            std::abort();
        }
        return EventFd;
    }

    void ReadEventFd(){
        uint64_t res = 0;
        int ret = read(_EventFd, &res, sizeof(res));
        if(ret < 0){
            if(errno == EINTR || errno == EAGAIN){
                return;
            }
            ERR_LOG("READ EVENTFD ERROR");
            std::abort();
        }
        return;
    }

    void WakeUpEventFd(){
        uint64_t val = 1;
        int ret = write(_EventFd, &val, sizeof(val));
        if(ret < 0){
            if(errno == EINTR){
                return;
            }
            ERR_LOG("WRITE VAL ERROR");
            std::abort();
        }
        return;
    }
public:
    EventLoop()
        :_ThreadID(std::this_thread::get_id()),
        _EventFd(CreateEventFd()),
        _EventChannel(new Channel(this, _EventFd)),
        _TimerWheel(this)
    {
        _EventChannel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
        _EventChannel->EnableRead();
    }

        void Start(){
            for(;;){
                std::vector<Channel*> Activities;
                _Poller.Poll(Activities);

                for(auto &channel : Activities){
                    channel->HandleEvent();
                }

                RunAllTask();
            }
        }

    bool IsInLoop(){
        return (_ThreadID == std::this_thread::get_id());
    }

    void AssertInLoop(){
        assert(_ThreadID == std::this_thread::get_id());
    }

    // 判断将要执⾏的任务是否处于当前线程中，如果是则执⾏，不是则压⼊队列。
    void RunInLoop(const Functor &cb){
        if (IsInLoop()){
            return cb();
        }
        return QueueInLoop(cb);
    }

    // 将操作压⼊任务池
    void QueueInLoop(const Functor &cb){
        {
            std::unique_lock<std::mutex> _lock(_Mutex);
            _Tasks.push_back(cb);
        }
        // 唤醒有可能因为没有事件就绪，⽽导致的epoll阻塞；
        // 其实就是给eventfd写⼊⼀个数据，eventfd就会触发可读事件
        WakeUpEventFd();
    }

    // 添加/修改描述符的事件监控
    void UpdateEvent(Channel *channel) { return _Poller.UpdateChannel(channel); }

    // 移除描述符的监控
    void RemoveEvent(Channel *channel) { return _Poller.RemoveChannel(channel); }
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _TimerWheel.TimerAdd(id, delay, cb); }

    void TimerRefresh(uint64_t id){ return _TimerWheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _TimerWheel.TimerCancel(id); }
    bool HasTimer(uint64_t id) { return _TimerWheel.HasTimer(id); }
};



#endif // __EVENTLOOP_HPP__