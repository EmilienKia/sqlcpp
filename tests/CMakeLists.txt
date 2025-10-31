include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(unit-tests
        catch.hpp
        runner.cpp
        tests-sqlite.cpp
        tests-postgresql.cpp
        tests-mariadb.cpp
)
target_link_libraries(unit-tests sqlcpp sqlcpp-sqlite sqlcpp-postgresql sqlcpp-mariadb)
add_test(NAME unit-tests COMMAND unit-tests)


add_executable(factory-local-tests
        catch.hpp
        runner.cpp
        factory_local.cpp
)
target_link_libraries(factory-local-tests sqlcpp sqlcpp-sqlite)
target_link_options(factory-local-tests PRIVATE "-Wl,--no-as-needed")
add_test(NAME factory-local-tests COMMAND factory-local-tests)



