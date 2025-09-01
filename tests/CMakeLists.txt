include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(utest
        catch.hpp
        runner.cpp
        tests-sqlite.cpp
        tests-postgresql.cpp
        tests-mariadb.cpp
)

target_link_libraries(utest sqlcpp sqlcpp-sqlite sqlcpp-postgresql sqlcpp-mariadb)

add_test(NAME utest COMMAND utest)

