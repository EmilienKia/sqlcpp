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

#include "sqlcpp/postgresql.hpp"
#include "sqlcpp/details.hpp"

#include <postgresql/16/server/catalog/pg_type_d.h>

#include <string>
#include <iostream>
#include <limits>
#include <sstream>


/*
 * Refs:
 * - https://www.postgresql.org/docs/current/libpq.html
 * - https://www.postgresql.org/docs/current/libpq-connect.html
 * - https://www.postgresql.org/docs/current/libpq-status.html
 * - https://www.postgresql.org/docs/current/libpq-exec.html
 * - https://www.postgresql.org/docs/current/sql-prepare.html
 *
 * Implementation notes:
 * Postgres' methods PQcmdTuples(...) and PQoidValue(...) are really restrictive, and may return low or underestimated results.
 *
 * TODO:
 * - Implement generic bind by name
 * - Implement binary format
 */


namespace sqlcpp::postgresql
{

class statement;
class resultset;


class helpers
{
    helpers() = delete;
public:
    static blob parse_blob(const std::string_view& str);
    static value_type column_type_from_oid(Oid oid);
    static value get_value(PGresult* res, unsigned int row, unsigned int col);
};

blob helpers::parse_blob(const std::string_view& str) {
    if(str.size()>=2 && str[0] == '\\' && str[1] == 'x') {
        // Hex format
        blob res;
        res.reserve((str.size()-2)/2);
        for(size_t i=2 ; i<str.size() ; i+=2) {
            char c = 0;
            for(size_t j=0 ; j<2 && i+j<str.size() ; ++j) {
                c <<= 4;
                if(str[i+j] >= '0' && str[i+j] <= '9') {
                    c |= (str[i+j] - '0');
                } else if(str[i+j] >= 'a' && str[i+j] <= 'f') {
                    c |= (str[i+j] - 'a' + 10);
                } else if(str[i+j] >= 'A' && str[i+j] <= 'F') {
                    c |= (str[i+j] - 'A' + 10);
                } else {
                    // Invalid character
                    return {};
                }
            }
            res.push_back(c);
        }
        return res;
    } else {
        // Escape format
        blob res;
        res.reserve(str.size());
        for(size_t i=0 ; i<str.size() ; ++i) {
            if(str[i] == '\\') {
                if(i+1 < str.size()) {
                    if(str[i+1] >= '0' && str[i+1] <= '3') {
                        // Octal format
                        char c = 0;
                        for(size_t j=0 ; j<3 && i+1+j < str.size() && str[i+1+j]>='0' && str[i+1+j]<='7' ; ++j) {
                            c <<= 3;
                            c |= (str[i+1+j]-'0');
                        }
                        res.push_back(c);
                        i += 3;
                    } else if(str[i+1] == '\\') {
                        res.push_back('\\');
                        ++i;
                    } else {
                        // Invalid escape sequence
                        return {};
                    }
                } else {
                    // Invalid escape sequence
                    return {};
                }
            } else {
                res.push_back(str[i]);
            }
        }
        return res;
    }
}


value_type helpers::column_type_from_oid(Oid oid)
{
    switch(oid) {
        case BOOLOID:
            return value_type::BOOL;
        case INT2OID:
        case INT4OID:
            return value_type::INT;
        case INT8OID:
            return value_type::INT64;
        case FLOAT4OID:
        case FLOAT8OID:
            return value_type::DOUBLE;
        case TEXTOID:
        case VARCHAROID:
        case BPCHAROID:
        case NAMEOID:
        case CHAROID:
            return value_type::STRING;
        case BYTEAOID:
            return value_type::BLOB;
        default:
            return value_type::UNSUPPORTED;
    }
}


value helpers::get_value(PGresult* res, unsigned int row, unsigned int col) {
    if (PQgetisnull(res, row, col)) {
        return {nullptr};
    }

    bool isBinary = PQfformat(res, col) != 0;
    int size = PQgetlength(res, row, col);
    const char *val = PQgetvalue(res, row, col);

    if (isBinary) {
        switch(PQftype(res, col)) {
            case BOOLOID:
                // TODO EKI !!! What are the values ?
                return value_type::BOOL;
            case INT2OID:
            case INT4OID:
                switch(size) {
                    case sizeof(short):
                        return (int) *(const short *) val;
                    case sizeof(int):
                        return (int) *(const int *) val;
                    default:
                        // TODO log an error or an exception
                        return {};
                }
            case INT8OID:
                if(size == sizeof(int64_t)) {
                    return (int64_t) *(const int64_t *) val;
                } else {
                    // TODO log an error or an exception
                    return {};
                }
            case FLOAT4OID:
            case FLOAT8OID:
                switch(size) {
                    case sizeof(float):
                        return (double) *(const float *) val;
                    case sizeof(double ):
                        return (double) *(const double *) val;
                    default:
                        // TODO log an error or an exception
                        return {};
                }
            case TEXTOID:
            case VARCHAROID:
            case BPCHAROID:
            case NAMEOID:
            case CHAROID:
                return std::string(val, val+size);
            case BYTEAOID:
                // TODO To be tested ???
                return blob{val, val+size};

            default:
                return {};
        }
    } else {
        switch(PQftype(res, col)) {
            case BOOLOID:
                // TODO EKI !!! What are the values ?
                return *val == 't';
            case INT2OID:
            case INT4OID:
                return std::stoi(val);
            case INT8OID:
                return std::stoll(val);
            case FLOAT4OID:
            case FLOAT8OID:
                return std::stod(val);
            case TEXTOID:
            case VARCHAROID:
            case BPCHAROID:
            case NAMEOID:
            case CHAROID:
                return std::string(val, val+size);
            case BYTEAOID:
                return helpers::parse_blob(std::string_view(val, size));
            default:
                return {};
        }
    }

}




//
// PostgreSQL's resultset iterator
//

class resultset_row_iterator_impl : public sqlcpp::resultset_row_iterator_impl, protected row
{
protected:
    std::shared_ptr<PGresult> _stmt;
    size_t _row = 0;

public:
    resultset_row_iterator_impl(std::shared_ptr<PGresult> stmt):
        _stmt(stmt)
    {}

    virtual ~resultset_row_iterator_impl() = default;

    bool ok() const;
    operator bool() const { return ok(); }

    const row& get() const override;
    bool next() override;
    bool different(const sqlcpp::resultset_row_iterator_impl& other) const override;

    size_t size() const override;

    value get_value(unsigned int index) const override;

    std::string get_value_string(unsigned int index) const override;
    blob get_value_blob(unsigned int index) const override;
    bool get_value_bool(unsigned int index) const override;
    int get_value_int(unsigned int index) const override;
    int64_t get_value_int64(unsigned int index) const override;
    double get_value_double(unsigned int index) const override;
};

const row& resultset_row_iterator_impl::get() const
{
    return *this;
}

bool resultset_row_iterator_impl::next()
{
    return _stmt && ++_row < PQntuples(_stmt.get());
}

bool resultset_row_iterator_impl::ok() const
{
    return _stmt && _row < PQntuples(_stmt.get());
}

bool resultset_row_iterator_impl::different(const sqlcpp::resultset_row_iterator_impl& other) const
{
    if(auto impl = dynamic_cast<const resultset_row_iterator_impl*>(&other) ; impl!=nullptr) {
        if(!ok() && !impl->ok()) {
            // Both invalid, consider they are the same
            return false;
        } else {
            // Both valid, compare stmt and row
            return _stmt != impl->_stmt || _row != impl->_row;
        }
    } else {
        // Not the same type, obviously different
        return true;
    }
}

size_t resultset_row_iterator_impl::size() const
{
    return PQnfields(_stmt.get());
}

value resultset_row_iterator_impl::get_value(unsigned int index) const
{
    return helpers::get_value(_stmt.get(), _row, index);
}

std::string resultset_row_iterator_impl::get_value_string(unsigned int index) const 
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return "NULL";
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        // TODO To be tested ???
        return {val, val+size};
    } else {
        return {val, val+size};
    }
}

blob resultset_row_iterator_impl::get_value_blob(unsigned int index) const 
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return {};
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        // TODO To be tested ???
        return {val, val+size};
    } else {
        return helpers::parse_blob(std::string_view(val, size));
    }
}

bool resultset_row_iterator_impl::get_value_bool(unsigned int index) const
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return false;
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        // TODO Todo ???
        return false;
    } else {
        return *val == 't';
    }
}

int resultset_row_iterator_impl::get_value_int(unsigned int index) const 
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return 0;
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        switch(size) {
            case sizeof(short):
                return (int) *(const short *) val;
            case sizeof(int):
                return (int) *(const int *) val;
            default:
                // TODO log an error or an exception
                return 0;
        }
    } else {
        return std::stoi(val);
    }
}

int64_t resultset_row_iterator_impl::get_value_int64(unsigned int index) const 
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return 0;
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        if(size == sizeof(int64_t)) {
            return (int64_t) *(const int64_t *) val;
        } else {
            // TODO log an error or an exception
            return 0;
        }
    } else {
        return std::stoll(val);
    }
}

double resultset_row_iterator_impl::get_value_double(unsigned int index) const 
{
    if (PQgetisnull(_stmt.get(), _row, index)) {
        return 0;
    }

    bool isBinary = PQfformat(_stmt.get(), index) != 0;
    int size = PQgetlength(_stmt.get(), _row, index);
    const char *val = PQgetvalue(_stmt.get(), _row, index);

    if(isBinary) {
        switch(size) {
            case sizeof(float):
                return (double) *(const float *) val;
            case sizeof(double ):
                return (double) *(const double *) val;
            default:
                // TODO log an error or an exception
                return {};
        }
    } else {
        return std::stod(val);
    }
}



//
// PostgreSQL's resultset
//
class resultset : public sqlcpp::cursor_resultset
{
protected:
    std::shared_ptr<PGresult> _res;

public:
    resultset(PGresult* res) :
        _res(res, PQclear)
        {}

    virtual ~resultset() = default;

    unsigned long long affected_rows() const override;
    unsigned long long last_insert_id() const override;

    unsigned int column_count() const override;
    unsigned int row_count() const /*override*/;

    std::string column_name(unsigned int index) const override;
    unsigned int column_index(const std::string& name) const override;
    std::string column_origin_name(unsigned int index) const override;
    std::string table_origin_name(unsigned int index) const override;
    value_type column_type(unsigned int index) const override;

    bool has_row() const override;

    sqlcpp::resultset_row_iterator begin() const override;
    sqlcpp::resultset_row_iterator end() const override;

};

unsigned long long resultset::affected_rows() const {
    std::string str = PQcmdTuples(_res.get());
    return str.empty() ? 0 : std::stoull(str);
}

unsigned long long resultset::last_insert_id() const {
    return PQoidValue(_res.get());
}

unsigned int resultset::column_count() const
{
    int res = PQnfields(_res.get());
    return res;
}

unsigned int resultset::row_count() const
{
    return PQntuples(_res.get());
}

std::string resultset::column_name(unsigned int index) const
{
    return PQfname(_res.get(), index);
}

unsigned int resultset::column_index(const std::string& name) const
{
    int res = PQfnumber(_res.get(), name.c_str());
    if(res >= 0) {
        return res;
    }
    return std::numeric_limits<unsigned int>::max();
}

std::string resultset::column_origin_name(unsigned int index) const 
{
    // Not supported directly for PostgreSQL
    // Look at PQftablecol
    // TODO
    return "";
}

std::string resultset::table_origin_name(unsigned int index) const
{
    // Not supported for PostgreSQL
    // Look at PQftable
    // TODO
    return "";
}

value_type resultset::column_type(unsigned int index) const
{
    return helpers::column_type_from_oid(PQftype(_res.get(), index));
}

bool resultset::has_row() const
{
    ExecStatusType type = PQresultStatus(_res.get());
    return (type == PGRES_TUPLES_OK
// TODO Support single-row or tuple chunk modes
//            || type == PGRES_SINGLE_TUPLE || type == PGRES_TUPLES_CHUNK

            )
        && PQntuples(_res.get()) > 0;
}

sqlcpp::resultset_row_iterator resultset::begin() const
{
    return std::move(sqlcpp::cursor_resultset::create_iterator(std::make_unique<resultset_row_iterator_impl>(_res)));
}

sqlcpp::resultset_row_iterator resultset::end() const
{
    return std::move(sqlcpp::cursor_resultset::create_iterator(std::make_unique<resultset_row_iterator_impl>(nullptr)));
}



//
// PostgreSQL's statement
//

class statement : public sqlcpp::statement
{
protected:
    std::weak_ptr<PGconn> _db;
    std::string _stmt_name;
    mutable std::shared_ptr<PGresult> _stmt_info;
    std::vector<value> _params;

    PGresult* execute_prepared();

public:
    explicit statement(std::shared_ptr<PGconn> db, const std::string& stmt_name) :
        _db(db), _stmt_name(stmt_name)
        {}

    virtual ~statement() {}

    std::shared_ptr<sqlcpp::cursor_resultset> execute() override;
    void execute(std::function<void(const row&)> func) override;
    std::shared_ptr<sqlcpp::buffered_resultset> execute_buffered() override;

    unsigned int parameter_count() const override;
    int parameter_index(const std::string& name) const override;
    std::string parameter_name(unsigned int index) const override;

    statement& bind(const std::string& name, std::nullptr_t) override;
    statement& bind(const std::string& name, const std::string& value) override;
    statement& bind(const std::string& name, const std::string_view& value) override;
    statement& bind(const std::string& name, const blob& value) override;
    statement& bind(const std::string& name, bool value) override;
    statement& bind(const std::string& name, int value) override;
    statement& bind(const std::string& name, int64_t value) override;
    statement& bind(const std::string& name, double value) override;
    statement& bind(const std::string& name, const value& value) override;

    statement& bind(unsigned int index, std::nullptr_t) override;
    statement& bind(unsigned int index, const std::string& value) override;
    statement& bind(unsigned int index, const std::string_view& value) override;
    statement& bind(unsigned int index, const blob& value) override;
    statement& bind(unsigned int index, bool value) override;
    statement& bind(unsigned int index, int value) override;
    statement& bind(unsigned int index, int64_t value) override;
    statement& bind(unsigned int index, double value) override;
    statement& bind(unsigned int index, const value& value) override;
};

PGresult* statement::execute_prepared()
{
    size_t sz = _params.size();

    std::vector<std::string> values;
    values.reserve(sz);
    std::vector<const char*> rc;
    rc.reserve(sz);

    for(int i=1; i<sz; i++) {
        const auto& v = _params[i];
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::is_same_v<T, std::monostate>) {
                values.emplace_back("");
                rc.push_back(nullptr);
            } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
                values.emplace_back("");
                rc.push_back(nullptr);
            } else if constexpr(std::is_same_v<T, std::string>) {
                values.push_back(arg);
                rc.push_back(values.back().c_str());
            } else if constexpr(std::is_same_v<T, blob>) {
                values.push_back("\\x" + details::blob_to_hex_string(arg));
                rc.push_back(values.back().c_str());
            } else if constexpr(std::is_same_v<T, bool>) {
                values.push_back(arg ? "TRUE" : "FALSE");
                rc.push_back(values.back().c_str());
            } else {
                values.push_back(std::to_string(arg));
                rc.push_back(values.back().c_str());
            }
        }, v);
    }

    return PQexecPrepared(_db.lock().get(), _stmt_name.c_str(), rc.size(), rc.data(), nullptr, nullptr, 0);
}

std::shared_ptr<sqlcpp::cursor_resultset> statement::execute()
{
    PGresult *res = execute_prepared();
    // TODO support binary format for slight better performances
    switch(PQresultStatus(res)) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK:
            return std::make_shared<resultset>(res);
        default:
            std::cerr << "Failed to execute statement: " << PQerrorMessage(_db.lock().get()) << std::endl;
            PQclear(res);
            // TODO throw exception
            return {};
    }
}

void statement::execute(std::function<void(const row&)> func)
{
    PGresult *res = execute_prepared();
    // TODO support binary format for slight better performances
    switch(PQresultStatus(res)) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK: {
            int col_count = PQnfields(res);
            int row_count = PQntuples(res);
            for (int row_index = 0; row_index < row_count; ++row_index) {
                details::generic_row row;
                for (int col_index = 0; col_index < col_count; ++col_index) {
                    row.add_value(helpers::get_value(res, row_index, col_index));
                }
                func(row);
            }
            PQclear(res);
            break;
        }
        default:
            std::cerr << "Failed to execute statement: " << PQerrorMessage(_db.lock().get()) << std::endl;
            // TODO throw exception
            PQclear(res);
    }
}

std::shared_ptr<sqlcpp::buffered_resultset> statement::execute_buffered()
{
    PGresult *res = execute_prepared();
    // TODO support binary format for slight better performances
    switch(PQresultStatus(res)) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK: {
            auto buff = std::make_shared<details::generic_buffered_resultset>();

            std::string affected_rows_str = PQcmdTuples(res);
            unsigned long long affected_rows = affected_rows_str.empty() ? 0 : std::stoull(affected_rows_str);
            unsigned long long last_insert_id = PQoidValue(res);

            int col_count = PQnfields(res);
            for (int index = 0; index < col_count; ++index) {
                std::string col_name = PQfname(res, index);
                value_type col_type = helpers::column_type_from_oid(PQftype(res, index));
                std::string column_origin_name; // TODO not supported directly by PGSQL
                std::string table_origin_name; // TODO not supported directly by PGSQL
                buff->add_column(col_name, col_type, column_origin_name, table_origin_name);
            }

            int row_count = PQntuples(res);
            for (int row_index = 0; row_index < row_count; ++row_index) {
                details::generic_row row;
                for (int col_index = 0; col_index < col_count; ++col_index) {
                    row.add_value(helpers::get_value(res, row_index, col_index));
                }
                buff->add_row(std::move(row));
            }
            PQclear(res);
            return buff;
        }
        default:
            std::cerr << "Failed to execute statement: " << PQerrorMessage(_db.lock().get()) << std::endl;
            PQclear(res);
            // TODO throw exception
            return {};
    }

    return {};
}

unsigned int statement::parameter_count() const
{
    if(!_stmt_info) {
        PGresult* res = PQdescribePrepared(_db.lock().get(), _stmt_name.c_str());
        if(PQresultStatus(_stmt_info.get()) != PGRES_COMMAND_OK) {
            // TODO process error, throw exception
            return 0;
        }
        _stmt_info = std::shared_ptr<PGresult>(res , PQclear);
    }
    return PQnparams(_stmt_info.get());
}

int statement::parameter_index(const std::string& name) const 
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return -1;
}

std::string statement::parameter_name(unsigned int index) const 
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return "";
}

statement& statement::bind(const std::string& name, std::nullptr_t) 
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const std::string& value)
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const std::string_view& value)
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const blob& value) 
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, bool value)
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, int value)
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, int64_t value)  
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, double value)  
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
    return *this;
}

statement& statement::bind(const std::string& name, const value& value)  
{
    // Parameter names not supported for PostgreSQL yet
    // TODO
//    std::visit([&](auto&& arg) {
//        bind(name, arg);
//    }, value);
    return *this;
}

static inline std::vector<value>& ensure(std::vector<value>& params, unsigned int index)
{
    if(params.size() <= index) {
        params.resize(index + 1);
    }
    return params;
}


statement& statement::bind(unsigned int index, std::nullptr_t)  
{
    ensure(_params, index)[index] = nullptr;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string& value)  
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const std::string_view& value)  
{
    ensure(_params, index)[index] = std::string(value);
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const blob& value)  
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, bool value)
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int value)
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, int64_t value)  
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, double value)  
{
    ensure(_params, index)[index] = value;
    // TODO process error, throw exception
    return *this;
}

statement& statement::bind(unsigned int index, const value& value)  
{
    std::visit([&](auto&& arg) {
        bind(index, arg);
    }, value);
    return *this;
}

//
// PostgreSQL's connection
//

connection::connection(PGconn* db):
_db(db, &PQfinish)
{
}

connection::~connection()
{
}

std::shared_ptr<connection> connection::create(const std::string& connection_string)
{
    PGconn* db = PQconnectdb(connection_string.c_str());
    if(ConnStatusType status = PQstatus(db); status!=CONNECTION_OK) {
        PQfinish(db);
        // TODO throw exception
        return {};
    }
    return std::make_shared<connection>(db);
}

std::shared_ptr<stats_result> connection::execute(const std::string& query)
{
    char* err_msg = nullptr;

    PGresult* res = PQexec(_db.get(), query.c_str());
    switch(PQresultStatus(res)) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK: {
            std::string str = PQcmdTuples(res);
            unsigned long long affected_rows = !str.empty() ? std::stoull(str) : 0;
            Oid last_inserted = PQoidValue(res);
            PQclear(res);
            return std::make_shared<details::simple_stats_result>(affected_rows, last_inserted);
        }
        default:
            std::cerr << "Failed to execute statement: " << PQerrorMessage(_db.get()) << std::endl;
            PQclear(res);
            // TODO throw exception
            return nullptr;
    }
}

std::shared_ptr<sqlcpp::statement> connection::prepare(const std::string& query)
{
    static unsigned int count = 0;
    std::ostringstream oss;
    oss << "prepared-" << count++;
    std::string stmt_name = oss.str(); // TODO Generate a unique statement name
    PGresult* res = PQprepare(_db.get(), stmt_name.c_str(), query.c_str(), 0, nullptr);
    switch(PQresultStatus(res)) {
        case PGRES_COMMAND_OK:
            PQclear(res);
            return std::make_shared<statement>(_db, stmt_name);
        default:
            std::cerr << "Failed to prepare statement: " << PQerrorMessage(_db.get()) << std::endl;
            PQclear(res);
            // TODO throw exception
            return {};
    }
}

} // namespace sqlcpp::postgresql
