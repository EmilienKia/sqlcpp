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

#include "sqlcpp/sqlite.hpp"

#include <iostream>

TEST_CASE("Simple SQLite", "[sqlite]")
{
    auto db = sqlcpp::sqlite::connection::create(":memory:");
    REQUIRE( !!db );

    db->execute(
        "CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, int64 INT, double REAL, text TEXT, blob BLOB);" // STRICT
        "INSERT INTO test(int64, double, text, blob) VALUES(1, 2.0, 'Hello', X'0102030461626364');"
        "INSERT INTO test(int64, double, text, blob) VALUES(2, 4.0, 'World', 'Hello');"
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

    REQUIRE( rset->column_index("toto") == std::numeric_limits<unsigned int>::max() );
    REQUIRE( rset->column_index("toto") == (unsigned int)-1 );

    REQUIRE( rset->column_type(0) == sqlcpp::value_type::INT64 );
    REQUIRE( rset->column_type(1) == sqlcpp::value_type::INT64 );
    REQUIRE( rset->column_type(2) == sqlcpp::value_type::DOUBLE );
    REQUIRE( rset->column_type(3) == sqlcpp::value_type::STRING );

// NOTE : SQLite does not retrieve as BLOB type but as NULL if the value is NULL
// TODO Work on it
    REQUIRE( rset->column_type(4) == sqlcpp::value_type::BLOB );

// NOTE: row count not supported for SQLite as is
//    REQUIRE( rset->row_count() == 3 );

    auto it = rset->begin();

    {
        auto& raw = *it;
        REQUIRE( std::holds_alternative<int64_t>(raw.get_value(0)) );
        REQUIRE( std::holds_alternative<int64_t>(raw.get_value(1)) );
        REQUIRE( std::holds_alternative<double>(raw.get_value(2)) );
        REQUIRE( std::holds_alternative<std::string>(raw.get_value(3)) );
        REQUIRE( std::holds_alternative<sqlcpp::blob>(raw.get_value(4)) );

        REQUIRE( std::get<int64_t>(raw.get_value(0)) == 1 );
        REQUIRE( std::get<int64_t>(raw.get_value(1)) == 1 );
        REQUIRE( std::get<double>(raw.get_value(2)) == 2.0 );
        REQUIRE( std::get<std::string>(raw.get_value(3)) == "Hello" );
        REQUIRE( std::get<sqlcpp::blob>(raw.get_value(4)) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );

        REQUIRE( raw.get_value_int(0) == 1 );
        REQUIRE( raw.get_value_int64(0) == 1 );
        REQUIRE( raw.get_value_int64(1) == 1 );
        REQUIRE( raw.get_value_double(2) == 2.0 );
        REQUIRE( raw.get_value_string(3) == "Hello" );
        REQUIRE( raw.get_value_blob(4) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
    }

    {
        auto& raw = *++it;

        REQUIRE( raw.get_value_int(0) == 2 );
        REQUIRE( raw.get_value_int64(0) == 2 );
        REQUIRE( raw.get_value_int64(1) == 2 );
        REQUIRE( raw.get_value_double(2) == 4.0 );
        REQUIRE( raw.get_value_string(3) == "World" );
        REQUIRE( raw.get_value_blob(4) == sqlcpp::blob{'H', 'e', 'l', 'l', 'o'} );
    }

    {
        auto& raw = *++it;

        REQUIRE( raw.get_value_int(0) == 3 );
        REQUIRE( raw.get_value_int64(0) == 3 );
        REQUIRE( raw.get_value_int64(1) == 3 );
        REQUIRE( raw.get_value_double(2) == 8.0 );
        REQUIRE( raw.get_value_string(3) == "!!!" );

        REQUIRE( std::holds_alternative<std::nullptr_t >(raw.get_value(4)) );
    }

// TODO test boolean type
}

TEST_CASE("SQLite Variable Binding", "[sqlite][binding]")
{
    auto db = sqlcpp::sqlite::connection::create(":memory:");
    REQUIRE( !!db );

    // Create test table for binding tests
    db->execute(
        "CREATE TABLE binding_test ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "int_val INTEGER, "
        "real_val REAL, "
        "text_val TEXT, "
        "blob_val BLOB, "
        "bool_val INTEGER"
        ");"
    );

    SECTION("Bind by index")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, ?, ?, ?, ?)");
        REQUIRE( !!stmt );

        stmt->bind(0, static_cast<int64_t>(42));
        stmt->bind(1, 3.14);
        stmt->bind(2, std::string("test"));
        stmt->bind(3, sqlcpp::blob{0x01, 0x02, 0x03});
        stmt->bind(4, true);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify inserted data
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val, bool_val FROM binding_test WHERE id = 1");
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;

        REQUIRE( row.get_value_int64(0) == 42 );
        REQUIRE( row.get_value_double(1) == 3.14 );
        REQUIRE( row.get_value_string(2) == "test" );
        REQUIRE( row.get_value_blob(3) == sqlcpp::blob{0x01, 0x02, 0x03} );
        REQUIRE( row.get_value_int(4) == 1 );
    }

    SECTION("Bind by name")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(:int_val, :real_val, :text_val, :blob_val, :bool_val)");
        REQUIRE( !!stmt );

        stmt->bind(":int_val", static_cast<int64_t>(100));
        stmt->bind(":real_val", 2.71);
        stmt->bind(":text_val", std::string("named"));
        stmt->bind(":blob_val", sqlcpp::blob{0xAA, 0xBB});
        stmt->bind(":bool_val", false);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify inserted data
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val, bool_val FROM binding_test WHERE int_val = 100");
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;

        REQUIRE( row.get_value_int64(0) == 100 );
        REQUIRE( row.get_value_double(1) == 2.71 );
        REQUIRE( row.get_value_string(2) == "named" );
        REQUIRE( row.get_value_blob(3) == sqlcpp::blob{0xAA, 0xBB} );
        REQUIRE( row.get_value_int(4) == 0 );
    }

    SECTION("Bind NULL values")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val) VALUES(?, ?, ?, ?)");
        REQUIRE( !!stmt );

        stmt->bind_null(0);
        stmt->bind_null(1);
        stmt->bind_null(2);
        stmt->bind_null(3);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify NULL values
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val FROM binding_test WHERE int_val IS NULL");
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;

        REQUIRE( std::holds_alternative<std::nullptr_t>(row.get_value(0)) );
        REQUIRE( std::holds_alternative<std::nullptr_t>(row.get_value(1)) );
        REQUIRE( std::holds_alternative<std::nullptr_t>(row.get_value(2)) );
        REQUIRE( std::holds_alternative<std::nullptr_t>(row.get_value(3)) );
    }

    SECTION("Multiple executions with different bindings")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, text_val) VALUES(?, ?)");
        REQUIRE( !!stmt );

        // First execution
        stmt->bind(0, static_cast<int64_t>(1));
        stmt->bind(1, std::string("first"));
        stmt->execute();

        // Second execution with different values
        stmt->bind(0, static_cast<int64_t>(2));
        stmt->bind(1, std::string("second"));
        stmt->execute();

        // Verify both rows
        auto select_stmt = db->prepare("SELECT COUNT(*) FROM binding_test WHERE int_val IN (1, 2)");
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;
        REQUIRE( row.get_value_int64(0) == 2 );
    }

    SECTION("Bind different integer types")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val) VALUES(?)");
        REQUIRE( !!stmt );

        // Test int
        stmt->bind(0, 123);
        stmt->execute();

        // Test int64_t
        stmt->bind(0, static_cast<int64_t>(456));
        stmt->execute();

        // Test int32_t
        stmt->bind(0, static_cast<int32_t>(789));
        stmt->execute();

        // Verify all values
        auto select_stmt = db->prepare("SELECT COUNT(*) FROM binding_test WHERE int_val IN (123, 456, 789)");
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;
        REQUIRE( row.get_value_int64(0) == 3 );
    }
}

