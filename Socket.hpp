#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include "Log.hpp"

class NetWrok
{
public:
    NetWrok(){
        // 避免服务器因为给断开连接的客⼾端进⾏send触发异常导致程序崩溃，因此忽略SIGPIPE信号
        // SIGPIPE信号的默认处理是终⽌进程
        DBG_LOG("SIGPIPE INIT");
        signal(SIGPIPE, SIG_IGN);
    }
};
// 定义静态全局是为了保证构造函数中的信号忽略处理能够在程序启动阶段就被直接执⾏
static NetWrok g_network;

class Socket
{
private:
    int _fd;
    struct sockaddr_in _addr;

public:
    Socket()
        :_fd(-1)
    {}

    Socket(int fd)
        :_fd(fd)
    {}

    ~Socket(){
        if(_fd != -1){
            close(_fd);
        }
    }

    bool SetNonBlock(){
        int flag = fcntl(_fd, F_GETFL, 0);
        if(flag == -1){
            ERR_LOG("Get socket flag failed");
            return false;
        }
        fcntl(_fd, F_SETFL, flag | O_NONBLOCK);
        return true;
    }

    void ReuseAddr(){
        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    void ReusePort(){
        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    }

    bool Create(){
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if(_fd == -1){
            ERR_LOG("Create socket failed");
            return false;
        }
        return true;
    }

    bool Bind(const char* ip, uint16_t port){
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip);
        if(bind(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            ERR_LOG("Bind socket failed");
            return false;
        }
        return true;
    }

    bool Listen(int backlog = 1024){
        if(listen(_fd, 1024) == -1){
            ERR_LOG("Listen socket failed");
            return false;
        }
        return true;
    }

    bool Connect(const std::string& ip, uint16_t port){
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if(connect(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            ERR_LOG("Connect socket failed");
            return false;
        }
        return true;
    }

    int Accept(){
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int clientFd = accept(_fd, (struct sockaddr*)&addr, &len);
        if(clientFd == -1){
            ERR_LOG("Accept socket failed");
            return -1;
        }
        return clientFd;
    }

    int AcceptNonBlock(){
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int clientFd = accept(_fd, (struct sockaddr*)&addr, &len);
        if(clientFd == -1){
            ERR_LOG("Accept socket failed");
            return -1;
        }
        return clientFd;
    }

    int ConnectNonBlock(const std::string& ip, uint16_t port){
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if(connect(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            if(errno == EINPROGRESS){
                return 0;
            }
            ERR_LOG("Connect socket failed");
            return -1;
        }
        return 1;
    }


    ssize_t Recv(void* buf, size_t len, int flag = 0){
        ssize_t recvLen = recv(_fd, buf, len, flag);
        if(recvLen == -1){
            ERR_LOG("Recv socket failed");
            return -1;
        }
        return recvLen;
    }

    ssize_t RecvNonBlock(void* buf, size_t len, int flag = MSG_DONTWAIT){
        return Recv(buf, len, flag);
    }

    ssize_t Send(const void* buf, size_t len, int flag = 0){
        ssize_t sendLen = send(_fd, buf, len, flag);
        if(sendLen == -1){
            ERR_LOG("Send socket failed");
            return -1;
        }
        return sendLen;
    }

    ssize_t SendNonBlock(const void* buf, size_t len, int flag = MSG_DONTWAIT){
        if(len == 0) return 0;
        return Send(buf, len, flag);
    }

    void Close(){
        if(_fd != -1){
            close(_fd);
            _fd = -1;
        }
    }

    bool CreateServer(uint64_t port, const std::string& ip = "0.0.0.0", bool block_flag = false){
        if(Create() == false){
            return false;
        }
        if(block_flag == false){
            if(SetNonBlock() == false){
                return false;
            }
        }
        if(Bind(ip.c_str(), port) == false){
            return false;
        }
        if(Listen() == false){
            return false;
        }
        ReuseAddr();
        ReusePort();
        return true;
    }

    bool CreateClient(const std::string& ip, uint64_t port, bool block_flag = false){
        if(Create() == false){
            return false;
        }
        if(block_flag == false){
            if(SetNonBlock() == false){
                return false;
            }
        }
        if(Connect(ip, port) == false){
            return false;
        }
        return true;
    }
};


#endif // __SOCKET_HPP__