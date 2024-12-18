# CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(RUDP)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)

# Find glog package
find_package(Glog REQUIRED)

# Include directories
include_directories(${GLOG_INCLUDE_DIRS})

# Add executable for server
add_executable(server server.cpp)

# Link glog and pthread to server
target_link_libraries(server ${GLOG_LIBRARIES} glog pthread)

# Add executable for client
add_executable(client client.cpp)

# Link glog and pthread to client
target_link_libraries(client ${GLOG_LIBRARIES} glog pthread)

# Add executable for client-hello
add_executable(client-hello client-hello.cpp)

# Link glog and pthread to client
target_link_libraries(client-hello ${GLOG_LIBRARIES} glog pthread)

# Add executable for server-hello
add_executable(server-hello server-hello.cpp)

# Link glog and pthread to server
target_link_libraries(server-hello ${GLOG_LIBRARIES} glog pthread)

