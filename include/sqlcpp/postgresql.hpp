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

#ifndef SQLCPP_POSTGRESQL_HPP
#define SQLCPP_POSTGRESQL_HPP

#include "../sqlcpp.hpp"

#include <postgresql/libpq-fe.h>

#include <string>
#

namespace sqlcpp::postgresql
{
    class connection : public sqlcpp::connection
    {
    protected:
        typedef sqlcpp::connection parent_t;

        std::shared_ptr<PGconn> _db;

    public:
        connection(PGconn* db);
        virtual ~connection();

        static std::shared_ptr<connection> create(const std::string& connection_string);

        void execute(const std::string& query) override;

        std::shared_ptr<sqlcpp::statement> prepare(const std::string& query) override;
    };


} // namespace sqlcpp::postgresql
#endif // SQLCPP_POSTGRESQL_HPP
