cmake_minimum_required(VERSION 3.10)
project(lf-queue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

add_executable(
    lockfree
    "lockfree.cpp"
)

add_executable(
    locked
    "locked.cpp"
)
