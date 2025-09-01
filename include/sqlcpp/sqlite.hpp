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

#ifndef SQLCPP_SQLITE_HPP
#define SQLCPP_SQLITE_HPP

#include "../sqlcpp.hpp"

#include <sqlite3.h>

#include <string>

namespace sqlcpp::sqlite
{
    class connection : public sqlcpp::connection
    {
    protected:
        typedef sqlcpp::connection parent_t;

        sqlite3* _db;

    public:
        connection(sqlite3* db);
        virtual ~connection();

        static std::shared_ptr<connection> create(const std::string& connection_string);

        void execute(const std::string& query) override;

        std::shared_ptr<sqlcpp::statement> prepare(const std::string& query) override;
    };


} // namespace sqlcpp::sqlite
#endif // SQLCPP_SQLITE_HPP
