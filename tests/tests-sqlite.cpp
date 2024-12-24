/*
 * Copyright (C) 2024 Emilien Kia <emilien.kia+dev@gmail.com>
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

#include "catch.hpp"

#include "sqlcpp/sqlite.hpp"

#include <iostream>

TEST_CASE("Simple SQLite", "[sqlite]")
{
    auto db = sqlcpp::sqlite::connection::create(":memory:");
    REQUIRE( !!db );

    db->execute(
        "CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, int64 INT64, double DOUBLE, text TEXT, blob BLOB);"
        "INSERT INTO test(int64, double, text) VALUES(1, 2.0, 'Hello');"
        "INSERT INTO test(int64, double, text) VALUES(2, 4.0, 'World');"
        "INSERT INTO test(int64, double, text) VALUES(3, 8.0, '!!!');"
    );

    auto stmt = db->prepare("SELECT * FROM test");
    REQUIRE( !!stmt );

    auto rset = stmt->execute();
    REQUIRE( !!rset );

    REQUIRE( rset->column_count() == 5 );
    REQUIRE( rset->column_name(0) == "id" );
    REQUIRE( rset->column_name(1) == "int64" );
    REQUIRE( rset->column_name(2) == "double" );
    REQUIRE( rset->column_name(3) == "text" );
    REQUIRE( rset->column_name(4) == "blob" );

    // REQUIRE( rset->column_name(5) == "id" );

    REQUIRE( rset->column_index("id") == 0 );
    REQUIRE( rset->column_index("int64") == 1 );
    REQUIRE( rset->column_index("double") == 2 );
    REQUIRE( rset->column_index("text") == 3 );
    REQUIRE( rset->column_index("blob") == 4 );

    // REQUIRE( rset->column_index("toto") == -1 );

    int count = 0;

    for(auto& row : *rset)
    {
        count++;

        std::cout << " >> ";
        for(int i = 0; i < 5; i++)
        {
            std::cout << " - " << row.get_value_string(i); 
        }
        std::cout << std::endl;


        if(count >= 4) {
            std::clog << "BREAK" << std::endl;
            break;
        }
    }


}
