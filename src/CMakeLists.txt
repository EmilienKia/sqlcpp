
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

target_link_libraries(sqlcpp-postgresql sqlcpp PostgreSQL::PostgreSQL)

#
# MariaDB library (conditional)
#
if(MARIADB_FOUND)
    add_library(sqlcpp-mariadb SHARED
            ../include/sqlcpp/mariadb.hpp
            mariadb.cpp
    )
    
    target_link_libraries(sqlcpp-mariadb sqlcpp ${MARIADB_LIBRARIES})
endif()

#
# ODBC library
#
add_library(sqlcpp-odbc SHARED
        ../include/sqlcpp/odbc.hpp
        odbc.cpp
)

# Link ODBC libraries depending on detection method
if(UNIX AND ODBC_FOUND)
    # Use pkg-config variables for unixODBC
    target_link_libraries(sqlcpp-odbc sqlcpp ${ODBC_LIBRARIES})
    target_include_directories(sqlcpp-odbc PRIVATE ${ODBC_INCLUDE_DIRS})
    target_compile_options(sqlcpp-odbc PRIVATE ${ODBC_CFLAGS_OTHER})
else()
    # Use manual detection variables
    target_link_libraries(sqlcpp-odbc sqlcpp ${ODBC_LIBRARY})
    target_include_directories(sqlcpp-odbc PRIVATE ${ODBC_INCLUDE_DIR})
endif()
