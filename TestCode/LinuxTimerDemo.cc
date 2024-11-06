#include <iostream>
#include <cstdio>
#include <string>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/select.h>

int main()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd == -1)
    {
        std::cerr << "timerfd_create error" << std::endl;
        return -1;
    }

    // 设置定时器
    struct itimerspec new_value;
    // 设置定时器首次超时时间和之后的超时时间
    new_value.it_value.tv_sec = 3;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;

    // 启动定时器
    if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1)
    {
        std::cerr << "timerfd_settime error" << std::endl;
        return -1;
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(timerfd, &readfds);

    time_t t = time(NULL);
    while (true)
    {
        int ret = select(timerfd + 1, &readfds, NULL, NULL, NULL);
        if (ret == -1)
        {
            std::cerr << "select error" << std::endl;
            return -1;
        }
        else if (ret == 0)
        {
            std::cerr << "select timeout" << std::endl;
            return -1;
        }

        if (FD_ISSET(timerfd, &readfds))
        {
            uint64_t exp;
            ssize_t s = read(timerfd, &exp, sizeof(uint64_t));
            if (s != sizeof(uint64_t))
            {
                std::cerr << "read error" << std::endl;
                return -1;
            }

            std::string str = ctime(&t);
            str[str.size() - 1] = '\0';
            std::cout << str << " timerfd timeout" << std::endl;
            std::cout << "time internal : " << time(NULL) - t << std::endl;
            std::cout << "exp = " << exp << std::endl;
        }
        // timerfd_settime(timerfd, 0, &new_value, NULL);
    }

    close(timerfd);
    return 0;
}