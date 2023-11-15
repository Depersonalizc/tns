project(libiptcp VERSION 1.0.1 DESCRIPTION "An emulated TCP/IP stack in userspace")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(iptcp SHARED
    src/network_node.cpp 
    src/host_node.cpp 
    src/router_node.cpp 
    src/network_interface.cpp

    src/ip/routing_table.cpp 
    src/ip/datagram.cpp
    src/ip/rip_message.cpp
    src/ip/util.cpp
    src/ip/protocols.cpp

    src/tcp/sockets.cpp
    src/tcp/states.cpp
    src/tcp/tcp_stack.cpp

    src/util/lnx_parser/lnxconfig.cpp
    src/util/lnx_parser/parse_lnx.cpp
    src/util/util.cpp src/util/thread_pool.cpp src/util/periodic_thread.cpp
)

target_include_directories(iptcp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# include(CMakePrintHelpers)  # cmake_print_variables
# cmake_print_variables(CMAKE_INSTALL_LIBDIR)
# cmake_print_variables(CMAKE_INSTALL_INCLUDEDIR)
# cmake_print_variables(PROJECT_SOURCE_DIR)