cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.16)

project(sqlcpp-simple-example)

set(CMAKE_CXX_STANDARD 17)

find_package(sqlcpp REQUIRED CONFIG COMPONENTS sqlcpp)

add_executable(simple-example
        simple-example.cpp
)
target_link_libraries(simple-example sqlcpp::sqlcpp)
