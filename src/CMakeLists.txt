
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

#
# Base library
#

add_library(sqlcpp SHARED
        ../include/sqlcpp.hpp
        sqlcpp.cpp
)

#
# SQLite library
#
add_library(sqlcpp-sqlite SHARED
        ../include/sqlcpp/sqlite.hpp
        sqlite.cpp
)

target_link_libraries(sqlcpp-sqlite sqlcpp SQLite::SQLite3)

#
# PostgreSQL library
#
add_library(sqlcpp-postgresql SHARED
        ../include/sqlcpp/postgresql.hpp
        postgresql.cpp
)

target_link_libraries(sqlcpp-sqlite sqlcpp PostgreSQL::PostgreSQL)

#
# MariaDB library
#
add_library(sqlcpp-mariadb SHARED
        ../include/sqlcpp/mariadb.hpp
        mariadb.cpp
)

target_link_libraries(sqlcpp-mariadb sqlcpp ${MARIADB_LIBRARIES})
