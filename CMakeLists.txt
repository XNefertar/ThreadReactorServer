cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(ThreadReactorServer VERSION 1.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR})

# 查找主项目源文件
file(GLOB_RECURSE MAIN_SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.cc")
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*TestCode/.*")

# 查找测试代码源文件
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_SOURCE_DIR}/TestCode/*.cpp" "${CMAKE_SOURCE_DIR}/TestCode/*.cc")

# 添加主项目可执行文件
add_executable(ThreadReactorServer ${MAIN_SOURCES})

# 链接 pthread 库
target_link_libraries(ThreadReactorServer pthread)

# 为每个测试代码生成单独的可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} pthread)
endforeach()