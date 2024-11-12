#ifndef __POLLER_HPP__
#define __POLLER_HPP__

#include <unordered_map>
#include <unistd.h>
#include <sys/epoll.h>
#include "Channel.hpp"
#include "Log.hpp"

#define MAX_POLLER_SIZE 1024
class Poller
{
private:
    int _epollfd;
    struct epoll_event _events[MAX_POLLER_SIZE];
    std::unordered_map<int, Channel*> _channels;

private:
    void UpdateChannel(Channel* channel, int op){
        int fd = channel->Getfd();
        struct epoll_event ev;
        ev.events = channel->GetEvents();
        ev.data.fd = fd;
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
        _epollfd = epoll_create(MAX_POLLER_SIZE);
        if(_epollfd < 0){
            ERR_LOG("epoll_create error");
            exit(1);
        }
    }

    void UpdateChannel(Channel* channel){
        if(HasChannel(channel)){
            UpdateChannel(channel, EPOLL_CTL_MOD);
        }
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

    void Poll(std::vector<Channel*>& activeChannels){
        int nfds = epoll_wait(_epollfd, _events, MAX_POLLER_SIZE, -1);
        if(nfds < 0){
            ERR_LOG("epoll_wait error");
            exit(1);
        }
        for(int i = 0; i < nfds; i++){
            Channel* channel = _channels[_events[i].data.fd];
            channel->SetRevents(_events[i].events);
            activeChannels.push_back(channel);
        }
        return;
    }
};



#endif // __POLLER_HPP__