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

#include "../include/sqlcpp/sqlcpp.hpp"

#include <filesystem>

#include "../include/sqlcpp/details.hpp"

#include <stdexcept>
#include <iostream>

namespace sqlcpp
{

//
// SQLCPP connection creation
//

std::shared_ptr<connection> connection::create(const std::string& connection_string)
{
    return {};
}

//
// Value management
//

std::string details::blob_to_hex_string(const blob& data) {
    static const char hex_digits[] = "0123456789abcdef";
    std::string hex_str;
    hex_str.reserve(data.size() * 2);
    for (unsigned char byte : data) {
        hex_str.push_back(hex_digits[byte >> 4]);
        hex_str.push_back(hex_digits[byte & 0x0F]);
    }
    return hex_str;
}

std::string to_string(const value& val)
{
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return ""; // NULL ??
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, blob>) {
            return details::blob_to_hex_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "TRUE" : "FALSE";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else {
            return "";
        }
    }, val);
}

blob to_blob(const value& val)
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
    }, val);
}

bool to_bool(const value& val)
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
    }, val);
}

int to_int(const value& val)
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
    }, val);
}

int64_t to_int64(const value& val)
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
    }, val);
}

double to_double(const value& val) {
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
    }, val);
}

std::optional<std::string> to_string_opt(const value& val)
{
    return std::visit([](auto&& arg) -> std::optional<std::string> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, blob>) {
            return details::blob_to_hex_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "TRUE" : "FALSE";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else {
            return {};
        }
    }, val);
}

std::optional<blob> to_blob_opt(const value& val) {
    return std::visit([&](auto&& arg) -> std::optional<blob> {
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
    }, val);
}

std::optional<bool> to_bool_opt(const value& val)
{
    return std::visit([&](auto&& arg) -> std::optional<bool>  {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::string>) {
            return arg == "TRUE" || arg == "true" || arg == "ON" || arg == "on" || arg == "1";
        } else if constexpr(std::is_same_v<T, blob>) {
            return !arg.empty();
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg;
        } else {
            return arg != 0;
        }
    }, val);
}

std::optional<int> to_int_opt(const value& val)
{
    return std::visit([&](auto&& arg) -> std::optional<int> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stoi(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return {};
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1 : 0;
        } else {
            return (int) arg;
        }
    }, val);
}

std::optional<int64_t> to_int64_opt(const value& val)
{
    return std::visit([&](auto&& arg) -> std::optional<int64_t> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stoll(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return {};
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1 : 0;
        } else {
            return (long long) arg;
        }
    }, val);
}

std::optional<double> to_double_opt(const value& val)
{
    return std::visit([&](auto&& arg) -> std::optional<double> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr(std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return {};
        } else if constexpr(std::is_same_v<T, std::string>) {
            return std::stod(arg);
        } else if constexpr(std::is_same_v<T, blob>) {
            return 0.0;
        } else if constexpr(std::is_same_v<T, bool>) {
            return arg ? 1.0 : 0.0;
        } else {
            return (double) arg;
        }
    }, val);
}

//
// Basic row
//

std::vector<value> row::get_values() const
{
    std::vector<value> res;
    for (size_t i=0; i<size(); ++i) {
        res.push_back(std::move(get_value(i)));
    }
    return res;
}

//
// Generic row
//

details::generic_row::generic_row(const row& row)
{
    size_t count = row.size();
    for(size_t i=0; i<count; ++i) {
        _values.push_back(row.get_value(i));
    }
}

details::generic_row::generic_row(const std::vector<value>& row):
    _values(row)
{
}


details::generic_row::generic_row(size_t count):
    _values(count)
{
}

value details::generic_row::get_value(unsigned index) const
{
    return index < _values.size() ? _values[index] : value{};
}

value& details::generic_row::get_value(unsigned int index)
{
    if (index >= _values.size()) {
        _values.resize(index + 1);
    }
    return _values[index];
}

std::string details::generic_row::get_value_string(unsigned index) const
{
    return index < _values.size() ? to_string(_values[index]) : "";
}

blob details::generic_row::get_value_blob(unsigned index) const
{
    return index < _values.size() ? to_blob(_values[index]) : blob{};
}

bool details::generic_row::get_value_bool(unsigned index) const
{
    return index < _values.size() ? to_bool(_values[index]) : false;
}

int details::generic_row::get_value_int(unsigned index) const
{
    return index < _values.size() ? to_int(_values[index]) : 0;
}

int64_t details::generic_row::get_value_int64(unsigned index) const
{
    return index < _values.size() ? to_int64(_values[index]) : 0;
}

double details::generic_row::get_value_double(unsigned index) const
{
    return index < _values.size() ? to_double(_values[index]) : .0;
}

//
// Generic buffered resultset
//

unsigned int details::generic_buffered_resultset::column_index(const std::string &name) const
{
    for (const auto& column : _columns) {
        if (column.name == name) {
            return column.index;
        }
    }
    return ~0u;
}

resultset_row_iterator details::generic_buffered_resultset::begin() const
{
    return {std::make_shared<generic_buffered_resultset_row_iterator_impl>(_rows.begin(), _rows.end())};
}

resultset_row_iterator details::generic_buffered_resultset::end() const
{
    return {std::make_shared<generic_buffered_resultset_row_iterator_impl>(_rows.end(), _rows.end())};
}

//
// generic_buffered_resultset_row_iterator_impl
//

const row& details::generic_buffered_resultset_row_iterator_impl::get() const
{
    return *_iter;
}

bool details::generic_buffered_resultset_row_iterator_impl::next()
{
    return (++_iter) != _end;
}

bool details::generic_buffered_resultset_row_iterator_impl::different(const resultset_row_iterator_impl &other) const
{
    if(auto impl = dynamic_cast<const generic_buffered_resultset_row_iterator_impl*>(&other) ; impl!=nullptr) {
        return _iter != impl->_iter;
    } else {
        return true;
    }
}



//
// Simple stats result
//

unsigned long long details::simple_stats_result::affected_rows() const
{
    return _affected_rows;
}

unsigned long long details::simple_stats_result::last_insert_id() const
{
    return _last_insert_id;
}


//
// SQLCPP statement
//

statement& statement::bind_null(const std::string& name)
{
    return bind(name, nullptr);
}

statement& statement::bind_null(unsigned int index)
{
    return bind(index, nullptr);
}

//
// SQLCPP resultset row iterator
//

resultset_row_iterator& resultset_row_iterator::operator++()
{
    if(_impl) {
        _impl->next();
    }
    return *this;
}

void resultset_row_iterator::operator++(int)
{
    if(_impl) {
        _impl->next();
    }
}

bool resultset_row_iterator::operator!=(const resultset_row_iterator& other) const
{
    if(!_impl) {
        return !!other._impl;
    } else if(!other._impl) {
        return true;
    } else {
        return _impl->different(*other._impl);
    }
}

const row& resultset_row_iterator::operator*() const
{
    if(_impl) {
        return _impl->get();
    } else {
        throw std::runtime_error("Invalid iterator");
    }
}

const row& resultset_row_iterator::operator->() const
{
    if(_impl) {
        return _impl->get();
    } else {
        throw std::runtime_error("Invalid iterator");
    }
}

} // namespace sqlcpp
