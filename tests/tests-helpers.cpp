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

#include <iostream>

#include "sqlcpp/details.hpp"

std::ostream& operator << ( std::ostream& os, sqlcpp::details::var_bind const& value ) {
    os << "vb{\"" << value.name << "\"," << value.index << "," << value.position << "}";
    return os;
}



#include "catch.hpp"


TEST_CASE("Query parser", "[parser]") {

    REQUIRE_THAT(
        sqlcpp::details::query_parser::split("SELECT * FROM table WHERE something=\"'$2\\\"@toto\" AND other='\"\\':$' and id=:023 OR name=$name or value=?"),
        Catch::Matchers::Contains(
            std::vector<sqlcpp::details::query_parser::part>{
                {sqlcpp::details::query_parser::part::VARIABLE, "", 23},
                {sqlcpp::details::query_parser::part::VARIABLE, "name", -1},
                {sqlcpp::details::query_parser::part::VARIABLE, "", -1}
            })
    );
}

TEST_CASE("Query splitter", "[parser]") {

    auto [query, symbol, exp_gen_req, exp_binds] = GENERATE(
        table<std::string, std::string, std::string, std::vector<sqlcpp::details::var_bind>> ({
        {"INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, ?, ?, ?, ?)",
            "?",
            "INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, ?, ?, ?, ?)",
            std::vector<sqlcpp::details::var_bind>{
            {"", 0, 0},
            {"", 1, 1},
            {"", 2, 2},
            {"", 3, 3},
            {"", 4, 4}
        }},

        {"INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, :, @, $, ?)",
            ":",
            "INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(:, :, :, :, :)",
            std::vector<sqlcpp::details::var_bind>{
            {"", 0, 0},
            {"", 1, 1},
            {"", 2, 2},
            {"", 3, 3},
            {"", 4, 4}
        }},

        {"INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?int, :real, @text, $blob, ?bool)",
            ":",
            "INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(:, :, :, :, :)",
            std::vector<sqlcpp::details::var_bind>{
            {"int", 0, 0},
            {"real", 1, 1},
            {"text", 2, 2},
            {"blob", 3, 3},
            {"bool", 4, 4}
        }},

        {"INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?0, :01, @0002, $3, ?4)",
            ":",
            "INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(:, :, :, :, :)",
            std::vector<sqlcpp::details::var_bind>{
            {"", 0, 0},
            {"", 1, 1},
            {"", 2, 2},
            {"", 3, 3},
            {"", 4, 4}
        }},

        {"INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(?, :, @3, $, ?6)",
            ":",
            "INSERT INTO binding_test(int_val, real_val, text_val, blob_val, bool_val) VALUES(:, :, :, :, :)",
            std::vector<sqlcpp::details::var_bind>{
            {"", 0, 0},
            {"", 1, 1},
            {"", 2, -1},
            {"", 3, 2},
            {"", 4, 3},
            {"", 5, -1},
            {"", 6, 4}
        }},
    }));

    auto [req, binds] = sqlcpp::details::query_parser::parse(query, symbol);
    REQUIRE( req == exp_gen_req );
    REQUIRE_THAT( binds, Catch::Matchers::Equals(exp_binds) );

}