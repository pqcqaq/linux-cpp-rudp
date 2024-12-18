# CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(RUDP)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)

# Find glog package
find_package(Glog REQUIRED)

# Include directories
include_directories(${GLOG_INCLUDE_DIRS})

# Glob all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

# Add executable and link libraries
add_executable(rudp ${SOURCES})
target_link_libraries(rudp ${GLOG_LIBRARIES} glog pthread)

