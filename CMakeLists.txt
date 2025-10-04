cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.16)

project(sqlcpp VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)



# Add SQLite CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add SQLite3 library
find_package(SQLite3 REQUIRED)

# Add PostgreSQL library
find_package(PostgreSQL REQUIRED)

#Add MariaDB library
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SqlcppConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/SqlcppConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlcpp
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfigVersion.cmake
        COMPATIBILITY ExactVersion
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SqlcppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/sqlcppConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlcpp
        COMPONENT sqlcpp-dev
)

