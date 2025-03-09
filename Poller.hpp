#ifndef __POLLER_HPP__
#define __POLLER_HPP__

#include <unordered_map>
#include <unistd.h>
#include <sys/epoll.h>
#include "Channel.hpp"
#include "Log.hpp"

#define MAX_POLLER_SIZE 1024
// 使用 epoll 机制实现 Poller
class Poller
{
private:
    int _epollfd;
    // _events 数组用于存储 epoll 事件
    struct epoll_event _events[MAX_POLLER_SIZE];
    // 使用 hash_map 存储描述符和 Channel 的映射关系
    std::unordered_map<int, Channel*> _channels;

private:
    // 更新描述符的事件
    void UpdateChannel(Channel* channel, int op){
        int fd = channel->Getfd();
        struct epoll_event ev;
        ev.events = channel->GetEvents();
        ev.data.fd = fd;
        // epoll_ctl: epoll的事件注册函数
        // int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
        // op:
        // EPOLL_CTL_ADD: 注册新的fd到epfd中
        // EPOLL_CTL_MOD: 修改已经注册的fd的监听事件
        // EPOLL_CTL_DEL: 从epfd中删除一个fd
        if(epoll_ctl(_epollfd, op, fd, &ev) < 0){
            ERR_LOG("epoll_ctl error");
            exit(1);
        }
        return;
    }

    bool HasChannel(Channel* channel){
        return _channels.find(channel->Getfd()) != _channels.end();
    }

public:
    Poller(){
        // 创建一个 epoll 句柄
        _epollfd = epoll_create(MAX_POLLER_SIZE);
        if(_epollfd < 0){
            ERR_LOG("epoll_create error");
            exit(1);
        }
    }

    void UpdateChannel(Channel* channel){
        // 修改事件
        if(HasChannel(channel)){
            UpdateChannel(channel, EPOLL_CTL_MOD);
        }
        // 添加事件
        else{
            _channels[channel->Getfd()] = channel;
            UpdateChannel(channel, EPOLL_CTL_ADD);
        }
    }

    void RemoveChannel(Channel* channel){
        if(!HasChannel(channel)){
            return;
        }
        if(_channels.find(channel->Getfd()) == _channels.end()){
            return;
        }
        _channels.erase(channel->Getfd());
        UpdateChannel(channel, EPOLL_CTL_DEL);
    }

    // epoll_wait: 等待事件的产生
    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
    void Poll(std::vector<Channel*>& activeChannels){
        // 阻塞式调用，等待事件的产生
        // 是否阻塞由 timeout 决定
        // timeout = -1: 阻塞
        // timeout = 0: 立即返回
        // timeout > 0: 等待 timeout 毫秒后返回
        int nfds = epoll_wait(_epollfd, _events, MAX_POLLER_SIZE, -1);
        if(nfds < 0){
            ERR_LOG("epoll_wait error");
            exit(1);
        }
        for(int i = 0; i < nfds; i++){
            // epoll_wait 返回的是就绪事件数量
            // 需要遍历 events[0] ~ events[n - 1] 处理事件
            // 通过 data.fd 获取就绪的描述符
            Channel* channel = _channels[_events[i].data.fd];
            // 交由 Channel 处理
            channel->SetRevents(_events[i].events);
            activeChannels.push_back(channel);
        }
        return;
    }
};



#endif // __POLLER_HPP__