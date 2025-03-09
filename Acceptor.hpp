#include "Socket.hpp"
#include "EventLoop.hpp"
#include "Channel.hpp"

class Acceptor{
    using AcceptCallback = std::function<void(int)>;
private:
    Socket _Socket;
    EventLoop* _Loop;
    Channel _Channel;

    AcceptCallback _AcceptCallback;
private:
    void HandleRead(){
        int fd = _Socket.Accept();
        if(fd == -1){
            return;
        }
        if(_AcceptCallback){
            _AcceptCallback(fd);
        }
    }

    int CreateServer(int port){
        bool ret = _Socket.CreateServer(port);
        assert(ret);
        return _Socket.GetFd();
    }

public:
    Acceptor(EventLoop* loop, int port)
        :_Loop(loop),
        _Socket(CreateServer(port)),
        _Channel(loop, _Socket.GetFd())
    {
        _Channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }

    void SetAcceptCallback(const AcceptCallback& callback){
        _AcceptCallback = callback;
    }

    void Listen(){
        _Channel.EnableRead();
    }
};