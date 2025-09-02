# ODBC Driver for sqlcpp

This document describes the generic ODBC driver implementation for the sqlcpp library.

## Overview

The ODBC driver provides a generic interface to connect to any database that supports ODBC (Open Database Connectivity). This allows sqlcpp to work with a wide variety of database systems without requiring database-specific drivers.

## Features

- **Universal Database Support**: Connect to any database with ODBC drivers
- **Standard sqlcpp Interface**: Implements all base sqlcpp classes and methods
- **Cross-Platform**: Works on Windows (with Windows ODBC) and Unix systems (with unixODBC)
- **Full Data Type Support**: Maps ODBC data types to sqlcpp value types
- **Error Handling**: Comprehensive error reporting using ODBC diagnostic functions
- **Parameter Binding**: Supports both positional and named parameter binding
- **Prepared Statements**: Full support for prepared statements and result sets

## Architecture

The ODBC driver consists of four main classes:

### `sqlcpp::odbc::connection`
- Inherits from `sqlcpp::connection`
- Manages ODBC environment and connection handles
- Provides factory method for creating connections

### `sqlcpp::odbc::statement` 
- Inherits from `sqlcpp::statement`
- Manages ODBC statement handles
- Supports parameter binding and query execution

### `sqlcpp::odbc::resultset`
- Inherits from `sqlcpp::resultset`
- Provides metadata about query results
- Implements iterator pattern for row access

### `sqlcpp::odbc::row`
- Inherits from `sqlcpp::row`
- Provides access to individual column values
- Handles data type conversion from ODBC to sqlcpp types

## Data Type Mapping

| ODBC Type | sqlcpp Type | Notes |
|-----------|-------------|-------|
| SQL_CHAR, SQL_VARCHAR | STRING | Text data |
| SQL_BINARY, SQL_VARBINARY | BLOB | Binary data |
| SQL_BIT | BOOL | Boolean values |
| SQL_TINYINT, SQL_SMALLINT, SQL_INTEGER | INT | 32-bit integers |
| SQL_BIGINT | INT64 | 64-bit integers |
| SQL_REAL, SQL_FLOAT, SQL_DOUBLE | DOUBLE | Floating point |
| SQL_DECIMAL, SQL_NUMERIC | DOUBLE | Decimal numbers |

## Usage Examples

### Basic Connection
```cpp
#include "sqlcpp/odbc.hpp"

// Using a DSN
auto conn = sqlcpp::odbc::connection::create("DSN=MyDatabase;UID=user;PWD=pass");

// Using a driver connection string
auto conn = sqlcpp::odbc::connection::create(
    "DRIVER={SQLite3 ODBC Driver};Database=/path/to/db.sqlite;");
```

### Executing Queries
```cpp
// Simple execution
conn->execute("CREATE TABLE test (id INTEGER, name TEXT)");

// Prepared statement
auto stmt = conn->prepare("INSERT INTO test (id, name) VALUES (?, ?)");
stmt->bind(0, 1);
stmt->bind(1, std::string("Test"));
stmt->execute();

// Query with results
auto query = conn->prepare("SELECT id, name FROM test WHERE id = ?");
query->bind(0, 1);
auto result = query->execute();

for (auto row : *result) {
    int id = row.get_value_int(0);
    std::string name = row.get_value_string(1);
    std::cout << "ID: " << id << ", Name: " << name << std::endl;
}
```

## Connection String Formats

Different databases require different connection string formats:

### SQLite
```
DRIVER={SQLite3 ODBC Driver};Database=/path/to/database.db;
```

### SQL Server
```
DRIVER={ODBC Driver 17 for SQL Server};SERVER=server;DATABASE=db;UID=user;PWD=pass;
```

### MySQL/MariaDB
```
DRIVER={MySQL ODBC 8.0 Driver};SERVER=host;DATABASE=db;USER=user;PASSWORD=pass;
```

### PostgreSQL
```
DRIVER={PostgreSQL};SERVER=host;PORT=5432;DATABASE=db;UID=user;PWD=pass;
```

## Requirements

### Linux/Unix
- unixODBC development libraries (`unixodbc-dev` package)
- Appropriate ODBC drivers for your target database

### Windows
- Windows ODBC (included with Windows)
- Appropriate ODBC drivers for your target database

## Building

The ODBC driver is automatically built when ODBC libraries are detected:

```bash
mkdir build && cd build
cmake ..
make sqlcpp-odbc
```

## Error Handling

The driver provides detailed error information using ODBC diagnostic functions. All ODBC errors are converted to C++ exceptions with descriptive error messages including:
- SQLSTATE codes
- Native error codes  
- Detailed error descriptions

## Limitations

- **Named Parameters**: ODBC doesn't natively support named parameters. The driver provides basic support by parsing the query string, but positional parameters (`?`) are recommended.
- **Row Count**: Getting the total row count for result sets may not be available for all drivers.
- **Connection Pooling**: Connection pooling must be configured at the ODBC driver level.

## Testing

A basic test suite is included in `tests/tests-odbc.cpp`. The tests verify:
- Interface compliance with base sqlcpp classes
- Proper error handling for invalid connections
- Correct inheritance hierarchy

To run tests with an actual database, configure an ODBC DSN and update the test connection strings.

## Thread Safety

Thread safety depends on the underlying ODBC driver. Most modern ODBC drivers support multi-threading, but check your specific driver documentation.

## Performance

Performance characteristics depend on the underlying ODBC driver and database. For optimal performance:
- Use prepared statements for repeated queries
- Enable connection pooling in the ODBC driver
- Configure appropriate buffer sizes for large result sets
- Use transactions for bulk operations