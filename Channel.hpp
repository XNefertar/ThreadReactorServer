#ifndef __Channel_HPP__
#define __Channel_HPP__

#include <functional>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include "EventLoop.hpp"

// 用于管理描述符的事件
// 并调用相应的回调函数
class Channel
{
private:
    int _fd;
    // _events: 监听事件
    // _revents: 就绪事件（连接触发事件）
    uint32_t _events;
    uint32_t _revents;

    // 事件循环
    EventLoop* _loop;

    // 事件回调函数类型
    // 使用std::function实现
    using EventCallback = std::function<void()>;

    // 事件回调函数
    // 读事件、写事件、错误事件、关闭事件、其他事件
    EventCallback _readCallback;
    EventCallback _writeCallback;
    EventCallback _errorCallback;
    EventCallback _closeCallback;
    EventCallback _eventCallback;

public:
    Channel(EventLoop* loop, int fd)
        : _loop(loop),
          _fd(fd),
          _events(0),
          _revents(0)
    {}

    int Getfd() const { return _fd; }
    uint32_t GetEvents() const { return _events; }
    uint32_t GetRevents() const { return _revents; }

    void SetRevents(uint32_t revents) { _revents = revents; } // 设置活动事件
    void SetEvents(uint32_t events) { _events = events; }

    // 设置事件回调函数
    void SetReadCallback(const EventCallback& cb)   { _readCallback = cb; }
    void SetWriteCallback(const EventCallback& cb)  { _writeCallback = cb; }
    void SetErrorCallback(const EventCallback& cb)  { _errorCallback = cb; }
    void SetCloseCallback(const EventCallback& cb)  { _closeCallback = cb; }
    void SetEventCallback(const EventCallback& cb)  { _eventCallback = cb; }

    // 事件类型判断
    bool ReadAble() const   { return _revents & EPOLLIN; }
    bool WriteAble() const  { return _revents & EPOLLOUT; }
    bool Error() const      { return _revents & EPOLLERR; }
    bool Close() const      { return _revents & EPOLLHUP; }

    void Update();
    void Remove();

    // 事件处理
    // epoll 机制触发事件后，调用该函数处理事件
    void EnableRead()   { _events |= EPOLLIN; Update(); } // TODO: Update
    void EnableWrite()  { _events |= EPOLLOUT; Update(); }
    void DisableRead()  { _events &= ~EPOLLIN; Update(); }
    void DisableWrite() { _events &= ~EPOLLOUT; Update(); }
    void DisableAll()   { _events = 0; Update(); }

    void HandleEvent()
    {
        // (EPOLLIN && EPOLLRDHUP && EPOLLPRI) related to read
        // EPOLLOUT related to write
        // EPOLLERR related to error
        // EPOLLHUP related to close

        // 一般来说，读事件可能与其他事件一同发生
        // 所以要与 if-else 隔断
        if ((   
            _revents & EPOLLIN
         || _revents & EPOLLPRI 
         || _revents & EPOLLRDHUP) 
         && _readCallback)
        { _readCallback(); }


        // 而其他事件一般独立发生
        // 可以直接使用 if-else 结构
        if (_revents & EPOLLOUT && _writeCallback) { _writeCallback(); }
        else if (_revents & EPOLLERR && _errorCallback) { _errorCallback(); }
        else if (_revents & EPOLLHUP && _closeCallback) { _closeCallback(); }
        if (_eventCallback) { _eventCallback(); }
    }
};

void Channel::Update(){
    _loop->UpdateEvent(this);
}

void Channel::Remove(){
    _loop->RemoveEvent(this);
}

#endif // __Channel_HPP__