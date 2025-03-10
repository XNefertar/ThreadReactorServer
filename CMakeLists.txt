cmake_minimum_required(VERSION 3.10)
project(EchoServer)

# 指定 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件和头文件
set(SRC_FILES
    EchoServer.cc
)

set(HEADERS
    TCPServer.hpp
)

# 添加可执行文件
add_executable(EchoServer ${SRC_FILES} ${HEADERS})

# 指定头文件搜索路径
target_include_directories(EchoServer PRIVATE ${CMAKE_SOURCE_DIR})

# 可选：链接库（如 pthread、Boost 等）
target_link_libraries(EchoServer pthread)
