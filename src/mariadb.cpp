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
#include "sqlcpp/details.hpp"

#include <regex>
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
 * - BOOL(EAN) is just an alias for TINYINT(1), all TINYINT(1) will be considered as BOOLEAN : https://dev.mysql.com/doc/refman/9.4/en/numeric-type-syntax.html
 */

namespace sqlcpp::mariadb {

namespace {

MYSQL_BIND bind0 = {.buffer_length = 42};


blob string_to_blob(const std::string &s) {
    blob b;
    b.insert(b.end(), s.begin(), s.end());
    return b;
}

blob string_to_blob(const std::string_view &s) {
    blob b;
    b.insert(b.end(), s.begin(), s.end());
    return b;
}


template<typename T>
blob num_to_blob(T v) {
    blob b;
    b.resize(b.size() + sizeof(T), 0);
    std::memcpy(b.data() + b.size() - sizeof(T), &v, sizeof(T));
    return b;
}




} // namespace sqlcpp::mariadb::<<anon>>



//
// Resultset iterator
//

class resultset;

class resultset_row_iterator_impl : public sqlcpp::resultset_row_iterator_impl, protected row_base
{
protected:
    std::shared_ptr<resultset> _resultset;

    std::vector<value> _current_row;

    void fetch_next_row();

public:
    resultset_row_iterator_impl(std::shared_ptr<resultset> resultset);

    virtual ~resultset_row_iterator_impl() = default;

    const row_base& get() const override;
    bool next() override;
    bool different(const sqlcpp::resultset_row_iterator_impl& other) const override;

    size_t size() const override;

    value get_value(unsigned int index) const override;

    std::string get_value_string(unsigned int index) const override;
    blob get_value_blob(unsigned int index) const override;
    bool get_value_bool(unsigned int index) const override;
    int get_value_int(unsigned int index) const override;
    int64_t get_value_int64(unsigned int index) const override;
    double get_value_double(unsigned int index) const override;
};

resultset_row_iterator_impl::resultset_row_iterator_impl(std::shared_ptr<resultset> resultset) :
_resultset(std::move(resultset))
{
    fetch_next_row();
}

const row_base& resultset_row_iterator_impl::get() const
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

size_t resultset_row_iterator_impl::size() const
{
    return _current_row.size();
}

value resultset_row_iterator_impl::get_value(unsigned int index) const
{
    return _current_row[index];
}

std::string resultset_row_iterator_impl::get_value_string(unsigned int index) const
{
    return to_string(_current_row[index]);
}

blob resultset_row_iterator_impl::get_value_blob(unsigned int index) const
{
    return to_blob(_current_row[index]);
}

bool resultset_row_iterator_impl::get_value_bool(unsigned int index) const
{
    return to_bool(_current_row[index]);
}

int resultset_row_iterator_impl::get_value_int(unsigned int index) const
{
    return to_int(_current_row[index]);
}

int64_t resultset_row_iterator_impl::get_value_int64(unsigned int index) const
{
    return to_int64(_current_row[index]);
}

double resultset_row_iterator_impl::get_value_double(unsigned int index) const
{
    return to_double(_current_row[index]);
}

//
// MySql data fetcher
//
class mysql_statement
{
protected:
    std::shared_ptr<MYSQL_STMT> _stmt;
    bool _executed = false;

    std::vector<std::string> _column_names;
    std::vector<std::string> _column_origin_names;
    std::vector<std::string> _table_origin_names;
    std::vector<value_type> _column_types;

    std::vector<unsigned long> _lengths;
    std::vector<my_bool> _is_nulls;
    std::vector<enum_field_types> _my_types;
    std::vector<unsigned int> _flags;
    std::vector<blob> _buffers;

    std::vector<MYSQL_BIND> _binds;

public:
    mysql_statement(MYSQL_STMT* stmt) : _stmt(stmt, mysql_stmt_close) {}
    mysql_statement(std::shared_ptr<MYSQL_STMT> stmt) : _stmt(stmt) {}
    ~mysql_statement() {
        close();
        for(auto& bind : _binds) {
            /*
                    if(bind.buffer!=nullptr) {
                        delete[] static_cast<char *>(bind.buffer);
                    }
            */
            bind = bind0;
        }
    }

    bool ok() const { return _stmt!=nullptr; }
    operator bool() const { return ok(); }

    bool executed() const {
        return _executed;
    }

    void close() {
        if (_stmt) {
            _stmt.reset();
        }
    }

    void store_all_results();

    void prepare_buffers();
    std::vector<value> fetch_next_row();
    std::vector<value> fetch_row(unsigned long long index);

    unsigned long long affected_rows() const {
        return ok() ? mysql_stmt_affected_rows(_stmt.get()) : 0;
    }
    unsigned long long last_insert_id() const {
        return ok() ? mysql_stmt_insert_id(_stmt.get()) : 0;
    }

    unsigned int parameter_count() const {
        return ok() ? mysql_stmt_param_count(_stmt.get()) : 0;
    }

    unsigned int column_count() const {
        return _column_names.size();
    }

    std::string column_name(unsigned int index) const {
        if (index < _column_names.size()) {
            return _column_names[index];
        } else {
            return "";
        }
    }

    unsigned int column_index(const std::string& name) const {
        for (size_t idx = 0; idx < _column_names.size(); ++idx) {
            if (name == _column_names[idx]) {
                return idx;
            }
        }
        return std::numeric_limits<unsigned int>::max();
    }

    std::string column_origin_name(unsigned int index) const {
        if (index < _column_origin_names.size()) {
            return _column_origin_names[index];
        } else {
            return "";
        }
    }

    std::string table_origin_name(unsigned int index) const {
        if (index < _table_origin_names.size()) {
            return _table_origin_names[index];
        } else {
            return "";
        }
    }

    value_type column_type(unsigned int index) const;

    template<typename T>
    static inline void set(std::vector<T>& params, size_t index, const T& value, const T& def)
    {
        if(params.size() <= index) {
            params.resize(index + 1, def);
        }
        params[index] = value;
    }

    void bind(unsigned int index, std::nullptr_t);
    void bind(unsigned int index, const std::string& value);
    void bind(unsigned int index, const std::string_view& value);
    void bind(unsigned int index, const blob& value);
    void bind(unsigned int index, bool value);
    void bind(unsigned int index, int value);
    void bind(unsigned int index, int64_t value);
    void bind(unsigned int index, double value);
    void bind(unsigned int index, const value& value);

    bool execute();

    bool has_row() const {
        return mysql_stmt_num_rows(_stmt.get()) != 0;
    }

    unsigned long long row_count() const {
        if (ok()) {
            unsigned long long res = mysql_stmt_num_rows(_stmt.get());
            if(res <= std::numeric_limits<unsigned int>::max()) {
                return static_cast<unsigned int>(res);
            }
        }
        // TODO log a warning ?
        return std::numeric_limits<unsigned int>::max();
    }

    void consume_results(std::function<void(const row_base&)> func);

};

void mysql_statement::store_all_results()
{
    if (ok()) {
        if(mysql_stmt_store_result(_stmt.get())!=0) {
            int err = mysql_stmt_errno(_stmt.get());
            const char* errstr = mysql_stmt_error(_stmt.get());
            std::cout << "Error store results: " << err << " - " << errstr << std::endl;
        }
    }
}

void mysql_statement::prepare_buffers()
{
    if (ok()) {
        // Retrieve metadata for result columns
        MYSQL_RES* metadata = mysql_stmt_result_metadata(_stmt.get());
        if(metadata==nullptr) {
            if(mysql_stmt_field_count(_stmt.get()) == 0) {
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
                    if (field.length==1) {
                        _column_types.push_back(value_type::BOOL);
                    } else {
                        _column_types.push_back(value_type::INT);
                    }
                    _my_types.push_back(MYSQL_TYPE_TINY);
                    break;
                case MYSQL_TYPE_SHORT:
                case MYSQL_TYPE_LONG:
                case MYSQL_TYPE_INT24:
                case MYSQL_TYPE_YEAR:
                    // TODO test for unsigned : UNSIGNED_FLAG
                    _column_types.push_back(value_type::INT);
                    _my_types.push_back(MYSQL_TYPE_LONG);
                    break;
                case MYSQL_TYPE_LONGLONG:
                    _column_types.push_back(value_type::INT64);
                    _my_types.push_back(MYSQL_TYPE_LONGLONG);
                    break;
                case MYSQL_TYPE_FLOAT:
                case MYSQL_TYPE_DOUBLE:
                    _column_types.push_back(value_type::DOUBLE);
                    _my_types.push_back(MYSQL_TYPE_DOUBLE);
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
                        _column_types.push_back(value_type::BLOB);
                        _my_types.push_back(MYSQL_TYPE_BLOB);
                    } else {
                        _column_types.push_back(value_type::STRING);
                        _my_types.push_back(MYSQL_TYPE_STRING);
                    }
                    break;
                case MYSQL_TYPE_NULL:
                    _column_types.push_back(value_type::NULL_VALUE);
                    _my_types.push_back(MYSQL_TYPE_NULL);
                    break;
                case MYSQL_TYPE_TIMESTAMP:
                case MYSQL_TYPE_DATE:
                case MYSQL_TYPE_TIME:
                case MYSQL_TYPE_DATETIME:
                case MYSQL_TYPE_NEWDATE:
                default:
                    // Unsupported type
                    _column_types.push_back(value_type::UNSUPPORTED);
                    _my_types.push_back(MYSQL_TYPE_NULL);
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
            bind.buffer_type = _my_types[i];
            bind.is_null = &_is_nulls[i];
            bind.length = &_lengths[i];
            switch (_my_types[i]) {
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
}


std::vector<value> mysql_statement::fetch_next_row()
{
    if (ok()) {
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
                        case MYSQL_TYPE_TINY: {
                            // TODO handle one-bit as boolean
                            char val = *(char*)bind.buffer;
                            if (_column_types[i]==value_type::BOOL) {
                                result.emplace_back((bool)val!=0);
                            } else {
                                result.emplace_back((int)val);
                            }
                            break;
                        }
                        case MYSQL_TYPE_SHORT: {
                            short val = *(short*)bind.buffer;
                            result.emplace_back((int)val);
                            break;
                        }
                        case MYSQL_TYPE_LONG:
                        case MYSQL_TYPE_INT24:
                        case MYSQL_TYPE_YEAR: {
                            int val = *(int*)bind.buffer;
                            result.emplace_back(val);
                            break;
                        }
                        case MYSQL_TYPE_LONGLONG: {
                            int64_t val = *(int64_t*)bind.buffer;
                            result.emplace_back(val);
                            break;
                        }
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
                            if(_my_types[i]==MYSQL_TYPE_BLOB) {
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
    return {};
}

void mysql_statement::consume_results(std::function<void(const row_base&)> func)
{
    prepare_buffers();
    for (std::vector<value> row = fetch_next_row(); !row.empty(); row = fetch_next_row()) {
        func(details::generic_row(row));
    }
}

std::vector<value> mysql_statement::fetch_row(unsigned long long index)
{
    if (ok()) {
        mysql_stmt_data_seek(_stmt.get(), index);
        return fetch_next_row();
    } else {
        return {};
    }
}

value_type mysql_statement::column_type(unsigned int index) const
{
    return _column_types[index];
}

void mysql_statement::bind(unsigned int index, std::nullptr_t)
{
    set<unsigned long>(_lengths, index, 0, 0);
    set<my_bool>(_is_nulls, index, 1, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_NULL, MYSQL_TYPE_NULL);
    set(_buffers, index, blob(), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, const std::string& value)
{
    set<unsigned long>(_lengths, index, value.length(), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_STRING, MYSQL_TYPE_NULL);
    set(_buffers, index, string_to_blob(value), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, const std::string_view& value)
{
    set<unsigned long>(_lengths, index, value.length(), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_STRING, MYSQL_TYPE_NULL);
    set(_buffers, index, string_to_blob(value), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, const blob& value)
{
    blob val;
    set<unsigned long>(_lengths, index, value.size(), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_BLOB, MYSQL_TYPE_NULL);
    set(_buffers, index, value, blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, bool value)
{
    blob val;
    set<unsigned long>(_lengths, index, 1, 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_TINY, MYSQL_TYPE_NULL);
    set(_buffers, index, num_to_blob((unsigned char)(value ? 1 : 0)), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, int value)
{
    blob val;
    set<unsigned long>(_lengths, index, sizeof(int), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_LONG, MYSQL_TYPE_NULL);
    set(_buffers, index, num_to_blob(value), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, int64_t value)
{
    blob val;
    set<unsigned long>(_lengths, index, sizeof(int64_t), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_LONGLONG, MYSQL_TYPE_NULL);
    set(_buffers, index, num_to_blob(value), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, double value)
{
    blob val;
    set<unsigned long>(_lengths, index, sizeof(double), 0);
    set<my_bool>(_is_nulls, index, 0, 0);
    set<enum_field_types>(_my_types, index, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_NULL);
    set(_buffers, index, num_to_blob(value), blob());
    // TODO process error, throw exception
}

void mysql_statement::bind(unsigned int index, const value& value)
{
    std::visit([&](auto&& arg) {
        bind(index, arg);
    }, value);
}


bool mysql_statement::execute()
{
    if (!ok()) {
        return false;
    }

    // Bind parameters, if any
    if(!_my_types.empty()) {
        std::vector<MYSQL_BIND> binds;
        binds.resize(_my_types.size(), bind0);

        for(size_t idx = 0; idx<_my_types.size(); idx++) {
            binds[idx].buffer_type = _my_types[idx];
            binds[idx].buffer_length = _lengths[idx];
            binds[idx].buffer = _buffers[idx].data();
            binds[idx].length = &_lengths[idx];
            binds[idx].is_null = (my_bool * ) & _is_nulls[idx];
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
        return false;
    }

    return true;
}



//
// Iterable ResultSet
//

class resultset : public sqlcpp::cursor_resultset, public std::enable_shared_from_this<resultset>
{
public:
    resultset(std::shared_ptr<mysql_statement> stmt) : _stmt(stmt) {_stmt->prepare_buffers();}
    ~resultset() override =default;

    unsigned long long affected_rows() const override {return _stmt->affected_rows();}
    unsigned long long last_insert_id() const override {return _stmt->last_insert_id();}

    unsigned int column_count() const override {return _stmt->column_count();}
    std::string column_name(unsigned int index) const override {return _stmt->column_name(index);}
    unsigned int column_index(const std::string& name) const override {return _stmt->column_index(name);}
    std::string column_origin_name(unsigned int index) const override {return _stmt->column_origin_name(index);}
    std::string table_origin_name(unsigned int index) const override {return _stmt->table_origin_name(index);}
    value_type column_type(unsigned int index) const override {return _stmt->column_type(index);}

    bool has_row() const override;

    std::vector<value> fetch_next_row() {return _stmt->fetch_next_row();}

    sqlcpp::resultset_row_iterator begin() const override;
    sqlcpp::resultset_row_iterator end() const override;

protected:
    friend class resultset_row_iterator_impl;

    std::shared_ptr<mysql_statement> _stmt;

private:
    void fetch_metadata();
};

// Late implementation to have all required declarations
void resultset_row_iterator_impl::fetch_next_row() {
    if(_resultset) {
        _current_row = _resultset->fetch_next_row();
        if(_current_row.empty()) {
            _resultset = nullptr;
        }
    } else {
        _current_row.clear();
    }
}


bool resultset::has_row() const
{
    return _stmt->has_row();
}

sqlcpp::resultset_row_iterator resultset::begin() const
{
    std::shared_ptr<resultset> self = const_cast<resultset*>(this)->shared_from_this();
    return std::move(sqlcpp::cursor_resultset::create_iterator(std::make_shared<resultset_row_iterator_impl>(self)));
}

sqlcpp::resultset_row_iterator resultset::end() const
{
    return std::move(sqlcpp::cursor_resultset::create_iterator(std::make_shared<resultset_row_iterator_impl>(nullptr)));
}

//
// Buffered resultset
//

class buffered_resultset : public sqlcpp::buffered_resultset, public std::enable_shared_from_this<buffered_resultset>
{
public:
    buffered_resultset(std::shared_ptr<mysql_statement> stmt) : _stmt(stmt) {
        _stmt->store_all_results();
        _stmt->prepare_buffers();
    }
    ~buffered_resultset() override =default;

    unsigned long long affected_rows() const override {return _stmt->affected_rows();}
    unsigned long long last_insert_id() const override {return _stmt->last_insert_id();}

    unsigned int column_count() const override {return _stmt->column_count();}
    std::string column_name(unsigned int index) const override {return _stmt->column_name(index);}
    unsigned int column_index(const std::string& name) const override {return _stmt->column_index(name);}
    std::string column_origin_name(unsigned int index) const override {return _stmt->column_origin_name(index);}
    std::string table_origin_name(unsigned int index) const override {return _stmt->table_origin_name(index);}
    value_type column_type(unsigned int index) const override {return _stmt->column_type(index);}

    unsigned int row_count() const /*override*/;
    bool has_row() const override;

    iterator begin() const override;

    iterator end() const override;

    const row_base & get_row(unsigned long long index) const override;

protected:
    friend class resultset_row_iterator_impl;

    std::shared_ptr<mysql_statement> _stmt;
    mutable details::generic_row _current_row;

};

unsigned int buffered_resultset::row_count() const
{
    return _stmt->row_count();
}

buffered_resultset::iterator buffered_resultset::begin() const
{
    // TODO
    return {};
}

buffered_resultset::iterator buffered_resultset::end() const
{
    // TODO
    return {};
}

bool buffered_resultset::has_row() const
{
    return _stmt->has_row();
}

const row_base & buffered_resultset::get_row(unsigned long long index) const
{
    _current_row.set_values(_stmt->fetch_row(index));
    return _current_row;
}


//
// Statement
//

class statement : public sqlcpp::statement
{
private:
    std::shared_ptr<mysql_statement> _stmt;
//    std::shared_ptr<MYSQL_STMT> _stmt;

    std::vector<value> _params;


    std::vector<unsigned long> _lengths;
    std::vector<my_bool> _nulls;
    std::vector<blob> _buffers;
    std::vector<enum_field_types> _types;

public:
    statement(std::shared_ptr<mysql_statement> stmt):
    _stmt(stmt)
    {}
    statement(MYSQL_STMT* stmt):
    _stmt(std::make_shared<mysql_statement>(stmt))
    {}

    virtual ~statement() =default;

    std::shared_ptr<sqlcpp::cursor_resultset> execute() override;
    void execute(std::function<void(const row_base&)> func) override;
    std::shared_ptr<sqlcpp::buffered_resultset> execute_buffered() override;

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
    return _stmt->parameter_count();
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

statement& statement::bind(unsigned int index, std::nullptr_t)
{
    _stmt->bind(index-1, nullptr);
    return *this;
}

statement& statement::bind(unsigned int index, const std::string& value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, const std::string_view& value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, const blob& value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, bool value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, int value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, int64_t value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, double value)
{
    _stmt->bind(index-1, value);
    return *this;
}

statement& statement::bind(unsigned int index, const value& value)
{
    _stmt->bind(index-1, value);
    return *this;
}


std::shared_ptr<sqlcpp::cursor_resultset> statement::execute()
{
    if (_stmt->execute()) {
        return std::make_shared<resultset>(_stmt);
    } else {
        return nullptr;
    }
}

void statement::execute(std::function<void(const row_base&)> func)
{
    if (_stmt->execute()) {
        _stmt->consume_results(func);
    }
}

std::shared_ptr<sqlcpp::buffered_resultset> statement::execute_buffered()
{
    if (_stmt->execute()) {
        return std::make_shared<buffered_resultset>(_stmt);
    } else {
        return nullptr;
    }
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

    if (_last_stmt) {
        _last_stmt->close();
        _last_stmt.reset();
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

    auto mdb_stmt = std::make_shared<mysql_statement>(stmt);
    _last_stmt = mdb_stmt;

    return std::make_shared<statement>(mdb_stmt);
}

std::shared_ptr<stats_result> connection::execute(const std::string& sql) {

    if (_last_stmt) {
        _last_stmt->close();
        _last_stmt.reset();
    }

    if (mysql_real_query(_db.get(), sql.c_str(), sql.length()) != 0) {
        // TODO throw exception
        int err = mysql_errno(_db.get());
        const char* errstr = mysql_error(_db.get());
        throw new std::runtime_error(errstr);

//        throw statement_exception(mysql_error(mysql_), mysql_errno(mysql_));
    }

    uint64_t total_affected_rows = 0;
    uint64_t real_last_inserted_id = 0;

    // Flush all results, just in case there are multiple statements
    do {
        MYSQL_RES *res = mysql_use_result(_db.get());
        uint64_t affected_rows = mysql_affected_rows(_db.get());
        uint64_t last_inserted_id = mysql_insert_id(_db.get());
        if (res == nullptr) {
            int err = mysql_errno(_db.get());
            if (err != 0) {
                // TODO throw exception
                const char *errstr = mysql_error(_db.get());
                return nullptr;
            }
        } else {
            mysql_free_result(res);
        }
        if (affected_rows != ~0ull) {
            total_affected_rows += affected_rows;
        }
        if (last_inserted_id != 0) {
            real_last_inserted_id = last_inserted_id;
        }
    } while(mysql_next_result(_db.get())==0);
    return std::make_shared<details::simple_stats_result>(total_affected_rows, real_last_inserted_id);
}



//
// MariaDB connection factory
//

class mariadb_connection_factory : public details::connection_factory
{
public:
    mariadb_connection_factory() = default;
    ~mariadb_connection_factory() override = default;

    std::vector<std::string> supported_schemes() const override;
    std::shared_ptr<sqlcpp::connection> do_create_connection(const std::string_view& url) override;
};

std::vector<std::string> mariadb_connection_factory::supported_schemes() const
{
    return {"my", "mysql", "maria", "mariadb"};
}

std::shared_ptr<sqlcpp::connection> mariadb_connection_factory::do_create_connection(const std::string_view& url)
{
    return connection::create(std::string("mariadb:")+url.data());
}

__attribute__((constructor))
void register_connection_factory() {
    static std::shared_ptr<mariadb_connection_factory> _factory;
    _factory = std::make_shared<mariadb_connection_factory>();
    details::connection_factory_registry::get().register_factory(_factory);
    if (!_factory) {
        _factory = std::make_shared<mariadb_connection_factory>();
        details::connection_factory_registry::get().register_factory(_factory);
    }
}



} // namespace sqlcpp::mariadb
