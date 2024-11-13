#ifndef __Channel_HPP__
#define __Channel_HPP__

#include <functional>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include "EventLoop.hpp"

class Channel
{
private:
    int _fd;
    uint32_t _events;
    uint32_t _revents;

    EventLoop* _loop; // TODO: EventLoop

    using EventCallback = std::function<void()>;
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

    void SetReadCallback(const EventCallback& cb)   { _readCallback = cb; }
    void SetWriteCallback(const EventCallback& cb)  { _writeCallback = cb; }
    void SetErrorCallback(const EventCallback& cb)  { _errorCallback = cb; }
    void SetCloseCallback(const EventCallback& cb)  { _closeCallback = cb; }
    void SetEventCallback(const EventCallback& cb)  { _eventCallback = cb; }

    bool ReadAble() const   { return _revents & EPOLLIN; }
    bool WriteAble() const  { return _revents & EPOLLOUT; }
    bool Error() const      { return _revents & EPOLLERR; }
    bool Close() const      { return _revents & EPOLLHUP; }

    void Update();
    void Remove();

    void EnableRead()   { _events |= EPOLLIN; Update(); } // TODO: Update
    void EnableWrite()  { _events |= EPOLLOUT; Update(); }
    void DisableRead()  { _events &= ~EPOLLIN; Update(); }
    void DisableWrite() { _events &= ~EPOLLOUT; Update(); }
    void DisableAll()   { _events = 0; Update(); }

    void HandleEvent()
    {
        // (EPOLLIN && EPOLLRDHUP && EPOLLPRI) related to read
        if ((_revents & EPOLLIN || _revents & EPOLLPRI || _revents & EPOLLRDHUP) && _readCallback) { _readCallback(); }
        if (_revents & EPOLLOUT && _writeCallback) { _writeCallback(); }
        else if (_revents & EPOLLERR && _errorCallback) { _errorCallback(); }
        else if (_revents & EPOLLHUP && _closeCallback) { _closeCallback(); }
        if (_eventCallback) { _eventCallback(); }
    }
};

void Channel::Update()
{
    _loop->UpdateEvent(this);
}
void Channel::Remove()
{
    _loop->RemoveEvent(this);
}


#endif // __Channel_HPP__