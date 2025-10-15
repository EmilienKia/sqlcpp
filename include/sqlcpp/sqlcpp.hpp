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

#ifndef SQLCPP_HPP
#define SQLCPP_HPP


#include <functional>
#include <memory>
#include <iterator>
#include <optional>
#include <vector>
#include <variant>
#include <string>
#include <string_view>

#include "sqlcpp.hpp"

namespace sqlcpp
{

class statement;
class stats_result;
class cursor_resultset;
class buffered_resultset;
class resultset_row_iterator;
class resultset_row_iterator_impl;
class row;

class connection
{
protected:
    connection() = default;

public:
    virtual ~connection() = default;
    static std::shared_ptr<connection> create(const std::string& connection_string);
    virtual std::shared_ptr<stats_result> execute(const std::string& query) = 0;
    virtual std::shared_ptr<statement> prepare(const std::string& query) = 0;
};



enum value_type {
    NONE = -1,
    NULL_VALUE = 0,
    STRING,
    BLOB,
    BOOL,
    INT,
    INT64,
    DOUBLE,
    UNSUPPORTED
};

typedef std::vector<unsigned char> blob;
typedef std::variant<std::monostate, std::nullptr_t, std::string, blob, bool, int, int64_t, double> value;

inline bool is_null(const value& v) {
    return std::holds_alternative<std::nullptr_t>(v);
}

template<typename T>
bool is(const value& val) {
    return std::holds_alternative<T>(val);
}

std::string to_string(const value& val);
blob to_blob(const value& val);
bool to_bool(const value& val);
int to_int(const value& val);
int64_t to_int64(const value& val);
double to_double(const value& val);

std::optional<std::string> to_string_opt(const value& val);
std::optional<blob> to_blob_opt(const value& val);
std::optional<bool> to_bool_opt(const value& val);
std::optional<int> to_int_opt(const value& val);
std::optional<int64_t> to_int64_opt(const value& val);
std::optional<double> to_double_opt(const value& val);


template<typename T>
T as(const value& val);





class statement
{
protected:
    statement() = default;

public:
    virtual ~statement() = default;

    virtual std::shared_ptr<cursor_resultset> execute() = 0;
    virtual void execute(std::function<void(const row&)> func) = 0;
    virtual std::shared_ptr<buffered_resultset> execute_buffered() = 0;

    virtual unsigned int parameter_count() const = 0;
    virtual int parameter_index(const std::string& name) const = 0;
    virtual std::string parameter_name(unsigned int index) const = 0;

    virtual statement& bind_null(const std::string& name);
    virtual statement& bind(const std::string& name, std::nullptr_t) = 0;
    virtual statement& bind(const std::string& name, const std::string& value) = 0;
    virtual statement& bind(const std::string& name, const std::string_view& value) = 0;
    virtual statement& bind(const std::string& name, const blob& value) = 0;
    virtual statement& bind(const std::string& name, bool value) = 0;
    virtual statement& bind(const std::string& name, int value) = 0;
    virtual statement& bind(const std::string& name, int64_t value) = 0;
    virtual statement& bind(const std::string& name, double value) = 0;
    virtual statement& bind(const std::string& name, const value& value) = 0;

    virtual statement& bind_null(unsigned int index);
    virtual statement& bind(unsigned int index, std::nullptr_t) = 0;
    virtual statement& bind(unsigned int index, const std::string& value) = 0;
    virtual statement& bind(unsigned int index, const std::string_view& value) = 0;
    virtual statement& bind(unsigned int index, const blob& value) = 0;
    virtual statement& bind(unsigned int index, bool value) = 0;
    virtual statement& bind(unsigned int index, int value) = 0;
    virtual statement& bind(unsigned int index, int64_t value) = 0;
    virtual statement& bind(unsigned int index, double value) = 0;
    virtual statement& bind(unsigned int index, const value& value) = 0;
};

class row
{
protected:
    row() = default;
public:
    virtual ~row() = default;

    virtual size_t size() const = 0;

    virtual value get_value(unsigned int index) const = 0;

    value operator[](unsigned int index)const { return get_value(index); }

    virtual std::string get_value_string(unsigned int index) const = 0;
    virtual blob get_value_blob(unsigned int index) const = 0;
    virtual bool get_value_bool(unsigned int index) const = 0;
    virtual int get_value_int(unsigned int index) const = 0;
    virtual int64_t get_value_int64(unsigned int index) const = 0;
    virtual double get_value_double(unsigned int index) const = 0;

    virtual std::vector<value> get_values() const;
};

class resultset_row_iterator_impl
{
protected:
    resultset_row_iterator_impl() = default;
public:
    virtual ~resultset_row_iterator_impl() = default;

    virtual const row& get() const =0;
    virtual bool next() = 0;
    virtual bool different(const resultset_row_iterator_impl& other) const = 0;
};

class resultset_row_iterator : public std::input_iterator_tag
{
protected:
    std::shared_ptr<resultset_row_iterator_impl> _impl;

public:
    resultset_row_iterator(std::shared_ptr<resultset_row_iterator_impl>&& impl) : _impl(std::move(impl)) {}

    using value_type = row;

    resultset_row_iterator() = default;
    ~resultset_row_iterator() = default;

    resultset_row_iterator(const resultset_row_iterator&) = delete;
    resultset_row_iterator(resultset_row_iterator&&) = default;

    resultset_row_iterator& operator=(const resultset_row_iterator&) = delete;
    resultset_row_iterator& operator=(resultset_row_iterator&&) = default;

    resultset_row_iterator& operator++();
    void operator++(int);
    bool operator!=(const resultset_row_iterator& other) const;
    const row& operator*() const;
    const row& operator->() const;
};

class stats_result
{
public:
    virtual ~stats_result() = default;

    virtual unsigned long long affected_rows() const =0;
    virtual unsigned long long last_insert_id() const =0;
};

class cursor_resultset : public stats_result
{
protected:
    cursor_resultset() = default;

    static resultset_row_iterator create_iterator(std::shared_ptr<resultset_row_iterator_impl>&& impl) {
        return {std::move(impl)};
    }

public:
    typedef resultset_row_iterator iterator;

    virtual ~cursor_resultset() = default;

    virtual unsigned int column_count() const = 0;

    virtual std::string column_name(unsigned int index) const = 0;
    virtual unsigned int column_index(const std::string& name) const = 0;
    virtual std::string column_origin_name(unsigned int index) const = 0;
    virtual std::string table_origin_name(unsigned int index) const = 0;
    virtual value_type column_type(unsigned int index) const = 0;

    virtual bool has_row() const = 0;
    operator bool() const { return has_row();}

    virtual iterator begin() const =0;
    virtual iterator end() const =0;

};

class buffered_resultset : public cursor_resultset
{
protected:
    buffered_resultset() = default;

public:
    virtual unsigned int row_count() const = 0;

    virtual const row& get_row(unsigned long long index) const = 0;
};





//
// Late template implementations
//
template<>
inline std::string as(const value& val) {
    return to_string(val);
}

template<>
inline blob as(const value& val) {
    return to_blob(val);
}

template<>
inline bool as(const value& val) {
    return to_bool(val);
}

template<>
inline int as(const value& val) {
    return to_int(val);
}

template<>
inline int64_t as(const value& val) {
    return to_int64(val);
}

template<>
inline double as(const value& val) {
    return to_double(val);
}

template<>
inline std::optional<std::string> as(const value& val) {
    return to_string_opt(val);
}

template<>
inline std::optional<blob> as(const value& val) {
    return to_blob_opt(val);
}

template<>
inline std::optional<bool> as(const value& val) {
    return to_bool_opt(val);
}

template<>
inline std::optional<int> as(const value& val) {
    return to_int_opt(val);
}

template<>
inline std::optional<int64_t> as(const value& val) {
    return to_int64_opt(val);
}

template<>
inline std::optional<double> as(const value& val) {
    return to_double_opt(val);
}



} // namespace sqlcpp
#endif // SQLCPP_HPP
