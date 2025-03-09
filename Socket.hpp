#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include "Log.hpp"

#define MAX_LISTENFD 5
#define MAX_EVENT 1024
#define MAX_LISTEN_NUM 1024
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

    // 设置非阻塞
    bool SetNonBlock(){
        int flag = fcntl(_fd, F_GETFL, 0);
        if(flag == -1){
            ERR_LOG("Get socket flag failed");
            return false;
        }
        fcntl(_fd, F_SETFL, flag | O_NONBLOCK);
        return true;
    }

    // 设置地址复用
    void ReuseAddr(){
        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    // 设置端口复用
    void ReusePort(){
        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    }

    // 创建socket
    // 地址复用和端口复用标志位
    bool Create(bool AddrReuseFlag = 0, bool PortReuseFlag = 0){
        // 设置地址和端口复用
        if(AddrReuseFlag) { ReuseAddr(); }
        if(PortReuseFlag) { ReusePort(); }

        // 创建socket
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if(_fd == -1){
            ERR_LOG("Create socket failed");
            return false;
        }
        return true;
    }

    // bind socket
    // 绑定ip和端口
    bool Bind(const std::string& ip, uint16_t port){
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if(bind(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            ERR_LOG("Bind socket failed");
            return false;
        }
        return true;
    }


    // listen socket
    // 监听socket
    bool Listen(int backlog = MAX_LISTEN_NUM){
        if(listen(_fd, MAX_LISTEN_NUM) == -1){
            ERR_LOG("Listen socket failed");
            return false;
        }
        return true;
    }

    // connect socket
    // 连接socket
    bool Connect(const std::string& ip, uint16_t port){
        _addr.sin_family = AF_INET;
        // htons: 将主机字节序转换为网络字节序
        // 网络通信使用的是大端字节序，需要统一字节序
        // host to network short
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if(connect(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            ERR_LOG("Connect socket failed");
            return false;
        }
        return true;
    }

    // accept socket
    // 接受socket
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
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 没有客户端连接，非阻塞模式下返回 -1
                return -1;
            } 
            else {
                ERR_LOG("Accept socket failed");
                return -1;
            }
        }
        // 设置客户端套接字为非阻塞模式
        int flags = fcntl(clientFd, F_GETFL, 0);
        if(flags == -1){
            ERR_LOG("Get client socket flags failed");
            close(clientFd);
            return -1;
        }
        if(fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1){
            ERR_LOG("Set client socket non-blocking failed");
            close(clientFd);
            return -1;
        }
        return clientFd;
    }

    int ConnectNonBlock(const std::string& ip, uint16_t port){
        // 设置套接字为非阻塞模式
        int flags = fcntl(_fd, F_GETFL, 0);
        if(flags == -1){
            ERR_LOG("Get socket flags failed");
            return -1;
        }
        if(fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1){
            ERR_LOG("Set socket non-blocking failed");
            return -1;
        }
    
        // 设置地址信息
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
        // 尝试连接
        if(connect(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
            if(errno == EINPROGRESS){
                // 连接正在进行中，非阻塞模式下返回 0
                return 0;
            }
            ERR_LOG("Connect socket failed");
            return -1;
        }
        // 连接成功，返回 1
        return 1;
    }


    ssize_t Recv(void* buf, size_t len, int flag = 0){
        // 调用系统接口
        ssize_t recvLen = recv(_fd, buf, len, flag);
        if(recvLen == -1){
            ERR_LOG("Recv socket failed");
            return -1;
        }
        return recvLen;
    }
    // 设置 recv 函数为非阻塞模式
    // 对应的标志为 MSG_DONTWAIT
    ssize_t RecvNonBlock(void* buf, size_t len, int flag = MSG_DONTWAIT){
        return Recv(buf, len, flag);
    }


    ssize_t Send(const void* buf, size_t len, int flag = 0){
        // 调用系统接口
        ssize_t sendLen = send(_fd, buf, len, flag);
        if(sendLen == -1){
            ERR_LOG("Send socket failed");
            return -1;
        }
        return sendLen;
    }
    // 设置 send 函数为非阻塞模式
    // 对应的标志为 MSG_DONTWAIT
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

    // 创建服务器
    // 创建服务器的同时绑定地址和端口
    // 可以选择是否设置为非阻塞模式
    // 默认为不设置非阻塞模式
    bool CreateServer(uint16_t port, const std::string& ip = "0.0.0.0", bool block_flag = false){
        if(!Create(true, true)){
            return false;
        }
        if(block_flag){
            // 设置非阻塞模式
            if(!SetNonBlock()){
                return false;
            }
        }
        // 绑定地址和端口
        if(!Bind(ip.c_str(), port)){
            return false;
        }
        // 监听端口
        if(!Listen()){
            return false;
        }

        // 设置地址和端口复用
        // ReuseAddr();
        // ReusePort();
        return true;
    }

    // 创建客户端
    // 创建客户端的同时连接服务器
    // 可以选择是否设置为非阻塞模式
    // 默认为不设置非阻塞模式
    bool CreateClient(const std::string& ip, uint64_t port, bool block_flag = false){
        if(!Create()){
            return false;
        }
        if(block_flag){
            if(!SetNonBlock()){
                return false;
            }
        }
        if(!Connect(ip, port)){
            return false;
        }
        return true;
    }

    int GetFd() const{
        return _fd;
    }
};


#endif // __SOCKET_HPP__