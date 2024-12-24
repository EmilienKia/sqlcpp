include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(utest
        catch.hpp
        runner.cpp
        tests-sqlite.cpp
)

target_link_libraries(utest sqlcpp sqlcpp-sqlite)

add_test(NAME utest COMMAND utest)

