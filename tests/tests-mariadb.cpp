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

#include "catch.hpp"

#include "sqlcpp/mariadb.hpp"

#include <iostream>

#include "sqlcpp/details.hpp"

static std::string connection_string = "mariadb://mariadb:tartopom@localhost:3306/testdb";

TEST_CASE("Simple MariaDB", "[mariadb]")
{
    auto db = sqlcpp::mariadb::connection::create(connection_string);
    REQUIRE( !!db );

    db->execute(
            "DROP TABLE IF EXISTS test; "
            "CREATE TABLE test (id INT AUTO_INCREMENT PRIMARY KEY, int64_val BIGINT, double_val DOUBLE, text_val TEXT, blob_val BLOB, bool_val BOOLEAN); "
            "INSERT INTO test(int64_val, double_val, text_val, blob_val, bool_val) VALUES(1, 2.0, 'Hello', UNHEX('0102030461626364'), TRUE); "
            "INSERT INTO test(int64_val, double_val, text_val, blob_val, bool_val) VALUES(2, 4.0, 'World', 'Hello', FALSE); "
            "INSERT INTO test(int64_val, double_val, text_val, blob_val, bool_val) VALUES(3, 8.0, '!!!', NULL, NULL); "
    );

    // Iterable execution
    SECTION("Iterable statement execution") {
        auto stmt = db->prepare("SELECT * FROM test");
        REQUIRE( !!stmt );

        auto rset = stmt->execute();
        REQUIRE( !!rset );

        REQUIRE( rset->column_count() == 6 );
        REQUIRE( rset->column_name(0) == "id" );
        REQUIRE( rset->column_name(1) == "int64_val" );
        REQUIRE( rset->column_name(2) == "double_val" );
        REQUIRE( rset->column_name(3) == "text_val" );
        REQUIRE( rset->column_name(4) == "blob_val" );
        REQUIRE( rset->column_name(5) == "bool_val" );

        REQUIRE( rset->column_index("id") == 0 );
        REQUIRE( rset->column_index("int64_val") == 1 );
        REQUIRE( rset->column_index("double_val") == 2 );
        REQUIRE( rset->column_index("text_val") == 3 );
        REQUIRE( rset->column_index("blob_val") == 4 );
        REQUIRE( rset->column_index("bool_val") == 5 );

        REQUIRE( rset->column_index("unknown") == std::numeric_limits<unsigned int>::max() );

        REQUIRE( rset->column_type(0) == sqlcpp::value_type::INT );
        REQUIRE( rset->column_type(1) == sqlcpp::value_type::INT64 );
        REQUIRE( rset->column_type(2) == sqlcpp::value_type::DOUBLE );
        REQUIRE( rset->column_type(3) == sqlcpp::value_type::STRING );
        REQUIRE( rset->column_type(4) == sqlcpp::value_type::BLOB );
        REQUIRE( rset->column_type(5) == sqlcpp::value_type::BOOL );

        auto it = rset->begin();

        {
            auto& row = *it;
            REQUIRE( std::holds_alternative<int>(row.get_value(0)) );
            REQUIRE( std::holds_alternative<int64_t>(row.get_value(1)) );
            REQUIRE( std::holds_alternative<double>(row.get_value(2)) );
            REQUIRE( std::holds_alternative<std::string>(row.get_value(3)) );
            REQUIRE( std::holds_alternative<sqlcpp::blob>(row.get_value(4)) );
            REQUIRE( std::holds_alternative<bool>(row.get_value(5)) );

            REQUIRE( std::get<int>(row.get_value(0)) == 1 );
            REQUIRE( std::get<int64_t>(row.get_value(1)) == 1 );
            REQUIRE( std::get<double>(row.get_value(2)) == 2.0 );
            REQUIRE( std::get<std::string>(row.get_value(3)) == "Hello" );
            REQUIRE( std::get<sqlcpp::blob>(row.get_value(4)) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( std::get<bool>(row.get_value(5)) == true );

            REQUIRE( row.get_value_int(0) == 1 );
            REQUIRE( row.get_value_int64(1) == 1 );
            REQUIRE( row.get_value_double(2) == 2.0 );
            REQUIRE( row.get_value_string(3) == "Hello" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( row.get_value_bool(5) == true );
        }

        {
            auto& row = *++it;

            REQUIRE( row.get_value_int(0) == 2 );
            REQUIRE( row.get_value_int64(1) == 2 );
            REQUIRE( row.get_value_double(2) == 4.0 );
            REQUIRE( row.get_value_string(3) == "World" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{'H','e','l','l','o'} );
            REQUIRE( row.get_value_bool(5) == false );
        }

        {
            auto& row = *++it;

            REQUIRE( row.get_value_int(0) == 3 );
            REQUIRE( row.get_value_int64(1) == 3 );
            REQUIRE( row.get_value_double(2) == 8.0 );
            REQUIRE( row.get_value_string(3) == "!!!" );

            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(4)) );
            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(5)) );
        }
    }


    // Buffered execution
    SECTION("Buffered statement execution"){
        auto stmt = db->prepare("SELECT * FROM test");
        REQUIRE( !!stmt );

        auto rset = stmt->execute_buffered();
        REQUIRE( !!rset );

        REQUIRE( rset->column_count() == 6 );
        REQUIRE( rset->column_name(0) == "id" );
        REQUIRE( rset->column_name(1) == "int64_val" );
        REQUIRE( rset->column_name(2) == "double_val" );
        REQUIRE( rset->column_name(3) == "text_val" );
        REQUIRE( rset->column_name(4) == "blob_val" );
        REQUIRE( rset->column_name(5) == "bool_val" );

        REQUIRE( rset->column_index("id") == 0 );
        REQUIRE( rset->column_index("int64_val") == 1 );
        REQUIRE( rset->column_index("double_val") == 2 );
        REQUIRE( rset->column_index("text_val") == 3 );
        REQUIRE( rset->column_index("blob_val") == 4 );
        REQUIRE( rset->column_index("bool_val") == 5 );

        REQUIRE( rset->column_index("unknown") == std::numeric_limits<unsigned int>::max() );

        REQUIRE( rset->column_type(0) == sqlcpp::value_type::INT );
        REQUIRE( rset->column_type(1) == sqlcpp::value_type::INT64 );
        REQUIRE( rset->column_type(2) == sqlcpp::value_type::DOUBLE );
        REQUIRE( rset->column_type(3) == sqlcpp::value_type::STRING );
        REQUIRE( rset->column_type(4) == sqlcpp::value_type::BLOB );
        REQUIRE( rset->column_type(5) == sqlcpp::value_type::BOOL );

        REQUIRE( rset->row_count() == 3 );

        {
            auto& row = rset->get_row(2);

            REQUIRE( row.get_value_int(0) == 3 );
            REQUIRE( row.get_value_int64(1) == 3 );
            REQUIRE( row.get_value_double(2) == 8.0 );
            REQUIRE( row.get_value_string(3) == "!!!" );

            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(4)) );
            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(5)) );
        }

        {
            auto& row = rset->get_row(1);

            REQUIRE( row.get_value_int(0) == 2 );
            REQUIRE( row.get_value_int64(1) == 2 );
            REQUIRE( row.get_value_double(2) == 4.0 );
            REQUIRE( row.get_value_string(3) == "World" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{'H','e','l','l','o'} );
            REQUIRE( row.get_value_bool(5) == false );
        }

        {
            auto& row = rset->get_row(0);
            REQUIRE( std::holds_alternative<int>(row.get_value(0)) );
            REQUIRE( std::holds_alternative<int64_t>(row.get_value(1)) );
            REQUIRE( std::holds_alternative<double>(row.get_value(2)) );
            REQUIRE( std::holds_alternative<std::string>(row.get_value(3)) );
            REQUIRE( std::holds_alternative<sqlcpp::blob>(row.get_value(4)) );
            REQUIRE( std::holds_alternative<bool>(row.get_value(5)) );

            REQUIRE( std::get<int>(row.get_value(0)) == 1 );
            REQUIRE( std::get<int64_t>(row.get_value(1)) == 1 );
            REQUIRE( std::get<double>(row.get_value(2)) == 2.0 );
            REQUIRE( std::get<std::string>(row.get_value(3)) == "Hello" );
            REQUIRE( std::get<sqlcpp::blob>(row.get_value(4)) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( std::get<bool>(row.get_value(5)) == true );

            REQUIRE( row.get_value_int(0) == 1 );
            REQUIRE( row.get_value_int64(1) == 1 );
            REQUIRE( row.get_value_double(2) == 2.0 );
            REQUIRE( row.get_value_string(3) == "Hello" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( row.get_value_bool(5) == true );
        }

    }


    // Callback execution
    SECTION("Callback statement execution"){
        auto stmt = db->prepare("SELECT * FROM test");
        REQUIRE( !!stmt );

        std::vector<sqlcpp::details::generic_row> results;

        stmt->execute([&](const sqlcpp::row_base& row) {
            results.emplace_back(row);
        });

        REQUIRE( results.size() == 3 );

        {
            const auto& row = results[2];

            REQUIRE( row.get_value_int(0) == 3 );
            REQUIRE( row.get_value_int64(1) == 3 );
            REQUIRE( row.get_value_double(2) == 8.0 );
            REQUIRE( row.get_value_string(3) == "!!!" );

            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(4)) );
            REQUIRE( std::holds_alternative<std::nullptr_t >(row.get_value(5)) );
        }

        {
            const auto& row = results[1];

            REQUIRE( row.get_value_int(0) == 2 );
            REQUIRE( row.get_value_int64(1) == 2 );
            REQUIRE( row.get_value_double(2) == 4.0 );
            REQUIRE( row.get_value_string(3) == "World" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{'H','e','l','l','o'} );
            REQUIRE( row.get_value_bool(5) == false );
        }

        {
            const auto& row = results[0];

            REQUIRE( std::holds_alternative<int>(row.get_value(0)) );
            REQUIRE( std::holds_alternative<int64_t>(row.get_value(1)) );
            REQUIRE( std::holds_alternative<double>(row.get_value(2)) );
            REQUIRE( std::holds_alternative<std::string>(row.get_value(3)) );
            REQUIRE( std::holds_alternative<sqlcpp::blob>(row.get_value(4)) );
            REQUIRE( std::holds_alternative<bool>(row.get_value(5)) );

            REQUIRE( std::get<int>(row.get_value(0)) == 1 );
            REQUIRE( std::get<int64_t>(row.get_value(1)) == 1 );
            REQUIRE( std::get<double>(row.get_value(2)) == 2.0 );
            REQUIRE( std::get<std::string>(row.get_value(3)) == "Hello" );
            REQUIRE( std::get<sqlcpp::blob>(row.get_value(4)) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( std::get<bool>(row.get_value(5)) == true );

            REQUIRE( row.get_value_int(0) == 1 );
            REQUIRE( row.get_value_int64(1) == 1 );
            REQUIRE( row.get_value_double(2) == 2.0 );
            REQUIRE( row.get_value_string(3) == "Hello" );
            REQUIRE( row.get_value_blob(4) == sqlcpp::blob{0x01, 0x02, 0x03, 0x04, 0x61, 0x62, 0x63, 0x64} );
            REQUIRE( row.get_value_bool(5) == true );
        }
    }

    // Cleanup
    db->execute("DROP TABLE test;");


}

TEST_CASE("MariaDB Variable Binding", "[mariadb][binding]")
{
    auto db = sqlcpp::mariadb::connection::create(connection_string);
    REQUIRE( !!db );

    // Create test table for binding tests
    db->execute(
            "DROP TABLE IF EXISTS binding_test;"
            "CREATE TABLE binding_test ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "int_val BIGINT, "
            "real_val DOUBLE, "
            "text_val TEXT, "
            "blob_val BLOB, "
            "bool_val BOOLEAN"
            ");"
    );

    SECTION("Bind by index")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, ?, ?, ?, ?)");
        REQUIRE( !!stmt );

        stmt->bind(1, static_cast<int64_t>(42));
        stmt->bind(2, 3.14);
        stmt->bind(3, std::string("test"));
        stmt->bind(4, sqlcpp::blob{0x01, 0x02, 0x03});
        stmt->bind(5, true);

        auto result = stmt->execute();
        REQUIRE( !!result );

        // Verify inserted data
        auto select_stmt = db->prepare("SELECT int_val, real_val, text_val, blob_val, bool_val FROM binding_test");
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

    SECTION("Bind NULL values")
    {
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, real_val, text_val, blob_val) VALUES(?, ?, ?, ?)");
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
        auto stmt = db->prepare("INSERT INTO binding_test(int_val, text_val) VALUES(?, ?)");
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
        auto select_stmt = db->prepare("SELECT COUNT(*) FROM binding_test WHERE int_val IN (?, ?)");
        select_stmt->bind(1, static_cast<int64_t>(1));
        select_stmt->bind(2, static_cast<int64_t>(2));
        auto rset = select_stmt->execute();
        REQUIRE( !!rset );

        auto it = rset->begin();
        auto& row = *it;
        REQUIRE( row.get_value_int64(0) == 2 );
    }

    // Cleanup
//    db->execute("DROP TABLE binding_test;");
}

