#include <mutex>
#include <thread>
#include <vector>
#include <cassert>
#include <functional>
#include <condition_variable>
#include <unistd.h>
#include <sys/eventfd.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <list>
#include <unistd.h>
#include <sys/timerfd.h>
#include <cstdio>
#include <functional>
#include <sys/epoll.h>
#include <sys/types.h>
#include <typeinfo>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <ctime>
#include <cstdio>
#include <thread>
#include <sstream>


#define MAX_POLLER_SIZE 1024
#define MAX_LISTENFD 5
#define MAX_EVENT 1024
#define MAX_LISTEN_NUM 1024


// 日志宏颜色等级
#define RESET_COLOR "\033[0m"
#define BLUE_COLOR "\033[34m"
#define YELLOW_COLOR "\033[33m"
#define RED_COLOR "\033[31m"

// 日志等级
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF

// 日志等级转字符串
#define LEVEL_TO_STRING(x) (\
    (x == INF) ? BLUE_COLOR "INF" RESET_COLOR : \
    (x == DBG) ? YELLOW_COLOR "DBG" RESET_COLOR : \
    (x == ERR) ? RED_COLOR "ERR" RESET_COLOR : "UNKNOWN" \
)

// 日志输出
#define LOG(level, format, ...) do { \
    if (level < LOG_LEVEL) break; \
    time_t t = time(NULL); \
    struct tm *ltm = localtime(&t); \
    char tmp[32] = {0}; \
    strftime(tmp, 31, "%Y-%m-%d %H:%M:%S", ltm); \
    std::ostringstream oss; \
    oss << std::this_thread::get_id(); \
    fprintf(stdout, "[%s %s %s:%d %s] " format "\n", oss.str().c_str(), tmp, __FILE__, __LINE__, LEVEL_TO_STRING(level), ##__VA_ARGS__); \
} while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)



class Buffer
{
private:
    std::vector<char> _buffer;
    ssize_t _readIndex;
    ssize_t _writeIndex;

public:
    // Constructor
    Buffer(ssize_t size = 1024)
        : _buffer(size), _readIndex(0), _writeIndex(0)
    {}
    // destructor
    ~Buffer() = default;

    // 获取缓冲区指针的方法
    char *GetBegin()      { return &_buffer[0]; }
    char *GetWriteIndex() { return &_buffer[_writeIndex]; }
    char *GetReadIndex()  { return &_buffer[_readIndex]; }

    // 获取缓冲区大小的方法
    ssize_t GetSize() { return _buffer.size(); }
    // 获取缓冲区的可读大小
    // 返回尾部剩余空间
    uint64_t GetTailRestSize() { return _buffer.size() - _writeIndex; }
    // 获取缓冲区的可读大小
    uint64_t GetReadableSize() { return _writeIndex - _readIndex; }
    // 获取缓冲区的头部剩余空间
    uint64_t GetHeadRestSize() { return _readIndex; }

    void UpdateReadIndex(uint64_t len)  { _readIndex += len; }
    void UpdateWriteIndex(uint64_t len) { _writeIndex += len; }

    // 考虑扩容
    void ExpansionWriteSize(uint64_t len){
        // 如果缓冲区尾部剩余空间足够
        // 则无需扩容
        if(len <= GetTailRestSize()) return;

        // 如果尾部剩余空间不够
        // 但是头部剩余空间加尾部剩余空间足够
        // 则将数据搬移到头部剩余空间
        if(len <= GetHeadRestSize() + GetTailRestSize()){
            // std::copy(InputIterator begin, InputIterator end, OutputIterator destination);
            std::copy(GetReadIndex(), GetWriteIndex(), GetBegin());
            _writeIndex -= _readIndex;
            _readIndex = 0;
        }
        else{
            // 如果头部剩余空间加尾部剩余空间不够
            // 则扩容
            DBG_LOG("Buffer resize %d", _writeIndex + len);
            _buffer.resize(_writeIndex + len);
        }
    }

    // 
    void Write(const void* data, uint64_t len){
        if(len == 0) return;
        // 考虑是否需要扩容
        ExpansionWriteSize(len);
        const char* str = static_cast<const char*>(data);
        std::copy(str, str + len, GetWriteIndex());
    }
    void WritePush(const char* data, uint64_t len){
        Write(data, len);
        UpdateWriteIndex(len);
    }


    void WriteString(std::string str){
        Write(str.c_str(), str.size());
    }
    void WriteStringPush(std::string str){
        WritePush(str.c_str(), str.size());
    }

    
    void WriteBuffer(Buffer& buffer){
        Write(buffer.GetReadIndex(), buffer.GetReadableSize());
    }
    void WriteBufferPush(Buffer& buffer){
        WritePush(buffer.GetReadIndex(), buffer.GetReadableSize());
    }


    void Read(void* OutBuffer, uint64_t len){
        if(len == 0) return;
        if(len > GetReadableSize()){
            ERR_LOG("Buffer Read Error, len: %d, readableSize: %d", len, GetReadableSize());
            return;
        }
        std::copy(GetReadIndex(), GetReadIndex() + len, (char*)OutBuffer);
    }
    void ReadPop(void* OutBuffer, uint64_t len){
        Read(OutBuffer, len);
        UpdateReadIndex(len);
    }


    std::string ReadAsString(uint64_t len){
        assert(len <= GetReadableSize());

        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    std::string ReadAsStringPop(uint64_t len){
        std::string str = ReadAsString(len);
        UpdateReadIndex(len);
        return str;
    }

    // void ReadBuffer(Buffer& buffer, uint64_t len){
    //     assert(len <= GetReadableSize());
    //     buffer.Write(GetReadIndex(), len);
    // }

    // void ReadBufferPop(Buffer& buffer, uint64_t len){
    //     ReadBuffer(buffer, len);
    //     UpdateReadIndex(len);
    // }

    char* FindCRLF(){
        for(int i = _readIndex; i < _writeIndex - 1; i++){
            if(_buffer[i] == '\r' && _buffer[i + 1] == '\n'){
                return &_buffer[i];
            }
        }
        return nullptr;
    }

    std::string GetLine(){
        char* crlf = FindCRLF();
        if(crlf == nullptr) return "";
        std::string str = ReadAsString(crlf - GetReadIndex());
        UpdateReadIndex(2);
        return str;
    }

    std::string GetLinePop(){
        std::string str = GetLine();
        UpdateReadIndex(str.size());
        return str;
    }

    void Clear(){
        _readIndex = 0;
        _writeIndex = 0;
    }
};



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

class Poller;
class EventLoop;
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




class Poller
{
// 使用 epoll 机制实现 Poller

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


using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
    using TaskFunc = std::function<void()>;
    using ReleaseFunc = std::function<void()>;
private:
    uint64_t _id;
    uint32_t _timeout;
    bool _isCanceled;
    TaskFunc _taskFunc;
    ReleaseFunc _releaseFunc;
public:
    TimerTask(uint64_t id, int timeout, const TaskFunc &taskFunc)
        : _id(id),
          _timeout(timeout),
          _taskFunc(taskFunc),
          _isCanceled(false)
    {}

    ~TimerTask()
    {
        if (_taskFunc && !_isCanceled) { _taskFunc(); }
        if (_releaseFunc) { _releaseFunc(); }
    }

    void SetTaskFunc(const TaskFunc &taskFunc) { _taskFunc = taskFunc; }
    void SetReleaseFunc(const ReleaseFunc &releaseFunc) { _releaseFunc = releaseFunc; }
    void Cancel() { _isCanceled = true; }
    uint64_t GetID() const { return _id; }
    uint32_t GetTimeout() const { return _timeout; }
};



class TimerWheel
{
    using WeakTask   = std::weak_ptr<TimerTask>;
    using TimerMap   = std::unordered_map<uint64_t, WeakTask>;
    using TimerSet   = std::unordered_set<uint64_t>;
    using SharedPtr  = std::shared_ptr<TimerTask>;
    using TaskList   = std::list<SharedPtr>;
    using Wheel      = std::vector<TaskList>;

private:
    int _Capacity;
    int _Tick;
    Wheel _Wheel;
    TimerMap _TimerMap;

    EventLoop *_Loop;
    // 定时器描述符
    int _Timerfd; 
    std::unique_ptr<Channel> _TimerChannel;

private:
    static int CreateTimerfd();
    int ReadTimerfd();
    void AddTimer(const SharedPtr &timer);
    void RemoveTimer(uint64_t id);
    void RunOntimeTask();
    void OnTimerTask();
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefreshInLoop(uint64_t id);
    void TimerCancelInLoop(uint64_t id);

public:
    TimerWheel(EventLoop *Loop)
        : _Capacity(60),
          _Tick(0),
          _Wheel(_Capacity),
          _Loop(Loop),
          _Timerfd(CreateTimerfd()),
          _TimerChannel(new Channel(Loop, _Timerfd))
    {
        _TimerChannel->SetReadCallback(std::bind(&TimerWheel::OnTimerTask, this));
        _TimerChannel->EnableRead();
    }

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);

    bool HasTimer(uint64_t id){
        return _TimerMap.find(id) != _TimerMap.end();
    }
};

int TimerWheel::CreateTimerfd()
{
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0){
        perror("timerfd_create error");
        exit(1);
    }

    struct itimerspec new_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    timerfd_settime(timer_fd, 0, &new_value, nullptr);
    return timer_fd;
}

int TimerWheel::ReadTimerfd()
{
    uint64_t times;
    ssize_t n = read(_Timerfd, &times, sizeof(times));
    if (n != sizeof(times)){
        perror("read timerfd error");
        exit(1);
    }
    return times;
}

void TimerWheel::AddTimer(const SharedPtr &timer){
    int timeout = timer->GetTimeout();
    int index = (_Tick + timeout) % _Capacity;
    _Wheel[index].push_back(timer);
    _TimerMap[timer->GetID()] = timer;
}

void TimerWheel::RemoveTimer(uint64_t id){
    auto iter = _TimerMap.find(id);
    if (iter != _TimerMap.end()){
        _TimerMap.erase(iter);
    }
}

void TimerWheel::RunOntimeTask(){
    _Tick = (_Tick + 1) % _Capacity;
    _Wheel[_Tick].clear();
}

void TimerWheel::OnTimerTask(){
    int times = ReadTimerfd();
    for(int i = 0; i < times; i++){
        RunOntimeTask();
    }
}

void TimerWheel::TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb){
    SharedPtr pt(new TimerTask(id, delay, cb));
    pt->SetReleaseFunc(std::bind(&TimerWheel::RemoveTimer, this, id));

    int newPos = (_Tick + delay) % _Capacity;
    _Wheel[newPos].push_back(pt);
    _TimerMap[id] = WeakTask(pt);
}

void TimerWheel::TimerRefreshInLoop(uint64_t id){
    auto iter = _TimerMap.find(id);
    if(iter == _TimerMap.end()) return;
    SharedPtr pt = iter->second.lock();
    int delay = pt->GetTimeout();
    int new_pos = (delay + _Tick) % _Capacity;
    _Wheel[new_pos].push_back(pt); 
}

void TimerWheel::TimerCancelInLoop(uint64_t id){
    auto it = _TimerMap.find(id);
    if(it == _TimerMap.end()){
        return;
    }
    SharedPtr pt = it->second.lock();
    if (pt){
        pt->Cancel();
    }
}


class EventLoop
{
    using Functor = std::function<void()>;
private:
    int _EventFd;
    std::thread::id _ThreadID; // 线程ID
    TimerWheel _TimerWheel; // 定时器模块
    std::unique_ptr<Channel> _EventChannel; // 管理和处理文件描述符上的事件
    Poller _Poller; // 文件描述符监控
    std::vector<Functor> _Tasks; // 任务池
    std::mutex _Mutex;

public:
    void RunAllTask(){
        // 为什么不直接在加锁状态下直接执行_tasks内的任务，而使用swap函数？
        // 1. 会长时间持有锁，导致程序整体的执行效率下降；
        // 2. swap函数高效且不增加内存开销，本质上是常数时间的操作；
        // 3. 将任务队列的执行和管理解耦，保证二者之间的独立性
        std::vector<Functor> _TempFunctor;
        {
            std::lock_guard lk(_Mutex);
            _Tasks.swap(_TempFunctor);
        }
        // 在临时函数容器中执行任务队列
        for(auto &func : _TempFunctor){
            func();
        }
        return;
    }

    static int CreateEventFd(){
        int EventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if(EventFd < 0){
            ERR_LOG("CREATE EVENTFD ERROR");
            std::abort();
        }
        return EventFd;
    }

    void ReadEventFd(){
        uint64_t res = 0;
        int ret = read(_EventFd, &res, sizeof(res));
        if(ret < 0){
            if(errno == EINTR || errno == EAGAIN){
                return;
            }
            ERR_LOG("READ EVENTFD ERROR");
            std::abort();
        }
        return;
    }

    void WakeUpEventFd(){
        uint64_t val = 1;
        int ret = write(_EventFd, &val, sizeof(val));
        if(ret < 0){
            if(errno == EINTR){
                return;
            }
            ERR_LOG("WRITE VAL ERROR");
            std::abort();
        }
        return;
    }
public:
    EventLoop()
        :_ThreadID(std::this_thread::get_id()),
        _EventFd(CreateEventFd()),
        _EventChannel(new Channel(this, _EventFd)),
        _TimerWheel(this)
    {
        _EventChannel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
        _EventChannel->EnableRead();
    }

    void Start(){
        for(;;){
            std::vector<Channel*> Activities;
            _Poller.Poll(Activities);

            for(auto &channel : Activities){
                channel->HandleEvent();
            }

            RunAllTask();
        }
    }

    bool IsInLoop(){
        return (_ThreadID == std::this_thread::get_id());
    }

    void AssertInLoop(){
        assert(_ThreadID == std::this_thread::get_id());
    }

    // 判断将要执⾏的任务是否处于当前线程中，如果是则执⾏，不是则压⼊队列。
    void RunInLoop(const Functor &cb){
        if (IsInLoop()){
            return cb();
        }
        return QueueInLoop(cb);
    }

    // 将操作压⼊任务池
    void QueueInLoop(const Functor &cb){
        {
            std::unique_lock<std::mutex> _lock(_Mutex);
            _Tasks.push_back(cb);
        }
        // 唤醒有可能因为没有事件就绪，⽽导致的epoll阻塞；
        // 其实就是给eventfd写⼊⼀个数据，eventfd就会触发可读事件
        WakeUpEventFd();
    }

    // 添加/修改描述符的事件监控
    void UpdateEvent(Channel *channel) { return _Poller.UpdateChannel(channel); }

    // 移除描述符的监控
    void RemoveEvent(Channel *channel) { return _Poller.RemoveChannel(channel); }
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _TimerWheel.TimerAdd(id, delay, cb); }

    void TimerRefresh(uint64_t id){ return _TimerWheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _TimerWheel.TimerCancel(id); }
    bool HasTimer(uint64_t id) { return _TimerWheel.HasTimer(id); }
};


class LoopThread
{
private:
    std::mutex _Mutex;
    std::condition_variable _Cond;
    EventLoop* _Loop;
    std::thread _Thread;

private:
    void ThreadEntry(){
        EventLoop loop;
        {
            std::lock_guard lk(_Mutex);
            _Loop = &loop;
            _Cond.notify_one();
        }
        loop.Start();
    }

public:
    LoopThread()
        :_Loop(nullptr)
        ,_Thread(std::bind(&LoopThread::ThreadEntry, this))
    {}

    EventLoop* GetLoop(){
        EventLoop* loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(_Mutex);
            _Cond.wait(lock, [&](){
                return _Loop != nullptr;
            });
            loop = _Loop;
        }
        return loop;
    }
};


class LoopThreadPool
{
private:
    int _ThreadCount;
    int _NextIdx;
    EventLoop* _BaseLoop;
    std::vector<LoopThread*> _Threads;
    std::vector<EventLoop*> _Loops;
public:
    LoopThreadPool(EventLoop* baseloop)
        :_ThreadCount(0)
        ,_NextIdx(0)
        ,_BaseLoop(baseloop)
    {}

    void SetThreadCount(int count){
        _ThreadCount = count;
    }
    void Create(){
        if(_ThreadCount > 0){
            _Threads.resize(_ThreadCount);
            _Loops.resize(_ThreadCount);
            for(int i = 0; i < _ThreadCount; ++i){
                _Threads[i] = new LoopThread();
                _Loops[i] = _Threads[i]->GetLoop();
            }
        }
        return;
    }
    EventLoop* NextLoop(){
        if(_ThreadCount == 0){
            return _BaseLoop;
        }
        _NextIdx = (_NextIdx + 1) % _ThreadCount;
        return _Loops[_NextIdx];
    }
};



class Any
{
private:
    
    class PlaceHolder
    {
    public:
        virtual ~PlaceHolder() {}
        virtual const std::type_info& type() = 0;
        virtual PlaceHolder* clone() = 0;
    };

    template<class T>
    class Holder : public PlaceHolder
    {
    public:
        Holder(const T& data)
            :_data(data)
        {}
        ~Holder() = default;
        const std::type_info& type(){
            return typeid(T);
        }
        PlaceHolder* clone(){
            return new Holder(_data);
        }
    public:
        T _data;
        
    };

private:
    PlaceHolder* _content;

public:
    Any(): _content(nullptr){}

    template <typename T>
    Any(const T &data) : _content(new Holder<T>(data)) {}
    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {}
    ~Any()
    {
        if (_content)
            delete _content;
    }

    // 若 content 不为空，则返回 content 的类型信息，否则返回 void 类型信息
    const std::type_info &type(){
        return _content ? _content->type() : typeid(void);
    }

    // 获取 data 指针
    template<typename T>
    T* get(){
        assert(typeid(T) == _content->type());
        return &(static_cast<Holder<T>*>(_content)->_data);
    }
};


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

void Channel::Update(){
    _loop->UpdateEvent(this);
}

void Channel::Remove(){
    _loop->RemoveEvent(this);
}

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb){
    auto iter = _TimerMap.find(id);
    if(iter == _TimerMap.end()) return;
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}

void TimerWheel::TimerRefresh(uint64_t id){
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}

void TimerWheel::TimerCancel(uint64_t id){
    _Loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

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