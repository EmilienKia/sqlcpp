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

#include "sqlcpp/postgresql.hpp"

#include <iostream>

static std::string connection_string = "postgresql://postgres:tartopom@localhost/testdb";

TEST_CASE("Simple PostgreSQL", "[postgresql]")
{
    auto db = sqlcpp::postgresql::connection::create(connection_string);
    REQUIRE( !!db );

    db->execute(
        "DROP TABLE IF EXISTS test;"
        "CREATE TABLE test (id SERIAL4 PRIMARY KEY, int64 BIGINT, double FLOAT8, text TEXT, blob BYTEA, bool BOOL);"
        "INSERT INTO test(int64, double, text, blob, bool) VALUES(1, 2.0, 'Hello', '\\x0102030461626364', TRUE);"
        "INSERT INTO test(int64, double, text, blob, bool) VALUES(2, 4.0, 'World', 'Hello', FALSE);"
        "INSERT INTO test(int64, double, text, blob, bool) VALUES(3, 8.0, '!!!', NULL, NULL);"
    );

    auto stmt = db->prepare("SELECT * FROM test");
    REQUIRE( !!stmt );

    auto rset = stmt->execute();
    REQUIRE( !!rset );

    REQUIRE( rset->column_count() == 6 );
    REQUIRE( rset->column_name(0) == "id" );
    REQUIRE( rset->column_name(1) == "int64" );
    REQUIRE( rset->column_name(2) == "double" );
    REQUIRE( rset->column_name(3) == "text" );
    REQUIRE( rset->column_name(4) == "blob" );
    REQUIRE( rset->column_name(5) == "bool" );

    // REQUIRE( rset->column_name(5) == "id" );

    REQUIRE( rset->column_index("id") == 0 );
    REQUIRE( rset->column_index("int64") == 1 );
    REQUIRE( rset->column_index("double") == 2 );
    REQUIRE( rset->column_index("text") == 3 );
    REQUIRE( rset->column_index("blob") == 4 );
    REQUIRE( rset->column_index("bool") == 5 );

    REQUIRE( rset->column_index("toto") == std::numeric_limits<unsigned int>::max() );
    REQUIRE( rset->column_index("toto") == (unsigned int)-1 );

    REQUIRE( rset->column_type(0) == sqlcpp::value_type::INT );
    REQUIRE( rset->column_type(1) == sqlcpp::value_type::INT64 );
    REQUIRE( rset->column_type(2) == sqlcpp::value_type::DOUBLE );
    REQUIRE( rset->column_type(3) == sqlcpp::value_type::STRING );
    REQUIRE( rset->column_type(4) == sqlcpp::value_type::BLOB );
    REQUIRE( rset->column_type(5) == sqlcpp::value_type::BOOL );

    REQUIRE( rset->row_count() == 3 );

    auto it = rset->begin();

    {
        auto& raw = *it;
        REQUIRE( std::holds_alternative<int>(raw.get_value(0)) );
        REQUIRE( std::holds_alternative<int64_t>(raw.get_value(1)) );
        REQUIRE( std::holds_alternative<double>(raw.get_value(2)) );
        REQUIRE( std::holds_alternative<std::string>(raw.get_value(3)) );
        REQUIRE( std::holds_alternative<sqlcpp::blob>(raw.get_value(4)) );
        REQUIRE( std::holds_alternative<bool>(raw.get_value(5)) );

        REQUIRE( std::get<int>(raw.get_value(0)) == 1 );
        REQUIRE( std::get<int64_t>(raw.get_value(1)) == 1 );
        REQUIRE( std::get<double>(raw.get_value(2)) == 2.0 );
        REQUIRE( std::get<std::string>(raw.get_value(3)) == "Hello" );
        REQUIRE( std::get<sqlcpp::blob>(raw.get_value(4)) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
        REQUIRE( std::get<bool>(raw.get_value(5)) == true );

        REQUIRE( raw.get_value_int(0) == 1 );
        REQUIRE( raw.get_value_int64(1) == 1 );
        REQUIRE( raw.get_value_double(2) == 2.0 );
        REQUIRE( raw.get_value_string(3) == "Hello" );
        REQUIRE( raw.get_value_blob(4) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
        REQUIRE( raw.get_value_bool(5) == true );
    }

    {
        auto& raw = *++it;

        REQUIRE( raw.get_value_int(0) == 2 );
        REQUIRE( raw.get_value_int64(1) == 2 );
        REQUIRE( raw.get_value_double(2) == 4.0 );
        REQUIRE( raw.get_value_string(3) == "World" );
        REQUIRE( raw.get_value_blob(4) == sqlcpp::blob{'H','e','l','l','o'} );
        REQUIRE( raw.get_value_bool(5) == false );
    }

    {
        auto& raw = *++it;

        REQUIRE( raw.get_value_int(0) == 3 );
        REQUIRE( raw.get_value_int64(1) == 3 );
        REQUIRE( raw.get_value_double(2) == 8.0 );
        REQUIRE( raw.get_value_string(3) == "!!!" );

        REQUIRE( std::holds_alternative<std::nullptr_t >(raw.get_value(4)) );
        REQUIRE( std::holds_alternative<std::nullptr_t >(raw.get_value(5)) );
    }

    // Cleanup
    db->execute("DROP TABLE test;");
}

TEST_CASE("PostgreSQL Variable Binding", "[postgresql][binding]")
{
    // Note: Connection string should be adapted to your PostgreSQL setup
    auto db = sqlcpp::postgresql::connection::create(connection_string);
    REQUIRE( !!db );

    // Create test table for binding tests
    db->execute(
        "DROP TABLE IF EXISTS binding_test;"
        "CREATE TABLE binding_test ("
        "id SERIAL PRIMARY KEY, "
        "int_val BIGINT, "
        "real_val DOUBLE PRECISION, "
        "text_val TEXT, "
        "blob_val BYTEA, "
        "bool_val BOOLEAN"
        ");"
    );

    SECTION("Bind by index")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES($1, $2, $3, $4, $5)");
        REQUIRE( !!stmt );

        stmt->bind(1, static_cast<int64_t>(42));
        stmt->bind(2, 3.14);
        stmt->bind(3, std::string("test"));
        stmt->bind(4, sqlcpp::blob{0x01, 0x02, 0x03});
        stmt->bind(5, true);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify inserted data
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val, bool_val FROM binding_test WHERE id = $1");
        select_stmt->bind(1, static_cast<int64_t>(1));
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;

        REQUIRE( row.get_value_int64(0) == 42 );
        REQUIRE( row.get_value_double(1) == 3.14 );
        REQUIRE( row.get_value_string(2) == "test" );
        REQUIRE( row.get_value_blob(3) == sqlcpp::blob{0x01, 0x02, 0x03} );
        REQUIRE( row.get_value_bool(4) == true );
    }

    SECTION("Bind by name")
    {
        // Note: Not implemented yet for PostgreSQL - using positional parameters
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES($1, $2, $3, $4, $5)");
        REQUIRE( !!stmt );

        // Note: Named binding not implemented yet for PostgreSQL
        // stmt->bind("int_val", static_cast<int64_t>(100));
        // stmt->bind("real_val", 2.71);
        // stmt->bind("text_val", std::string("named"));
        // stmt->bind("blob_val", sqlcpp::blob{0xAA, 0xBB});
        // stmt->bind("bool_val", false);

        // Using positional binding instead
        stmt->bind(1, static_cast<int64_t>(100));
        stmt->bind(2, 2.71);
        stmt->bind(3, std::string("named"));
        stmt->bind(4, sqlcpp::blob{0xAA, 0xBB});
        stmt->bind(5, false);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify inserted data
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val, bool_val FROM binding_test WHERE int_val = $1");
        select_stmt->bind(1, static_cast<int64_t>(100));
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;

        REQUIRE( row.get_value_int64(0) == 100 );
        REQUIRE( row.get_value_double(1) == 2.71 );
        REQUIRE( row.get_value_string(2) == "named" );
        REQUIRE( row.get_value_blob(3) == sqlcpp::blob{0xAA, 0xBB} );
        REQUIRE( row.get_value_bool(4) == false );
    }

    SECTION("Bind NULL values")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val) VALUES($1, $2, $3, $4)");
        REQUIRE( !!stmt );

        stmt->bind_null(1);
        stmt->bind_null(2);
        stmt->bind_null(3);
        stmt->bind_null(4);

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
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, text_val) VALUES($1, $2)");
        REQUIRE( !!stmt );

        // First execution
        stmt->bind(1, static_cast<int64_t>(1));
        stmt->bind(2, std::string("first"));
        stmt->execute();

        // Second execution with different values
        stmt->bind(1, static_cast<int64_t>(2));
        stmt->bind(2, std::string("second"));
        stmt->execute();

        // Verify both rows
        auto select_stmt = db->prepare("SELECT COUNT(*) FROM binding_test WHERE int_val IN ($1, $2)");
        select_stmt->bind(1, static_cast<int64_t>(1));
        select_stmt->bind(2, static_cast<int64_t>(2));
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;
        REQUIRE( row.get_value_int64(0) == 2 );
    }

    SECTION("Bind different integer types")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val) VALUES($1)");
        REQUIRE( !!stmt );

        // Test int
        stmt->bind(1, 123);
        stmt->execute();

        // Test int64_t
        stmt->bind(1, static_cast<int64_t>(456));
        stmt->execute();

        // Test int32_t
        stmt->bind(1, static_cast<int32_t>(789));
        stmt->execute();

        // Verify all values
        auto select_stmt = db->prepare("SELECT COUNT(*) FROM binding_test WHERE int_val IN ($1, $2, $3)");
        select_stmt->bind(1, 123);
        select_stmt->bind(2, static_cast<int64_t>(456));
        select_stmt->bind(3, static_cast<int32_t>(789));
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;
        REQUIRE( row.get_value_int64(0) == 3 );
    }

    // Cleanup
    db->execute("DROP TABLE binding_test;");
}
