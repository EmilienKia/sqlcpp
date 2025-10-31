SqlCpp - C++ SQL client library
===============================

Summary
-------

SqlCpp is an open-source modern C++17 SQL client library.
It offers a simple, elegant, and straightforward API for connecting and interacting with various SQL databases (SQLite, MySQL/MariaDB, and PostgreSQL).

### Features

* 🚀 Modern C++17 - Leverages modern C++ features for type safety (using std::variant) and ease of use
* 🎯 Unified Interface - Write once, use with any supported database
* 📦 Simple Core - Easy integration into your projects
* 🔌 Multiple Database Backends - Supports SQLite, PostgreSQL, and MariaDB/MySQL
* ⚡ Prepared Statements - Efficient and secure parameterized queries
* 🔄 Multiple Result Modes - Cursor-based or buffered result sets
* 🏗️ CMake Integration - Modern CMake support with exported targets

### Basic example

```cpp
#include <sqlcpp/sqlcpp.hpp>
#include <iostream>

int main() {
    // Connection to the dabatase
    auto db = sqlcpp::connection::create("sqlite:memory:");
    
    // Simple query execution
    db->execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    
    // Prepared statement with binding then execution
    auto stmt = db->prepare("INSERT INTO users(name) VALUES(:name)");
    stmt->bind(":name", "Alice");
    stmt->execute();
    
    // Reading results
    auto query = db->prepare("SELECT * FROM users");
    query->execute([](const sqlcpp::row& row) {
        std::cout << "ID: " << sqlcpp::to_string(row[0]) 
                  << ", Name: " << sqlcpp::to_string(row[1]) 
                  << std::endl;
    });
    
    return 0;
}
```

### License

SqlCpp is provided under the GNU Lesser General Public License v3.0 (LGPL-3.0).
See the LICENSE file for details.

Usage in a project
------------------

SqlCpp comes with a core library providing basic interfaces and connection driver factory.
This factory will automatically load the required driver at runtime.

You only need to add the core library to your project and link against it.

### CMake project

SqlCpp comes with all the CMake package configuration files required to integrate it into your project.
You just need to find the SqlCpp package and link against it.

```cmake
find_package(SqlCpp REQUIRED CONFIG COMPONENTS sqlcpp)

add_executable(mon_app main.cpp)
target_link_libraries(mon_app sqlcpp::sqlcpp)
```

### Database connection

First, you need to include the SqlCpp core header file.
It provides the `sqlcpp` namespace with all the required classes and functions.

```cpp
#include <sqlcpp/sqlcpp.hpp>
```

Then, you can use the SqlCpp API to connect to a database and execute queries.
You need to use the `sqlcpp::connection::create` factory function to create a connection to a database.

This `create(url)` method accept a connection URL, will automatically load the required driver and create a connection to the database.

```cpp
auto db = sqlcpp::connection::create("sqlite:file:data.db");
```

The scheme of this URL describes the database type. 
Currently, are supported: sqlite, mysql, postgresql, and mariadb.

The rest of the URL is passed to the driver to connect to the database.
Usually, the rest of the URL depends on the database type.

For example:
* `sqlite:file:data.db` : SQLite file database, see [SQLite Connection URIs format](https://www.sqlite.org/uri.html)
* `sqlite::memory:` : SQLite memory database
* `postgresql://user:secret@localhost:5433/somedb` : PostgreSQL database, see [PostgreSQL Connection URIs format](https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING-URIS)
* `mysql://user:secret@localhost:3306/somedb` : MySQL/MariaDB database

### Simple SQL query execution

You can use the `sqlcpp::connection::execute(query)` method to execute a simple SQL query.

This query must be a well-formed SQL query suited to all the specificities of the targeted database system.

This query could be multiple statements separated by semicolons. They are executed in order.
And usually, but depending on the underlying client connector, the statements are executed in the same transaction.

With this simple interface, it is not possible to bind variables to the query nor to retrieve the results.

### Prepared statements

You can create a prepared statement using the `sqlcpp::connection::prepare(query)` method.

#### Variable binding

This method accepts a single statement SQL query as a parameter.
This query must be a well-formed SQL query and may hold variables to bind.

Once prepared, you receive a `sqlcpp::statement` object.
This object provides the `bind(name, value)` or `bind(index, value)` methods to bind a variable to the statement.
Note, as the usual practice of many database systems, indexes are 1-based. 

```cpp
auto select_stmt = db->prepare("SELECT * FROM users WHERE name = $1");
select_stmt->bind(1, "John");
```

The format of variables to bind is specific to the database system:
* SQLite : `?`, `?NNN`, `:VVV`, `@VVV`, `$VVV`,... Named and index bindings are both supported.
See [SQLite Parameter Binding](https://www.sqlite.org/c3ref/bind_blob.html)
* PostgreSQL : `$NNN`. Only index binding is supported.
See [PostgreSQL Parameter Binding](https://www.postgresql.org/docs/current/sql-prepare.html)
* MySQL/MariaDB : `?`. Only index binding is supported.
See [MySQL Parameter Binding](https://dev.mysql.com/doc/refman/9.5/en/sql-prepared-statements.html)

#### Query execution and retrieve results 

Once ready, you can execute the prepared statement.

You have three ways to execute the query, dependening on the way you would consume results.

##### Iterable cursor-based resultset

`statement::execute()` will retrieve a cursor-based resultset.

This resultset is iterable and will retrieve rows one by one throw an iterator with begin() and end() methods.

```cpp
auto select_stmt = db->prepare("SELECT * FROM users);
auto resultset = select_stmt->execute();
for (const auto& row : *resultset) {
    std::cout << "ID: " << sqlcpp::to_string(row[0]) 
              << ", Name: " << sqlcpp::to_string(row[1]) 
              << std::endl;
}
```

Rows will be retrieved one by one while the resultset if iterated.

In this mode, the row count is not available.

##### Buffered resultset

`statement::execute_buffered()` will retrieve a buffered resultset.

This execution mode will retrieve and cache all the rows in memory at execution time.
The execution time could be longer, but the access to each row is faster and could be at random index.
The row count is known.

##### Callback resultset

`statement::execute(callback)` will retrieve a resultset using a callback function. This function will be called for each row.

This execution mode is efficient, but the callback function must be able to handle all the rows.

```cpp
auto select_stmt = db->prepare("SELECT * FROM users);
stmt->execute([&](const sqlcpp::row& row) {
    // Process each row
});
```

### Availability of the drivers
The SqlCpp library provides drivers for various database systems, including SQLite, PostgreSQL, MySQL, and more.

By default, the SqlCpp library will automatically load the required driver at runtime.
On Linux, drivers are installed into system directories, (usually on /usr/lib/sqlcpp/ or /usr/local/lib/sqlcpp/).
Drivers on these directories are automatically loaded.

If drivers are not present on the system, they might be linked directly to the application.
You just have to add the CMake module to the link dependencies.

```cmake
target_link_libraries(target sqlcpp::sqlite)
```

the following modules are available:
* `sqlcpp::sqlcpp-sqlite` : for SQLite
* `sqlcpp::sqlcpp-postgresql` : for PostgreSQL 
* `sqlcpp::sqlcpp-mariadb` : for MySQL and MariaDB.

NOTE: there is a bug in the CMake configuration files that prevents the obvious linking to drivers without specifying some transient find_package() options.

As the linker might remove dependencies to explicitly unused symbols and libraries, you might need to specify to not remove the libraries at link time:

```cmake
target_link_libraries(target sqlcpp::sqlite)
target_link_options(target PRIVATE "-Wl,--no-as-needed")
```

Or you can just call driver registration functions directly:

```cpp
#include <sqlcpp/sqlite/sqlite.hpp>
...
sqlcpp::sqlite::register_connection_factory();
...
```

Building SqlCpp
---------------

### CMake project

SqlCpp is provided as a CMake project.
You need to grab the dependencies, checkout the project, create a build directory, and run CMake.

```bash
cd sqlcpp
mkdir build
cd build
cmake ..
make
```

Then, if you want to locally install it in /usr/local/, just type:
```bash
sudo make install
```

#### Dependencies

SqlCpp is a pure C++17 project, it requires a C++17 toolchain (tested with g++) and the database clients libraries to build drivers:
* [SQLite](https://www.sqlite.org/)
* [PostgreSQL](https://www.postgresql.org/)
* [MySQL/MariaDB](https://mariadb.org/)

### Debian packages

SqlCpp also comes with Debian packaging recipes.
You need to check out the project, and in the source directory, invoke the Debian package build command:

```bash
dpkg-buildpackage -us -uc
```

All the libs and devel packages will be created in the parent directory.



