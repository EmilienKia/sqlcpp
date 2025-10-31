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

#ifndef SQLCPP_DETAILS_HPP
#define SQLCPP_DETAILS_HPP

#include <filesystem>
#include <map>

#include "sqlcpp.hpp"

namespace sqlcpp::details {

std::string blob_to_hex_string(const blob& data) ;


class simple_stats_result : public stats_result
{
protected:
    unsigned long long _affected_rows;
    unsigned long long _last_insert_id;

public:
    simple_stats_result(unsigned long long affected_rows, unsigned long long last_insert_id)
        : _affected_rows(affected_rows), _last_insert_id(last_insert_id) {
    }
    virtual ~simple_stats_result() = default;
    unsigned long long affected_rows() const override;
    unsigned long long last_insert_id() const override;
};


class generic_row : public row_base
{
protected:
    std::vector<value> _values;

public:
    generic_row() = default;
    generic_row(const generic_row&) = default;
    generic_row(generic_row&&) = default;
    explicit generic_row(size_t count);
    ~generic_row() override = default;
    generic_row& operator=(const generic_row&) = default;
    generic_row& operator=(generic_row&&) = default;

    explicit generic_row(const row_base&);
    explicit generic_row(const std::vector<value>&);

    size_t size() const override { return _values.size(); }

    std::vector<value> get_values() const override {
        return _values;
    }

    void add_value(const value& value) { _values.push_back(value); }
    void add_value(value&& value) { _values.push_back(std::move(value)); }

    void set_values(const std::vector<value>& values) { _values = values; }
    void set_values(std::vector<value>&& values) { _values = std::move(values); }

    value get_value(unsigned index) const override;
    value& get_value(unsigned int index);
    value& operator[](unsigned int index) { return get_value(index); }

    std::string get_value_string(unsigned index) const override;
    blob get_value_blob(unsigned index) const override;
    bool get_value_bool(unsigned index) const override;
    int get_value_int(unsigned index) const override;
    int64_t get_value_int64(unsigned index) const override;
    double get_value_double(unsigned index) const override;
};


class generic_buffered_resultset : public buffered_resultset
{
protected:

    struct column_info {
        std::string name;
        value_type type;
        size_t index;
        std::string origin_name;
        std::string table_origin_name;
    };

    std::vector<column_info> _columns;
    std::vector<generic_row> _rows;

    unsigned long long _affected_rows;
    unsigned long long _last_insert_id;

public:
    generic_buffered_resultset() = default;
    ~generic_buffered_resultset() override = default;


    void add_column(const std::string& name, value_type type, const std::string& origin_name, const std::string& table_origin_name) {
        _columns.push_back(column_info{.name = name, .type = type, .index = _columns.size(), .origin_name = origin_name, .table_origin_name = table_origin_name});
    }

    void add_row(const generic_row& row) {
        _rows.push_back(row);
    }

    void affected_rows(unsigned long long affected_rows) {
        _affected_rows = affected_rows;
    }

    unsigned long long affected_rows() const override {
        return _affected_rows;
    }

    void last_insert_id(unsigned long long last_insert_id) {
        _last_insert_id = last_insert_id;
    }

    unsigned long long last_insert_id() const override {
        return _last_insert_id;
    }

    unsigned int column_count() const override {
        return _columns.size();
    }

    std::string column_name(unsigned index) const override {
        return _columns[index].name;
    }

    unsigned int column_index(const std::string &name) const override;

    std::string column_origin_name(unsigned index) const override {
        return _columns[index].origin_name;
    }

    std::string table_origin_name(unsigned index) const override {
        return _columns[index].table_origin_name;
    }

    value_type column_type(unsigned index) const override {
        return _columns[index].type;
    }

    bool has_row() const override {
        return !_rows.empty();
    }

    const row_base& get_row(unsigned long long index) const override {
        return _rows.at(index);
    }

    iterator begin() const override ;
    iterator end() const override;

    unsigned int row_count() const override {
        return _rows.size();
    }

};

class generic_buffered_resultset_row_iterator_impl : public resultset_row_iterator_impl
{
protected:
    std::vector<generic_row>::const_iterator _iter;
    std::vector<generic_row>::const_iterator _end;
public:
    generic_buffered_resultset_row_iterator_impl(std::vector<generic_row>::const_iterator iter, std::vector<generic_row>::const_iterator end) : _iter(iter), _end(end) {}
    ~generic_buffered_resultset_row_iterator_impl() override = default;

    const row_base& get() const override;
    bool next() override;
    bool different(const resultset_row_iterator_impl &other) const override;
};


class connection_factory
{
protected:
    connection_factory() = default;

public:
    virtual ~connection_factory() = default;

    virtual std::vector<std::string> supported_schemes() const = 0;
    virtual std::shared_ptr<connection> do_create_connection(const std::string_view& url) = 0;
};

class connection_factory_registry {
protected:
    static connection_factory_registry _instance;

    std::map<std::string, std::shared_ptr<connection_factory>> _factories;
    std::shared_ptr<connection_factory> get_factory(const std::string& scheme);

    std::shared_ptr<connection_factory> lookup_for_factory(const std::string& scheme);
    std::shared_ptr<connection_factory> lookup_for_factory(const std::string& scheme, const std::filesystem::path& driver_dir_path);
    void load_factory_library(const std::filesystem::path& lib_path);

public:
    static connection_factory_registry& get();

    void register_factory(std::shared_ptr<connection_factory> factory);
    std::shared_ptr<connection> create_connection(const std::string_view& url);
};

}
#endif //SQLCPP_DETAILS_HPP