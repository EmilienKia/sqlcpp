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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sqlcpp/mariadb.hpp"
#include <regex>
#include <sstream>
#include <cstring>
#include <limits>
#include <utility>
#include <iostream>

/*
 * Refs:
 * - https://mariadb.com/kb/en/about-mariadb-connector-c/
 * - https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions
 * - https://mariadb.com/docs/connectors/mariadb-connector-c/mariadb-connectorc-types-and-definitions
 * - https://mariadb.com/docs/server/reference/error-codes/mariadb-error-code-reference
 * - https://dev.mysql.com/doc/c-api/9.4/en/c-api-data-structures.html
 *
 * Implementation notes:
 */

namespace sqlcpp::mariadb {

namespace {

static MYSQL_BIND bind0 = {.buffer_length = 42};

} // namespace sqlcpp::mariadb




//
// Resultset iterator
//

class resultset;

class resultset_row_iterator_impl : public sqlcpp::resultset_row_iterator_impl, protected row
{
protected:
    std::shared_ptr<resultset> _resultset;

    std::vector<value> _current_row;

    void fetch_next_row();

public:
    resultset_row_iterator_impl(std::shared_ptr<resultset> resultset);

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

resultset_row_iterator_impl::resultset_row_iterator_impl(std::shared_ptr<resultset> resultset) :
_resultset(std::move(resultset))
{
    fetch_next_row();
}

sqlcpp::row& resultset_row_iterator_impl::get()
{
    return *this;
}

bool resultset_row_iterator_impl::next()
{
    fetch_next_row();
    return _current_row.empty();
}

bool resultset_row_iterator_impl::different(const sqlcpp::resultset_row_iterator_impl& other) const
{
    if(auto impl = dynamic_cast<const resultset_row_iterator_impl*>(&other) ; impl!=nullptr) {
        return _resultset != impl->_resultset;
    } else {
        return true;
    }
}


value resultset_row_iterator_impl::get_value(unsigned int index) const
{
    return _current_row[index];
}

static inline std::string to_hex_string(const blob& data) {
    static const char hex_digits[] = "0123456789abcdef";
    std::string hex_str;
    hex_str.reserve(data.size() * 2);
    for (unsigned char byte : data) {
        hex_str.push_back(hex_digits[byte >> 4]);
        hex_str.push_back(hex_digits[byte & 0x0F]);
    }
    return hex_str;
}




std::string resultset_row_iterator_impl::get_value_string(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return "";
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return "";
        } else if constexpr(std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr(std::is_same_v<T, blob>) {
            return std::string (arg.begin(), arg.end());
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? "TRUE" : "FALSE";
        } else {
            return std::to_string(arg);
        }
    }, _current_row[index]);
}

blob resultset_row_iterator_impl::get_value_blob(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> blob {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::string>) {
            return blob(arg.begin(), arg.end());
        } else if constexpr(std::is_same_v<T, blob>) {
            return arg;
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? blob{1} : blob{0};
        } else {
            // TODO add value to blob conversion
            return {};
        }
    }, _current_row[index]);
}

bool resultset_row_iterator_impl::get_value_bool(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return false;
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return false;
        } else if constexpr(std::is_same_v<T, std::string>) {
            return arg == "TRUE" || arg == "true" || arg == "ON" || arg == "on" || arg == "1";
        } else if constexpr(std::is_same_v<T, blob>) {
            return !arg.empty();
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg;
        } else {
            return arg != 0;
        }
    }, _current_row[index]);
}

int resultset_row_iterator_impl::get_value_int(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return 0;
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return 0;
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stoi(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return {};
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1 : 0;
        } else {
            return (int) arg;
        }
    }, _current_row[index]);
}

int64_t resultset_row_iterator_impl::get_value_int64(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> int64_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return 0;
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return 0;
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stoll(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return {};
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1 : 0;
        } else {
            return (long long) arg;
        }
    }, _current_row[index]);
}

double resultset_row_iterator_impl::get_value_double(unsigned int index) const
{
    return std::visit([&](auto&& arg) -> double {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return 0.0;
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return 0.0;
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stod(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return 0.0;
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1.0 : 0.0;
        } else {
            return (double ) arg;
        }
    }, _current_row[index]);
}



//
// ResultSet
//

class resultset : public sqlcpp::resultset, public std::enable_shared_from_this<resultset>
{
private:
    std::shared_ptr<MYSQL_STMT> _stmt;

public:
    resultset(std::shared_ptr<MYSQL_STMT> stmt) : _stmt(stmt) {fetch_metadata();}
    ~resultset();

    unsigned int column_count() const override;
    std::string column_name(unsigned int index) const override;
    unsigned int column_index(const std::string& name) const override;
    std::string column_origin_name(unsigned int index) const override;
    std::string table_origin_name(unsigned int index) const override;
    value_type column_type(unsigned int index) const override;
    unsigned int row_count() const override;

    bool has_row() const override;

    sqlcpp::resultset_row_iterator begin() const override;
    sqlcpp::resultset_row_iterator end() const override;

protected:
    friend class resultset_row_iterator_impl;

    std::vector<value> fetch();

private:
    void fetch_metadata();

    std::vector<std::string> _column_names;
    std::vector<std::string> _column_origin_names;
    std::vector<std::string> _table_origin_names;

    std::vector<unsigned long> _lengths;
    std::vector<my_bool> _is_nulls;
    std::vector<enum_field_types> _types;
    std::vector<unsigned int> _flags;
    std::vector<blob> _buffers;

    std::vector<MYSQL_BIND> _binds;
};

resultset::~resultset()
{
    for(auto& bind : _binds) {
/*
        if(bind.buffer!=nullptr) {
            delete[] static_cast<char *>(bind.buffer);
        }
*/
        bind = bind0;
    }
}

void resultset::fetch_metadata()
{
    if(mysql_stmt_store_result(_stmt.get())!=0) {
        int err = mysql_stmt_errno(_stmt.get());
        const char* errstr = mysql_stmt_error(_stmt.get());
        std::cout << "Error store results: " << err << " - " << errstr << std::endl;
    }

    // Retrieve metadata for result columns
    MYSQL_RES* metadata = mysql_stmt_result_metadata(_stmt.get());
    if(metadata==nullptr) {
        if(mysql_stmt_field_count(_stmt.get()) == 0) {;
            // Query does not return data (it was not a SELECT)
            return;
        }
        int err = mysql_stmt_errno(_stmt.get());
        const char* errstr = mysql_stmt_error(_stmt.get());

        std::cout << "Error retrieving metadata: " << err << " - " << errstr << std::endl;
    }

    unsigned int column_count= mysql_num_fields(metadata);
    MYSQL_FIELD *fields;
    fields = mysql_fetch_fields(metadata);
    for(size_t f=0; f<column_count; f++) {
        MYSQL_FIELD& field = fields[f];
        _column_names.emplace_back(field.name, field.name_length);
        _column_origin_names.emplace_back(field.org_name, field.org_name_length);
        _table_origin_names.emplace_back(field.org_table, field.org_table_length);

        switch(field.type) {
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_YEAR:
                // TODO test for unsigned : UNSIGNED_FLAG
                _types.push_back(MYSQL_TYPE_LONG);
                break;
            case MYSQL_TYPE_LONGLONG:
                _types.push_back(MYSQL_TYPE_LONGLONG);
                break;
            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
                _types.push_back(MYSQL_TYPE_DOUBLE);
                break;
            case MYSQL_TYPE_STRING:
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_VARCHAR:
            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
                // NOTE: BLOB_FLAG is set for BLOB and TEXT
                if(field.flags & BINARY_FLAG) {
                    _types.push_back(MYSQL_TYPE_BLOB);
                } else {
                    _types.push_back(MYSQL_TYPE_STRING);
                }
                break;
            case MYSQL_TYPE_NULL:
                _types.push_back(MYSQL_TYPE_NULL);
                break;
            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_NEWDATE:
            default:
                // Unsupported type
                _types.push_back(MYSQL_TYPE_NULL);
                break;
        }
//        _types.push_back(field.type);
        _lengths.push_back(std::max(field.length, field.max_length));
        _flags.push_back(field.flags);
        _is_nulls.push_back(0);
    }
    mysql_free_result(metadata);

    // Prepare buffers for results
    _binds.resize(column_count, bind0);
    for(size_t i=0; i<column_count; ++i) {
        MYSQL_BIND &bind = _binds[i];
        bind.buffer_type = _types[i];
        bind.is_null = &_is_nulls[i];
        bind.length = &_lengths[i];
        switch (_types[i]) {
            case MYSQL_TYPE_TINY:
                _buffers.emplace_back(sizeof(char));
                break;
            case MYSQL_TYPE_SHORT:
                _buffers.emplace_back(sizeof(short));
                break;
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_YEAR:
                _buffers.emplace_back(sizeof(int));
                break;
            case MYSQL_TYPE_LONGLONG:
                _buffers.emplace_back(sizeof(int64_t));
                break;
            case MYSQL_TYPE_FLOAT:
                _buffers.emplace_back(sizeof(float));
                break;
            case MYSQL_TYPE_DOUBLE:
                _buffers.emplace_back(sizeof(double));
                break;
            case MYSQL_TYPE_STRING:
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_VARCHAR:
            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
                _buffers.emplace_back(_lengths[i]);
                break;
            case MYSQL_TYPE_NULL:
                // No buffer needed
                _buffers.emplace_back(0);
                break;
            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_NEWDATE:
            default:
                // Unsupported type
                _buffers.emplace_back(0);
                break;
        }
        bind.buffer = _buffers[i].data();
        bind.buffer_length = _buffers[i].size();
    }
    if(mysql_stmt_bind_result(_stmt.get(), _binds.data())!=0) {
        // TODO process error, throw exception
        int err = mysql_stmt_errno(_stmt.get());
        const char* errstr = mysql_stmt_error(_stmt.get());

        std::cout << "Error binding results: " << err << " - " << errstr << std::endl;
    }
}

std::vector<value> resultset::fetch()
{
    int res = mysql_stmt_fetch(_stmt.get());
    if(res!=0 && res!=MYSQL_NO_DATA && res!=MYSQL_DATA_TRUNCATED) {
        // TODO process error, throw exception
        return {};
    } else if(res==MYSQL_NO_DATA) {
        // No data
        return {};
    } else {
        std::vector<value> result;
        result.reserve(_binds.size());
        for(size_t i=0; i<_binds.size(); ++i) {
            const MYSQL_BIND &bind = _binds[i];
            if(bind.is_null!=nullptr && *bind.is_null != 0 || bind.is_null_value != 0) {
                result.emplace_back(nullptr);
            } else {
                switch (bind.buffer_type) {
                    case MYSQL_TYPE_TINY:
                        // TODO handle one-bit as boolean
                        result.emplace_back((int)*(char*)bind.buffer);
                        break;
                    case MYSQL_TYPE_SHORT:
                        result.emplace_back((int)*(short*)bind.buffer);
                        break;
                    case MYSQL_TYPE_LONG:
                    case MYSQL_TYPE_INT24:
                    case MYSQL_TYPE_YEAR:
                        result.emplace_back(*(int*)bind.buffer);
                        break;
                    case MYSQL_TYPE_LONGLONG:
                        result.emplace_back((int64_t)*(int64_t*)bind.buffer);
                        break;
                    case MYSQL_TYPE_FLOAT:
                        result.emplace_back((double)*(float*)bind.buffer);
                        break;
                    case MYSQL_TYPE_DOUBLE:
                        result.emplace_back(*(double*)bind.buffer);
                        break;
                    case MYSQL_TYPE_STRING:
                    case MYSQL_TYPE_VAR_STRING:
                    case MYSQL_TYPE_VARCHAR:
                    case MYSQL_TYPE_BLOB:
                    case MYSQL_TYPE_TINY_BLOB:
                    case MYSQL_TYPE_MEDIUM_BLOB:
                    case MYSQL_TYPE_LONG_BLOB:
                        // Always retrieve a BLOB for binary and text data without flags, look at the predeclared type
                        if(_types[i]==MYSQL_TYPE_BLOB) {
                            result.emplace_back(blob((const unsigned char *) bind.buffer,
                                                     (const unsigned char *) bind.buffer + (size_t) *bind.length));
                        } else {
                            result.emplace_back(std::string((const char *) bind.buffer, (size_t)*bind.length));
                        }
                        break;
                    case MYSQL_TYPE_NULL:
                        result.emplace_back(nullptr);
                        break;
                    case MYSQL_TYPE_TIMESTAMP:
                    case MYSQL_TYPE_DATE:
                    case MYSQL_TYPE_TIME:
                    case MYSQL_TYPE_DATETIME:
                    case MYSQL_TYPE_NEWDATE:
                    default:
                        // Unsupported type
                        result.emplace_back();
                        break;
                }
            }
        }
        if(res==MYSQL_DATA_TRUNCATED) {
            // TODO handle truncated data
        }
        return result;
    }
}

// Late implementation to have all required declarations
void resultset_row_iterator_impl::fetch_next_row() {
    if(_resultset) {
        _current_row = _resultset->fetch();
        if(_current_row.empty()) {
            _resultset = nullptr;
        }
    } else {
        _current_row.clear();
    }
}

unsigned int resultset::column_count() const
{
    return _column_names.size();
}

std::string resultset::column_name(unsigned int index) const
{
    if (index < _column_names.size()) {
        return _column_names[index];
    } else {
        return "";
    }
}
unsigned int resultset::column_index(const std::string& name) const
{
    for (size_t idx = 0; idx < _column_names.size(); ++idx) {
        if (name == _column_names[idx]) {
            return idx;
        }
    }
    return std::numeric_limits<unsigned int>::max();
}

std::string resultset::column_origin_name(unsigned int index) const
{
    if (index < _column_origin_names.size()) {
        return _column_origin_names[index];
    } else {
        return "";
    }
}

std::string resultset::table_origin_name(unsigned int index) const
{
    if (index < _table_origin_names.size()) {
        return _table_origin_names[index];
    } else {
        return "";
    }
}


value_type resultset::column_type(unsigned int index) const
{
    switch(_types[index]) {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_YEAR:
            return value_type::INT;
        case MYSQL_TYPE_LONGLONG:
            return value_type::INT64;
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return value_type::DOUBLE;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_NEWDATE:
            return value_type::STRING; // No Date type in sqlcpp yet
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
            return value_type::STRING;
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
            return value_type::BLOB;
        case MYSQL_TYPE_NULL:
            return value_type::NULL_VALUE;
        default:
            return value_type::UNSUPPORTED;
    }
}

unsigned int resultset::row_count() const {
    unsigned long long res = mysql_stmt_num_rows(_stmt.get());
    if(res <= std::numeric_limits<unsigned int>::max()) {
        return static_cast<unsigned int>(res);
    }
    // TODO log a warning ?
    return std::numeric_limits<unsigned int>::max();
}

bool resultset::has_row() const
{
    return mysql_stmt_num_rows(_stmt.get()) != 0;
}

sqlcpp::resultset_row_iterator resultset::begin() const
{
    std::shared_ptr<resultset> self = const_cast<resultset*>(this)->shared_from_this();
    return std::move(sqlcpp::resultset::create_iterator(std::make_shared<resultset_row_iterator_impl>(self)));
}

sqlcpp::resultset_row_iterator resultset::end() const
{
    return std::move(sqlcpp::resultset::create_iterator(std::make_shared<resultset_row_iterator_impl>(nullptr)));
}


//
// Statement
//

class statement : public sqlcpp::statement
{
private:
    std::shared_ptr<MYSQL_STMT> _stmt;

    std::vector<value> _params;


    std::vector<unsigned long> _lengths;
    std::vector<my_bool> _nulls;
    std::vector<blob> _buffers;
    std::vector<enum_field_types> _types;

public:
    statement(MYSQL_STMT* stmt):
        _stmt(stmt, mysql_stmt_close)
        {}

    virtual ~statement() =default;

    std::shared_ptr<sqlcpp::resultset> execute() override;

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

unsigned int statement::parameter_count() const {
    return mysql_stmt_param_count(_stmt.get());
}

int statement::parameter_index(const std::string& name) const
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return -1;
}

std::string statement::parameter_name(unsigned int index) const
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return "";
}

statement& statement::bind(const std::string& name, std::nullptr_t)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const std::string& value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const std::string_view& value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const blob& value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, bool value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, int value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, int64_t value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, double value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const value& value)
{
    // Parameter names not supported for MariaDB yet
    // TODO
//    std::visit([&](auto&& arg) {
//        bind(name, arg);
//    }, value);
    return *this;
}


namespace {


blob to_blob(const std::string &s) {
    blob b;
    b.insert(b.end(), s.begin(), s.end());
    return b;
}

blob to_blob(const std::string_view &s) {
    blob b;
    b.insert(b.end(), s.begin(), s.end());
    return b;
}

template<typename T>
blob to_blob(T v) {
    blob b;
    b.resize(b.size() + sizeof(T), 0);
    std::memcpy(b.data() + b.size() - sizeof(T), &v, sizeof(T));
    return b;
}



}



template<typename T>
static inline void set(std::vector<T>& params, size_t index, const T& value, const T& def)
{
    if(params.size() <= index) {
        params.resize(index + 1, def);
    }
    params[index] = value;
}

statement& statement::bind(unsigned int index, std::nullptr_t)
{
    index--; // Convert to 0-based index
    set<unsigned long>(_lengths, index, 0, 0);
    set<my_bool>(_nulls, index, 1, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_NULL, MYSQL_TYPE_NULL);
    set(_buffers, index, blob(), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string& value)
{
    index--; // Convert to 0-based index
    set<unsigned long>(_lengths, index, value.length(), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_STRING, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob(value), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string_view& value)
{
    index--; // Convert to 0-based index
    set<unsigned long>(_lengths, index, value.length(), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_STRING, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob(value), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const blob& value)
{
    index--; // Convert to 0-based index
    blob val;
    set<unsigned long>(_lengths, index, value.size(), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_BLOB, MYSQL_TYPE_NULL);
    set(_buffers, index, value, blob());    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, bool value)
{
    index--; // Convert to 0-based index
    blob val;
    set<unsigned long>(_lengths, index, 1, 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_TINY, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob((unsigned char)(value ? 1 : 0)), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int value)
{
    index--; // Convert to 0-based index
    blob val;
    set<unsigned long>(_lengths, index, sizeof(int), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_LONG, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob(value), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int64_t value)
{
    index--; // Convert to 0-based index
    blob val;
    set<unsigned long>(_lengths, index, sizeof(int64_t), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_LONGLONG, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob(value), blob());
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, double value)
{
    index--; // Convert to 0-based index
    blob val;
    set<unsigned long>(_lengths, index, sizeof(double), 0);
    set<my_bool>(_nulls, index, 0, 0);
    set<enum_field_types>(_types, index, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_NULL);
    set(_buffers, index, to_blob(value), blob());
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


std::shared_ptr<sqlcpp::resultset> statement::execute()
{
    // Bind parameters, if any
    if(!_types.empty()) {
        std::vector<MYSQL_BIND> binds;
        binds.resize(_types.size(), bind0);

        for(size_t idx = 0; idx<_types.size(); idx++) {
            binds[idx].buffer_type = _types[idx];
            binds[idx].buffer_length = _lengths[idx];
            binds[idx].buffer = _buffers[idx].data();
            binds[idx].length = &_lengths[idx];
            binds[idx].is_null = (my_bool * ) & _nulls[idx];
        }

        if (mysql_stmt_bind_param(_stmt.get(), binds.data()) != 0) { // skip index 0
            // TODO throw exception
            // throw statement_exception(mysql_stmt_error(_stmt.get()), mysql_stmt_errno(_stmt.get()));
            int err = mysql_stmt_errno(_stmt.get());
            const char* errstr = mysql_stmt_error(_stmt.get());
        }
    }

    if (mysql_stmt_execute(_stmt.get()) != 0) {
        // TODO throw exception
        // throw statement_exception(mysql_stmt_error(stmt_), mysql_stmt_errno(stmt_));
        int err = mysql_stmt_errno(_stmt.get());
        const char* errstr = mysql_stmt_error(_stmt.get());
        return {};
    }

    return std::make_shared<resultset>(_stmt);
}








//
// MariaDB's connection
//

connection::connection(MYSQL* db):
    _db(db, &mysql_close)
{
    if(db== nullptr) {
        // TODO throw an exception
    }
}

std::shared_ptr<connection> connection::create(const std::string& connection_string) {
    if(bind0.buffer_length!=0) {// init only once
        memset(&bind0, 0, sizeof(MYSQL_BIND));
    }

    // Parse connection string format: mariadb://user:password@host:port/database
    std::regex uri_regex(R"(mariadb://([^:]+):([^@]+)@([^:]+):(\d+)/(.+))");
    std::smatch matches;

    if (!std::regex_match(connection_string, matches, uri_regex)) {
//        throw connection_exception("Invalid MariaDB connection string format");
    }

    std::string username = matches[1].str();
    std::string password = matches[2].str();
    std::string host = matches[3].str();
    unsigned int port = std::stoul(matches[4].str());
    std::string database = matches[5].str();

    return create(host, port, database, username, password);
}

std::shared_ptr<connection> connection::create(const std::string& host,
                                               unsigned int port,
                                               const std::string& database,
                                               const std::string& username,
                                               const std::string& password) {

    MYSQL* mysql = mysql_init(nullptr);

    if (mysql_real_connect(mysql, host.c_str(), username.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, CLIENT_MULTI_STATEMENTS) == nullptr) {

        int err = mysql_errno(mysql);

        MARIADB_CONNECTION_ERROR;

        // TODO throw an exception with mysql_error(mysql)
        throw new std::runtime_error(mysql_error(mysql));


        // diag("Error (%d): %s (%d) in %s line %d", rc, mysql_error(mysql), \
        //         mysql_errno(mysql), __FILE__, __LINE__);\
        // throw connection_exception("Failed to connect to MariaDB server");
        return {};
    }
    return std::make_shared<connection>(mysql);
}

std::shared_ptr<sqlcpp::statement> connection::prepare(const std::string& sql) {
    if(sql.empty()) {
        // TODO throw connection_exception("SQL query is empty");
    }

    MYSQL_STMT* stmt = mysql_stmt_init(_db.get());

    // Force update of max_length on result set metadata fetch
    static const my_bool update_max_length = 1;
    if(mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void *)&update_max_length)) {
        // TODO throw an exception with mysql_error(mysql)
        int err = mysql_errno(_db.get());
        const char* errstr = mysql_error(_db.get());
        mysql_stmt_close(stmt);
        return nullptr;
    }

    if(mysql_stmt_prepare(stmt, sql.c_str(), sql.length())) {
        // TODO throw an exception with mysql_error(mysql)
        // diag("Error: %s (%s: %d)", mysql_stmt_error(stmt), __FILE__, __LINE__);
        int err = mysql_errno(_db.get());
        const char* errstr = mysql_error(_db.get());
        mysql_stmt_close(stmt);
        return nullptr;
    }

    return std::make_shared<statement>(stmt);
}

void connection::execute(const std::string& sql) {
    if (mysql_real_query(_db.get(), sql.c_str(), sql.length()) != 0) {
        // TODO throw exception
        int err = mysql_errno(_db.get());
        const char* errstr = mysql_error(_db.get());
        throw new std::runtime_error(errstr);

//        throw statement_exception(mysql_error(mysql_), mysql_errno(mysql_));
    }

    // Flush all results, just in case there are multiple statements
    do {
        MYSQL_RES *res = mysql_use_result(_db.get());
        if (res == nullptr) {
            int err = mysql_errno(_db.get());
            if (err != 0) {
                // TODO throw exception
                const char *errstr = mysql_error(_db.get());
                return;
            }
        } else {
            mysql_free_result(res);
        }
    } while(mysql_next_result(_db.get())==0);
}

} // namespace sqlcpp::mariadb
