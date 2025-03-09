#ifndef __CONNECTION_HPP__
#define __CONNECTION_HPP__

#include <memory>
#include "EventLoop.hpp"
#include "Socket.hpp"
#include "Channel.hpp"
#include "Buffer.hpp"
#include "Any.hpp"

class Connection;

typedef enum { DISCONNECTED, CONNECTING, CONNECTDE, DISCONNECTING } Connstatus;
using PtrConnection = std::shared_ptr<Connection>;


class Connection: public std::enable_shared_from_this<Connection>{
private:
    int _Sockfd;
    uint64_t _ConnId;
    bool _EnableInactiveRelease;
    EventLoop* _Loop;
    Socket _Socket;
    Channel _Channel;
    Buffer _InputBuffer;
    Buffer _OutputBuffer;
    Connstatus _Status;
    PtrConnection _Ptr;
    Any _Context;

    using ConnectionCallback = std::function<void(const PtrConnection&)>;
    using MessageCallback = std::function<void(const PtrConnection&, Buffer*)>;
    using CloseCallback = std::function<void(const PtrConnection&)>;
    using AnyEventCallback = std::function<void(const PtrConnection&)>;

    ConnectionCallback _ConnectionCallback;
    MessageCallback _MessageCallback;
    CloseCallback _CloseCallback;
    AnyEventCallback _AnyEventCallback;

    CloseCallback _ServerCloseCallback;

private:
    void HandleRead();
    void HandleWrite();
    void HandleError();
    void HandleClose();
    void HandleEvent();

    void EstablishedInLoop(){
        if(_EnableInactiveRelease){
            _Loop->TimerRefresh(_ConnId);
        }
        if(_AnyEventCallback){
            _AnyEventCallback(shared_from_this());
        }
    }

    void ReleaseInLoop(){
        _Status = DISCONNECTED;
        _Channel.Remove();
        _Socket.Close();

        if(_Loop->HasTimer(_ConnId)){
            CancelInactiveReleaseInLoop();
        }
        if(_CloseCallback){
            _CloseCallback(shared_from_this());
        }
        if(_ServerCloseCallback){
            _ServerCloseCallback(shared_from_this());
        }
    }

    void SendInLoop(Buffer& buffer){
        if(_Status == DISCONNECTED){
            return;
        }
        _OutputBuffer.WriteBufferPush(buffer);
        if(_Channel.WriteAble() == false){
            _Channel.EnableWrite();
        }
    }

    void ShutdownInloop(){
        _Status = DISCONNECTING;
        if(_InputBuffer.GetReadableSize() > 0){
            if(_MessageCallback){
                _MessageCallback(shared_from_this(), &_InputBuffer);
            }
        }

        if(_OutputBuffer.GetReadableSize() > 0){
            if(_Channel.WriteAble() == false){
                _Channel.EnableWrite();
            }
        }
        if(_OutputBuffer.GetReadableSize() == 0){
            Release();
        }
    }

    void EnableInactiveReleaseInLoop(uint32_t timeout){
        _EnableInactiveRelease = true;
        if(_Loop->HasTimer(_ConnId)){
            return _Loop->TimerRefresh(_ConnId);
        }
        _Loop->TimerAdd(_ConnId, timeout, std::bind(&Connection::ReleaseInLoop, shared_from_this()));
    }
    
    void CancelInactiveReleaseInLoop(){
        _EnableInactiveRelease = false;
        if(_Loop->HasTimer(_ConnId)){
            _Loop->TimerCancel(_ConnId);
        }
    }

    void UpgradeInLoop(const Any& context,
                const ConnectionCallback& connectionCallback,
                const MessageCallback& messageCallback,
                const CloseCallback& closeCallback,
                const AnyEventCallback& anyEventCallback){
        _Context = context;
        _ConnectionCallback = connectionCallback;
        _MessageCallback = messageCallback;
        _CloseCallback = closeCallback;
        _AnyEventCallback = anyEventCallback;
    }

public:
    Connection(EventLoop* loop, int sockfd, uint64_t connId):
        _Sockfd(sockfd),
        _ConnId(connId),
        _EnableInactiveRelease(false),
        _Loop(loop),
        _Socket(sockfd),
        _Channel(loop, sockfd),
        _Status(DISCONNECTED),
        _Ptr(this),
        _Context(),
        _ConnectionCallback(),
        _MessageCallback(),
        _CloseCallback(),
        _AnyEventCallback(),
        _ServerCloseCallback(){
            _Channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
            _Channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
            _Channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
            _Channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
            _Channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        }

    ~Connection(){
        DBG_LOG("RELEASE CONNECTION:%p", this);
    }

    int GetFd() const{ return _Sockfd; }
    int GetId() const{ return _ConnId; }
    bool IsConnected() const{ return _Status == CONNECTDE; }

    void SetContext(const Any& context){ _Context = context; }
    Any* GetContext(){ return &_Context; }
    
    void SetConnectedCallback(const ConnectionCallback& cb) { _ConnectionCallback = cb;  }
    void SetMessageCallback(const MessageCallback& cb)      { _MessageCallback = cb;     }
    void SetCloseCallback(const CloseCallback& cb)          { _CloseCallback = cb;       }
    void SetAnyEventCallback(const AnyEventCallback& cb)    { _AnyEventCallback = cb;    }
    void SetServerCloseCallback(const CloseCallback& cb)    { _ServerCloseCallback = cb; }

    void Established(){
        _Loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    }

    void Send(const char* data, size_t len){
        Buffer buffer;
        buffer.WritePush(data, len);
        _Loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buffer)));
    }

    void Shutdown(){
        _Loop->RunInLoop(std::bind(&Connection::ShutdownInloop, this));
    }

    void Release(){
        _Loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }

    void EnableInactiveRelease(uint32_t timeout){
        _Loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, timeout));
    }

    void CancelInactiveRelease(){
        _Loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }

    void Upgrade(const Any& context,
                const ConnectionCallback& connectionCallback,
                const MessageCallback& messageCallback,
                const CloseCallback& closeCallback,
                const AnyEventCallback& anyEventCallback){
        _Loop->AssertInLoop();
        _Loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, connectionCallback, messageCallback, closeCallback, anyEventCallback));
    }
};

void Connection::HandleRead(){
    char buffer[65536];
    ssize_t n = _Socket.RecvNonBlock(buffer, sizeof(buffer));
    if(n < 0){
        return ShutdownInloop();
    }

    _InputBuffer.WritePush(buffer, n);
    if(_InputBuffer.GetReadableSize() > 0){
        return _MessageCallback(shared_from_this(), &_InputBuffer);
    }
}

void Connection::HandleWrite(){
    ssize_t ret = _Socket.SendNonBlock(_OutputBuffer.GetReadIndex(), _OutputBuffer.GetReadableSize());
    if(ret < 0){
        if(_InputBuffer.GetReadableSize() > 0){
            return _MessageCallback(shared_from_this(), &_InputBuffer);
        }
        return Release();
    }

    _OutputBuffer.UpdateReadIndex(ret);
    if(_OutputBuffer.GetReadableSize() == 0){
        _Channel.DisableWrite();
        if(_Status == DISCONNECTING){
            return Release();
        }
    }
    return;
}

void Connection::HandleError(){
    return HandleClose();
}

void Connection::HandleClose(){
    if(_InputBuffer.GetReadableSize() > 0){
        _MessageCallback(shared_from_this(), &_InputBuffer);
    }
    return Release();
}

void Connection::HandleEvent(){
    if(_EnableInactiveRelease){
        _Loop->TimerRefresh(_ConnId);
    }
    if(_AnyEventCallback){
        _AnyEventCallback(shared_from_this());
    }
}

#endif // __CONNECTION_HPP__