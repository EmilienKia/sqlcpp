cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.16)

project(sqlcpp VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

enable_testing()


#
# Options
#

# Driver directory : directory where to install driver lib alias symlinks. Usually sqlcpp subdir of LIBDIR
set(SQLCPP_DRIVER_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/sqlcpp" CACHE STRING "Directory for SQL driver symlinks")
# Relative path from driver dir to LIBDIR. Used to create driver symlinks. Should be relative to keep package installability.
set(SQL_DRIVER_TO_LIBDIR_REL_PATH "../")
# Driver search dirs : list of directories where to search for drivers at runtime, separated by ';'. Empty to use default values.
set(SQLCPP_DRIVER_SEARCH_DIRS "" CACHE STRING "List of directories where to search for drivers at runtime, separated by ';'. Empty to use default values.")


# Define the list of driver search directories
if(SQLCPP_DRIVER_SEARCH_DIRS)
    # Use the list explicitly provided by user
    set(SQLCPP_DRIVER_DIRS_LIST "${SQLCPP_DRIVER_SEARCH_DIRS}")
else()
    set(SQLCPP_DRIVER_DIRS_LIST "")
    # If installed in /usr ou /usr/local, add standard sqlcpp driver dirs
    if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
        list(APPEND SQLCPP_DRIVER_DIRS_LIST "/usr/local/lib/sqlcpp")
        list(APPEND SQLCPP_DRIVER_DIRS_LIST "/usr/lib/sqlcpp")
    else()
        list(APPEND SQLCPP_DRIVER_DIRS_LIST "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/sqlcpp")
    endif()
endif()
set(SQLCPP_DRIVER_DIRS_CPP "")
foreach(dir ${SQLCPP_DRIVER_DIRS_LIST})
    string(APPEND SQLCPP_DRIVER_DIRS_CPP "        \"${dir}\",\n")
endforeach()
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlcpp_config.hpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcpp_config.hpp
        @ONLY
)

message("Driver installation dir : ${CMAKE_INSTALL_PREFIX} / ${SQLCPP_DRIVER_INSTALL_DIR}")
message("Driver search dir list : ${SQLCPP_DRIVER_DIRS_LIST}")

#
# Dependencies
#

# Add SQLite CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add SQLite3 library
find_package(SQLite3 REQUIRED)

# Add PostgreSQL library
find_package(PostgreSQL REQUIRED)

# Add MariaDB library
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

#
# Subdirs
#

add_subdirectory(src)
add_subdirectory(tests)


#
# Install CMake configuration files
#

install(EXPORT sqlcppTargets
        FILE sqlcppTargets.cmake
        NAMESPACE sqlcpp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlcpp
        COMPONENT sqlcpp-dev
)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sqlcppConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlcpp
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfigVersion.cmake
        COMPATIBILITY ExactVersion
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlcpp
        COMPONENT sqlcpp-dev
)

