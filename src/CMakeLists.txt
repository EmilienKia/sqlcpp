
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_library(sqlcpp SHARED
        ../include/sqlcpp.hpp
        sqlcpp.cpp
)

add_library(sqlcpp-sqlite SHARED
        ../include/sqlcpp/sqlite.hpp
        sqlite.cpp
)

target_link_libraries(sqlcpp-sqlite sqlcpp SQLite::SQLite3)

