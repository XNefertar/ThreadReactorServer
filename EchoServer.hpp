#include "TCPServer.hpp"
class EchoServer{
private:
    TCPServer _Server;
private:
    void OnConnected(const PtrConnection& conn){
        DBG_LOG("OnConnected");
    }
    void OnMessage(const PtrConnection& conn, Buffer* buffer){
        DBG_LOG("OnMessage");
        conn->Send(buffer->GetReadIndex(), buffer->GetReadableSize());
        buffer->UpdateReadIndex(buffer->GetReadableSize());
        conn->Shutdown();
    }
    void OnClose(const PtrConnection& conn){
        DBG_LOG("CLOSE CONNECTION:%p", conn.get());
    }
public:
    EchoServer(int port, int threadNum = 0, int timeout = 0, bool enableInactiveRelease = false)
        :_Server(port)
    {
        _Server.SetThreadCount(2);
        _Server.SetEnableInactiveRelease(10);
        _Server.SetConnectedCallback(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
        _Server.SetMessageCallback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
        _Server.SetCloseCallback(std::bind(&EchoServer::OnClose, this, std::placeholders::_1));
    }
    void Start(){
        _Server.Start();
    }
};