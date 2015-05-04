cmake_minimum_required(VERSION 2.8.11)
project(Executor)
execute_process(
	COMMAND git submodule init
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
execute_process(
	COMMAND git submodule update
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set(CMAKE_CXX_FLAGS "-std=c++14 -march=native -mtune=native -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG -flto")

find_package(GTest)
include_directories(
  ${GTEST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src)

add_executable(executor_test test/TestExecutor.cpp)
target_link_libraries(executor_test gtest gtest_main)
add_test(executor_test executor_test)
enable_testing()
