
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

#
# Macro definition
#

# Create a driver symlink for the installed shared library
macro(install_sqlcpp_symlink target_lib component_name)
    foreach (symlink_name ${ARGN})
        add_custom_command(TARGET ${target_lib}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${SQL_DRIVER_TO_LIBDIR_REL_PATH}$<TARGET_FILE_NAME:${target_lib}>" ${SQLCPP_DRIVER_BUILD_DIR}/${symlink_name}
        )
        install(FILES ${SQLCPP_DRIVER_BUILD_DIR}/${symlink_name}
                DESTINATION ${SQLCPP_DRIVER_INSTALL_DIR}
                COMPONENT ${component_name}
        )
    endforeach()
endmacro()

#
# Driver subdirectory
#
set(SQLCPP_DRIVER_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/sqlcpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sqlcpp")

#
# Base library
#

add_library(sqlcpp SHARED
        ../include/sqlcpp/sqlcpp.hpp
        sqlcpp.cpp
)
target_include_directories(sqlcpp PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(sqlcpp PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(sqlcpp PROPERTIES
        PUBLIC_HEADER ../include/sqlcpp/sqlcpp.hpp
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION 0
)
install(TARGETS sqlcpp
        EXPORT sqlcppTargets
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT sqlcpp
            NAMELINK_COMPONENT sqlcpp-dev
        PUBLIC_HEADER
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlcpp"
            COMPONENT sqlcpp-dev
)

#
# SQLite library
#
add_library(sqlcpp-sqlite SHARED
        ../include/sqlcpp/sqlite.hpp
        sqlite.cpp
)
target_link_libraries(sqlcpp-sqlite sqlcpp SQLite::SQLite3)
set_target_properties(sqlcpp-sqlite PROPERTIES
        PUBLIC_HEADER ../include/sqlcpp/sqlite.hpp
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION 0
)
install(TARGETS sqlcpp-sqlite
        EXPORT sqlcppTargets
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT sqlite
            NAMELINK_COMPONENT sqlite-dev
        PUBLIC_HEADER
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlcpp"
            COMPONENT sqlite-dev
)
install_sqlcpp_symlink(sqlcpp-sqlite sqlite sqlite.so)

#
# PostgreSQL library
#
add_library(sqlcpp-postgresql SHARED
        ../include/sqlcpp/postgresql.hpp
        postgresql.cpp
)
target_link_libraries(sqlcpp-postgresql sqlcpp PostgreSQL::PostgreSQL)
set_target_properties(sqlcpp-postgresql PROPERTIES
        PUBLIC_HEADER ../include/sqlcpp/postgresql.hpp
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION 0
)
install(TARGETS sqlcpp-postgresql
        EXPORT sqlcppTargets
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT postgresql
            NAMELINK_COMPONENT postgresql-dev
        PUBLIC_HEADER
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlcpp"
            COMPONENT postgresql-dev
)
install_sqlcpp_symlink(sqlcpp-postgresql postgresql pg.so pgsql.so postgre.so postgres.so postgresql.so)

#
# MariaDB library
#
add_library(sqlcpp-mariadb SHARED
        ../include/sqlcpp/mariadb.hpp
        mariadb.cpp
)
target_link_libraries(sqlcpp-mariadb sqlcpp ${MARIADB_LIBRARIES})
set_target_properties(sqlcpp-mariadb PROPERTIES
        PUBLIC_HEADER ../include/sqlcpp/mariadb.hpp
        VERSION ${CMAKE_PROJECT_VERSION}
        SOVERSION 0
)
install(TARGETS sqlcpp-mariadb
        EXPORT sqlcppTargets
        LIBRARY
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT mariadb
            NAMELINK_COMPONENT mariadb-dev
        PUBLIC_HEADER
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlcpp"
            COMPONENT mariadb-dev
)
install_sqlcpp_symlink(sqlcpp-mariadb mariadb my.so mysql.so maria.so mariadb.so)
