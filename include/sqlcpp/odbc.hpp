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

#ifndef SQLCPP_ODBC_HPP
#define SQLCPP_ODBC_HPP

#include "../sqlcpp.hpp"

#include <sql.h>
#include <sqlext.h>

#include <string>
#include <memory>

namespace sqlcpp::odbc
{
    class connection : public sqlcpp::connection, public std::enable_shared_from_this<connection>
    {
    protected:
        typedef sqlcpp::connection parent_t;

        SQLHENV _env;
        SQLHDBC _dbc;

    public:
        connection(SQLHENV env, SQLHDBC dbc);
        virtual ~connection();

        static std::shared_ptr<connection> create(const std::string& connection_string);

        void execute(const std::string& query) override;

        std::shared_ptr<sqlcpp::statement> prepare(const std::string& query) override;
    };

    class statement : public sqlcpp::statement
    {
    protected:
        typedef sqlcpp::statement parent_t;

        std::shared_ptr<connection> _connection;
        SQLHSTMT _stmt;
        std::string _query;

    public:
        statement(std::shared_ptr<connection> conn, SQLHSTMT stmt, const std::string& query);
        virtual ~statement();

        std::shared_ptr<sqlcpp::resultset> execute() override;

        unsigned int parameter_count() const override;
        int parameter_index(const std::string& name) const override;
        std::string parameter_name(unsigned int index) const override;

        sqlcpp::statement& bind(const std::string& name, std::nullptr_t) override;
        sqlcpp::statement& bind(const std::string& name, const std::string& value) override;
        sqlcpp::statement& bind(const std::string& name, const std::string_view& value) override;
        sqlcpp::statement& bind(const std::string& name, const blob& value) override;
        sqlcpp::statement& bind(const std::string& name, bool value) override;
        sqlcpp::statement& bind(const std::string& name, int value) override;
        sqlcpp::statement& bind(const std::string& name, int64_t value) override;
        sqlcpp::statement& bind(const std::string& name, double value) override;
        sqlcpp::statement& bind(const std::string& name, const value& value) override;

        sqlcpp::statement& bind(unsigned int index, std::nullptr_t) override;
        sqlcpp::statement& bind(unsigned int index, const std::string& value) override;
        sqlcpp::statement& bind(unsigned int index, const std::string_view& value) override;
        sqlcpp::statement& bind(unsigned int index, const blob& value) override;
        sqlcpp::statement& bind(unsigned int index, bool value) override;
        sqlcpp::statement& bind(unsigned int index, int value) override;
        sqlcpp::statement& bind(unsigned int index, int64_t value) override;
        sqlcpp::statement& bind(unsigned int index, double value) override;
        sqlcpp::statement& bind(unsigned int index, const value& value) override;
    };

    class row : public sqlcpp::row
    {
    protected:
        typedef sqlcpp::row parent_t;

        SQLHSTMT _stmt;
        unsigned int _column_count;

    public:
        row(SQLHSTMT stmt, unsigned int column_count);
        virtual ~row() = default;

        value get_value(unsigned int index) const override;

        std::string get_value_string(unsigned int index) const override;
        blob get_value_blob(unsigned int index) const override;
        bool get_value_bool(unsigned int index) const override;
        int get_value_int(unsigned int index) const override;
        int64_t get_value_int64(unsigned int index) const override;
        double get_value_double(unsigned int index) const override;
    };

    class resultset_row_iterator_impl : public sqlcpp::resultset_row_iterator_impl
    {
    protected:
        typedef sqlcpp::resultset_row_iterator_impl parent_t;

        SQLHSTMT _stmt;
        std::unique_ptr<row> _current_row;
        bool _has_data;
        unsigned int _column_count;

    public:
        resultset_row_iterator_impl(SQLHSTMT stmt, unsigned int column_count, bool has_data);
        virtual ~resultset_row_iterator_impl() = default;

        sqlcpp::row& get() override;
        bool next() override;
        bool different(const sqlcpp::resultset_row_iterator_impl& other) const override;
    };

    class resultset : public sqlcpp::resultset
    {
    protected:
        typedef sqlcpp::resultset parent_t;

        SQLHSTMT _stmt;
        unsigned int _column_count;
        unsigned int _row_count;
        bool _has_data;

    public:
        resultset(SQLHSTMT stmt);
        virtual ~resultset() = default;

        unsigned int column_count() const override;
        unsigned int row_count() const override;

        std::string column_name(unsigned int index) const override;
        unsigned int column_index(const std::string& name) const override;
        std::string column_origin_name(unsigned int index) const override;
        std::string table_origin_name(unsigned int index) const override;
        value_type column_type(unsigned int index) const override;

        bool has_row() const override;

        iterator begin() const override;
        iterator end() const override;
    };

} // namespace sqlcpp::odbc

#endif // SQLCPP_ODBC_HPP