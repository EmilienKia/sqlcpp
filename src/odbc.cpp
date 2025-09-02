/*
 * Copyright (C) 2024-2025 Emilien Kia <emilien.kia+dev@gmail.com>
 *
 * sqlcpp is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * sqlcpp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.";
 */

#include "sqlcpp/odbc.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// Prevent macro conflicts
#ifdef BOOL
#undef BOOL
#endif

/*
 * ODBC Implementation Notes:
 * - ODBC uses 1-based indexing for parameters and columns
 * - We map this to 0-based indexing for consistency with the sqlcpp interface
 * - Error handling should check SQL return codes and throw appropriate exceptions
 * - ODBC requires proper sequence of operations for optimal performance
 */

namespace sqlcpp::odbc
{

//
// ODBC Helper functions
//

static void check_odbc_error(SQLRETURN ret, SQLSMALLINT handle_type, SQLHANDLE handle, const std::string& operation)
{
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        SQLCHAR state[7];
        SQLCHAR message[256];
        SQLINTEGER native_error;
        SQLSMALLINT message_len;
        
        SQLGetDiagRec(handle_type, handle, 1, state, &native_error, message, sizeof(message), &message_len);
        
        std::string error_msg = operation + " failed: " + std::string(reinterpret_cast<char*>(state)) + 
                               " - " + std::string(reinterpret_cast<char*>(message));
        throw std::runtime_error(error_msg);
    }
}

//
// ODBC Connection
//

connection::connection(SQLHENV env, SQLHDBC dbc) : _env(env), _dbc(dbc)
{
}

connection::~connection()
{
    if (_dbc != SQL_NULL_HDBC) {
        SQLDisconnect(_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, _dbc);
    }
    if (_env != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, _env);
    }
}

std::shared_ptr<connection> connection::create(const std::string& connection_string)
{
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLRETURN ret;
    
    try {
        // Allocate environment handle
        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
        check_odbc_error(ret, SQL_HANDLE_ENV, env, "SQLAllocHandle(ENV)");
        
        // Set ODBC version
        ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        check_odbc_error(ret, SQL_HANDLE_ENV, env, "SQLSetEnvAttr(ODBC_VERSION)");
        
        // Allocate connection handle
        ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
        check_odbc_error(ret, SQL_HANDLE_ENV, env, "SQLAllocHandle(DBC)");
        
        // Connect to database
        SQLCHAR out_conn_str[1024];
        SQLSMALLINT out_conn_str_len;
        ret = SQLDriverConnect(dbc, NULL, 
                              reinterpret_cast<SQLCHAR*>(const_cast<char*>(connection_string.c_str())),
                              SQL_NTS, out_conn_str, sizeof(out_conn_str), &out_conn_str_len, SQL_DRIVER_NOPROMPT);
        check_odbc_error(ret, SQL_HANDLE_DBC, dbc, "SQLDriverConnect");
        
        return std::shared_ptr<connection>(new connection(env, dbc));
    }
    catch (...) {
        if (dbc != SQL_NULL_HDBC) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        }
        if (env != SQL_NULL_HENV) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
        throw;
    }
}

void connection::execute(const std::string& query)
{
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret;
    
    try {
        ret = SQLAllocHandle(SQL_HANDLE_STMT, _dbc, &stmt);
        check_odbc_error(ret, SQL_HANDLE_DBC, _dbc, "SQLAllocHandle(STMT)");
        
        ret = SQLExecDirect(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), SQL_NTS);
        check_odbc_error(ret, SQL_HANDLE_STMT, stmt, "SQLExecDirect");
        
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }
    catch (...) {
        if (stmt != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        }
        throw;
    }
}

std::shared_ptr<sqlcpp::statement> connection::prepare(const std::string& query)
{
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret;
    
    try {
        ret = SQLAllocHandle(SQL_HANDLE_STMT, _dbc, &stmt);
        check_odbc_error(ret, SQL_HANDLE_DBC, _dbc, "SQLAllocHandle(STMT)");
        
        ret = SQLPrepare(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), SQL_NTS);
        check_odbc_error(ret, SQL_HANDLE_STMT, stmt, "SQLPrepare");
        
        return std::shared_ptr<sqlcpp::statement>(new statement(shared_from_this(), stmt, query));
    }
    catch (...) {
        if (stmt != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        }
        throw;
    }
}

//
// ODBC Statement
//

statement::statement(std::shared_ptr<connection> conn, SQLHSTMT stmt, const std::string& query) :
    _connection(conn), _stmt(stmt), _query(query)
{
}

statement::~statement()
{
    if (_stmt != SQL_NULL_HSTMT) {
        SQLFreeHandle(SQL_HANDLE_STMT, _stmt);
    }
}

std::shared_ptr<sqlcpp::resultset> statement::execute()
{
    SQLRETURN ret = SQLExecute(_stmt);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLExecute");
    
    return std::shared_ptr<sqlcpp::resultset>(new resultset(_stmt));
}

unsigned int statement::parameter_count() const
{
    SQLSMALLINT count = 0;
    SQLRETURN ret = SQLNumParams(_stmt, &count);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLNumParams");
    return static_cast<unsigned int>(count);
}

int statement::parameter_index(const std::string& name) const
{
    // ODBC doesn't have built-in named parameter support
    // This is a simple implementation that assumes :name syntax
    size_t pos = _query.find(":" + name);
    if (pos == std::string::npos) {
        return -1;
    }
    
    // Count parameters before this position
    int index = 0;
    for (size_t i = 0; i < pos; ++i) {
        if (_query[i] == '?') {
            index++;
        }
    }
    return index;
}

std::string statement::parameter_name(unsigned int index) const
{
    // ODBC doesn't have built-in named parameter support
    // Return empty string for positional parameters
    return "";
}

sqlcpp::statement& statement::bind(const std::string& name, std::nullptr_t)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), nullptr);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, const std::string& value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, const std::string_view& value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, const blob& value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, bool value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, int value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, int64_t value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, double value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(const std::string& name, const value& value)
{
    int idx = parameter_index(name);
    if (idx >= 0) {
        return bind(static_cast<unsigned int>(idx), value);
    }
    throw std::runtime_error("Parameter not found: " + name);
}

sqlcpp::statement& statement::bind(unsigned int index, std::nullptr_t)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_VARCHAR, 0, 0, nullptr, 0, nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(NULL)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, const std::string& value)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                                    value.length(), 0, const_cast<char*>(value.c_str()), value.length(), nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(string)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, const std::string_view& value)
{
    return bind(index, std::string(value));
}

sqlcpp::statement& statement::bind(unsigned int index, const blob& value)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY,
                                    value.size(), 0, const_cast<unsigned char*>(value.data()), value.size(), nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(blob)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, bool value)
{
    return bind(index, static_cast<int>(value ? 1 : 0));
}

sqlcpp::statement& statement::bind(unsigned int index, int value)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                                    0, 0, &value, sizeof(value), nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(int)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, int64_t value)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                                    0, 0, &value, sizeof(value), nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(int64)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, double value)
{
    SQLRETURN ret = SQLBindParameter(_stmt, index + 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                                    0, 0, &value, sizeof(value), nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLBindParameter(double)");
    return *this;
}

sqlcpp::statement& statement::bind(unsigned int index, const value& value)
{
    std::visit([this, index](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // Unsupported/unset value
            throw std::runtime_error("Cannot bind unset value");
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            bind(index, nullptr);
        } else {
            bind(index, v);
        }
    }, value);
    return *this;
}

//
// ODBC Row
//

row::row(SQLHSTMT stmt, unsigned int column_count) : _stmt(stmt), _column_count(column_count)
{
}

value row::get_value(unsigned int index) const
{
    if (index >= _column_count) {
        throw std::out_of_range("Column index out of range");
    }
    
    SQLLEN data_type;
    SQLRETURN ret = SQLColAttribute(_stmt, index + 1, SQL_DESC_TYPE, nullptr, 0, nullptr, &data_type);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLColAttribute(TYPE)");
    
    switch (data_type) {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            return get_value_string(index);
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
            return get_value_blob(index);
        case SQL_BIT:
            return get_value_bool(index);
        case SQL_TINYINT:
        case SQL_SMALLINT:
        case SQL_INTEGER:
            return get_value_int(index);
        case SQL_BIGINT:
            return get_value_int64(index);
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            return get_value_double(index);
        default:
            return get_value_string(index); // Fallback to string
    }
}

std::string row::get_value_string(unsigned int index) const
{
    char buffer[4096];
    SQLLEN indicator;
    
    SQLRETURN ret = SQLGetData(_stmt, index + 1, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
    if (ret == SQL_NULL_DATA || indicator == SQL_NULL_DATA) {
        return "";
    }
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLGetData(string)");
    
    return std::string(buffer);
}

blob row::get_value_blob(unsigned int index) const
{
    SQLLEN indicator;
    
    // First, get the length
    SQLRETURN ret = SQLGetData(_stmt, index + 1, SQL_C_BINARY, nullptr, 0, &indicator);
    if (ret == SQL_NULL_DATA || indicator == SQL_NULL_DATA) {
        return {};
    }
    
    if (indicator <= 0) {
        return {};
    }
    
    blob result(indicator);
    ret = SQLGetData(_stmt, index + 1, SQL_C_BINARY, result.data(), result.size(), &indicator);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLGetData(blob)");
    
    return result;
}

bool row::get_value_bool(unsigned int index) const
{
    return get_value_int(index) != 0;
}

int row::get_value_int(unsigned int index) const
{
    int value;
    SQLLEN indicator;
    
    SQLRETURN ret = SQLGetData(_stmt, index + 1, SQL_C_LONG, &value, sizeof(value), &indicator);
    if (ret == SQL_NULL_DATA || indicator == SQL_NULL_DATA) {
        return 0;
    }
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLGetData(int)");
    
    return value;
}

int64_t row::get_value_int64(unsigned int index) const
{
    int64_t value;
    SQLLEN indicator;
    
    SQLRETURN ret = SQLGetData(_stmt, index + 1, SQL_C_SBIGINT, &value, sizeof(value), &indicator);
    if (ret == SQL_NULL_DATA || indicator == SQL_NULL_DATA) {
        return 0;
    }
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLGetData(int64)");
    
    return value;
}

double row::get_value_double(unsigned int index) const
{
    double value;
    SQLLEN indicator;
    
    SQLRETURN ret = SQLGetData(_stmt, index + 1, SQL_C_DOUBLE, &value, sizeof(value), &indicator);
    if (ret == SQL_NULL_DATA || indicator == SQL_NULL_DATA) {
        return 0.0;
    }
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLGetData(double)");
    
    return value;
}

//
// ODBC Resultset Row Iterator Implementation
//

resultset_row_iterator_impl::resultset_row_iterator_impl(SQLHSTMT stmt, unsigned int column_count, bool has_data) :
    _stmt(stmt), _column_count(column_count), _has_data(has_data)
{
    if (_has_data) {
        _current_row = std::make_unique<row>(_stmt, _column_count);
    }
}

sqlcpp::row& resultset_row_iterator_impl::get()
{
    if (!_current_row) {
        throw std::runtime_error("No current row available");
    }
    return *_current_row;
}

bool resultset_row_iterator_impl::next()
{
    SQLRETURN ret = SQLFetch(_stmt);
    if (ret == SQL_NO_DATA) {
        _has_data = false;
        _current_row.reset();
        return false;
    }
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLFetch");
    
    if (!_current_row) {
        _current_row = std::make_unique<row>(_stmt, _column_count);
    }
    return true;
}

bool resultset_row_iterator_impl::different(const sqlcpp::resultset_row_iterator_impl& other) const
{
    if (auto impl = dynamic_cast<const resultset_row_iterator_impl*>(&other); impl != nullptr) {
        return _stmt == impl->_stmt ? _has_data != impl->_has_data : true;
    }
    return true;
}

//
// ODBC Resultset
//

resultset::resultset(SQLHSTMT stmt) : _stmt(stmt), _column_count(0), _row_count(0)
{
    SQLSMALLINT columns;
    SQLRETURN ret = SQLNumResultCols(_stmt, &columns);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLNumResultCols");
    _column_count = static_cast<unsigned int>(columns);
    
    // Check if there's data available
    ret = SQLFetch(_stmt);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        _has_data = true;
        // Reset cursor to beginning for iterator usage
        SQLFreeStmt(_stmt, SQL_CLOSE);
        SQLExecute(_stmt);
    } else if (ret == SQL_NO_DATA) {
        _has_data = false;
    } else {
        check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLFetch");
    }
    
    // Row count is not easily available in ODBC without fetching all rows
    _row_count = 0; // TODO: Implement if needed
}

unsigned int resultset::column_count() const
{
    return _column_count;
}

unsigned int resultset::row_count() const
{
    return _row_count;
}

std::string resultset::column_name(unsigned int index) const
{
    if (index >= _column_count) {
        throw std::out_of_range("Column index out of range");
    }
    
    char column_name[256];
    SQLSMALLINT name_len;
    SQLRETURN ret = SQLColAttribute(_stmt, index + 1, SQL_DESC_NAME, column_name, sizeof(column_name), &name_len, nullptr);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLColAttribute(NAME)");
    
    return std::string(column_name, name_len);
}

unsigned int resultset::column_index(const std::string& name) const
{
    for (unsigned int i = 0; i < _column_count; ++i) {
        if (column_name(i) == name) {
            return i;
        }
    }
    throw std::runtime_error("Column not found: " + name);
}

std::string resultset::column_origin_name(unsigned int index) const
{
    if (index >= _column_count) {
        throw std::out_of_range("Column index out of range");
    }
    
    char origin_name[256];
    SQLSMALLINT name_len;
    SQLRETURN ret = SQLColAttribute(_stmt, index + 1, SQL_DESC_BASE_COLUMN_NAME, origin_name, sizeof(origin_name), &name_len, nullptr);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        return std::string(origin_name, name_len);
    }
    return column_name(index); // Fallback to column name
}

std::string resultset::table_origin_name(unsigned int index) const
{
    if (index >= _column_count) {
        throw std::out_of_range("Column index out of range");
    }
    
    char table_name[256];
    SQLSMALLINT name_len;
    SQLRETURN ret = SQLColAttribute(_stmt, index + 1, SQL_DESC_BASE_TABLE_NAME, table_name, sizeof(table_name), &name_len, nullptr);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        return std::string(table_name, name_len);
    }
    return ""; // Not available
}

value_type resultset::column_type(unsigned int index) const
{
    if (index >= _column_count) {
        throw std::out_of_range("Column index out of range");
    }
    
    SQLLEN data_type;
    SQLRETURN ret = SQLColAttribute(_stmt, index + 1, SQL_DESC_TYPE, nullptr, 0, nullptr, &data_type);
    check_odbc_error(ret, SQL_HANDLE_STMT, _stmt, "SQLColAttribute(TYPE)");
    
    switch (data_type) {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            return sqlcpp::value_type::STRING;
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
            return sqlcpp::value_type::BLOB;
        case SQL_BIT:
            return sqlcpp::value_type::BOOL;
        case SQL_TINYINT:
        case SQL_SMALLINT:
        case SQL_INTEGER:
            return sqlcpp::value_type::INT;
        case SQL_BIGINT:
            return sqlcpp::value_type::INT64;
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            return sqlcpp::value_type::DOUBLE;
        default:
            return sqlcpp::value_type::UNSUPPORTED;
    }
}

bool resultset::has_row() const
{
    return _has_data;
}

resultset::iterator resultset::begin() const
{
    return create_iterator(std::make_shared<resultset_row_iterator_impl>(_stmt, _column_count, _has_data));
}

resultset::iterator resultset::end() const
{
    return create_iterator(std::make_shared<resultset_row_iterator_impl>(_stmt, _column_count, false));
}

} // namespace sqlcpp::odbc