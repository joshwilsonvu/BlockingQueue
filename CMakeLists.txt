cmake_minimum_required(VERSION 3.7)
project(Multithreading)

# Set GoogleTest directory
set(GTEST_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/googletest)
# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wextra -pedantic -pedantic-errors")

# Include GoogleTest into the build
include(ExternalProject)
find_package(Threads)
ExternalProject_Add(googletest
        GIT_REPOSITORY https://github.com/google/googletest
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${GTEST_DIRECTORY}
        )

# Include the GoogleTest headers and libraries
include_directories(${GTEST_DIRECTORY}/include)
link_directories(${GTEST_DIRECTORY}/lib)

# Make the project root directory the working directory when we run
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Build an executable for BlockingQueueTest.cpp and link to gtest
add_executable(BlockingQueueTest BlockingQueueTest.cpp BlockingQueue.h)
add_dependencies(BlockingQueueTest googletest)
target_link_libraries(BlockingQueueTest gtest ${CMAKE_THREAD_LIBS_INIT})




