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

#include "catch.hpp"

#include "sqlcpp/sqlcpp.hpp"

TEST_CASE("SQLite-linked factory connection creation", "[factory][sqlite]") {
    auto db = sqlcpp::connection::create("sqlite:memory:");
    REQUIRE( !!db );

    db->execute(
        "CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, int64 INT, double REAL, text TEXT, blob BLOB);" // STRICT
        "INSERT INTO test(int64, double, text, blob) VALUES(1, 2.0, 'Hello', X'0102030461626364');"
        "INSERT INTO test(int64, double, text, blob) VALUES(2, 4.0, 'World', 'Hello');"
        "INSERT INTO test(int64, double, text) VALUES(3, 8.0, '!!!');"
    );

    auto stmt = db->prepare("SELECT * FROM test");
    REQUIRE( !!stmt );

    auto rset = stmt->execute_buffered();
    REQUIRE( !!rset );
    REQUIRE( rset->column_count() == 5 );
    REQUIRE( rset->row_count() == 3 );

    // Cleanup
    db->execute("DROP TABLE test;");
}

TEST_CASE("Not found or invalid factory connection creation", "[factory]") {

    auto db = sqlcpp::connection::create("toto:memory:");
    REQUIRE( !db );

}

TEST_CASE("Installed postgres factory connection creation", "[factory][postgres]") {

    {
        auto db = sqlcpp::connection::create("postgresql://postgres:tartopom@localhost/testdb");
        REQUIRE( !!db );
    }

    {
        auto db = sqlcpp::connection::create("pg://postgres:tartopom@localhost/testdb");
        REQUIRE( !!db );
    }
}


TEST_CASE("Installed mariadb factory connection creation", "[factory][mariadb]") {

    {
        auto db = sqlcpp::connection::create("mariadb://mariadb:tartopom@localhost:3306/testdb");
        REQUIRE( !!db );
    }

    {
        auto db = sqlcpp::connection::create("my://mariadb:tartopom@localhost:3306/testdb");
        REQUIRE( !!db );
    }
}
