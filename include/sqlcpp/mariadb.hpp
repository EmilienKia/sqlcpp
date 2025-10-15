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

#pragma once

#include "sqlcpp.hpp"
#include <mariadb/mysql.h>
#include <memory>
#include <string>

namespace sqlcpp::mariadb {

class mysql_statement;

class connection : public sqlcpp::connection
{
protected:
    std::shared_ptr<MYSQL> _db;

    std::shared_ptr<mysql_statement> _last_stmt;

public:
    static std::shared_ptr<connection> create(const std::string& connection_string);
    static std::shared_ptr<connection> create(const std::string& host,
                                              unsigned int port,
                                              const std::string& database,
                                              const std::string& username,
                                              const std::string& password);

    connection(MYSQL* db);
    virtual ~connection() = default;

    std::shared_ptr<sqlcpp::statement> prepare(const std::string& sql) override;
    std::shared_ptr<stats_result> execute(const std::string& sql) override;
};


} // namespace sqlcpp::mariadb