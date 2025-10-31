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

#include <sqlcpp/sqlcpp.hpp>

#include <iostream>

int main() {

    auto db = sqlcpp::connection::create("sqlite::memory:");

    if (!db) {
        std::cerr << "Failed to create connection" << std::endl;
    }

    db->execute(
        "CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, int64 INT, double REAL, text TEXT, blob BLOB);"
        "INSERT INTO test(int64, double, text, blob) VALUES(1, 2.0, 'Hello', X'0102030461626364');"
        "INSERT INTO test(int64, double, text, blob) VALUES(2, 4.0, 'World', 'Hello');"
        "INSERT INTO test(int64, double, text) VALUES(3, 8.0, '!!!');"
    );

    {
        auto stmt = db->prepare("SELECT * FROM test");

        stmt->execute([&](const sqlcpp::row& row) {
            std::cout
                << "id="    << sqlcpp::to_string(row[0]) << " "
                << "int64=" << sqlcpp::to_string(row[1]) << " "
                << "double="<< sqlcpp::to_string(row[2]) << " "
                << "text="  << sqlcpp::to_string(row[3]) << " "
                << "blob="  << sqlcpp::to_string(row[4]) << " "
                << std::endl;
        });
    }

    {
        auto stmt = db->prepare("SELECT * FROM test");
        auto rset = stmt->execute();
        for (const sqlcpp::row& row : *rset) {
            std::cout
                << "id="    << sqlcpp::to_string(row[0]) << " "
                << "int64=" << sqlcpp::to_string(row[1]) << " "
                << "double="<< sqlcpp::to_string(row[2]) << " "
                << "text="  << sqlcpp::to_string(row[3]) << " "
                << "blob="  << sqlcpp::to_string(row[4]) << " "
                << std::endl;

        }
    }


    return 0;
}