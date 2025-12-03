cmake_minimum_required(VERSION 3.10)
project(GYKProtocolParser)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译器标志
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
    # Windows下需要链接ws2_32库用于socket
    set(SOCKET_LIBS ws2_32)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(SOCKET_LIBS)
endif()

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 添加源文件
set(COMMON_SOURCES
    src/ProtocolParser.cpp
)

# 创建TCP服务器可执行文件
add_executable(tcp_server
    src/tcp_server.cpp
    ${COMMON_SOURCES}
)

# 创建TCP客户端可执行文件
add_executable(tcp_client
    src/tcp_client.cpp
)

# 链接库
target_link_libraries(tcp_server ${SOCKET_LIBS})
target_link_libraries(tcp_client ${SOCKET_LIBS})

# 设置输出目录
set_target_properties(tcp_server tcp_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
