cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.16)

project(sqlcpp VERSION 1.0.0)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Add SQLite CMake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add SQLite3 library
find_package(SQLite3 REQUIRED)

# Add PostgreSQL library
find_package(PostgreSQL REQUIRED)

# Add MariaDB library (optional)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB libmariadb)

# Add ODBC library
find_library(ODBC_LIBRARY NAMES odbc odbcinst REQUIRED)
find_path(ODBC_INCLUDE_DIR NAMES sql.h REQUIRED)



add_subdirectory(src)
add_subdirectory(tests)


 