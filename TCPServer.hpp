#include "EventLoop.hpp"
#include "Acceptor.hpp"
#include "Connection.hpp"

class TCPServer{
private:
    uint64_t _NextID;
    int _Port;
    int _Timeout;
    bool _EnableInactiveRelease;
    EventLoop _BaseLoop;
    Acceptor _Acceptor;
    LoopThreadPool _ThreadPool;
    std::unordered_map<uint64_t, PtrConnection> _Connections;

    using ConnectedCallback = std::function<void(const PtrConnection&)>;
    using MessageCallback = std::function<void(const PtrConnection&, Buffer*)>;
    using CloseCallback = std::function<void(const PtrConnection&)>;
    using AnyEventCallback = std::function<void(const PtrConnection&)>;
    using Functor = std::function<void()>;

    ConnectedCallback _ConnectedCallback;
    MessageCallback _MessageCallback;
    CloseCallback _CloseCallback;
    AnyEventCallback _AnyEventCallback;

private:
    void RunAfterInLoop(int timeout, const Functor& task){
        _NextID++;
        _BaseLoop.TimerAdd(_NextID, timeout, task);
    }

    void NewConnection(int fd){
        _NextID++;
        PtrConnection conn(new Connection(&_BaseLoop, fd, _NextID));
        conn->SetConnectedCallback(_ConnectedCallback);
        conn->SetMessageCallback(_MessageCallback);
        conn->SetCloseCallback(_CloseCallback);
        conn->SetAnyEventCallback(_AnyEventCallback);
        conn->SetServerCloseCallback(std::bind(&TCPServer::RemoveConnection, this, std::placeholders::_1));
        if(_EnableInactiveRelease){
            conn->EnableInactiveRelease(_Timeout);
        }
        conn->Established();
        _Connections[_NextID] = conn;
    }

    void RemoveConnectionInLoop(const PtrConnection& conn){
        int ID = conn->GetId();
        auto iter = _Connections.find(ID);
        if(iter != _Connections.end()){
            _Connections.erase(iter);
        }
    }

    void RemoveConnection(const PtrConnection& conn){
        _BaseLoop.QueueInLoop(std::bind(&TCPServer::RemoveConnectionInLoop, this, conn));
    }

public:
    TCPServer(int port)
        :_NextID(0)
        ,_Port(port)
        ,_Timeout(0)
        ,_EnableInactiveRelease(false)
        ,_BaseLoop()
        ,_Acceptor(&_BaseLoop, port)
        ,_ThreadPool(&_BaseLoop)
    {
        _Acceptor.SetAcceptCallback(std::bind(&TCPServer::NewConnection, this, std::placeholders::_1));
        _Acceptor.Listen();
    }

    void SetThreadCount(int count){
        return _ThreadPool.SetThreadCount(count);
    }
    void SetConnectedCallback(const ConnectedCallback& cb){
        _ConnectedCallback = cb;
    }
    void SetMessageCallback(const MessageCallback& cb){
        _MessageCallback = cb;
    }
    void SetCloseCallback(const CloseCallback& cb){
        _CloseCallback = cb;
    }
    void SetAnyEventCallback(const AnyEventCallback& cb){
        _AnyEventCallback = cb;
    }
    void SetEnableInactiveRelease(uint32_t timeout){
        _EnableInactiveRelease = true;
        _Timeout = timeout;
    }
    void RunAfter(const Functor& task, int timeout){
        _BaseLoop.RunInLoop(std::bind(&TCPServer::RunAfterInLoop, this, timeout, task));
    }

    void Start(){
        _ThreadPool.Create();
        _BaseLoop.Start();
    }
};