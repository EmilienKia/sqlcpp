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

#include "sqlcpp/sqlite.hpp"

#include <iostream>
#include <limits>

/*
 * Refs:
 * - https://sqlite.org/cintro.html
 * - https://sqlite.org/c3ref/intro.html
 * - https://sqlite.org/datatype3.html
 * - https://sqlite.org/c3ref/bind_blob.html
 * - https://sqlite.org/c3ref/bind_parameter_name.html
 * - https://sqlite.org/lang_expr.html#varparam
 *
 * Implementation notes:
 * SQLite is typeless, so we have to do our best to map types.
 * STRICT tables help a bit, but not always.
 *
 * There are no 32-bit Integer type, only INT64 named INTEGER (INT).
 * Integers are always retrieved as int64.
 * When bound, values are cast to int64
 * When requested explicitly, int are requested as int64 then cast to int32.
 *
 * There is no BOOL field type, a field cannot be bool.
 * When bound, values are bind to int (0 or 1)
 * When requested explicitly, values are cast to bool as:
 * - NULL is false
 * - INTEGER or REAL is false if 0, true otherwise
 * - TEXT is false if "false", true otherwise
 * - BLOB is false if empty, true otherwise
 */

namespace sqlcpp::sqlite
{

class statement;
class resultset;


//
// SQLite's resultset iterator
//

class resultset_row_iterator_impl : public sqlcpp::resultset_row_iterator_impl, protected row
{
protected:
    std::shared_ptr<sqlite3_stmt> _stmt;
    int _state = SQLITE_OK;

public:
    resultset_row_iterator_impl(std::shared_ptr<sqlite3_stmt> stmt, int state) :
        _stmt(stmt),
        _state(state)
        {}

    virtual ~resultset_row_iterator_impl() = default;

    sqlcpp::row& get() override;
    bool next() override;
    bool different(const sqlcpp::resultset_row_iterator_impl& other) const override;

    virtual value get_value(unsigned int index) const override;

    virtual std::string get_value_string(unsigned int index) const override;
    virtual blob get_value_blob(unsigned int index) const override;
    virtual bool get_value_bool(unsigned int index) const override;
    virtual int get_value_int(unsigned int index) const override;
    virtual int64_t get_value_int64(unsigned int index) const override;
    virtual double get_value_double(unsigned int index) const override;
};

sqlcpp::row& resultset_row_iterator_impl::get()
{
    return *this;
}

bool resultset_row_iterator_impl::next()
{
    _state = sqlite3_step(_stmt.get());
    switch(_state) {
        case SQLITE_DONE:
            return false;
        case SQLITE_ROW:
            return true;
        default:
            // TODO process errors
            return false;
    }
}

bool resultset_row_iterator_impl::different(const sqlcpp::resultset_row_iterator_impl& other) const
{
    if(auto impl = dynamic_cast<const resultset_row_iterator_impl*>(&other) ; impl!=nullptr) {
        return _stmt == impl->_stmt ? _state!=impl->_state : true ;
    } else {
        return true;
    }
}


value resultset_row_iterator_impl::get_value(unsigned int index) const
{
    switch(sqlite3_column_type(_stmt.get(), index)) {
        case SQLITE_NULL:
            return {nullptr};
        case SQLITE_INTEGER:
            return {get_value_int64(index)};
        case SQLITE_FLOAT:
            return {get_value_double(index)};
        case SQLITE_TEXT:
            return get_value_string(index);
        case SQLITE_BLOB:
            return {get_value_blob(index)};
        default:
            return value_type::UNSUPPORTED;
    }
}

std::string resultset_row_iterator_impl::get_value_string(unsigned int index) const 
{
    const char* s = reinterpret_cast<const char*>(sqlite3_column_text(_stmt.get(), index));
    return std::string(s != nullptr ? s : "");
}

blob resultset_row_iterator_impl::get_value_blob(unsigned int index) const 
{
    const void* data = sqlite3_column_blob(_stmt.get(), index);
    int size = sqlite3_column_bytes(_stmt.get(), index);
    if(data == nullptr || size == 0) {
        return {};
    }
    return blob(reinterpret_cast<const unsigned char*>(data), reinterpret_cast<const unsigned char*>(data) + size);
}

bool resultset_row_iterator_impl::get_value_bool(unsigned int index) const
{
    switch(sqlite3_column_type(_stmt.get(), index)) {
        case SQLITE_NULL:
            return false;
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
            return get_value_int64(index) != 0;
        case SQLITE_TEXT:
            return get_value_string(index) != "false";
        case SQLITE_BLOB:
            return !get_value_blob(index).empty();
        default:
            return false;
    }
}

int resultset_row_iterator_impl::get_value_int(unsigned int index) const 
{
    return sqlite3_column_int(_stmt.get(), index);
}

int64_t resultset_row_iterator_impl::get_value_int64(unsigned int index) const 
{
    return sqlite3_column_int64(_stmt.get(), index);
}

double resultset_row_iterator_impl::get_value_double(unsigned int index) const 
{
    return sqlite3_column_double(_stmt.get(), index);
}


//
// SQLite's resultset
//
class resultset : public sqlcpp::resultset
{
protected:
    std::shared_ptr<sqlite3_stmt> _stmt;
    int _state = SQLITE_OK;

public:
    resultset(std::shared_ptr<sqlite3_stmt> stmt, int state) :
        _stmt(stmt),
        _state(state)
        {}

    virtual ~resultset() = default;

    unsigned int column_count() const override;
    unsigned int row_count() const override;

    std::string column_name(unsigned int index) const override;
    unsigned int column_index(const std::string& name) const override;
    std::string column_origin_name(unsigned int index) const override;
    std::string table_origin_name(unsigned int index) const override;
    value_type column_type(unsigned int index) const override;

    bool has_row() const override;

    sqlcpp::resultset_row_iterator begin() const override;
    sqlcpp::resultset_row_iterator end() const override;
};

unsigned int resultset::column_count() const
{
    return sqlite3_column_count(_stmt.get());
}

unsigned int resultset::row_count() const
{
    // TODO
    // Is possible ??
    return 0;
}

std::string resultset::column_name(unsigned int index) const
{
    return sqlite3_column_name(_stmt.get(), index);
}

unsigned int resultset::column_index(const std::string& name) const
{
    for(int idx = 0; idx < column_count(); ++idx) {
        if(name == column_name(idx)) {
            return idx;
        }
    }
    return std::numeric_limits<unsigned int>::max();
}

std::string resultset::column_origin_name(unsigned int index) const 
{
    return sqlite3_column_origin_name(_stmt.get(), index);
}

std::string resultset::table_origin_name(unsigned int index) const
{
    return sqlite3_column_table_name(_stmt.get(), index);
}

value_type resultset::column_type(unsigned int index) const
{
    switch(sqlite3_column_type(_stmt.get(), index)) {
        case SQLITE_NULL:
            return value_type::NULL_VALUE;
        case SQLITE_INTEGER:
            return value_type::INT64;
        case SQLITE_FLOAT:
            return value_type::DOUBLE;
        case SQLITE_TEXT:
            return value_type::STRING;
        case SQLITE_BLOB:
            return value_type::BLOB;
        default:
            return value_type::UNSUPPORTED;
    }
}

bool resultset::has_row() const
{
    return _state == SQLITE_ROW;
}

sqlcpp::resultset_row_iterator resultset::begin() const
{
    return std::move(sqlcpp::resultset::create_iterator(
        std::make_unique<resultset_row_iterator_impl>(_stmt, _state)
        ));
}

sqlcpp::resultset_row_iterator resultset::end() const
{
    return std::move(sqlcpp::resultset::create_iterator(std::make_unique<resultset_row_iterator_impl>(_stmt, SQLITE_DONE)));
}



//
// SQLite's statement
//

class statement : public sqlcpp::statement
{
protected:
    std::shared_ptr<sqlite3_stmt> _stmt;

public:
    explicit statement(std::shared_ptr<sqlite3_stmt> stmt) :
        _stmt(stmt)
        {}

    explicit statement(sqlite3_stmt* stmt) :
        statement(std::shared_ptr<sqlite3_stmt>(stmt, sqlite3_finalize))
        {}

    virtual ~statement() {}

    std::unique_ptr<sqlcpp::resultset> execute() override;

    unsigned int parameter_count() const override;
    int parameter_index(const std::string& name) const override;
    std::string parameter_name(unsigned int index) const override;

    statement& bind(const std::string& name, std::nullptr_t) override;
    statement& bind(const std::string& name, const std::string& value) override;
    statement& bind(const std::string& name, const std::string_view& value) override;
    statement& bind(const std::string& name, const blob& value) override;
    statement& bind(const std::string& name, bool value) override;
    statement& bind(const std::string& name, int value) override;
    statement& bind(const std::string& name, int64_t value) override;
    statement& bind(const std::string& name, double value) override;
    statement& bind(const std::string& name, const value& value) override;

    statement& bind(unsigned int index, std::nullptr_t) override;
    statement& bind(unsigned int index, const std::string& value) override;
    statement& bind(unsigned int index, const std::string_view& value) override;
    statement& bind(unsigned int index, const blob& value) override;
    statement& bind(unsigned int index, bool value) override;
    statement& bind(unsigned int index, int value) override;
    statement& bind(unsigned int index, int64_t value) override;
    statement& bind(unsigned int index, double value) override;
    statement& bind(unsigned int index, const value& value) override;

};

std::unique_ptr<sqlcpp::resultset> statement::execute()
{
    int rc = sqlite3_step(_stmt.get());
    switch(rc) {
        case SQLITE_DONE:
        case SQLITE_ROW:
            return std::unique_ptr<sqlcpp::resultset>{new resultset(_stmt, rc)};
        default:
            // TODO process errors
            // Throw exception
            return {};
    }
}

unsigned int statement::parameter_count() const
{
    return sqlite3_bind_parameter_count(_stmt.get());
}

int statement::parameter_index(const std::string& name) const 
{
    // NOTE : in SQLite, index start at 1, not 0
    return sqlite3_bind_parameter_index(_stmt.get(), name.c_str()) - 1;
}

std::string statement::parameter_name(unsigned int index) const 
{
    // NOTE : in SQLite, index start at 1, not 0
    return sqlite3_bind_parameter_name(_stmt.get(), index + 1);
}

statement& statement::bind(const std::string& name, std::nullptr_t) 
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_null(_stmt.get(), idx+1);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, const std::string& value)
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_text(_stmt.get(), idx+1, value.c_str(), value.size(), SQLITE_TRANSIENT);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, const std::string_view& value)
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_text(_stmt.get(), idx+1, value.data(), value.size(), SQLITE_TRANSIENT);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, const blob& value) 
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_blob(_stmt.get(), idx+1, value.data(), value.size(), SQLITE_TRANSIENT);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, bool value)
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_int(_stmt.get(), idx+1, value ? 1 : 0);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, int value)
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_int(_stmt.get(), idx+1, value);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, int64_t value)  
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_int64(_stmt.get(), idx+1, value);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, double value)  
{
    int idx = parameter_index(name);
    if(idx >= 0) {
        sqlite3_bind_double(_stmt.get(), idx+1, value);
        // TODO process error, throw exception
    } else {
        // TODO process error, throw exception
    }
    return *this;
}

statement& statement::bind(const std::string& name, const value& value)  
{
    std::visit([&](auto&& arg) {
        bind(name, arg);
    }, value);
    return *this;
}

statement& statement::bind(unsigned int index, std::nullptr_t)  
{
    sqlite3_bind_null(_stmt.get(), index + 1);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string& value)  
{
    sqlite3_bind_text(_stmt.get(), index + 1, value.c_str(), value.size(), SQLITE_TRANSIENT);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string_view& value)  
{
    sqlite3_bind_text(_stmt.get(), index + 1, value.data(), value.size(), SQLITE_TRANSIENT);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const blob& value)  
{
    sqlite3_bind_blob(_stmt.get(), index + 1, value.data(), value.size(), SQLITE_TRANSIENT);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, bool value)
{
    sqlite3_bind_int(_stmt.get(), index + 1, value ? 1 : 0);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int value)
{
    sqlite3_bind_int(_stmt.get(), index + 1, value);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int64_t value)  
{
    sqlite3_bind_int64(_stmt.get(), index + 1, value);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, double value)  
{
    sqlite3_bind_double(_stmt.get(), index + 1, value);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const value& value)  
{
    std::visit([&](auto&& arg) {
        bind(index, arg);
    }, value);
    return *this;
}

//
// SQLite's connection
//

connection::connection(sqlite3* db):
_db(db)
{
}

connection::~connection()
{
    if(_db) {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

std::unique_ptr<connection> connection::create(const std::string& connection_string)
{
    sqlite3 *db;
    int rc = sqlite3_open(connection_string.c_str(), &db);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        // TODO throw exception
        return {};
    }
    return std::make_unique<connection>(db);
}

void connection::execute(const std::string& query)
{
    char* err_msg = nullptr;
    int rc = sqlite3_exec(_db, query.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to execute statement (" << rc << "): " << err_msg << std::endl;
        sqlite3_free(err_msg);
        // TODO throw exception
    }
}

std::unique_ptr<sqlcpp::statement> connection::prepare(const std::string& query)
{
    int rc;
    sqlite3_stmt* res;
    rc = rc = sqlite3_prepare_v2(_db, query.c_str(), query.size(), &res, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to execute statement (" << rc << "): " << sqlite3_errmsg(_db) << std::endl;
        // TODO throw exception
        return {};
    }
    return std::make_unique<statement>(res);
}



} // namespace sqlcpp::sqlite
