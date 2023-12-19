cmake_minimum_required(VERSION 3.0.0...3.27.4)
project(iptcp-ca-qsy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wunreachable-code -Wno-missing-field-initializers)

# iptcp library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# iptcp applications
set(APPLICATION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/app)
include_directories(${APPLICATION_DIR})
add_executable(vhost ${APPLICATION_DIR}/vhost.cpp)
add_executable(vrouter ${APPLICATION_DIR}/vrouter.cpp)
# link library
target_link_libraries(vhost iptcp)
target_link_libraries(vrouter iptcp)


# tests
set(TEST_DIR ${APPLICATION_DIR}/tests)
include_directories(${TEST_DIR})
add_executable(test_main ${TEST_DIR}/catch_amalgamated.cpp
                         ${TEST_DIR}/test_address.cpp
                         ${TEST_DIR}/test_datagram.cpp
                         ${TEST_DIR}/test_util_ip.cpp
                         ${TEST_DIR}/test_buffers.cpp
)
target_link_libraries(test_main iptcp)